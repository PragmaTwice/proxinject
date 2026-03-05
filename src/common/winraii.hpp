// Copyright 2022 PragmaTwice
//
// Licensed under the Apache License,
// Version 2.0(the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PROXINJECT_COMMON_WINRAII
#define PROXINJECT_COMMON_WINRAII

#include "tlhelp32.h"
#include "utils.hpp"
#include <Windows.h>
#include <memory>
#include <optional>

template <auto f> struct static_function {
  template <typename T> decltype(auto) operator()(T &&x) const {
    return f(std::forward<T>(x));
  }
};

struct handle : std::unique_ptr<void, static_function<CloseHandle>> {
  using base_type = std::unique_ptr<void, static_function<CloseHandle>>;

  using base_type::base_type;

  handle(HANDLE hd) : base_type(hd) {}
};

struct virtual_memory {
  void *const proc_handle;
  void *const mem_addr;
  const SIZE_T size_;

  virtual_memory(void *proc_handle, SIZE_T size)
      : proc_handle(proc_handle),
        mem_addr(VirtualAllocEx(proc_handle, nullptr, size,
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)),
        size_(size) {}

  virtual_memory(const virtual_memory &) = delete;
  virtual_memory(virtual_memory &&) = default;

  ~virtual_memory() {
    if (mem_addr) {
      VirtualFreeEx(proc_handle, mem_addr, 0, MEM_RELEASE);
    }
  }

  operator bool() const { return mem_addr != nullptr; }

  void *get() const { return mem_addr; }

  void *process_handle() const { return proc_handle; }

  SIZE_T size() const { return size_; }

  std::optional<SIZE_T> write(const void *buf, SIZE_T n) const {
    SIZE_T written_size;

    if (WriteProcessMemory(proc_handle, mem_addr, buf, n, &written_size)) {
      return written_size;
    }

    return std::nullopt;
  }

  auto write(const void *buf) { return write(buf, size_); }

  std::optional<SIZE_T> read(void *buf, SIZE_T n) const {
    SIZE_T read_size;

    if (ReadProcessMemory(proc_handle, mem_addr, buf, n, &read_size)) {
      return read_size;
    }

    return std::nullopt;
  }

  auto read(void *buf) { return read(buf, size_); }
};

HMODULE get_current_module() {
  HMODULE mod = nullptr;

  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                     (LPCTSTR)get_current_module, &mod);

  return mod;
}

std::wstring get_current_filename() {
  wchar_t path[1024 + 1] = {};

  GetModuleFileNameW(get_current_module(), path, 1024);

  return path;
}

template <typename T> struct scope_ptr_bind {
  T *&ptr;

  scope_ptr_bind(T *&ptr, T *bind) : ptr(ptr) { ptr = bind; }
  ~scope_ptr_bind() { ptr = nullptr; }
};

template <typename F> void match_process(F &&f) {
  if (handle snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) {
    PROCESSENTRY32W entry = {sizeof(PROCESSENTRY32W)};
    if (Process32FirstW(snapshot.get(), &entry)) {
      do {
        std::forward<F>(f)(entry);
      } while (Process32NextW(snapshot.get(), &entry));
    }
  }
}

inline handle create_mapping(const std::wstring &name, DWORD buf_size) {
  return CreateFileMappingW(INVALID_HANDLE_VALUE, // use paging file
                            NULL,                 // default security
                            PAGE_READWRITE,       // read/write access
                            0,        // maximum object size (high-order DWORD)
                            buf_size, // maximum object size (low-order DWORD)
                            name.c_str()); // name of mapping object
}

inline handle open_mapping(const std::wstring &name) {
  return OpenFileMappingW(FILE_MAP_ALL_ACCESS, // read/write access
                          FALSE,               // do not inherit the name
                          name.c_str());       // name of mapping object
}

struct mapped_buffer : std::unique_ptr<void, static_function<UnmapViewOfFile>> {
  using base_type = std::unique_ptr<void, static_function<UnmapViewOfFile>>;

  mapped_buffer(HANDLE mapping, DWORD offset = 0, SIZE_T size = 0)
      : base_type(MapViewOfFile(mapping,             // handle to map object
                                FILE_MAP_ALL_ACCESS, // read/write permission
                                0, offset, size)) {}
};

inline std::optional<std::wstring> get_process_filepath(DWORD pid) {
  handle process =
      OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

  DWORD size = MAX_PATH;
  auto filename = std::make_unique<wchar_t[]>(size);
  if (!QueryFullProcessImageNameW(process.get(), 0, filename.get(), &size)) {
    return std::nullopt;
  }

  return std::wstring(filename.get(), size);
}

inline auto get_process_name(DWORD pid) {
  std::wstring result;
  match_process([pid, &result](const PROCESSENTRY32W &entry) {
    if (pid == entry.th32ProcessID) {
      result = entry.szExeFile;
    }
  });

  std::string res_u8 = utf8_encode(result);
  if (res_u8.ends_with(".exe")) {
    return res_u8.substr(0, res_u8.size() - 4);
  }
  return res_u8;
}

template <typename F> void match_process_by_name(F &&f) {
  match_process([&f](const PROCESSENTRY32W &entry) {
    std::string name_u8 = utf8_encode(entry.szExeFile);
    if (name_u8.ends_with(".exe")) {
      auto name = name_u8.substr(0, name_u8.size() - 4);
      std::forward<F>(f)(name, entry.th32ProcessID);
    }
  });
}

template <typename F> void match_process_by_path(F &&f) {
  match_process([&f](const PROCESSENTRY32W &entry) {
    if (auto wpath = get_process_filepath(entry.th32ProcessID)) {
      auto path = utf8_encode(*wpath);
      std::forward<F>(f)(path, entry.th32ProcessID);
    }
  });
}

template <typename F> void enumerate_child_pids(DWORD pid, F &&f) {
  match_process([pid, &f](const PROCESSENTRY32W &entry) {
    if (pid == entry.th32ParentProcessID) {
      std::forward<F>(f)(entry.th32ProcessID);
    }
  });
}

inline std::optional<PROCESS_INFORMATION>
create_process(const std::wstring &command, DWORD creation_flags = 0) {
  STARTUPINFO startup_info{};
  PROCESS_INFORMATION process_info{};
  if (CreateProcessW(nullptr, std::wstring{command}.data(), nullptr, nullptr,
                     false, creation_flags, nullptr, nullptr, &startup_info,
                     &process_info) == 0) {
    return std::nullopt;
  }

  return process_info;
}

inline std::optional<PROCESS_INFORMATION>
create_process(const std::string &path, DWORD creation_flags = 0) {
  auto wpath = utf8_decode(path);
  return create_process(wpath, creation_flags);
}

#endif

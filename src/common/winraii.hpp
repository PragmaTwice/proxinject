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

  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
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
  PROCESSENTRY32 entry;
  entry.dwSize = sizeof(PROCESSENTRY32);

  handle snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

  if (Process32First(snapshot.get(), &entry) == TRUE) {
    while (Process32Next(snapshot.get(), &entry) == TRUE) {
      std::forward<F>(f)(entry.szExeFile, entry.th32ProcessID);
    }
  }
}

handle create_mapping(const std::wstring &name, DWORD buf_size) {
  return CreateFileMapping(INVALID_HANDLE_VALUE, // use paging file
                           NULL,                 // default security
                           PAGE_READWRITE,       // read/write access
                           0,        // maximum object size (high-order DWORD)
                           buf_size, // maximum object size (low-order DWORD)
                           name.c_str()); // name of mapping object
}

handle open_mapping(const std::wstring &name) {
  return OpenFileMapping(FILE_MAP_ALL_ACCESS, // read/write access
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

#endif

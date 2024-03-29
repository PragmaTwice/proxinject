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

#ifndef PROXINJECT_INJECTOR_INJECTOR
#define PROXINJECT_INJECTOR_INJECTOR

#include "utils.hpp"
#include "winraii.hpp"
#include <filesystem>

namespace fs = std::filesystem;

struct injector {
  static inline const FARPROC load_library =
      GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");

#if defined(_WIN64)
  static inline const char wow64_address_dumper_filename[] =
      "wow64-address-dumper.exe";

  static std::optional<FARPROC> get_wow64_load_library() {
    auto path = fs::path(get_current_filename())
                    .replace_filename(wow64_address_dumper_filename);

    if (!std::filesystem::exists(path)) {
      return std::nullopt;
    }

    auto pi = create_process(path.wstring(), CREATE_NO_WINDOW);
    if (!pi) {
      return std::nullopt;
    }

    WaitForSingleObject(pi->hProcess, INFINITE);

    DWORD ec = 0;
    if (!GetExitCodeProcess(pi->hProcess, &ec)) {
      return std::nullopt;
    }

    return reinterpret_cast<FARPROC>((uint64_t)ec);
  }

  static inline const FARPROC load_library_wow64 =
      get_wow64_load_library().value_or(nullptr);
#endif

  static bool inject(DWORD pid, HANDLE proc, std::uint16_t port, BOOL isWoW64,
                     std::wstring_view filename) {
    handle mapping =
        create_mapping(get_port_mapping_name(pid), sizeof(std::uint16_t));
    if (!mapping) {
      return false;
    }

    mapped_buffer port_buf(mapping.get());
    *(std::uint16_t *)port_buf.get() = port;

    virtual_memory mem(proc, (filename.size() + 1) * sizeof(wchar_t));
    if (!mem)
      return false;

    if (!mem.write(filename.data()))
      return false;

    FARPROC current_load_library =
#if defined(_WIN64)
        isWoW64 ? load_library_wow64 : load_library;
#else
        load_library;
#endif
    if (!current_load_library) {
      return false;
    }

    handle thread = CreateRemoteThread(
        proc, nullptr, 0, (LPTHREAD_START_ROUTINE)current_load_library,
        mem.get(), 0, nullptr);
    if (!thread)
      return false;

    WaitForSingleObject(thread.get(), INFINITE);

    return true;
  }

  static inline const char injectee_filename[] = "proxinjectee.dll";
  static inline const char injectee_wow64_filename[] = "proxinjectee32.dll";

  static std::optional<std::wstring>
  find_injectee(std::wstring_view self_binary_path, BOOL isWoW64) {
    auto path = fs::path(self_binary_path)
                    .replace_filename(isWoW64 ? injectee_wow64_filename
                                              : injectee_filename);

    if (!std::filesystem::exists(path)) {
      return std::nullopt;
    }

    return path.wstring();
  }

  static bool inject(DWORD pid, std::uint16_t port) {
    handle proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc)
      return false;

    BOOL isWoW64 = false;
#if defined(_WIN64)
    if (!IsWow64Process(proc.get(), &isWoW64)) {
      return false;
    }
#endif

    if (auto path = find_injectee(get_current_filename(), isWoW64)) {
      return inject(pid, proc.get(), port, isWoW64, path.value());
    }

    return false;
  }

  template <typename F>
  static void pid_by_name_wildcard(const std::string &name, F &&f) {
    match_process_by_name([name, &f](const std::string &pname, DWORD pid) {
      if (filename_wildcard_match(name.data(), pname.data())) {
        std::forward<F>(f)(pid);
      }
    });
  }

  template <typename F>
  static void pid_by_name_regex(const std::string &name, F &&f) {
    match_process_by_name([name, &f](const std::string &pname, DWORD pid) {
      if (regex_match_filename(name, pname)) {
        std::forward<F>(f)(pid);
      }
    });
  }

  template <typename F>
  static void pid_by_path_wildcard(const std::string &path, F &&f) {
    match_process_by_path([path, &f](const std::string &ppath, DWORD pid) {
      if (filename_wildcard_match(path.data(), ppath.data())) {
        std::forward<F>(f)(pid);
      }
    });
  }

  template <typename F>
  static void pid_by_path_regex(const std::string &path, F &&f) {
    match_process_by_path([path, &f](const std::string &ppath, DWORD pid) {
      if (regex_match_filename(path, ppath)) {
        std::forward<F>(f)(pid);
      }
    });
  }
};

#endif

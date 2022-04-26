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

#include "winraii.hpp"
#include <filesystem>

// utf8_encode/decode from cycfi::elements

// Convert a wide Unicode string to an UTF8 string
std::string utf8_encode(std::wstring const &wstr) {
  if (wstr.empty())
    return {};
  int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
                                 nullptr, 0, nullptr, nullptr);
  std::string result(size, 0);
  WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size,
                      nullptr, nullptr);
  return result;
}

// Convert an UTF8 string to a wide Unicode String
std::wstring utf8_decode(std::string const &str) {
  if (str.empty())
    return {};
  int size =
      MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
  std::wstring result(size, 0);
  MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
  return result;
}

namespace fs = std::filesystem;

struct injector {
  static inline const FARPROC load_library =
      GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryW");

  static bool inject(DWORD pid, std::wstring_view filename) {
    handle proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!proc)
      return false;

    virtual_memory mem(proc.get(), (filename.size() + 1) * sizeof(wchar_t));
    if (!mem)
      return false;

    if (!mem.write(filename.data()))
      return false;

    handle thread = CreateRemoteThread(proc.get(), nullptr, 0,
                                       (LPTHREAD_START_ROUTINE)load_library,
                                       mem.get(), 0, nullptr);
    if (!thread)
      return false;

    WaitForSingleObject(thread.get(), INFINITE);

    return true;
  }

  static inline const char injectee_filename[] = "proxinjectee.dll";

  static std::optional<std::wstring>
  find_injectee(std::wstring_view self_binary_path) {
    auto path = fs::path(self_binary_path).replace_filename(injectee_filename);

    if (!std::filesystem::exists(path)) {
      return std::nullopt;
    }

    return path.wstring();
  }

  static bool inject(DWORD pid) {
    if (auto path = find_injectee(get_current_filename())) {
      return inject(pid, path.value());
    }

    return false;
  }

  template <typename F> static void pid_by_name(std::string_view name, F &&f) {
    match_process([name, &f](std::wstring file, DWORD pid) {
      std::string file_u8 = utf8_encode(file);
      if (file_u8.ends_with(".exe") &&
          file_u8.substr(0, file_u8.size() - 4) == name) {
        std::forward<F>(f)(pid);
      }
    });
  }

  static auto get_process_name(DWORD pid) {
    std::wstring result;
    match_process([pid, &result](std::wstring_view file, DWORD pid_) {
      if (pid == pid_) {
        result = file;
      }
    });

    std::string res_u8 = utf8_encode(result);
    if (res_u8.ends_with(".exe")) {
      return res_u8.substr(0, res_u8.size() - 4);
    }
    return res_u8;
  }

  static std::optional<DWORD> create_process(const std::wstring &path) {
    fs::path p(path);

    if (p.parent_path().empty()) {
      wchar_t realpath[MAX_PATH];

      if (SearchPathW(nullptr, p.c_str(), L".exe", MAX_PATH, realpath,
                      nullptr) == 0) {
        return std::nullopt;
      }
      p = realpath;
    }

    if (!fs::exists(p)) {
      return std::nullopt;
    }

    STARTUPINFO startup_info{};
    PROCESS_INFORMATION process_info{};
    if (CreateProcessW(p.c_str(), nullptr, nullptr, nullptr, false,
                       CREATE_NEW_CONSOLE, nullptr, nullptr,
                       &startup_info, &process_info) == 0) {
      return std::nullopt;
    }

    return process_info.dwProcessId;
  }

  static std::optional<DWORD> create_process(const std::string &path) {
    auto wpath = utf8_decode(path);
    return create_process(wpath);
  }
};

#endif

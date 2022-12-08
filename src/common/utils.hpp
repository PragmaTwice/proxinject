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

#ifndef PROXINJECT_COMMON_UTILS
#define PROXINJECT_COMMON_UTILS

#include <string>

// trim from start (in place)
static inline void ltrim(std::string &s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
          }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](unsigned char ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
  ltrim(s);
  rtrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
  ltrim(s);
  return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
  rtrim(s);
  return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
  trim(s);
  return s;
}

static inline bool all_of_digit(const auto &v) {
  return std::all_of(v.begin(), v.end(),
                     [](auto c) { return std::isdigit(c); });
}

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

bool filename_wildcard_match(const char *pattern, const char *str) {
  for (; *pattern; ++pattern) {
    switch (*pattern) {
    case '?':
      if (*str == 0)
        return false;
      ++str;
      break;
    case '*': {
      if (pattern[1] == 0)
        return true;
      for (const char *ptr = str; *ptr; ++ptr)
        if (filename_wildcard_match(pattern + 1, ptr))
          return true;
      return false;
    }
    case '/':
    case '\\':
      if (*str != '/' && *str != '\\')
        return false;
      ++str;
      break;
    default:
      if (std::tolower(*str) != std::tolower(*pattern))
        return false;
      ++str;
    }
  }
  return *str == 0;
}

std::size_t replace_all_inplace(std::string &inout, std::string_view what,
                                std::string_view with) {
  std::size_t count{};
  for (std::string::size_type pos{};
       inout.npos != (pos = inout.find(what.data(), pos, what.length()));
       pos += with.length(), ++count) {
    inout.replace(pos, what.length(), with.data(), with.length());
  }
  return count;
}

std::string replace_all(const std::string &input, std::string_view what,
                        std::string_view with) {
  std::string result = input;
  replace_all_inplace(result, what, with);
  return result;
}

static inline const std::wstring port_mapping_name = L"PROXINJECT_PORT_IPC_";

std::wstring get_port_mapping_name(DWORD pid) {
  return port_mapping_name + std::to_wstring(pid);
}

std::string proxinject_copyright(const std::string &version) {
  return "proxinject " + version + "\n\n" + "Copyright (c) PragmaTwice\n" +
         "Licensed under the Apache License, Version 2.0";
}

std::string proxinject_description =
    "A socks5 proxy injection tool for Windows: just select some processes "
    "and make them proxy-able!\nPlease visit "
    "https://github.com/PragmaTwice/proxinject for more information.";

#endif

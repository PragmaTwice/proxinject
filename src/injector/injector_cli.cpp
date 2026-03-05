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

#include <asio.hpp>

#include "injector.hpp"
#include "injector_cli.hpp"
#include "utils.hpp"
#include "version.hpp"
#include <argparse/argparse.hpp>
#include <iostream>
#include <set>
#include <thread>

using argparse::ArgumentParser;
using namespace std;

auto create_parser() {
  ArgumentParser parser("proxinjector-cli", proxinject_version,
                        argparse::default_arguments::help);

  parser.add_description(proxinject_description);

  parser.add_argument("-v", "--version")
      .action([&](const auto & /*unused*/) {
        std::cout << proxinject_copyright(proxinject_version) << std::endl;
        std::exit(0);
      })
      .default_value(false)
      .help("prints version information and exits")
      .implicit_value(true)
      .nargs(0);

  parser.add_argument("-i", "--pid")
      .help("pid of a process to inject proxy (integer)")
      .default_value(vector<int>{})
      .scan<'d', int>()
      .append();

  parser.add_argument("-n", "--name")
      .help("short filename of a process with wildcard matching to inject "
            "proxy (string, without directory and file extension, e.g. "
            "`python`, `py*`, `py??on`)")
      .default_value(vector<string>{})
      .append();

  parser.add_argument("-P", "--path")
      .help("full filename of a process with wildcard matching to inject proxy "
            "(string, with directory and file extension, e.g. "
            "`C:/programs/python.exe`, `C:/programs/*.exe`)")
      .default_value(vector<string>{})
      .append();

  parser.add_argument("-r", "--name-regexp")
      .help("regular expression for short filename of a process "
            "to inject proxy (string, without directory and file "
            "extension, e.g. `python`, `py.*|exp.*`)")
      .default_value(vector<string>{})
      .append();

  parser.add_argument("-R", "--path-regexp")
      .help("regular expression for full filename of a process "
            "to inject proxy (string, with directory and file "
            "extension, e.g. `C:/programs/python.exe`, "
            "`C:/programs/(a|b).*\\.exe`)")
      .default_value(vector<string>{})
      .append();

  parser.add_argument("-e", "--exec")
      .help("command line started with an executable to create a new process "
            "and inject proxy (string, e.g. `python` or `C:\\Program "
            "Files\\a.exe --some-option`)")
      .default_value(vector<string>{})
      .append();

  parser.add_argument("-l", "--enable-log")
      .help("enable logging for network connections")
      .default_value(false)
      .implicit_value(true);

  parser.add_argument("-p", "--set-proxy")
      .help("set a proxy address for network connections (string, e.g. "
            "`127.0.0.1:1080`)")
      .default_value(string{});

  parser.add_argument("-w", "--new-console-window")
      .help("create a new console window while a new console process is "
            "executed in `-e`")
      .default_value(false)
      .implicit_value(true);

  parser.add_argument("-s", "--subprocess")
      .help("inject subprocesses created by these already injected processes")
      .default_value(false)
      .implicit_value(true);

  parser.add_argument("-t", "--proxy-type")
      .help("proxy type: socks5 (default) or http")
      .default_value(string{"socks5"});

  parser.add_argument("-U", "--proxy-user")
      .help("proxy username for authentication")
      .default_value(string{});

  parser.add_argument("-W", "--proxy-pass")
      .help("proxy password for authentication")
      .default_value(string{});

  parser.add_argument("--watch")
      .help("watch mode: wait and monitor for processes matching name pattern, "
            "inject them when they start (e.g. `--watch index` or `--watch py*`)")
      .default_value(string{});

  return parser;
}

int main(int argc, char *argv[]) {
  auto parser = create_parser();

  try {
    parser.parse_args(argc, argv);
  } catch (const runtime_error &err) {
    cerr << err.what() << endl;
    cerr << parser;
    return 1;
  }

  auto pids = parser.get<vector<int>>("-i");
  auto proc_names = parser.get<vector<string>>("-n");
  auto proc_paths = parser.get<vector<string>>("-P");
  auto proc_re_names = parser.get<vector<string>>("-r");
  auto proc_re_paths = parser.get<vector<string>>("-R");
  auto create_paths = parser.get<vector<string>>("-e");
  auto watch_pattern = parser.get<string>("--watch");

  if (pids.empty() && proc_names.empty() && create_paths.empty() && watch_pattern.empty()) {
    cerr << "Expected at least one of `-i`, `-n`, `-e` or `--watch`" << endl;
    cerr << parser;
    return 2;
  }

  asio::io_context io_context(1);
  injector_server server;

  auto acceptor = tcp::acceptor(io_context, auto_endpoint);
  server.set_port(acceptor.local_endpoint().port());
  info("connection port is set to {}", server.port_);

  asio::co_spawn(io_context,
                 listener<injectee_session_cli>(std::move(acceptor), server),
                 asio::detached);

  jthread io_thread([&io_context] { io_context.run(); });

  if (parser.get<bool>("-l")) {
    server.enable_log();
    info("logging enabled");
  }

  if (parser.get<bool>("-s")) {
    server.enable_subprocess();
    info("subprocess injection enabled");
  }

  if (auto proxy_str = trim_copy(parser.get<string>("-p"));
      !proxy_str.empty()) {
    if (auto res = parse_address(proxy_str)) {
      auto [addr, port] = res.value();
      // Try to parse as IP address first, then resolve as hostname
      asio::error_code ec;
      auto ip_addr = ip::make_address(addr, ec);
      if (!ec) {
        server.set_proxy(ip_addr, port);
        info("proxy address set to {}:{}", addr, port);
      } else {
        // Resolve hostname
        try {
          asio::io_context resolver_ctx;
          ip::tcp::resolver resolver(resolver_ctx);
          auto endpoints = resolver.resolve(addr, to_string(port));
          if (!endpoints.empty()) {
            auto resolved_addr = endpoints.begin()->endpoint().address();
            server.set_proxy(resolved_addr, port);
            info("proxy address {} resolved to {}:{}", addr, resolved_addr.to_string(), port);
          }
        } catch (const std::exception &e) {
          error("failed to resolve proxy hostname {}: {}", addr, e.what());
        }
      }
    }
  }

  if (auto proxy_type = parser.get<string>("-t"); proxy_type == "http") {
    server.set_proxy_type(1);
    info("proxy type set to http");
  } else {
    server.set_proxy_type(0);
    info("proxy type set to socks5");
  }

  if (auto username = parser.get<string>("-U"); !username.empty()) {
    server.set_proxy_username(username);
    info("proxy username set");
  }

  if (auto password = parser.get<string>("-W"); !password.empty()) {
    server.set_proxy_password(password);
    info("proxy password set");
  }

  bool has_process = false;
  auto report_injected = [&has_process](DWORD pid) {
    info("{}: injected", pid);
    has_process = true;
  };

  for (auto pid : pids) {
    if (pid > 0) {
      if (server.inject(pid)) {
        report_injected(pid);
      }
    }
  }

  for (const auto &name : proc_names) {
    injector::pid_by_name_wildcard(name,
                                   [&server, &report_injected](DWORD pid) {
                                     if (server.inject(pid)) {
                                       report_injected(pid);
                                     }
                                   });
  }

  for (const auto &path : proc_paths) {
    injector::pid_by_path_wildcard(path,
                                   [&server, &report_injected](DWORD pid) {
                                     if (server.inject(pid)) {
                                       report_injected(pid);
                                     }
                                   });
  }

  for (const auto &name : proc_re_names) {
    injector::pid_by_name_regex(name, [&server, &report_injected](DWORD pid) {
      if (server.inject(pid)) {
        report_injected(pid);
      }
    });
  }

  for (const auto &path : proc_re_paths) {
    injector::pid_by_path_regex(path, [&server, &report_injected](DWORD pid) {
      if (server.inject(pid)) {
        report_injected(pid);
      }
    });
  }

  for (const auto &file : create_paths) {
    DWORD creation_flags = parser.get<bool>("-w") ? CREATE_NEW_CONSOLE : 0;
    if (auto res = create_process(file, creation_flags)) {
      if (server.inject(res->dwProcessId)) {
        report_injected(res->dwProcessId);
      }
    }
  }

  // Watch mode: continuously monitor for new processes
  if (!watch_pattern.empty()) {
    server.set_watch_mode(true);
    info("watch mode: monitoring for processes matching '{}'", watch_pattern);
    info("press Ctrl+C to stop");

    std::set<DWORD> injected_pids;

    while (true) {
      injector::pid_by_name_wildcard(watch_pattern,
                                     [&server, &injected_pids](DWORD pid) {
                                       if (injected_pids.find(pid) == injected_pids.end()) {
                                         if (server.inject(pid)) {
                                           info("{}: injected (watch mode)", pid);
                                           injected_pids.insert(pid);
                                         }
                                       }
                                     });

      // Clean up terminated processes from the set
      for (auto it = injected_pids.begin(); it != injected_pids.end(); ) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, *it);
        if (hProcess == NULL) {
          it = injected_pids.erase(it);
        } else {
          DWORD exitCode;
          if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            it = injected_pids.erase(it);
          } else {
            ++it;
          }
          CloseHandle(hProcess);
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  } else if (!has_process) {
    info("no process has been injected, exit");
    io_context.stop();
  }

  io_thread.join();
}

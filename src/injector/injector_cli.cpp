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
#include "server.hpp"
#include "utils.hpp"
#include "version.hpp"
#include <argparse/argparse.hpp>
#include <iostream>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

using argparse::ArgumentParser;
using namespace std;
using namespace spdlog;

struct injectee_session_cli : injectee_session {
  using injectee_session::injectee_session;

  asio::awaitable<void> process_connect(const InjecteeConnect &msg) override {
    if (auto v = msg["proxy"_f])
      info("{}: connect {} via {}", (int)pid_, *msg["addr"_f], *v);
    else
      info("{}: connect {}", (int)pid_, *msg["addr"_f]);
    co_return;
  }

  asio::awaitable<void> process_pid() override {
    info("{}: established injectee connection", (int)pid_);
    co_return;
  }

  void process_close() override {
    info("{}: closed", (int)pid_);
    if (server_.clients.size() == 0) {
      info("all process have been exited, exit");

      auto exec = socket_.get_executor();
      exec.target<asio::io_context>()->stop();
    }
  }
};

optional<pair<string, uint16_t>> parse_address(const string &addr) {
  auto delimiter = addr.find_last_of(':');
  if (delimiter == string::npos) {
    return nullopt;
  }

  auto host = addr.substr(0, delimiter);
  uint16_t port = std::stoul(addr.substr(delimiter + 1));

  return make_pair(host, port);
}

int main(int argc, char *argv[]) {
  ArgumentParser parser("proxinjector-cli", proxinject_version);

  parser.add_argument("-i", "--pid")
      .help("pid of a process to inject proxy (integer)")
      .default_value(vector<int>{})
      .scan<'d', int>()
      .append();

  parser.add_argument("-n", "--name")
      .help("filename of a process to inject proxy (string, without path and "
            "file ext, i.e. `python`)")
      .default_value(vector<string>{})
      .append();

  parser.add_argument("-e", "--exec")
      .help("command line started with an executable to create a new process "
            "and inject proxy (string, i.e. `python` or `C:\\Program "
            "Files\\a.exe --some-option`)")
      .default_value(vector<string>{})
      .append();

  parser.add_argument("-l", "--enable-log")
      .help("enable logging for network connections")
      .default_value(false)
      .implicit_value(true);

  parser.add_argument("-p", "--set-proxy")
      .help("set a proxy address for network connections (string, i.e. "
            "`127.0.0.1:1080`)")
      .default_value(string{});

  parser.add_argument("-w", "--new-console-window")
      .help("create a new console window while a new console process is "
            "executed in `-e`")
      .default_value(false)
      .implicit_value(true);

  try {
    parser.parse_args(argc, argv);
  } catch (const runtime_error &err) {
    cerr << err.what() << endl;
    cerr << parser;
    return 1;
  }

  auto pids = parser.get<vector<int>>("-i");
  auto proc_names = parser.get<vector<string>>("-n");
  auto create_paths = parser.get<vector<string>>("-e");

  if (pids.empty() && proc_names.empty() && create_paths.empty()) {
    cerr << "Expected at least one of `-i`, `-n` or `-e`" << endl;
    cerr << parser;
    return 2;
  }

  asio::io_context io_context(1);
  injector_server server;

  asio::co_spawn(io_context,
                 listener<injectee_session_cli>(
                     tcp::acceptor(io_context, proxinject_endpoint), server),
                 asio::detached);

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto) { io_context.stop(); });

  thread io_thread([&io_context] { io_context.run(); });

  if (parser.get<bool>("-l")) {
    server.enable_log();
    info("logging enabled");
  }

  if (auto proxy_str = trim_copy(parser.get<string>("-p"));
      !proxy_str.empty()) {
    if (auto res = parse_address(proxy_str)) {
      auto [addr, port] = res.value();
      server.set_proxy(ip::address::from_string(addr), port);
      info("proxy address set to {}:{}", addr, port);
    }
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
    injector::pid_by_name(name, [&server, &report_injected](DWORD pid) {
      if (server.inject(pid)) {
        report_injected(pid);
      }
    });
  }

  for (const auto &file : create_paths) {
    if (auto res = injector::create_process(file, parser.get<bool>("-w"))) {
      if (server.inject(*res)) {
        report_injected(*res);
      }
    }
  }

  if (!has_process) {
    info("no process has been injected, exit");
    io_context.stop();
  }

  io_thread.join();
}

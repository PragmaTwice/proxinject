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

#ifndef PROXINJECT_INJECTOR_INJECTOR_CLI
#define PROXINJECT_INJECTOR_INJECTOR_CLI

#include "server.hpp"
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

using spdlog::info;
using spdlog::error;

struct injectee_session_cli : injectee_session {
  using injectee_session::injectee_session;

  asio::awaitable<void> process_connect(const InjecteeConnect &msg) override {
    if (auto v = msg["proxy"_f])
      info("{}: {} {} via {}", (int)pid_, *msg["syscall"_f], *msg["addr"_f],
           *v);
    else
      info("{}: {} {}", (int)pid_, *msg["syscall"_f], *msg["addr"_f]);
    co_return;
  }

  asio::awaitable<void> process_pid() override {
    info("{}: established injectee connection", (int)pid_);
    co_return;
  }

  void process_close() override {
    info("{}: closed", (int)pid_);
    if (server_.clients.size() == 0 && !server_.watch_mode_) {
      info("all processes have been exited, exit");

      exit(0);
    }
  }
};

std::optional<std::pair<std::string, uint16_t>>
parse_address(const std::string &addr) {
  auto delimiter = addr.find_last_of(':');
  if (delimiter == std::string::npos) {
    return std::nullopt;
  }

  auto host = addr.substr(0, delimiter);
  uint16_t port = std::stoul(addr.substr(delimiter + 1));

  return make_pair(host, port);
}

#endif

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
#include <filesystem>
#include <iostream>
#include <string>

injector_server server;

void do_server() {
  asio::io_context io_context(1);

  asio::co_spawn(
      io_context,
      listener(server, tcp::acceptor(io_context, proxinject_endpoint)),
      asio::detached);

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto) { io_context.stop(); });

  io_context.run();
}

int main(int argc, char *argv[]) {
  std::thread(do_server).detach();
  std::string opcode;

  while (true) {
    std::cout << "> ";
    std::cin >> opcode;
    if (!std::cin.good())
      break;

    if (opcode == "exit") {
      exit(0);
    } else if (opcode == "load") {
      DWORD pid;
      std::cin >> pid;
      std::cout << (server.inject(pid) ? "success" : "failed") << std::endl;
    } else if (opcode == "unload") {
      DWORD pid;
      std::cin >> pid;
      std::cout << (server.close(pid) ? "success" : "failed") << std::endl;
    } else if (opcode == "list") {
      for (const auto &[pid, _] : server.clients) {
        std::cout << pid << std::endl;
      }
    } else if (opcode == "add") {
      std::string s;
      std::cin >> s;
      injector::pid_by_name(s.c_str(), [](DWORD pid) {
        std::cout << pid << ": " << (server.inject(pid) ? "success" : "failed")
                  << std::endl;
      });
    } else if (opcode == "remove") {
      std::string s;
      std::cin >> s;
      injector::pid_by_name(s.c_str(), [](DWORD pid) {
        std::cout << pid << ": " << (server.close(pid) ? "success" : "failed")
                  << std::endl;
      });
    } else if (opcode == "set-proxy") {
      std::string addr;
      std::uint16_t port;
      std::cin >> addr >> port;
      server.set_proxy(ip::address::from_string(addr), port);
    } else if (opcode == "clear-proxy") {
      server.clear_proxy();
    } else if (opcode == "get-proxy") {
      if (auto v = server.get_config()["addr"_f]) {
        auto [addr, port] = to_asio(*v);
        std::cout << addr << ":" << port << std::endl;
      } else {
        std::cout << "empty" << std::endl;
      }
    } else if (opcode == "enable-log") {
      server.enable_log();
    } else if (opcode == "disable-log") {
      server.disable_log();
    } else if (opcode == "help") {
      std::cout
          << "commands:" << std::endl
          << "`load <pid>`: inject to a process" << std::endl
          << "`unload <pid>`: restore an injected process" << std::endl
          << "`add <process-name>`: inject all processes named <process-name> "
             "(no `.exe` suffix)"
          << std::endl
          << "`remove <process-name>`: restore all processes named "
             "<process-name> (no `.exe` suffix)"
          << std::endl
          << "`list`: list all injected process ids" << std::endl
          << "`set-proxy <address> <port>`: set proxy config for all injected "
             "processes"
          << std::endl
          << "`clear-proxy`: remove proxy config for all injected processes"
          << std::endl
          << "`get-proxy`: dump current proxy config" << std::endl
          << "`enable-log`: enable logging for process connection" << std::endl
          << "`disable-log`: disable logging for process connection"
          << std::endl
          << "`help`: show help messages" << std::endl
          << "`exit`: quit the program" << std::endl;
    } else {
      std::cout << "unknown command, try `help`" << std::endl;
    }
  }
}

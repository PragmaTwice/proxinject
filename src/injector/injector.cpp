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
      std::cout << (injector::inject(pid) ? "success" : "failed") << std::endl;
    } else if (opcode == "unload") {
      DWORD pid;
      std::cin >> pid;
      std::cout << (server.close(pid) ? "success" : "failed") << std::endl;
    } else if (opcode == "list") {
      for (const auto &[pid, _] : server.clients) {
        std::cout << pid << std::endl;
      }
    } else if (opcode == "set-config") {
      std::string addr;
      std::uint16_t port;
      std::cin >> addr >> port;
      server.config(ip::address::from_string(addr), port);
    } else if (opcode == "clear-config") {
      server.clear_config();
    } else if (opcode == "get-config") {
      if (auto v = server.get_config()) {
        auto [addr, port] = to_asio((*v)["addr"_f].value());
        std::cout << addr << ":" << port << std::endl;
      } else {
        std::cout << "empty" << std::endl;
      }
    } else {
      std::cout << "unknown command" << std::endl;
    }
  }
}

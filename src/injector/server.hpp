#include "async_io.hpp"
#include "schema.hpp"
#include <asio.hpp>
#include <map>
#include <iostream>

using tcp = asio::ip::tcp;

struct injectee_client {
  virtual ~injectee_client() {}

  virtual void stop() = 0;
};

using injectee_client_ptr = std::shared_ptr<injectee_client>;

struct injector_server {
  std::map<DWORD, injectee_client_ptr> clients;

  void open(DWORD pid, injectee_client_ptr ptr) { clients.emplace(pid, ptr); }

  bool remove(DWORD pid) {
    if (auto iter = clients.find(pid); iter != clients.end()) {
      clients.erase(iter);
      return true;
    }

    return false;
  }

  bool close(DWORD pid) {
    if (auto iter = clients.find(pid); iter != clients.end()) {
      iter->second->stop();
      return true;
    }

    return false;
  }
};

struct injectee_session : injectee_client,
                          std::enable_shared_from_this<injectee_session> {
  tcp::socket socket_;
  asio::steady_timer timer_;
  injector_server &server_;
  DWORD pid_;

  injectee_session(tcp::socket socket, injector_server &server)
      : socket_(std::move(socket)), timer_(socket_.get_executor()),
        server_(server), pid_(0) {
    timer_.expires_at(std::chrono::steady_clock::time_point::max());
  }

  void start() {
    asio::co_spawn(
        socket_.get_executor(),
        [self = shared_from_this()] { return self->reader(); }, asio::detached);
  }

  asio::awaitable<void> reader() {
    try {
      while (true) {
        auto msg = co_await async_read_message<InjecteeMessage>(socket_);
        asio::co_spawn(
            socket_.get_executor(),
            [self = shared_from_this(), msg = std::move(msg)] {
              return self->process(msg);
            },
            asio::detached);
      }
    } catch (std::exception &) {
      stop();
    }
  }

  asio::awaitable<void> process(const InjecteeMessage &msg) {
    if (msg["opcode"_f] == "pid") {
      pid_ = msg["pid"_f].value();
      server_.open(pid_, shared_from_this());
      std::cout << "recv pid " << pid_ << std::endl;
    }

    co_return;
  }

  void stop() {
    socket_.close();
    timer_.cancel();
    server_.remove(pid_);
  }
};

asio::awaitable<void> listener(injector_server &server,
                               tcp::acceptor acceptor) {
  for (;;) {
    std::make_shared<injectee_session>(
        co_await acceptor.async_accept(asio::use_awaitable), server)
        ->start();
  }
}

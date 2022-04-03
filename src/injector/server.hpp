#ifndef PROXINJECT_INJECTOR_SERVER
#define PROXINJECT_INJECTOR_SERVER

#include "async_io.hpp"
#include "schema.hpp"
#include <asio.hpp>
#include <iostream>
#include <map>

using tcp = asio::ip::tcp;

struct injectee_client {
  virtual ~injectee_client() {}

  virtual void stop() = 0;
  virtual asio::awaitable<void> config(const InjectorConfig &) = 0;
  virtual asio::awaitable<void> clear_config() = 0;
  virtual asio::any_io_executor get_context() = 0;
};

using injectee_client_ptr = std::shared_ptr<injectee_client>;

struct injector_server {
  std::map<DWORD, injectee_client_ptr> clients;
  std::optional<InjectorConfig> config_;
  std::mutex config_mutex;

  void open(DWORD pid, injectee_client_ptr ptr) { clients.emplace(pid, ptr); }

  void config(const ip::address &addr, std::uint32_t port) {
    std::lock_guard guard(config_mutex);
    config_ = InjectorConfig{from_asio(addr, port)};

    for (const auto &[_, client] : clients) {
      asio::co_spawn(
          client->get_context(),
          [cfg = *config_, client] { return client->config(cfg); },
          asio::detached);
    }
  }

  void clear_config() {
    std::lock_guard guard(config_mutex);
    config_ = std::nullopt;
    for (const auto &[_, client] : clients) {
      asio::co_spawn(
          client->get_context(), [client] { return client->clear_config(); },
          asio::detached);
    }
  }

  std::optional<InjectorConfig> get_config() {
    std::lock_guard guard(config_mutex);
    return config_;
  }

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

  asio::any_io_executor get_context() { return socket_.get_executor(); }

  asio::awaitable<void> config(const InjectorConfig &cfg) {
    co_await async_write_message(
        socket_, create_message<InjectorMessage, "config">(cfg));
  }

  asio::awaitable<void> clear_config() {
    co_await async_write_message(
        socket_, create_message<InjectorMessage, "config">(std::nullopt));
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
    if (auto v = compare_message<"pid">(msg)) {
      pid_ = *v;
      server_.open(pid_, shared_from_this());
      if (auto v = server_.get_config()) {
        co_await config(*v);
      }
    } else if (auto v = compare_message<"connect">(msg)) {
      auto [addr, port] = to_asio((*v)["addr"_f].value());
      std::cout << pid_ << ": " << (*v)["handle"_f].value() << ", " << addr
                << ":" << port;
      if (auto proxy = (*v)["proxy"_f]) {
        auto [addr, port] = to_asio(*proxy);
        std::cout << " via " << addr << ":" << port;
      }
      std::cout << std::endl;
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

#endif

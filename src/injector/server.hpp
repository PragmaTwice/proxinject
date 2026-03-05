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

#ifndef PROXINJECT_INJECTOR_SERVER
#define PROXINJECT_INJECTOR_SERVER

#include "async_io.hpp"
#include "injector.hpp"
#include "schema.hpp"
#include <asio.hpp>
#include <map>

using tcp = asio::ip::tcp;

struct injectee_client {
  virtual ~injectee_client() {}

  virtual void stop() = 0;
  virtual asio::awaitable<void> config(const InjectorConfig &) = 0;
  virtual asio::any_io_executor get_context() = 0;
};

using injectee_client_ptr = std::shared_ptr<injectee_client>;

struct injector_server {
  std::map<DWORD, injectee_client_ptr> clients;
  InjectorConfig config_;
  std::mutex config_mutex;

  std::uint16_t port_ = -1;
  bool watch_mode_ = false;

  void set_port(std::uint16_t port) { port_ = port; }
  void set_watch_mode(bool enabled = true) { watch_mode_ = enabled; }

  bool inject(DWORD pid) {
    if (port_ == -1) {
      return false;
    }
    if (clients.contains(pid)) {
      return false;
    } else {
      return injector::inject(pid, port_);
    }
  }

  bool open(DWORD pid, injectee_client_ptr ptr) {
    return clients.emplace(pid, ptr).second;
  }

  void broadcast_config() {
    for (const auto &[_, client] : clients) {
      asio::co_spawn(
          client->get_context(),
          [cfg = config_, client] { return client->config(cfg); },
          asio::detached);
    }
  }

  template <typename T> void config_proxy(T &&v) {
    std::lock_guard guard(config_mutex);
    config_["addr"_f] = std::forward<T>(v);

    broadcast_config();
  }

  void set_proxy(const ip::address &addr, std::uint32_t port) {
    config_proxy(from_asio(addr, port));
  }

  void clear_proxy() { config_proxy(std::nullopt); }

  void enable_log(bool enable = true) {
    std::lock_guard guard(config_mutex);
    config_["log"_f] = enable;

    broadcast_config();
  }

  void disable_log() { enable_log(false); }

  void enable_subprocess(bool enable = true) {
    std::lock_guard guard(config_mutex);
    config_["subprocess"_f] = enable;

    broadcast_config();
  }

  void disable_subprocess() { enable_subprocess(false); }

  void set_proxy_type(std::uint32_t type) {
    std::lock_guard guard(config_mutex);
    config_["proxy_type"_f] = type;

    broadcast_config();
  }

  void set_proxy_username(const std::string &username) {
    std::lock_guard guard(config_mutex);
    config_["username"_f] = username;

    broadcast_config();
  }

  void set_proxy_password(const std::string &password) {
    std::lock_guard guard(config_mutex);
    config_["password"_f] = password;

    broadcast_config();
  }

  InjectorConfig get_config() {
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

  virtual asio::awaitable<void> process_pid() { co_return; }
  virtual asio::awaitable<void> process_connect(const InjecteeConnect &msg) {
    co_return;
  }
  virtual asio::awaitable<void> process_subpid(std::uint16_t pid, bool result) {
    co_return;
  }
  virtual void process_close() {}

  asio::awaitable<void> process(const InjecteeMessage &msg) {
    if (auto v = compare_message<"pid">(msg)) {
      pid_ = *v;
      server_.open(pid_, shared_from_this());
      auto config_ = server_.get_config();
      if (config_["subprocess"_f] && *config_["subprocess"_f]) {
        enumerate_child_pids(pid_, [this](DWORD pid) { server_.inject(pid); });
      }
      co_await config(config_);
      co_await process_pid();
    } else if (auto v = compare_message<"connect">(msg)) {
      co_await process_connect(*v);
    } else if (auto v = compare_message<"subpid">(msg)) {
      co_await process_subpid(*v, server_.inject(*v));
    }
  }

  void stop() {
    socket_.close();
    timer_.cancel();
    server_.remove(pid_);
    process_close();
  }
};

template <typename Session = injectee_session>
asio::awaitable<void> listener(tcp::acceptor acceptor, auto &&...args) {
  for (;;) {
    std::make_shared<Session>(
        co_await acceptor.async_accept(asio::use_awaitable),
        std::forward<decltype(args)>(args)...)
        ->start();
  }
}

#endif

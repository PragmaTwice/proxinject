#include "async_io.hpp"
#include "queue.hpp"
#include "schema.hpp"
#include "winraii.hpp"
#include <iostream>

struct injectee_client : std::enable_shared_from_this<injectee_client> {
  tcp::socket socket_;
  tcp::endpoint endpoint_;
  asio::steady_timer timer_;
  blocking_queue<InjecteeMessage> &queue_;

  injectee_client(asio::io_context &io_context, const tcp::endpoint &endpoint,
                  blocking_queue<InjecteeMessage> &queue)
      : socket_(io_context), endpoint_(endpoint), timer_(io_context),
        queue_(queue) {
    timer_.expires_at(std::chrono::steady_clock::time_point::max());
  }

  asio::awaitable<void> start() {
    co_await socket_.async_connect(endpoint_, asio::use_awaitable);

    co_await async_write_message(
        socket_, create_message<InjecteeMessage, "pid">(GetCurrentProcessId()));

    asio::co_spawn(socket_.get_executor(), reader(), asio::detached);
    asio::co_spawn(socket_.get_executor(), writer(), asio::detached);
  }

  asio::awaitable<void> reader() {
    try {
      while (true) {
        auto msg = co_await async_read_message<InjectorMessage>(socket_);
        asio::co_spawn(
            socket_.get_executor(),
            [this, msg = std::move(msg)] { return process(msg); },
            asio::detached);
      }
    } catch (std::exception &) {
      stop();
    }
  }

  asio::awaitable<void> writer() {
    try {
      InjecteeMessage msg;
      while (queue_.pop(msg)) {
        co_await async_write_message(socket_, msg);
      }
    } catch (std::exception &) {
      stop();
    }
  }

  asio::awaitable<void> process(const InjectorMessage &msg) { co_return; }

  void stop() {
    queue_.shutdown();
    socket_.close();
    timer_.cancel();
  }
};

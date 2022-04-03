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

#ifndef PROXINJECT_INJECTEE_CLIENT
#define PROXINJECT_INJECTEE_CLIENT

#include "async_io.hpp"
#include "queue.hpp"
#include "schema.hpp"
#include "winnet.hpp"
#include <iostream>

struct injectee_config {
  InjectorConfig cfg;
  std::mutex mtx;

  void set(const InjectorConfig &config) {
    std::lock_guard guard(mtx);
    cfg = config;
  }

  InjectorConfig get() {
    std::lock_guard guard(mtx);

    return cfg;
  }
};

struct injectee_client : std::enable_shared_from_this<injectee_client> {
  tcp::socket socket_;
  tcp::endpoint endpoint_;
  asio::steady_timer timer_;
  blocking_queue<InjecteeMessage> &queue_;
  injectee_config &config_;

  injectee_client(asio::io_context &io_context, const tcp::endpoint &endpoint,
                  blocking_queue<InjecteeMessage> &queue,
                  injectee_config &config)
      : socket_(io_context), endpoint_(endpoint), timer_(io_context),
        queue_(queue), config_(config) {
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
      while (true) {
        InjecteeMessage msg = co_await queue_.pop();
        co_await async_write_message(socket_, msg);
      }
    } catch (std::exception &) {
      stop();
    }
  }

  asio::awaitable<void> process(const InjectorMessage &msg) {
    if (auto v = compare_message<"config">(msg)) {
      config_.set(*v);
    }

    co_return;
  }

  void stop() {
    socket_.close();
    timer_.cancel();
  }
};

#endif

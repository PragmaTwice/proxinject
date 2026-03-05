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

#ifndef PROXINJECT_COMMON_QUEUE
#define PROXINJECT_COMMON_QUEUE

#include <asio/experimental/channel.hpp>
#include <queue>

template <typename T> using channel = asio::experimental::channel<T>;

template <typename T> class blocking_queue {
  std::mutex _sync;
  std::queue<T> _qu;
  // Use void(error_code, int) to avoid template ambiguity with GCC 15+
  channel<void(asio::error_code, int)> chan;

public:
  blocking_queue(asio::io_context &ctx, size_t max)
      : chan(ctx.get_executor(), max) {}

  void push(const T &item) {
    std::unique_lock<std::mutex> lock(_sync);
    _qu.push(item);
    asio::co_spawn(chan.get_executor(),
                   chan.async_send(asio::error_code{}, 0, asio::use_awaitable),
                   asio::detached);
  }

  asio::awaitable<T> pop() {
    co_await chan.async_receive(asio::use_awaitable);
    std::unique_lock<std::mutex> lock(_sync);
    T item = std::move(_qu.front());
    _qu.pop();
    co_return item;
  }

  void cancel() { chan.cancel(); }

  ~blocking_queue() { chan.close(); }
};

#endif

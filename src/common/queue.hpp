#ifndef PROXINJECT_COMMON_QUEUE
#define PROXINJECT_COMMON_QUEUE

#include <asio/experimental/channel.hpp>
#include <queue>

template <typename T> using channel = asio::experimental::channel<T>;

template <typename T> class blocking_queue {
  std::mutex _sync;
  std::queue<T> _qu;
  channel<void(asio::error_code)> chan;

public:
  blocking_queue(asio::io_context &ctx, size_t max)
      : chan(ctx.get_executor(), max) {}

  void push(const T &item) {
    std::unique_lock<std::mutex> lock(_sync);
    _qu.push(item);
    asio::co_spawn(chan.get_executor(),
                   chan.async_send(asio::error_code{}, asio::use_awaitable),
                   asio::detached);
  }

  asio::awaitable<T> pop() {
    co_await chan.async_receive(asio::use_awaitable);
    std::unique_lock<std::mutex> lock(_sync);
    T item = std::move(_qu.front());
    _qu.pop();
    co_return item;
  }
};

#endif

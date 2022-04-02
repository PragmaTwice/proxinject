#include <asio.hpp>
#include <protopuf/message.h>
#include <span>
#include <iostream>

namespace ip = asio::ip;
using tcp = asio::ip::tcp;

template <typename Message, typename Stream>
asio::awaitable<Message> async_read_message(Stream &s) {
  std::int32_t len = 0;
  co_await asio::async_read(s, asio::buffer(&len, sizeof(len)),
                            asio::use_awaitable);

  std::vector<std::byte> buf(len);
  co_await asio::async_read(s, asio::buffer(buf, len),
                            asio::use_awaitable);

  auto [msg, remains] =
      pp::message_coder<Message>::decode(std::span(buf.begin(), buf.end()));
  co_return msg;
}

template <typename Message, typename Stream>
asio::awaitable<void> async_write_message(Stream &s, const Message &msg) {
  std::int32_t len = pp::skipper<pp::message_coder<Message>>::encode_skip(msg);

  std::vector<std::byte> buf(len);
  pp::message_coder<Message>::encode(msg, std::span(buf.begin(), buf.end()));

  co_await asio::async_write(s, asio::buffer(&len, sizeof(len)),
                             asio::use_awaitable);
  co_await asio::async_write(s, asio::buffer(buf, len),
                             asio::use_awaitable);
}

static inline auto proxinject_endpoint = tcp::endpoint(ip::address::from_string("127.0.0.1"), 33321);

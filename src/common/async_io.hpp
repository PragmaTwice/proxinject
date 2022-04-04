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

#ifndef PROXINJECT_COMMON_ASYNC_IO
#define PROXINJECT_COMMON_ASYNC_IO

#include <asio.hpp>
#include <iostream>
#include <protopuf/message.h>
#include <span>

namespace ip = asio::ip;
using tcp = asio::ip::tcp;

template <typename Message, typename Stream>
asio::awaitable<Message> async_read_message(Stream &s) {
  std::int32_t len = 0;
  co_await asio::async_read(s, asio::buffer(&len, sizeof(len)),
                            asio::use_awaitable);

  std::vector<std::byte> buf(len);
  co_await asio::async_read(s, asio::buffer(buf, len), asio::use_awaitable);

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
  co_await asio::async_write(s, asio::buffer(buf, len), asio::use_awaitable);
}

static inline const auto proxinject_endpoint =
    tcp::endpoint(ip::address::from_string("127.0.0.1"), 33321);

#endif

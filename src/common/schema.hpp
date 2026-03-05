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

#ifndef PROXINJECT_COMMON_SCHEMA
#define PROXINJECT_COMMON_SCHEMA

#include <protopuf/message.h>

using pp::operator""_f;

using IpAddr =
    pp::message<pp::uint32_field<"v4_addr", 1>, pp::bytes_field<"v6_addr", 2>,
                pp::string_field<"domain", 4>, pp::uint32_field<"port", 3>>;

inline std::pair<std::string, std::uint16_t> to_asio(const IpAddr &ip) {
  if (const auto &v = ip["v4_addr"_f]) {
    return {ip::address_v4(*v).to_string(), *ip["port"_f]};
  } else if (const auto &v = ip["v6_addr"_f]) {
    ip::address_v6::bytes_type addr;
    std::copy(v->begin(), v->end(), addr.begin());
    return {ip::address_v6(addr).to_string(), *ip["port"_f]};
  } else {
    const auto &domain = ip["domain"_f];
    return {*domain, *ip["port"_f]};
  }
}

inline IpAddr from_asio(const ip::address &addr, std::uint16_t port) {
  if (addr.is_v4()) {
    return IpAddr{addr.to_v4().to_uint(), {}, {}, port};
  } else {
    auto v = addr.to_v6().to_bytes();
    return IpAddr{{}, std::vector<unsigned char>{v.begin(), v.end()}, {}, port};
  }
}

template <typename OS> auto &operator<<(OS &stream, const IpAddr &addr) {
  auto [address, port] = to_asio(addr);
  return stream << address << ":" << port;
}

using InjecteeConnect = pp::message<
    pp::uint32_field<"handle", 1>, pp::message_field<"addr", 2, IpAddr>,
    pp::message_field<"proxy", 3, IpAddr>, pp::string_field<"syscall", 4>>;

using InjecteeMessage =
    pp::message<pp::string_field<"opcode", 1>,
                pp::message_field<"connect", 2, InjecteeConnect>,
                pp::uint32_field<"pid", 3>, pp::uint32_field<"subpid", 4>>;

using InjectorConfig =
    pp::message<pp::message_field<"addr", 1, IpAddr>, pp::bool_field<"log", 2>,
                pp::bool_field<"subprocess", 3>, pp::uint32_field<"proxy_type", 4>,
                pp::string_field<"username", 5>, pp::string_field<"password", 6>>;

using InjectorMessage =
    pp::message<pp::string_field<"opcode", 1>,
                pp::message_field<"config", 2, InjectorConfig>>;

template <typename M, pp::basic_fixed_string S, typename T>
M create_message(T &&v) {
  M msg;

  msg["opcode"_f] = S.data;
  msg.template get<S>() = std::forward<T>(v);

  return msg;
}

template <pp::basic_fixed_string S, typename M>
typename M::template get_type_by_name<S>::base_type compare_message(const M &msg) {
  if (msg["opcode"_f] == S.data) {
    return msg.template get_base<S>();
  }

  return std::nullopt;
}

#endif

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

#ifndef PROXINJECT_INJECTEE_WINNET
#define PROXINJECT_INJECTEE_WINNET

#include "winraii.hpp"
#include <WinSock2.h>

std::optional<IpAddr> to_ip_addr(const sockaddr *name) {
  char buf[1024];

  if (name->sa_family == AF_INET) {
    auto v4 = (const sockaddr_in *)name;
    return IpAddr((std::uint32_t)ntohl(v4->sin_addr.s_addr), {}, {},
                  ntohs(v4->sin_port));
  } else if (name->sa_family == AF_INET6) {
    auto v6 = (const sockaddr_in6 *)name;
    auto addr = std::bit_cast<std::array<unsigned char, 16>>(v6->sin6_addr);
    return IpAddr(std::nullopt,
                  std::vector<unsigned char>{addr.rbegin(), addr.rend()}, {},
                  ntohs(v6->sin6_port));
  }

  return std::nullopt;
}

std::pair<std::unique_ptr<sockaddr>, size_t> to_sockaddr(const IpAddr &addr) {
  if (auto v = addr["v4_addr"_f]) {
    auto res = new sockaddr_in();
    res->sin_family = AF_INET;
    res->sin_addr.s_addr = htonl(v.value());
    res->sin_port = htons(addr["port"_f].value());
    return {std::unique_ptr<sockaddr>{(sockaddr *)res}, sizeof(sockaddr_in)};
  } else {
    auto res = new sockaddr_in6();
    res->sin6_family = AF_INET6;
    std::array<unsigned char, 16> arr;
    std::copy(addr["v6_addr"_f].value().begin(),
              addr["v6_addr"_f].value().end(), arr.rbegin());
    res->sin6_addr = *(IN6_ADDR *)&arr;
    res->sin6_port = htons(addr["port"_f].value());
    return {std::unique_ptr<sockaddr>{(sockaddr *)res}, sizeof(sockaddr_in6)};
  }

  return {nullptr, 0};
}

bool is_localhost(const sockaddr *name) {
  if (name->sa_family == AF_INET) {
    auto v4 = (const sockaddr_in *)name;
    return v4->sin_addr.s_net == 0x7f;
  } else if (name->sa_family == AF_INET6) {
    auto v6 = (const sockaddr_in6 *)name;
    return v6->sin6_addr.u.Word[0] == 0 && v6->sin6_addr.u.Word[1] == 0 &&
           v6->sin6_addr.u.Word[2] == 0 && v6->sin6_addr.u.Word[3] == 0 &&
           v6->sin6_addr.u.Word[4] == 0 && v6->sin6_addr.u.Word[5] == 0 &&
           v6->sin6_addr.u.Word[6] == 0 && v6->sin6_addr.u.Byte[14] == 0 &&
           v6->sin6_addr.u.Byte[15] == 1;
  }

  return false;
}

bool is_inet(const sockaddr *name) {
  return name->sa_family == AF_INET || name->sa_family == AF_INET6;
}

bool sockequal(const sockaddr *l, const sockaddr *r) {
  if (l->sa_family == r->sa_family) {
    if (l->sa_family == AF_INET) {
      auto l4 = (const sockaddr_in *)l;
      auto r4 = (const sockaddr_in *)r;

      return l4->sin_addr.S_un.S_addr == r4->sin_addr.S_un.S_addr &&
             l4->sin_port == r4->sin_port;
    } else if (l->sa_family == AF_INET6) {
      auto l6 = (const sockaddr_in6 *)l;
      auto r6 = (const sockaddr_in6 *)r;

      return l6->sin6_port == r6->sin6_port &&
             std::equal(l6->sin6_addr.u.Byte, l6->sin6_addr.u.Byte + 16,
                        r6->sin6_addr.u.Byte);
    }

    return false; // FIXME: add equal checking for more family
  }

  return false;
}

#endif

#ifndef PROXINJECT_INJECTEE_WINNET
#define PROXINJECT_INJECTEE_WINNET

#include "winraii.hpp"
#include <WinSock2.h>

std::optional<IpAddr> to_ip_addr(const sockaddr *name) {
  char buf[1024];

  if (name->sa_family == AF_INET) {
    auto v4 = (const sockaddr_in *)name;
    return IpAddr((std::uint32_t)ntohl(v4->sin_addr.s_addr), {},
                  ntohs(v4->sin_port));
  } else if (name->sa_family == AF_INET6) {
    auto v6 = (const sockaddr_in6 *)name;
    auto addr = std::bit_cast<std::array<unsigned char, 16>>(v6->sin6_addr);
    return IpAddr(std::nullopt,
                  std::vector<unsigned char>{addr.rbegin(), addr.rend()},
                  ntohs(v6->sin6_port));
  }

  return std::nullopt;
}

std::unique_ptr<sockaddr> to_sockaddr(const IpAddr &addr) {
  auto res = std::make_unique<sockaddr>();

  if (auto v = addr["v4_addr"_f]) {
    auto v4 = (sockaddr_in *)res.get();
    v4->sin_family = AF_INET;
    v4->sin_addr.s_addr = htonl(v.value());
    v4->sin_port = htons(addr["port"_f].value());
  } else {
    auto v6 = (sockaddr_in6 *)res.get();
    v6->sin6_family = AF_INET6;
    std::array<unsigned char, 16> arr;
    std::copy(addr["v6_addr"_f].value().begin(),
              addr["v6_addr"_f].value().end(), arr.rbegin());
    v6->sin6_addr = std::bit_cast<IN6_ADDR>(arr);
    v6->sin6_port = htons(addr["port"_f].value());
  }

  return res;
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

#endif

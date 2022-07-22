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

#include <WinSock2.h>
#include <cstddef>
#include <schema.hpp>

constexpr const char SOCKS_VERSION = 5;
constexpr const char SOCKS_NO_AUTHENTICATION = 0;

constexpr const char SOCKS_CONNECT = 1;
constexpr const char SOCKS_IPV4 = 1;
constexpr const char SOCKS_DOMAINNAME = 3;
constexpr const char SOCKS_IPV6 = 4;

constexpr const char SOCKS_SUCCESS = 0;
constexpr const char SOCKS_GENERAL_FAILURE = 4;

constexpr const size_t SOCKS_REQUEST_MAX_SIZE = 134;

bool socks5_handshake(SOCKET s) {
  const char req[] = {SOCKS_VERSION, 1, SOCKS_NO_AUTHENTICATION};
  if (send(s, req, sizeof(req), 0) != sizeof(req))
    return false;

  char res[2];
  if (recv(s, res, sizeof(res), MSG_WAITALL) != sizeof(res))
    return false;

  return res[0] == SOCKS_VERSION && res[1] == SOCKS_NO_AUTHENTICATION;
}

char socks5_request_send(SOCKET s, char *buf, size_t size) {
  if (send(s, buf, size, 0) != size)
    return SOCKS_GENERAL_FAILURE;

  if (recv(s, buf, 4, 0) == SOCKET_ERROR)
    return SOCKS_GENERAL_FAILURE;

  if (buf[1] != SOCKS_SUCCESS)
    return buf[1];

  if (buf[3] == SOCKS_IPV4) {
    if (recv(s, buf + 4, 6, MSG_WAITALL) == SOCKET_ERROR)
      return SOCKS_GENERAL_FAILURE;
  } else if (buf[3] == SOCKS_IPV6) {
    if (recv(s, buf + 4, 18, MSG_WAITALL) == SOCKET_ERROR)
      return SOCKS_GENERAL_FAILURE;
  } else {
    return SOCKS_GENERAL_FAILURE;
  }

  return SOCKS_SUCCESS;
}

char socks5_request(SOCKET s, const sockaddr *addr) {
  char buf[SOCKS_REQUEST_MAX_SIZE] = {SOCKS_VERSION, SOCKS_CONNECT, 0};

  char *ptr = buf + 3;
  if (addr->sa_family == AF_INET) {
    auto v4 = (const sockaddr_in *)addr;
    *ptr++ = SOCKS_IPV4;
    *((ULONG *&)ptr)++ = v4->sin_addr.s_addr;
    *((USHORT *&)ptr)++ = v4->sin_port;
  } else if (addr->sa_family == AF_INET6) {
    auto v6 = (const sockaddr_in6 *)addr;
    *ptr++ = SOCKS_IPV6;
    ptr = std::copy((const char *)&v6->sin6_addr,
                    (const char *)(&v6->sin6_addr + 1), ptr);
    *((USHORT *&)ptr)++ = v6->sin6_port;
  } else {
    return SOCKS_GENERAL_FAILURE;
  }

  return socks5_request_send(s, buf, ptr - buf);
}

char socks5_request(SOCKET s, const IpAddr &addr) {
  char buf[SOCKS_REQUEST_MAX_SIZE] = {SOCKS_VERSION, SOCKS_CONNECT, 0};

  char *ptr = buf + 3;
  if (auto v4 = addr["v4_addr"_f]) {
    *ptr++ = SOCKS_IPV4;
    *((ULONG *&)ptr)++ = *v4;
  } else if (auto v6 = addr["v6_addr"_f]) {
    *ptr++ = SOCKS_IPV6;
    ptr = std::copy(v6->begin(), v6->end(), ptr);
  } else if (auto domain = addr["domain"_f]) {
    *ptr++ = SOCKS_DOMAINNAME;
    if (domain->size() >= 256) {
      return SOCKS_GENERAL_FAILURE;
    }
    *ptr++ = (char)domain->size();
    ptr = std::copy(domain->begin(), domain->end(), ptr);
  } else {
    return SOCKS_GENERAL_FAILURE;
  }
  *((USHORT *&)ptr)++ = *addr["port"_f];

  return socks5_request_send(s, buf, ptr - buf);
}

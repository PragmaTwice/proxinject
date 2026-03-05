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

#ifndef PROXINJECT_INJECTEE_HTTP_PROXY
#define PROXINJECT_INJECTEE_HTTP_PROXY

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <cstddef>
#include <schema.hpp>
#include <string>

constexpr const char HTTP_CONNECT_SUCCESS = 0;
constexpr const char HTTP_CONNECT_FAILURE = 1;

inline std::string base64_encode(const std::string &input) {
  static const char base64_chars[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string result;
  result.reserve(((input.size() + 2) / 3) * 4);

  size_t i = 0;
  while (i + 2 < input.size()) {
    unsigned char b0 = input[i++];
    unsigned char b1 = input[i++];
    unsigned char b2 = input[i++];

    result.push_back(base64_chars[b0 >> 2]);
    result.push_back(base64_chars[((b0 & 0x03) << 4) | (b1 >> 4)]);
    result.push_back(base64_chars[((b1 & 0x0f) << 2) | (b2 >> 6)]);
    result.push_back(base64_chars[b2 & 0x3f]);
  }

  if (i < input.size()) {
    unsigned char b0 = input[i++];
    result.push_back(base64_chars[b0 >> 2]);

    if (i < input.size()) {
      unsigned char b1 = input[i++];
      result.push_back(base64_chars[((b0 & 0x03) << 4) | (b1 >> 4)]);
      result.push_back(base64_chars[(b1 & 0x0f) << 2]);
      result.push_back('=');
    } else {
      result.push_back(base64_chars[(b0 & 0x03) << 4]);
      result.push_back('=');
      result.push_back('=');
    }
  }

  return result;
}

inline std::string format_host_port(const sockaddr *addr) {
  char host[INET6_ADDRSTRLEN];
  std::uint16_t port;

  if (addr->sa_family == AF_INET) {
    auto v4 = (const sockaddr_in *)addr;
    inet_ntop(AF_INET, &v4->sin_addr, host, sizeof(host));
    port = ntohs(v4->sin_port);
  } else if (addr->sa_family == AF_INET6) {
    auto v6 = (const sockaddr_in6 *)addr;
    inet_ntop(AF_INET6, &v6->sin6_addr, host, sizeof(host));
    port = ntohs(v6->sin6_port);
  } else {
    return "";
  }

  return std::string(host) + ":" + std::to_string(port);
}

inline std::string format_host_port(const IpAddr &addr) {
  std::string host;
  std::uint16_t port = *addr["port"_f];

  if (auto v = addr["v4_addr"_f]) {
    // Convert uint32 to dotted decimal
    std::uint32_t ip = *v;
    host = std::to_string((ip >> 24) & 0xFF) + "." +
           std::to_string((ip >> 16) & 0xFF) + "." +
           std::to_string((ip >> 8) & 0xFF) + "." +
           std::to_string(ip & 0xFF);
  } else if (auto v = addr["v6_addr"_f]) {
    // Format IPv6 address
    char buf[INET6_ADDRSTRLEN];
    IN6_ADDR in6;
    std::copy(v->begin(), v->end(), reinterpret_cast<unsigned char*>(&in6));
    inet_ntop(AF_INET6, &in6, buf, sizeof(buf));
    host = buf;
  } else if (auto v = addr["domain"_f]) {
    host = *v;
  }

  return host + ":" + std::to_string(port);
}

char http_connect(SOCKET s, const std::string &host_port,
                  const std::string &username, const std::string &password) {
  std::string request = "CONNECT " + host_port + " HTTP/1.1\r\n";
  request += "Host: " + host_port + "\r\n";

  if (!username.empty()) {
    std::string credentials = username + ":" + password;
    request += "Proxy-Authorization: Basic " + base64_encode(credentials) + "\r\n";
  }

  request += "\r\n";

  if (send(s, request.c_str(), (int)request.size(), 0) != (int)request.size())
    return HTTP_CONNECT_FAILURE;

  // Read response - look for HTTP/1.x 200
  char response[1024];
  int total_received = 0;
  bool found_end = false;

  while (total_received < sizeof(response) - 1 && !found_end) {
    int received = recv(s, response + total_received, 1, 0);
    if (received <= 0)
      return HTTP_CONNECT_FAILURE;
    total_received += received;

    // Check for end of headers (\r\n\r\n)
    if (total_received >= 4) {
      if (response[total_received - 4] == '\r' &&
          response[total_received - 3] == '\n' &&
          response[total_received - 2] == '\r' &&
          response[total_received - 1] == '\n') {
        found_end = true;
      }
    }
  }

  response[total_received] = '\0';

  // Check for success response (HTTP/1.x 200)
  if (total_received < 12)
    return HTTP_CONNECT_FAILURE;

  if (strncmp(response, "HTTP/1.", 7) != 0)
    return HTTP_CONNECT_FAILURE;

  // Find status code after "HTTP/1.x "
  const char *status_start = response + 9;
  if (strncmp(status_start, "200", 3) != 0)
    return HTTP_CONNECT_FAILURE;

  return HTTP_CONNECT_SUCCESS;
}

char http_connect(SOCKET s, const sockaddr *addr, const std::string &username,
                  const std::string &password) {
  std::string host_port = format_host_port(addr);
  if (host_port.empty())
    return HTTP_CONNECT_FAILURE;

  return http_connect(s, host_port, username, password);
}

char http_connect(SOCKET s, const IpAddr &addr, const std::string &username,
                  const std::string &password) {
  std::string host_port = format_host_port(addr);
  if (host_port.empty())
    return HTTP_CONNECT_FAILURE;

  return http_connect(s, host_port, username, password);
}

#endif

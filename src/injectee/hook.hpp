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

#ifndef PROXINJECT_INJECTEE_HOOK
#define PROXINJECT_INJECTEE_HOOK

#include "client.hpp"
#include "minhook.hpp"
#include "socks5.hpp"
#include "utils.hpp"
#include "winnet.hpp"
#include <map>
#include <protopuf/fixed_string.h>
#include <string>

inline blocking_queue<InjecteeMessage> *queue = nullptr;
inline injectee_config *config = nullptr;

template <auto F, pp::basic_fixed_string N>
struct hook_connect_fn : minhook::api<F, hook_connect_fn<F, N>> {
  using base = minhook::api<F, hook_connect_fn<F, N>>;

  template <typename... T>
  static int WSAAPI detour(SOCKET s, const sockaddr *name, int namelen,
                           T... args) {
    if (is_inet(name) && config && !is_localhost(name)) {
      auto cfg = config->get();
      if (auto v = to_ip_addr(name)) {

        auto proxy = cfg["addr"_f];
        auto log = cfg["log"_f];
        if (queue && log && log.value()) {
          queue->push(create_message<InjecteeMessage, "connect">(
              InjecteeConnect{(std::uint32_t)s, *v, proxy, N}));
        }

        if (proxy) {
          if (auto [addr, addr_size] = to_sockaddr(*proxy);
              addr && !sockequal(addr.get(), name)) {
            auto ret = base::original(s, &*addr, addr_size, args...);
            if (ret)
              return ret;

            if (!socks5_handshake(s)) {
              shutdown(s, SD_BOTH);
              return SOCKET_ERROR;
            }
            if (socks5_request(s, name) != SOCKS_SUCCESS) {
              shutdown(s, SD_BOTH);
              return SOCKET_ERROR;
            }

            return 0;
          }
        }
      }
    }
    return base::original(s, name, namelen, args...);
  }
};

struct hook_connect : hook_connect_fn<connect, "connect"> {};
struct hook_WSAConnect : hook_connect_fn<WSAConnect, "WSAConnect"> {};

struct hook_WSAConnectByList
    : minhook::api<WSAConnectByList, hook_WSAConnectByList> {
  static BOOL PASCAL detour(SOCKET s, PSOCKET_ADDRESS_LIST SocketAddress,
                            LPDWORD LocalAddressLength, LPSOCKADDR LocalAddress,
                            LPDWORD RemoteAddressLength,
                            LPSOCKADDR RemoteAddress, const timeval *timeout,
                            LPWSAOVERLAPPED Reserved) {
    if (config) {
      auto cfg = config->get();
      auto proxy = cfg["addr"_f];
      auto log = cfg["log"_f];

      for (size_t i = 0; i < SocketAddress->iAddressCount; ++i) {
        LPSOCKADDR name = SocketAddress->Address[i].lpSockaddr;

        if (is_inet(name) && !is_localhost(name)) {
          if (auto v = to_ip_addr(name)) {

            if (queue && log && log.value()) {
              queue->push(
                  create_message<InjecteeMessage, "connect">(InjecteeConnect{
                      (std::uint32_t)s, *v, proxy, "WSAConnectByList"}));
            }

            if (proxy) {
              if (auto [addr, addr_size] = to_sockaddr(*proxy);
                  addr && !sockequal(addr.get(), name)) {
                auto ret = connect(s, &*addr, addr_size);
                if (ret)
                  return ret;

                if (!socks5_handshake(s)) {
                  shutdown(s, SD_BOTH);
                  continue;
                }
                if (socks5_request(s, name) != SOCKS_SUCCESS) {
                  shutdown(s, SD_BOTH);
                  continue;
                }

                return 0;
              }
            }
          }
        }
      }

      if (proxy)
        return SOCKET_ERROR;
    }

    return original(s, SocketAddress, LocalAddressLength, LocalAddress,
                    RemoteAddressLength, RemoteAddress, timeout, Reserved);
  }
};

inline const std::map<std::string, std::uint16_t> service_map = {
#define X(name, port, _) {name, port},
#include "services.inc"
#undef X
};

inline std::optional<IpAddr> ipaddr_from_name(const std::string &nodename,
                                              const std::string &servicename) {
  std::uint16_t port;
  if (std::all_of(servicename.begin(), servicename.end(),
                  [](char c) { return std::isdigit(c); })) {
    port = std::stoi(servicename);
  } else if (auto iter = service_map.find(servicename);
             iter != service_map.end()) {
    port = iter->second;
  } else {
    return std::nullopt;
  }

  asio::error_code ec;
  if (auto addr = ip::make_address(nodename, ec); !ec) {
    return from_asio(addr, port);
  } else {
    return IpAddr({}, {}, nodename, port);
  }
}

inline std::optional<IpAddr> ipaddr_from_name(const std::wstring &nodename,
                                              const std::wstring &servicename) {

  return ipaddr_from_name(utf8_encode(nodename), utf8_encode(servicename));
}

template <auto F, pp::basic_fixed_string N>
struct hook_WSAConnectByName : minhook::api<F, hook_WSAConnectByName<F, N>> {
  using base = minhook::api<F, hook_WSAConnectByName<F, N>>;

  template <typename LPSTR>
  static BOOL PASCAL detour(SOCKET s, LPSTR nodename, LPSTR servicename,
                            LPDWORD LocalAddressLength, LPSOCKADDR LocalAddress,
                            LPDWORD RemoteAddressLength,
                            LPSOCKADDR RemoteAddress,
                            const struct timeval *timeout,
                            LPWSAOVERLAPPED Reserved) {
    if (config) {
      auto cfg = config->get();
      auto proxy = cfg["addr"_f];
      auto log = cfg["log"_f];

      if (auto addr = ipaddr_from_name(nodename, servicename)) {
        if (queue && log && log.value()) {
          queue->push(create_message<InjecteeMessage, "connect">(
              InjecteeConnect{(std::uint32_t)s, addr, proxy, N}));
        }
      }
    }

    return base::original(s, nodename, servicename, LocalAddressLength,
                          LocalAddress, RemoteAddressLength, RemoteAddress,
                          timeout, Reserved);
  }
};

struct hook_WSAConnectByNameA
    : hook_WSAConnectByName<WSAConnectByNameA, "WSAConnectByNameA"> {};
struct hook_WSAConnectByNameW
    : hook_WSAConnectByName<WSAConnectByNameW, "WSAConnectByNameW"> {};

template <typename T, typename... Ts> minhook::status create_hooks() {
  if (auto status = T::create(); status.error()) {
    return status;
  }

  if constexpr (sizeof...(Ts) > 0) {
    return create_hooks<Ts...>();
  }

  return MH_OK;
}

inline minhook::status hook_create_all() {
  return create_hooks<hook_connect, hook_WSAConnect, hook_WSAConnectByList,
                      hook_WSAConnectByNameA, hook_WSAConnectByNameW>();
}

#endif

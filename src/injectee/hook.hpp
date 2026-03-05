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
#include "http_proxy.hpp"
#include "minhook.hpp"
#include "socks5.hpp"
#include "utils.hpp"
#include "winnet.hpp"
#include <map>
#include <protopuf/fixed_string.h>
#include <string>

inline blocking_queue<InjecteeMessage> *queue = nullptr;
inline injectee_config *config = nullptr;
inline std::map<SOCKET, bool> *nbio_map = nullptr;

struct hook_ioctlsocket : minhook::api<ioctlsocket, hook_ioctlsocket> {
  static int WSAAPI detour(SOCKET s, long cmd, u_long FAR *argp) {
    if (nbio_map && cmd == FIONBIO) {
      (*nbio_map)[s] = *argp;
    }

    return original(s, cmd, argp);
  }
};
struct hook_WSAAsyncSelect : minhook::api<WSAAsyncSelect, hook_WSAAsyncSelect> {
  static int WSAAPI detour(SOCKET s, HWND hWnd, u_int wMsg, long lEvent) {
    if (nbio_map) {
      (*nbio_map)[s] = true;
    }

    return original(s, hWnd, wMsg, lEvent);
  }
};
struct hook_WSAEventSelect : minhook::api<WSAEventSelect, hook_WSAEventSelect> {
  static int WSAAPI detour(SOCKET s, WSAEVENT hEventObject,
                           long lNetworkEvents) {
    if (nbio_map) {
      (*nbio_map)[s] = true;
    }

    return original(s, hEventObject, lNetworkEvents);
  }
};

struct blocking_scope {
  SOCKET sock;

  blocking_scope(SOCKET s) : sock(s) {
    u_long nb = FALSE;
    hook_ioctlsocket::original(sock, FIONBIO, &nb);
  }
  ~blocking_scope() {
    if (nbio_map) {
      u_long nb = FALSE;
      if (auto iter = nbio_map->find(sock); iter != nbio_map->end()) {
        nb = (u_long)iter->second;
      }
      hook_ioctlsocket::original(sock, FIONBIO, &nb);
    }
  }

  blocking_scope(const blocking_scope &) = delete;
  blocking_scope(blocking_scope &&) = delete;
};

// Proxy type constants
constexpr std::uint32_t PROXY_TYPE_SOCKS5 = 0;
constexpr std::uint32_t PROXY_TYPE_HTTP = 1;

// Helper to perform proxy handshake based on config
inline bool proxy_handshake(SOCKET s, const InjectorConfig &cfg) {
  auto proxy_type = cfg["proxy_type"_f];
  auto username = cfg["username"_f];
  auto password = cfg["password"_f];

  std::string user = username ? *username : "";
  std::string pass = password ? *password : "";
  std::uint32_t type = proxy_type ? *proxy_type : PROXY_TYPE_SOCKS5;

  if (type == PROXY_TYPE_HTTP) {
    // HTTP CONNECT doesn't have a separate handshake phase
    return true;
  } else {
    // SOCKS5
    if (!user.empty()) {
      return socks5_handshake_with_auth(s, user, pass);
    } else {
      return socks5_handshake(s);
    }
  }
}

// Helper to perform proxy request based on config
template <typename AddrType>
inline bool proxy_request(SOCKET s, const InjectorConfig &cfg,
                          const AddrType &target_addr) {
  auto proxy_type = cfg["proxy_type"_f];
  auto username = cfg["username"_f];
  auto password = cfg["password"_f];

  std::string user = username ? *username : "";
  std::string pass = password ? *password : "";
  std::uint32_t type = proxy_type ? *proxy_type : PROXY_TYPE_SOCKS5;

  if (type == PROXY_TYPE_HTTP) {
    return http_connect(s, target_addr, user, pass) == HTTP_CONNECT_SUCCESS;
  } else {
    return socks5_request(s, target_addr) == SOCKS_SUCCESS;
  }
}

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
            blocking_scope scope(s);

            auto ret = base::original(s, addr.get(), addr_size, args...);
            if (ret)
              return ret;

            if (!proxy_handshake(s, cfg)) {
              shutdown(s, SD_BOTH);
              return SOCKET_ERROR;
            }
            if (!proxy_request(s, cfg, name)) {
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
                blocking_scope scope(s);

                auto ret = hook_connect::original(s, addr.get(), addr_size);
                if (ret)
                  return ret;

                if (!proxy_handshake(s, cfg)) {
                  shutdown(s, SD_BOTH);
                  continue;
                }
                if (!proxy_request(s, cfg, name)) {
                  shutdown(s, SD_BOTH);
                  continue;
                }

                *RemoteAddressLength =
                    std::min(*RemoteAddressLength, (DWORD)addr_size);
                memcpy(RemoteAddress, addr.get(), *RemoteAddressLength);

                sockaddr local;
                int local_size = sizeof(local);
                getsockname(s, &local, &local_size);

                *LocalAddressLength =
                    std::min(*LocalAddressLength, (DWORD)local_size);
                memcpy(LocalAddress, &local, *LocalAddressLength);

                return TRUE;
              }
            }
          }
        }
      }

      if (proxy)
        return FALSE;
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

  template <typename Char>
  static BOOL PASCAL detour(SOCKET s, Char *nodename, Char *servicename,
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

        if (proxy) {
          if (auto [proxysa, addr_size] = to_sockaddr(*proxy); proxysa) {
            blocking_scope scope(s);

            auto ret = hook_connect::original(s, proxysa.get(), addr_size);
            if (ret)
              return ret;

            if (!proxy_handshake(s, cfg)) {
              shutdown(s, SD_BOTH);
              return FALSE;
            }
            if (!proxy_request(s, cfg, *addr)) {
              shutdown(s, SD_BOTH);
              return FALSE;
            }

            sockaddr peer;
            int peer_size = sizeof(peer);
            getsockname(s, &peer, &peer_size);

            *RemoteAddressLength =
                std::min(*RemoteAddressLength, (DWORD)peer_size);
            memcpy(RemoteAddress, &peer, *RemoteAddressLength);

            sockaddr local;
            int local_size = sizeof(local);
            getsockname(s, &local, &local_size);

            *LocalAddressLength =
                std::min(*LocalAddressLength, (DWORD)local_size);
            memcpy(LocalAddress, &local, *LocalAddressLength);

            return TRUE;
          }
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

template <typename Char> struct startup_info_ptr_impl;

template <>
struct startup_info_ptr_impl<char> : std::type_identity<LPSTARTUPINFOA> {};

template <>
struct startup_info_ptr_impl<wchar_t> : std::type_identity<LPSTARTUPINFOW> {};

template <typename Char>
using startup_info_ptr = typename startup_info_ptr_impl<Char>::type;

template <auto F>
struct hook_CreateProcess : minhook::api<F, hook_CreateProcess<F>> {
  using base = minhook::api<F, hook_CreateProcess<F>>;

  template <typename Char>
  static BOOL WINAPI detour(const Char *lpApplicationName, Char *lpCommandLine,
                            LPSECURITY_ATTRIBUTES lpProcessAttributes,
                            LPSECURITY_ATTRIBUTES lpThreadAttributes,
                            BOOL bInheritHandles, DWORD dwCreationFlags,
                            LPVOID lpEnvironment,
                            const Char *lpCurrentDirectory,
                            startup_info_ptr<Char> lpStartupInfo,
                            LPPROCESS_INFORMATION lpProcessInformation) {
    BOOL res = base::original(
        lpApplicationName, lpCommandLine, lpProcessAttributes,
        lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
        lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    if (res && config) {
      auto cfg = config->get();
      auto subprocess = cfg["subprocess"_f];

      if (queue && subprocess && subprocess.value()) {
        queue->push(create_message<InjecteeMessage, "subpid">(
            lpProcessInformation->dwProcessId));
      }
    }

    return res;
  }
};

struct hook_CreateProcessA : hook_CreateProcess<CreateProcessA> {};
struct hook_CreateProcessW : hook_CreateProcess<CreateProcessW> {};

struct hook_ConnectEx {

  static inline LPFN_CONNECTEX ConnectEx = nullptr;

  static LPFN_CONNECTEX GetConnectEx() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    DWORD numBytes = 0;
    GUID guid = WSAID_CONNECTEX;
    LPFN_CONNECTEX ConnectExPtr = nullptr;

    WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, (void *)&guid, sizeof(guid),
             (void *)&ConnectExPtr, sizeof(ConnectExPtr), &numBytes, nullptr,
             nullptr);

    closesocket(s);
    return ConnectExPtr;
  }

  static inline decltype(ConnectEx) original = nullptr;

  static BOOL PASCAL detour(SOCKET s, const struct sockaddr *name, int namelen,
                            PVOID lpSendBuffer, DWORD dwSendDataLength,
                            LPDWORD lpdwBytesSent, LPOVERLAPPED lpOverlapped) {
    if (is_inet(name) && config && !is_localhost(name)) {
      auto cfg = config->get();
      if (auto v = to_ip_addr(name)) {

        auto proxy = cfg["addr"_f];
        auto log = cfg["log"_f];
        if (queue && log && log.value()) {
          queue->push(create_message<InjecteeMessage, "connect">(
              InjecteeConnect{(std::uint32_t)s, *v, proxy, "ConnectEx"}));
        }

        if (proxy) {
          if (auto [addr, addr_size] = to_sockaddr(*proxy);
              addr && !sockequal(addr.get(), name)) {
            blocking_scope scope(s);

            auto ret = hook_connect::original(s, addr.get(), addr_size);
            if (ret)
              return ret;

            if (!proxy_handshake(s, cfg)) {
              shutdown(s, SD_BOTH);
              return FALSE;
            }
            if (!proxy_request(s, cfg, name)) {
              shutdown(s, SD_BOTH);
              return FALSE;
            }

            if (lpSendBuffer) {
              int len =
                  send(s, (const char *)lpSendBuffer, dwSendDataLength, 0);
              if (len == SOCKET_ERROR) {
                shutdown(s, SD_BOTH);
                return FALSE;
              }

              *lpdwBytesSent = len;
            }

            return TRUE;
          }
        }
      }
    }
    return original(s, name, namelen, lpSendBuffer, dwSendDataLength,
                    lpdwBytesSent, lpOverlapped);
  }

  static minhook::status create() {
    ConnectEx = GetConnectEx();
    return minhook::create(ConnectEx, detour, original);
  }

  static minhook::status remove() { return minhook::remove(ConnectEx); }
};

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
                      hook_WSAConnectByNameA, hook_WSAConnectByNameW,
                      hook_CreateProcessA, hook_CreateProcessW,
                      hook_ioctlsocket, hook_WSAAsyncSelect,
                      hook_WSAEventSelect, hook_ConnectEx>();
}

#endif

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

#include "client.hpp"
#include "minhook.hpp"
#include "socks5.hpp"
#include "winnet.hpp"
#include <iostream>
#include <string>

blocking_queue<InjecteeMessage> *queue = nullptr;
injectee_config *config = nullptr;

template <auto F> struct hook_connect_fn : minhook::api<F, hook_connect_fn<F>> {
  using base = minhook::api<F, hook_connect_fn<F>>;

  template <typename... T>
  static int detour(SOCKET s, const sockaddr *name, int namelen, T... args) {
    if ((name->sa_family == AF_INET || name->sa_family == AF_INET6) && config &&
        !is_localhost(name)) {
      auto cfg = config->get();
      if (auto v = to_ip_addr(name)) {

        auto proxy = cfg["addr"_f];
        auto log = cfg["log"_f];
        if (queue && log && log.value()) {
          queue->push(create_message<InjecteeMessage, "connect">(
              InjecteeConnect{(std::uint32_t)s, *v, proxy}));
        }

        if (proxy) {
          if (auto [addr, addr_size] = to_sockaddr(*proxy); addr) {
            auto ret = base::original(s, &*addr, addr_size, args...);
            if (ret)
              return ret;

            if (!socks5_handshake(s))
              return SOCKET_ERROR;
            if (socks5_request(s, name) != SOCKS_SUCCESS)
              return SOCKET_ERROR;

            return 0;
          }
        }
      }
    }
    return base::original(s, name, namelen, args...);
  }
};

struct hook_connect : hook_connect_fn<connect> {};
struct hook_WSAConnect : hook_connect_fn<WSAConnect> {};

struct hook_WSAConnectByList
    : minhook::api<WSAConnectByList, hook_WSAConnectByList> {
  static BOOL detour(SOCKET s, PSOCKET_ADDRESS_LIST SocketAddress,
                     LPDWORD LocalAddressLength, LPSOCKADDR LocalAddress,
                     LPDWORD RemoteAddressLength, LPSOCKADDR RemoteAddress,
                     const timeval *timeout, LPWSAOVERLAPPED Reserved) {
    return original(s, SocketAddress, LocalAddressLength, LocalAddress,
                    RemoteAddressLength, RemoteAddress, timeout, Reserved);
  }
};

void do_client(HINSTANCE dll_handle) {
  {
    asio::io_context io_context(1);

    auto qu =
        std::make_unique<blocking_queue<InjecteeMessage>>(io_context, 1024);
    auto cfg = std::make_unique<injectee_config>();

    scope_ptr_bind queue_bind(queue, qu.get());
    scope_ptr_bind config_bind(config, cfg.get());

    injectee_client c(io_context, proxinject_endpoint, *queue, *config);
    asio::co_spawn(io_context, c.start(), asio::detached);

    io_context.run();
  }
  FreeLibrary(dll_handle);
}

BOOL WINAPI DllMain(HINSTANCE dll_handle, DWORD reason, LPVOID reserved) {
  switch (reason) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(dll_handle);
    minhook::init();
    hook_connect::create();
    hook_WSAConnect::create();
    hook_WSAConnectByList::create();
    minhook::enable();
    std::thread(do_client, dll_handle).detach();
    break;

  case DLL_PROCESS_DETACH:
    minhook::disable();
    minhook::deinit();
    break;
  }
  return TRUE;
}

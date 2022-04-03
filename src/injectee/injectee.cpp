#include "client.hpp"
#include "minhook.hpp"
#include "winnet.hpp"
#include <iostream>
#include <string>
#include "socks5.hpp"

blocking_queue<InjecteeMessage> queue;
injectee_config config;

struct hook_connect : minhook::api<connect, hook_connect> {
  static int detour(SOCKET s, const sockaddr *name, int namelen) {
    if ((name->sa_family == AF_INET || name->sa_family == AF_INET6) &&
        !is_localhost(name)) {
      if (auto v = to_ip_addr(name)) {
        auto proxy = config.get_addr();
        queue.push(create_message<InjecteeMessage, "connect">(
            InjecteeConnect{(std::uint32_t)s, *v, proxy}));
        if (proxy) {
          if (auto addr = to_sockaddr(*proxy)) {
            auto ret = original(s, &*addr, sizeof(sockaddr));
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
    return original(s, name, namelen);
  }
};

void do_client(HINSTANCE dll_handle) {
  asio::io_context io_context(1);

  injectee_client c(io_context, proxinject_endpoint, queue, config);
  asio::co_spawn(io_context, c.start(), asio::detached);

  io_context.run();
  FreeLibrary(dll_handle);
}

BOOL WINAPI DllMain(HINSTANCE dll_handle, DWORD reason, LPVOID reserved) {
  switch (reason) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(dll_handle);
    minhook::init();
    hook_connect::create();
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

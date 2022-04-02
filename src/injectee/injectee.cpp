#include "client.hpp"
#include "minhook.hpp"
#include <iostream>
#include <string>

std::string get_addr_string(const sockaddr *name) {
  char buf[1024];

  if (name->sa_family == AF_INET) {
    auto v4 = (const sockaddr_in *)name;
    inet_ntop(v4->sin_family, (const void *)&v4->sin_addr, buf, 1024 - 1);
    return std::string(buf) + ":" + std::to_string(ntohs(v4->sin_port));
  } else if (name->sa_family == AF_INET6) {
    auto v6 = (const sockaddr_in6 *)name;
    inet_ntop(v6->sin6_family, (const void *)&v6->sin6_addr, buf, 1024 - 1);
    return std::string(buf) + ":" + std::to_string(ntohs(v6->sin6_port));
  }

  return {};
}

bool is_localhost(const sockaddr *name) {
  if (name->sa_family == AF_INET) {
    auto v4 = (const sockaddr_in *)name;
    return v4->sin_addr.s_addr == 0x0100007f;
  }

  return false;
}

blocking_queue<InjecteeMessage> queue;

struct hook_connect : minhook::api<connect, hook_connect> {
  static int detour(SOCKET s, const sockaddr *name, int namelen) {
    if ((name->sa_family == AF_INET || name->sa_family == AF_INET6) &&
        !is_localhost(name)) {
      queue.push(create_message<InjecteeMessage, "connect">(
          InjecteeConnect{(std::int32_t)s, get_addr_string(name)}));
    }
    return original(s, name, namelen);
  }
};

void client(HINSTANCE dll_handle) {
  asio::io_context io_context(1);

  injectee_client c(io_context, proxinject_endpoint, queue);
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
    std::thread(client, dll_handle).detach();
    break;

  case DLL_PROCESS_DETACH:
    minhook::disable();
    minhook::deinit();
    break;
  }
  return TRUE;
}

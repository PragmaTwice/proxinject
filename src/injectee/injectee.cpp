#include "client.hpp"
#include "minhook.hpp"
#include <iostream>
#include <string>

std::optional<IpAddr> get_addr_string(const sockaddr *name) {
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

blocking_queue<InjecteeMessage> queue;

struct hook_connect : minhook::api<connect, hook_connect> {
  static int detour(SOCKET s, const sockaddr *name, int namelen) {
    if ((name->sa_family == AF_INET || name->sa_family == AF_INET6) &&
        !is_localhost(name)) {
      if (auto v = get_addr_string(name)) {
        queue.push(create_message<InjecteeMessage, "connect">(
            InjecteeConnect{(std::uint32_t)s, *v}));
      }
    }
    return original(s, name, namelen);
  }
};

void do_client(HINSTANCE dll_handle) {
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
    std::thread(do_client, dll_handle).detach();
    break;

  case DLL_PROCESS_DETACH:
    minhook::disable();
    minhook::deinit();
    break;
  }
  return TRUE;
}

#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include "minhook.hpp"

struct hook_connect : minhook::api<connect, hook_connect> {
    static int WINAPI detour(SOCKET s, const sockaddr* name, int namelen) {
        if (name->sa_family == AF_INET) {
            auto v4 = (const sockaddr_in*)name;
            std::cout << s << " -> " << inet_ntoa(v4->sin_addr) << ":" << ntohs(v4->sin_port) << "\n";
        }
        return original(s, name, namelen);
    }
};

BOOL WINAPI DllMain(HINSTANCE dll_handle, DWORD reason, LPVOID reserved)
{
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(dll_handle);
        minhook::init();
        hook_connect::create();
        minhook::enable();
        std::cout << "inited\n";
        break;

    case DLL_PROCESS_DETACH:
        minhook::disable();
        minhook::deinit();
        break;
    }
    return TRUE;
}

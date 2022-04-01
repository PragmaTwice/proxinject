#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include "minhook.hpp"
#include "rpc.hpp"

using namespace std;

struct hook_connect : minhook::api<connect, hook_connect> {
    static int detour(SOCKET s, const sockaddr* name, int namelen) {
        if (name->sa_family == AF_INET) {
            auto v4 = (const sockaddr_in*)name;
            auto addr = inet_ntoa(v4->sin_addr);
            std::cout << s << " -> " << addr << ":" << ntohs(v4->sin_port) << endl;
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
        break;

    case DLL_PROCESS_DETACH:
        minhook::disable();
        minhook::deinit();
        break;
    }
    return TRUE;
}

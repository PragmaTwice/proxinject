# proxinject

a socks5 proxy injection tool for Windows, making all programs proxy-able

## dependences

environments:
- C++ compiler (with C++20 support)
- Windows SDK (with winsock2 support)

libraries: (you do not need to download/install them manually, because these will be downloaded and configured automatically in cmake)
- minhook
- asio
- protopuf

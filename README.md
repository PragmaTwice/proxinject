# proxinject

a socks5 proxy injection tool for Windows: just select some processes and make them proxy-able!

## development dependences

### environments:

- C++ compiler (with C++20 support)
- Windows SDK (with winsock2 support)

### libraries: 
(you do not need to download/install them manually)

#### proxinjectee
- minhook
- asio (standalone)
- PragmaTwice/protopuf

#### proxinjector GUI
- asio (standalone)
- PragmaTwice/protopuf
- cycfi/elements

#### proxinjector CLI
- asio (standalone)
- PragmaTwice/protopuf
- p-ranav/argparse
- gabime/spdlog

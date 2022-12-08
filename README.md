<img src="resources/proxinject.png" width="128" alt="logo">

# proxinject
[![Build](https://github.com/PragmaTwice/proxinject/actions/workflows/build.yml/badge.svg)](https://github.com/PragmaTwice/proxinject/actions/workflows/build.yml)
[![Release](https://shields.io/github/v/release/PragmaTwice/proxinject?display_name=tag&include_prereleases)](https://github.com/PragmaTwice/proxinject/releases)
[![WinGet](https://img.shields.io/badge/winget-proxinject-blue)](https://github.com/microsoft/winget-pkgs/tree/master/manifests/p/PragmaTwice/proxinject)

*A socks5 proxy injection tool for **Windows**: just select some processes and make them proxy-able!*

## Preview

### proxinject GUI

![screenshot](./docs/screenshot.png)

### proxinject CLI
```
$ ./proxinjector-cli -h
Usage: proxinjector-cli [options]

A socks5 proxy injection tool for Windows: just select some processes and make them proxy-able!
Please visit https://github.com/PragmaTwice/proxinject for more information.

Optional arguments:
-h --help                       shows help message and exits [default: false]
-v --version                    prints version information and exits [default: false]
-i --pid                        pid of a process to inject proxy (integer) [default: {}]
-n --name                       short filename of a process with wildcard matching to inject proxy (string, without directory and file extension, e.g. `python`, `py*`, `py??on`) [default: {}]
-P --path                       full filename of a process with wildcard matching to inject proxy (string, with directory and file extension, e.g. `C:/programs/python.exe`, `C:/programs/*.exe`) [default: {}]
-r --name-regexp                regular expression for short filename of a process to inject proxy (string, without directory and file extension, e.g. `python`, `py.*|exp.*`) [default: {}]
-R --path-regexp                regular expression for full filename of a process to inject proxy (string, with directory and file extension, e.g. `C:/programs/python.exe`, `C:/programs/(a|b).*\.exe`) [default: {}]
-e --exec                       command line started with an executable to create a new process and inject proxy (string, e.g. `python` or `C:\Program Files\a.exe --some-option`) [default: {}]
-l --enable-log                 enable logging for network connections [default: false]
-p --set-proxy                  set a proxy address for network connections (string, e.g. `127.0.0.1:1080`) [default: ""]
-w --new-console-window         create a new console window while a new console process is executed in `-e` [default: false]
-s --subprocess                 inject subprocesses created by these already injected processes [default: false]
```

## How to Install

Choose whichever method you like:

- Download the latest portable archive (`.zip`) or installer (`.exe`) from the [Releases Page](https://github.com/PragmaTwice/proxinject/releases), OR
- Type `winget install PragmaTwice.proxinject` in the terminal ([winget](https://github.com/microsoft/winget-cli) is required)

Or build from source (not recommended for non-professionals):

```sh
# make sure your develop environment is well configured in powershell
git clone https://github.com/PragmaTwice/proxinject.git
cd proxinject
./build.ps1 -mode Release -arch x64 # build the project via CMake and msbuild
# your built binaries are now in the `./release` directory, enjoy it now!
makensis /DVERSION=$(git describe --tags) setup.nsi # (optional) genrate an installer via NSIS
```

## Development Dependencies

### environments:

- C++ compiler (with C++20 support, currently MSVC)
- Windows SDK (with winsock2 support)
- CMake 3

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

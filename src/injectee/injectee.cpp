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

#include "hook.hpp"
#include <utils.hpp>

void do_client(HINSTANCE dll_handle, std::uint16_t port) {
  {
    asio::io_context io_context(1);

    auto qu =
        std::make_unique<blocking_queue<InjecteeMessage>>(io_context, 1024);
    auto cfg = std::make_unique<injectee_config>();
    auto sock_map = std::make_unique<std::map<SOCKET, bool>>();

    scope_ptr_bind queue_bind(queue, qu.get());
    scope_ptr_bind config_bind(config, cfg.get());
    scope_ptr_bind map_bind(nbio_map, sock_map.get());

    injectee_client c(io_context, tcp::endpoint(localhost, port), *queue,
                      *config);
    asio::co_spawn(io_context, c.start(), asio::detached);

    io_context.run();
  }
  FreeLibrary(dll_handle);
}

std::uint16_t get_port() {
  handle mapping = open_mapping(get_port_mapping_name(GetCurrentProcessId()));

  mapped_buffer port_buf(mapping.get());
  return *(std::uint16_t *)port_buf.get();
}

BOOL WINAPI DllMain(HINSTANCE dll_handle, DWORD reason, LPVOID reserved) {
  switch (reason) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(dll_handle);
    minhook::init();

    hook_create_all();

    minhook::enable();
    std::thread(do_client, dll_handle, get_port()).detach();
    break;

  case DLL_PROCESS_DETACH:
    minhook::disable();
    minhook::deinit();
    break;
  }
  return TRUE;
}

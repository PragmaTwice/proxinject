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

#include <asio.hpp>

#include "injector.hpp"
#include "injector_gui.hpp"
#include "server.hpp"
#include "text_box.hpp"
#include "utils.hpp"

void do_server(injector_server &server, ce::view &view,
               process_vector &process_vec, auto &...elements) {
  asio::io_context io_context(1);

  auto acceptor = tcp::acceptor(io_context, auto_endpoint);
  server.set_port(acceptor.local_endpoint().port());

  asio::co_spawn(io_context,
                 listener<injectee_session_ui>(std::move(acceptor), server,
                                               view, process_vec, elements...),
                 asio::detached);

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto) { io_context.stop(); });

  io_context.run();
}

int main(int argc, char *argv[]) {
  injector_server server;
  process_vector process_vec;

  ce::app app(argc, argv, "proxinject", "proxinject");
  ce::window win(app.name());
  win.on_close = [&app]() { app.stop(); };

  ce::view view(win);

  auto &&[controls, list_ptr, log_ptr] =
      make_controls(server, view, process_vec);
  view.content(controls, ce::box(bg_color));

  std::thread([&] {
    do_server(server, view, process_vec, *list_ptr, *log_ptr);
  }).detach();

  app.run();
}

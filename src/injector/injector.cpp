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
#include "server.hpp"
#include "text_box.hpp"
#include "utils.hpp"
#include <elements.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

using namespace cycfi::elements;

auto constexpr bkd_color = rgba(35, 35, 37, 255);
auto background = box(bkd_color);

constexpr auto bred = colors::red.opacity(0.4);
constexpr auto bblue = colors::blue.opacity(0.4);
constexpr auto brblue = colors::royal_blue.opacity(0.4);

std::vector<std::pair<DWORD, std::string>> process_vec;

struct injectee_session_ui : injectee_session {
  injectee_session_ui(tcp::socket socket, injector_server &server, view &view_,
                      dynamic_list &list_, selectable_text_box &log_)
      : injectee_session(std::move(socket), server), view_(view_), list_(list_),
        log_(log_) {}

  view &view_;
  dynamic_list &list_;
  selectable_text_box &log_;

  asio::awaitable<void> process_connect(const InjecteeConnect &msg) override {
    std::stringstream stream;
    stream << (int)pid_ << ": connect " << msg["addr"_f].value();
    if (auto v = msg["proxy"_f])
      stream << " via " << *v;
    stream << "\n";
    log_.set_text(log_.get_text() + stream.str());
    view_.refresh();
    co_return;
  }

  asio::awaitable<void> process_pid() override {
    process_vec.emplace_back(pid_, injector::get_process_name(pid_));
    refresh();
    co_return;
  }

  void process_close() override {
    auto iter =
        std::find_if(process_vec.begin(), process_vec.end(),
                     [this](auto &&pair) { return pair.first == pid_; });
    if (iter != process_vec.end()) {
      process_vec.erase(iter);
      refresh();
    }
  }

  void refresh() {
    std::sort(process_vec.begin(), process_vec.end());
    list_.resize(process_vec.size());
    view_.refresh();
  }
};

void do_server(injector_server &server, view &view_, dynamic_list &list,
               selectable_text_box &log) {
  asio::io_context io_context(1);

  asio::co_spawn(io_context,
                 listener<injectee_session_ui>(
                     tcp::acceptor(io_context, proxinject_endpoint), server,
                     view_, list, log),
                 asio::detached);

  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](auto, auto) { io_context.stop(); });

  io_context.run();
}

auto make_controls(injector_server &server, view &view_) {
  auto [process_input, process_input_ptr] = input_box();
  auto [input_select, input_select_ptr] =
      selection_menu([](auto &&) {}, {"pid", "name"});

  auto inject_button = icon_button(icons::plus, 1.2, bblue);
  inject_button.on_click = [&server, input_select_ptr,
                            process_input_ptr](bool) {
    auto text = trim_copy(process_input_ptr->get_text());
    if (text.empty())
      return;
    if (input_select_ptr->get_text() == "pid") {
      if (all_of_digit(text)) {
        DWORD pid = std::stoul(text);
        if (pid != 0)
          server.inject(pid);
      }
    } else {
      injector::pid_by_name(text, [&server](DWORD pid) { server.inject(pid); });
    }
    process_input_ptr->set_text("");
  };

  auto remove_button = icon_button(icons::cancel, 1.2, bred);
  remove_button.on_click = [&server, input_select_ptr,
                            process_input_ptr](bool) {
    auto text = trim_copy(process_input_ptr->get_text());
    if (text.empty())
      return;
    if (input_select_ptr->get_text() == "pid") {
      if (all_of_digit(text)) {
        DWORD pid = std::stoul(text);
        if (pid != 0)
          server.close(pid);
      }
    } else {
      injector::pid_by_name(text, [&server](DWORD pid) { server.close(pid); });
    }
    process_input_ptr->set_text("");
  };

  auto process_list = share(dynamic_list(
      basic_cell_composer(process_vec.size(), [](size_t index) -> element_ptr {
        if (index < process_vec.size()) {
          auto [pid, name] = process_vec[index];
          return share(align_center(
              htile(align_center(hmax_size(80, label(std::to_string(pid)))),
                    align_center(hmin_size(100, label(name))))));
        }
        return share(align_center(label("unknown")));
      })));

  auto proxy_toggle = share(toggle_icon_button(icons::power, 1.2, brblue));
  proxy_toggle->on_click = [&server](bool on) {
    if (!on) {
      server.clear_proxy();
    }
  };

  auto log_toggle = toggle_icon_button(icons::doc, 1.2, brblue);
  log_toggle.on_click = [&server](bool on) {
    if (on) {
      server.enable_log();
    } else {
      server.disable_log();
    }
  };

  auto [addr_input, addr_input_ptr] = input_box("ip address");
  auto [port_input, port_input_ptr] = input_box("port");

  auto apply_proxy = icon_button(icons::ok, 1.2, bblue);
  apply_proxy.on_click = [&server, proxy_toggle, addr_input_ptr,
                          port_input_ptr](bool) {
    auto addr = trim_copy(addr_input_ptr->get_text());
    auto port = trim_copy(port_input_ptr->get_text());
    if (proxy_toggle->value() && all_of_digit(port))
      server.set_proxy(ip::address::from_string(addr), std::stoul(port));
  };

  auto log_box = share(selectable_text_box(""));

  // clang-format off
  return std::make_tuple(
      margin({10, 10, 10, 10},
        htile(
          hmin_size(200, 
            vtile(
              htile(
                hsize(80, input_select),
                left_margin(5, hmin_size(100, process_input)),
                left_margin(10, inject_button), 
                left_margin(5, remove_button)
              ),
              top_margin(10, 
                vmin_size(200, 
                  layer(vscroller(hold(process_list)), frame())
                )
              )
            )
          ),
          left_margin(10,
            hmin_size(200, 
              vtile(
                htile(
                  hmin_size(100, addr_input),
                  left_margin(5, hsize(100, port_input)),
                  left_margin(5, apply_proxy),
                  left_margin(10, hold(proxy_toggle)),
                  left_margin(5, log_toggle)
                ),
                top_margin(10,
                  vmin_size(200, 
                    layer(vscroller(hold(log_box)), frame())
                  )
                )
              )
            )
          )
        )
      ), process_list, log_box);
  // clang-format on
}

int main(int argc, char *argv[]) {
  injector_server server;

  app _app(argc, argv, "proxinject", "proxinject");
  window _win(_app.name());
  _win.on_close = [&_app]() { _app.stop(); };

  view view_(_win);

  auto &&[controls, list_ptr, log_ptr] = make_controls(server, view_);
  view_.content(controls, background);

  std::thread(do_server, std::ref(server), std::ref(view_), std::ref(*list_ptr),
              std::ref(*log_ptr))
      .detach();

  _app.run();
}

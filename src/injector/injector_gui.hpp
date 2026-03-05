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

#ifndef PROXINJECT_INJECTOR_INJECTOR_GUI
#define PROXINJECT_INJECTOR_INJECTOR_GUI

#include "server.hpp"
#include "ui_elements/dynamic_list.hpp"
#include "ui_elements/text_box.hpp"
#include "ui_elements/tooltip.hpp"

#include "utils.hpp"
#include "version.hpp"
#include <elements.hpp>
#include <sstream>

namespace ce = cycfi::elements;

auto constexpr bg_color = ce::rgba(35, 35, 37, 255);

constexpr auto bred = ce::colors::red.opacity(0.4);
constexpr auto bblue = ce::colors::blue.opacity(0.4);
constexpr auto brblue = ce::colors::royal_blue.opacity(0.4);
constexpr auto bcblue = ce::colors::cornflower_blue.opacity(0.4);

using process_vector = std::vector<std::pair<DWORD, std::string>>;

struct injectee_session_ui : injectee_session {
  injectee_session_ui(tcp::socket socket, injector_server &server,
                      ce::view &view_, process_vector &vec_, auto &list_,
                      auto &log_)
      : injectee_session(std::move(socket), server), view_(view_), vec_(vec_),
        list_(list_), log_(log_) {}

  ce::view &view_;
  process_vector &vec_;

  ce::dynamic_list_s &list_;
  ce::selectable_text_box &log_;

  asio::awaitable<void> process_connect(const InjecteeConnect &msg) override {
    auto curr_time = std::chrono::system_clock::now();
    auto curr_sec = std::chrono::system_clock::to_time_t(curr_time);
    auto curr_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        curr_time.time_since_epoch());
    auto curr_milli_part = curr_ms.count() % 1000;

    std::stringstream stream;
    stream << "["
           << std::put_time(std::localtime(&curr_sec), "%Y-%m-%d %H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << curr_milli_part
           << "] ";
    stream << (int)pid_ << ": " << *msg["syscall"_f] << " " << *msg["addr"_f];
    if (auto v = msg["proxy"_f])
      stream << " via " << *v;
    stream << "\n";

    view_.post([&log = log_, str = stream.str()] {
      log.set_text(log.get_text() + str);
    });
    view_.refresh();

    co_return;
  }

  asio::awaitable<void> process_pid() override {
    vec_.emplace_back(pid_, get_process_name(pid_));
    refresh();
    co_return;
  }

  void process_close() override {
    auto iter = std::find_if(vec_.begin(), vec_.end(), [this](auto &&pair) {
      return pair.first == pid_;
    });
    if (iter != vec_.end()) {
      vec_.erase(iter);
      refresh();
    }
  }

  void refresh() {
    std::sort(vec_.begin(), vec_.end());
    view_.post([&list = list_, &vec = vec_] { list.resize(vec.size()); });
    view_.refresh();
  }
};

template <typename T> auto make_tip_below(T &&element, const std::string &tip) {
  return ce::tooltip_below(
      std::forward<T>(element),
      ce::layer(ce::margin({20, 8, 20, 8}, ce::label(tip)), ce::panel{}));
}

template <typename T>
auto make_tip_below_r(T &&element, const std::string &tip) {
  auto res = make_tip_below(std::forward<T>(element), tip);

  res.location = [](const ce::rect &wh, const ce::rect &ctx) {
    return wh.move_to(ctx.right - wh.right, ctx.bottom);
  };

  return res;
}

const std::vector<std::pair<std::string_view, std::string_view>> input_tips{
    {"pid", "a process ID (e.g. `2333`)"},
    {"name",
     "a process name with wildcard matching (e.g. `python`, `py*`, `py??on`)"},
    {"name regexp",
     "a regular expression for process name (e.g. `python`, `py.*|firefox`)"},
    {"path", "a process full path with wildcard matching (e.g. "
             "`C:/program.exe`, `C:/programs/*.exe`)"},
    {"path regexp", "a regular expression for process full path (e.g. "
                    "`C:/program.exe`, `C:/programs/(a|b).*`)"},
    {"exec",
     "command line (e.g. `python`, `C:/programs/something --some-option`)"}};

const std::map<std::string_view, std::string_view>
    input_tip_texts(input_tips.begin(), input_tips.end());

const std::vector<std::string_view> input_tip_options = [] {
  std::vector<std::string_view> result;

  for (const auto &[key, _] : input_tips) {
    result.emplace_back(key);
  }

  return result;
}();

const std::vector<std::string_view> proxy_type_options = {"socks5", "http"};

auto make_controls(injector_server &server, ce::view &view,
                   process_vector &process_vec) {
  using namespace ce;

  auto [process_input, process_input_ptr] =
      input_box(std::string(input_tip_texts.at("pid")));

  auto [input_select, input_select_ptr] = selection_menu(
      [process_input_ptr](auto str) {
        process_input_ptr->_placeholder = input_tip_texts.at(str);
      },
      input_tip_options);

  auto inject_click = [input_select_ptr, process_input_ptr]<typename F>(F &&f) {
    auto text = trim_copy(process_input_ptr->get_text());
    if (text.empty())
      return;
    auto option = input_select_ptr->get_text();
    if (option == "pid") {
      if (!all_of_digit(text))
        return;

      DWORD pid = std::stoul(text);
      if (pid == 0)
        return;
      if (!std::forward<F>(f)(pid))
        return;
    } else if (option == "name") {
      bool success = false;
      injector::pid_by_name_wildcard(text, [&success, &f](DWORD pid) {
        if (std::forward<F>(f)(pid))
          success = true;
      });
      if (!success)
        return;
    } else if (option == "name regexp") {
      bool success = false;
      injector::pid_by_name_regex(text, [&success, &f](DWORD pid) {
        if (std::forward<F>(f)(pid))
          success = true;
      });
      if (!success)
        return;
    } else if (option == "path") {
      bool success = false;
      injector::pid_by_path_wildcard(text, [&success, &f](DWORD pid) {
        if (std::forward<F>(f)(pid))
          success = true;
      });
      if (!success)
        return;
    } else if (option == "path regexp") {
      bool success = false;
      injector::pid_by_path_regex(text, [&success, &f](DWORD pid) {
        if (std::forward<F>(f)(pid))
          success = true;
      });
      if (!success)
        return;
    } else if (option == "exec") {
      auto res = create_process(text);
      if (!res) {
        return;
      }

      if (!std::forward<F>(f)(res->dwProcessId)) {
        return;
      }
    }
    process_input_ptr->set_text("");
  };

  auto inject_button = icon_button(icons::plus, 1.2, bblue);
  inject_button.on_click = [&server, inject_click](bool) {
    inject_click([&server](DWORD x) { return server.inject(x); });
  };

  auto remove_button = icon_button(icons::cancel, 1.2, bred);
  remove_button.on_click = [&server, inject_click](bool) {
    inject_click([&server](DWORD x) { return server.close(x); });
  };

  auto process_list = share(dynamic_list_s(basic_cell_composer(
      process_vec.size(), [&process_vec](size_t index) -> element_ptr {
        if (index < process_vec.size()) {
          auto [pid, name] = process_vec[index];
          return share(align_center(
              htile(hsize(80, align_center(label(std::to_string(pid)))),
                    hmin_size(120, align_center(label(name))))));
        }
        return share(align_center(label("unknown")));
      })));

  auto [addr_input, addr_input_ptr] = input_box("IP address");
  auto [port_input, port_input_ptr] = input_box("port");
  auto [user_input, user_input_ptr] = input_box("username (optional)");
  auto [pass_input, pass_input_ptr] = input_box("password (optional)");

  auto [proxy_type_select, proxy_type_select_ptr] = selection_menu(
      [](auto) {},
      proxy_type_options);

  auto proxy_toggle = share(toggle_icon_button(icons::power, 1.2, brblue));
  proxy_toggle->on_click = [&server, proxy_toggle, addr_input_ptr,
                            port_input_ptr, user_input_ptr, pass_input_ptr,
                            proxy_type_select_ptr](bool on) {
    if (on) {
      auto addr = trim_copy(addr_input_ptr->get_text());
      auto port_str = trim_copy(port_input_ptr->get_text());
      if (all_of_digit(port_str) && !addr.empty() && !port_str.empty()) {
        auto port_num = std::stoul(port_str);

        // Try to parse as IP address first, then resolve as hostname
        asio::error_code ec;
        auto ip_addr = ip::make_address(addr, ec);
        if (!ec) {
          server.set_proxy(ip_addr, port_num);
        } else {
          // Resolve hostname
          try {
            asio::io_context resolver_ctx;
            ip::tcp::resolver resolver(resolver_ctx);
            auto endpoints = resolver.resolve(addr, port_str);
            if (!endpoints.empty()) {
              auto resolved_addr = endpoints.begin()->endpoint().address();
              server.set_proxy(resolved_addr, port_num);
            } else {
              proxy_toggle->value(false);
              return;
            }
          } catch (...) {
            proxy_toggle->value(false);
            return;
          }
        }

        // Set proxy type
        auto proxy_type = proxy_type_select_ptr->get_text();
        if (proxy_type == "http") {
          server.set_proxy_type(1);
        } else {
          server.set_proxy_type(0);
        }

        // Set authentication credentials
        auto username = trim_copy(user_input_ptr->get_text());
        auto password = trim_copy(pass_input_ptr->get_text());
        if (!username.empty()) {
          server.set_proxy_username(username);
        }
        if (!password.empty()) {
          server.set_proxy_password(password);
        }
      } else {
        proxy_toggle->value(false);
      }
    } else {
      server.clear_proxy();
    }
  };

  auto log_toggle = toggle_icon_button(icons::doc, 1.2, brblue);
  log_toggle.on_click = [&server](bool on) { server.enable_log(on); };

  auto subprocess_toggle = toggle_icon_button(icons::record, 1.2, brblue);
  subprocess_toggle.on_click = [&server](bool on) {
    server.enable_subprocess(on);
  };

  auto log_box = share(selectable_text_box(""));

  auto clean_button = icon_button(icons::trash, 1.2, bcblue);
  clean_button.on_click = [log_box](bool) { log_box->set_text(""); };

  auto info_button = icon_button(icons::info, 1.2, bcblue);
  info_button.on_click = [&view](bool) {
    view.add(message_box1(view, proxinject_copyright(proxinject_version),
                          icons::info, [] {}));
  };

  // clang-format off
  return std::make_tuple(
      margin({10, 10, 10, 10},
        htile(
          hmin_size(200,
            vtile(
              htile(
                hsize(80, input_select),
                left_margin(5, hmin_size(100, process_input)),
                left_margin(10, make_tip_below(inject_button, "add specific processes to inject")),
                left_margin(5, make_tip_below(remove_button, "remove specific processes from injecting"))
              ),
              top_margin(10,
                vmin_size(250,
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
                  left_margin(5, hsize(60, port_input)),
                  left_margin(5, hsize(60, make_tip_below_r(proxy_type_select, "socks5 or http proxy"))),
                  left_margin(10, make_tip_below_r(hold(proxy_toggle), "enable/disable proxy injection")),
                  left_margin(5, make_tip_below_r(log_toggle, "enable/disable connection log")),
                  left_margin(5, make_tip_below_r(subprocess_toggle, "enable/disable subprocess injection")),
                  left_margin(8, make_tip_below_r(clean_button, "clean the logging box")),
                  left_margin(5, make_tip_below_r(info_button, "software information"))
                ),
                top_margin(5,
                  htile(
                    hmin_size(100, make_tip_below(user_input, "proxy username for authentication")),
                    left_margin(5, hmin_size(100, make_tip_below(pass_input, "proxy password for authentication")))
                  )
                ),
                top_margin(10,
                  vmin_size(220,
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

#endif

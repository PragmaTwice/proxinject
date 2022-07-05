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

/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/

#ifndef PROXINJECT_INJECTOR_TOOLTIP
#define PROXINJECT_INJECTOR_TOOLTIP

#include <elements/element/popup.hpp>
#include <elements/element/proxy.hpp>
#include <functional>
#include <infra/support.hpp>

namespace cycfi {
namespace elements {
////////////////////////////////////////////////////////////////////////////
// Tooltip elements
////////////////////////////////////////////////////////////////////////////
class tooltip_below_element : public proxy_base {
public:
  using base_type = proxy_base;
  using on_hover_function = std::function<void(bool visible)>;

  template <typename Tip>
  tooltip_below_element(Tip &&tip, duration delay)
      : _tip(share(basic_popup(std::forward<Tip>(tip)))), _delay(delay) {}

  bool cursor(context const &ctx, point p, cursor_tracking status) override;
  bool key(context const &ctx, key_info k) override;

  on_hover_function on_hover = [](bool) {};
  std::function<rect(const rect &, const rect &)> location =
      [](const rect &wh, const rect &ctx) {
        return wh.move_to(ctx.left, ctx.bottom);
      };

private:
  using popup_ptr = std::shared_ptr<basic_popup_element>;
  enum status { tip_hidden, tip_delayed, tip_visible };

  rect tip_bounds(context const &ctx) const;
  void close_tip(view &view_);

  popup_ptr _tip;
  status _tip_status = tip_hidden;
  duration _delay;
  bool _cursor_in_tip = false;
};

template <typename Subject, typename Tip>
inline proxy<remove_cvref_t<Subject>, tooltip_below_element>
tooltip_below(Subject &&subject, Tip &&tip,
              duration delay = milliseconds{500}) {
  return {std::forward<Subject>(subject), std::forward<Tip>(tip), delay};
}
} // namespace elements
} // namespace cycfi

#endif

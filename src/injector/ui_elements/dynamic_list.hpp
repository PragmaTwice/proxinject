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

#ifndef PROXINJECT_INJECTOR_DYNAMIC_LIST
#define PROXINJECT_INJECTOR_DYNAMIC_LIST

#include <elements/element/dynamic_list.hpp>
#include <elements/view.hpp>

namespace cycfi {
namespace elements {

class dynamic_list_s : public element {
public:
  using composer_ptr = std::shared_ptr<cell_composer>;

  dynamic_list_s(composer_ptr composer) : _composer(composer) {}

  virtual view_limits limits(basic_context const &ctx) const override;
  void draw(context const &ctx) override;
  void layout(context const &ctx) override;

  void update();
  void update(basic_context const &ctx) const;

  virtual bool click(const context &ctx, mouse_button btn) override;
  virtual bool text(context const &ctx, text_info info) override;
  virtual bool key(const context &ctx, key_info k) override;
  virtual bool cursor(context const &ctx, point p,
                      cursor_tracking status) override;
  virtual bool scroll(context const &ctx, point dir, point p) override;
  virtual void drag(context const &ctx, mouse_button btn) override;

  void new_focus(context const &ctx, int index);
  bool wants_control() const override;
  bool wants_focus() const override;
  void begin_focus() override;
  void end_focus() override;
  element const *focus() const override;
  element *focus() override;
  void focus(std::size_t index);
  virtual void reset();
  void resize(size_t n);

  struct hit_info {
    element_ptr element;
    rect bounds = rect{};
    int index = -1;
  };

  virtual rect bounds_of(context const &ctx, int ix) const;
  virtual bool reverse_index() const { return false; }
  virtual hit_info hit_element(context const &ctx, point p, bool control) const;

protected:
  struct cell_info {
    double pos;
    double main_axis_size;
    element_ptr elem_ptr;
    int layout_id = -1;
  };

  // virtual methods to specialize in hdynamic or vdynamic
  virtual view_limits
  make_limits(float main_axis_size,
              cell_composer::limits secondary_axis_limits) const;
  virtual float get_main_axis_start(const rect &r);
  virtual float get_main_axis_end(const rect &r);
  virtual void make_bounds(context &ctx, float main_axis_start,
                           cell_info &info);

  using cells_vector = std::vector<cell_info>;
  mutable cells_vector _cells;

private:
  composer_ptr _composer;
  point _previous_size;
  std::size_t _previous_window_start = 0;
  std::size_t _previous_window_end = 0;

  mutable double _main_axis_full_size = 0;
  mutable int _layout_id = 0;
  mutable bool _update_request = true;

  int _focus = -1;
  int _saved_focus = -1;
  int _click_tracking = -1;
  int _cursor_tracking = -1;
  std::set<int> _cursor_hovering;
};

} // namespace elements
} // namespace cycfi

#endif

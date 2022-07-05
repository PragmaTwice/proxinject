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

#ifndef PROXINJECT_INJECTOR_TEXT_BOX
#define PROXINJECT_INJECTOR_TEXT_BOX

#include <elements.hpp>

namespace cycfi::elements {

struct selectable_text_box : static_text_box {
public:
  selectable_text_box(std::string text, font font_ = get_theme().text_box_font,
                      float size = get_theme().text_box_font_size);
  ~selectable_text_box();
  selectable_text_box(selectable_text_box &&rhs) = default;

  void draw(context const &ctx) override;
  bool click(context const &ctx, mouse_button btn) override;
  void drag(context const &ctx, mouse_button btn) override;
  bool cursor(context const &ctx, point p, cursor_tracking status) override;
  bool key(context const &ctx, key_info k) override;
  bool wants_focus() const override;
  void begin_focus() override;
  void end_focus() override;
  bool wants_control() const override;
  void layout(context const &ctx) override;
  view_limits limits(basic_context const &ctx) const override;

  bool text(context const &ctx, text_info info) override;
  void set_text(string_view text) override;

  using element::focus;
  using static_text_box::get_text;

  int select_start() const { return _select_start; }
  void select_start(int pos);
  int select_end() const { return _select_end; }
  void select_end(int pos);
  void select_all();
  void select_none();

  virtual void draw_selection(context const &ctx);
  virtual void draw_caret(context const &ctx);
  virtual bool word_break(char const *utf8) const;
  virtual bool line_break(char const *utf8) const;

protected:
  void scroll_into_view(context const &ctx, bool save_x);
  virtual void delete_(bool forward);
  virtual void cut(view &v, int start, int end);
  virtual void copy(view &v, int start, int end);
  virtual void paste(view &v, int start, int end);

private:
  struct glyph_metrics {
    char const *str;   // The start of the utf8 string
    point pos;         // Position where glyph is drawn
    rect bounds;       // Glyph bounds
    float line_height; // Line height
  };

  char const *caret_position(context const &ctx, point p);
  glyph_metrics glyph_info(context const &ctx, char const *s);

  struct state_saver;
  using state_saver_f = std::function<void()>;

  state_saver_f capture_state();

  int _select_start;
  int _select_end;
  float _current_x;
  state_saver_f _typing_state;
  bool _is_focus : 1;
  bool _show_caret : 1;
  bool _caret_started : 1;

  std::unique_ptr<std::mutex> _draw_mutex;
};

} // namespace cycfi::elements

#endif

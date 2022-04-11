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

#include "text_box.hpp"

namespace cycfi::elements {

using namespace std::chrono_literals;

selectable_text_box::selectable_text_box(std::string text, font font_,
                                         float size)
    : static_text_box(std::move(text), font_, size), _select_start(-1),
      _select_end(-1), _current_x(0), _is_focus(false), _show_caret(true),
      _caret_started(false) {}

selectable_text_box::~selectable_text_box() {}

void selectable_text_box::draw(context const &ctx) {
  draw_selection(ctx);
  static_text_box::draw(ctx);
  draw_caret(ctx);
}

bool selectable_text_box::click(context const &ctx, mouse_button btn) {
  if (btn.state != mouse_button::left)
    return false;

  _show_caret = true;

  if (!btn.down) // released? return early
    return true;

  if (_text.empty()) {
    _select_start = _select_end = 0;
    scroll_into_view(ctx, false);
    return true;
  }

  char const *_first = _text.data();
  char const *_last = _first + _text.size();

  if (char const *pos = caret_position(ctx, btn.pos)) {
    if (btn.num_clicks != 1) {
      char const *end = pos;
      char const *start = pos;

      auto fixup = [&]() {
        if (start != _first)
          start = next_utf8(_last, start);
        _select_start = int(start - _first);
        _select_end = int(end - _first);
      };

      if (btn.num_clicks == 2) {
        while (end < _last && !word_break(end))
          end = next_utf8(_last, end);
        while (start > _first && !word_break(start))
          start = prev_utf8(_first, start);
        fixup();
      } else if (btn.num_clicks == 3) {
        while (end < _last && !is_newline(uint8_t(*end)))
          end++;
        while (start > _first && !is_newline(uint8_t(*start)))
          start--;
        fixup();
      }
    } else {
      auto hit = int(pos - _first);
      if ((btn.modifiers == mod_shift) && (_select_start != -1)) {
        if (hit < _select_start)
          _select_start = hit;
        else
          _select_end = hit;
      } else {
        _select_end = _select_start = hit;
      }
    }
    _current_x = btn.pos.x - ctx.bounds.left;
    ctx.view.refresh(ctx);
  }
  return true;
}

void selectable_text_box::drag(context const &ctx, mouse_button btn) {
  char const *first = &_text[0];
  if (char const *pos = caret_position(ctx, btn.pos)) {
    _select_end = int(pos - first);
    ctx.view.refresh(ctx);
    scroll_into_view(ctx, true);
  }
}

bool selectable_text_box::cursor(context const &ctx, point p,
                                 cursor_tracking /* status */) {
  if (ctx.bounds.includes(p)) {
    set_cursor(cursor_type::ibeam);
    return true;
  }
  return false;
}

namespace {
void add_undo(context const &ctx, std::function<void()> &typing_state,
              std::function<void()> undo_f, std::function<void()> redo_f) {
  if (typing_state) {
    ctx.view.add_undo({typing_state, undo_f});
    typing_state = {}; // reset
  }
  ctx.view.add_undo({undo_f, redo_f});
}
} // namespace

bool selectable_text_box::text(context const &ctx, text_info info_) {
  _show_caret = true;

  if (_select_start == -1)
    return false;

  if (_select_start > _select_end)
    std::swap(_select_end, _select_start);

  std::string text = codepoint_to_utf8(info_.codepoint);

  if (!_typing_state)
    _typing_state = capture_state();

  // bool replace = false;
  // if (_select_start == _select_end) {
  //  _text.insert(_select_start, text);
  //} else {
  //  _text.replace(_select_start, _select_end - _select_start, text);
  //  replace = true;
  //}

  //_layout.text(_text.data(), _text.data() + _text.size());
  // layout(ctx);

  // if (replace) {
  //  _select_end = _select_start;
  //  scroll_into_view(ctx, true);
  //  _select_end = _select_start += text.length();
  //} else {
  //  _select_end = _select_start += text.length();
  //  scroll_into_view(ctx, true);
  //}
  return true;
}

void selectable_text_box::set_text(string_view text_) {
  static_text_box::set_text(text_);
  _select_start = std::min<int>(_select_start, text_.size());
  _select_end = std::min<int>(_select_end, text_.size());
}

bool selectable_text_box::key(context const &ctx, key_info k) {
  _show_caret = true;

  if (_select_start == -1 || k.action == key_action::release ||
      k.action == key_action::unknown)
    return false;

  bool move_caret = false;
  bool save_x = false;
  bool handled = false;

  int start = std::min(_select_end, _select_start);
  int end = std::max(_select_end, _select_start);
  std::function<void()> undo_f = capture_state();

  auto up_down = [this, &ctx, k, &move_caret]() {
    bool up = k.key == key_code::up;
    glyph_metrics info;
    info = glyph_info(ctx, &_text[_select_end]);
    if (info.str) {
      auto y = up ? -info.line_height : +info.line_height;
      auto pos = point{ctx.bounds.left + _current_x, info.pos.y + y};
      char const *cp = caret_position(ctx, pos);
      if (cp)
        _select_end = int(cp - &_text[0]);
      else
        _select_end = up ? 0 : int(_text.size());
      move_caret = true;
    }
  };

  auto next_char = [this]() {
    if (_select_end < static_cast<int>(_text.size())) {
      char const *end = _text.data() + _text.size();
      char const *p = next_utf8(end, &_text[_select_end]);
      _select_end = int(p - &_text[0]);
    }
  };

  auto prev_char = [this]() {
    if (_select_end > 0) {
      char const *start = &_text[0];
      char const *p = prev_utf8(start, &_text[_select_end]);
      _select_end = int(p - &_text[0]);
    }
  };

  auto next_word = [this]() {
    if (_select_end < static_cast<int>(_text.size())) {
      char const *p = &_text[_select_end];
      char const *end = _text.data() + _text.size();
      while (p != end && word_break(p))
        p = next_utf8(end, p);
      while (p != end && !word_break(p))
        p = next_utf8(end, p);
      _select_end = int(p - &_text[0]);
    }
  };

  auto prev_word = [this]() {
    if (_select_end > 0) {
      char const *start = &_text[0];
      char const *p = prev_utf8(start, &_text[_select_end]);
      while (p != start && word_break(p))
        p = prev_utf8(start, p);
      while (p != start && !word_break(p))
        p = prev_utf8(start, p);
      if (p != start) {
        char const *end = _text.data() + _text.size();
        p = next_utf8(end, p);
      }
      _select_end = int(p - &_text[0]);
    }
  };

  if (k.action == key_action::press || k.action == key_action::repeat) {
    switch (k.key) {
    case key_code::enter: {
      //_text.replace(start, end - start, "\n");
      //_select_start += 1;
      //_select_end = _select_start;
      // save_x = true;
      // add_undo(ctx, _typing_state, undo_f, capture_state());
      // handled = true;
    } break;

    case key_code::backspace:
    case key_code::_delete: {
      delete_(k.key == key_code::_delete);
      save_x = true;
      add_undo(ctx, _typing_state, undo_f, capture_state());
      handled = true;
    } break;

    case key_code::left:
      if (_select_end != -1) {
        if (k.modifiers & mod_alt)
          prev_word();
        else
          prev_char();
        if (!(k.modifiers & mod_shift))
          _select_start = _select_end = std::min(_select_start, _select_end);
      }
      move_caret = true;
      save_x = true;
      handled = true;
      break;

    case key_code::right:
      if (_select_end != -1) {
        if (k.modifiers & mod_alt)
          next_word();
        else
          next_char();
        if (!(k.modifiers & mod_shift))
          _select_start = _select_end = std::max(_select_start, _select_end);
      }
      move_caret = true;
      save_x = true;
      handled = true;
      break;

    case key_code::up:
    case key_code::down:
      if (_select_start != -1)
        up_down();
      handled = true;
      break;

    case key_code::a:
      if (k.modifiers & mod_action) {
        _select_start = 0;
        _select_end = int(_text.size());
        handled = true;
      }
      break;

    case key_code::x:
      if (k.modifiers & mod_action) {
        cut(ctx.view, start, end);
        save_x = true;
        add_undo(ctx, _typing_state, undo_f, capture_state());
        handled = true;
      }
      break;

    case key_code::c:
      if (k.modifiers & mod_action) {
        copy(ctx.view, start, end);
        handled = true;
      }
      break;

    case key_code::v:
      if (k.modifiers & mod_action) {
        paste(ctx.view, start, end);
        save_x = true;
        add_undo(ctx, _typing_state, undo_f, capture_state());
        handled = true;
      }
      break;

    case key_code::z:
      if (k.modifiers & mod_action) {
        if (_typing_state) {
          ctx.view.add_undo({_typing_state, undo_f});
          _typing_state = {}; // reset
        }

        if (k.modifiers & mod_shift)
          ctx.view.redo();
        else
          ctx.view.undo();
        handled = true;
      }
      break;

    default:
      break;
    }
  }

  if (move_caret) {
    clamp(_select_start, 0, int(_text.size()));
    clamp(_select_end, 0, int(_text.size()));
    if (!(k.modifiers & mod_shift))
      _select_start = _select_end;
  } else if (handled) {
    _layout.text(_text.data(), _text.data() + _text.size());
    layout(ctx);
    ctx.view.refresh(ctx);
  }

  if (handled)
    scroll_into_view(ctx, save_x);
  return handled;
}

bool selectable_text_box::wants_control() const { return true; }

void selectable_text_box::draw_caret(context const &ctx) {
  if (_select_start == -1)
    return;

  if (!_is_focus) // No caret if not focused
    return;

  auto &canvas = ctx.canvas;
  auto const &theme = get_theme();
  rect caret_bounds;
  bool has_caret = false;

  // Handle the case where text is empty
  if (_text.empty()) {
    auto size = _layout.metrics();
    auto line_height = size.ascent + size.descent + size.leading;
    auto width = theme.text_box_caret_width;
    auto left = ctx.bounds.left;
    auto top = ctx.bounds.top;

    if (_show_caret) {
      canvas.line_width(width);
      canvas.stroke_style(theme.text_box_caret_color);
      canvas.move_to({left + (width / 2), top});
      canvas.line_to({left + (width / 2), top + line_height});
      canvas.stroke();
    }

    has_caret = true;
    caret_bounds = rect{left, top, left + width, top + line_height};
  }
  // Draw the caret
  else if (_select_start == _select_end) {
    auto start_info = glyph_info(ctx, _text.data() + _select_start);
    auto width = theme.text_box_caret_width;
    rect &caret = start_info.bounds;

    if (_show_caret) {
      canvas.line_width(width);
      canvas.stroke_style(theme.text_box_caret_color);
      canvas.move_to({caret.left + (width / 2), caret.top});
      canvas.line_to({caret.left + (width / 2), caret.bottom});
      canvas.stroke();
    }

    has_caret = true;
    caret_bounds = rect{caret.left - 0.5f, caret.top, caret.left + width + 0.5f,
                        caret.bottom};
  }

  if (has_caret && !_caret_started) {
    auto tl = ctx.canvas.user_to_device(caret_bounds.top_left());
    auto br = ctx.canvas.user_to_device(caret_bounds.bottom_right());
    caret_bounds = {tl.x, tl.y, br.x, br.y};

    _caret_started = true;
    ctx.view.post(500ms, [this, &_view = ctx.view, caret_bounds]() {
      _show_caret = !_show_caret;
      _view.refresh(caret_bounds);
      _caret_started = false;
    });
  }
}

void selectable_text_box::draw_selection(context const &ctx) {
  if (_select_start == -1)
    return;

  auto &canvas = ctx.canvas;
  auto const &theme = get_theme();

  if (!_text.empty()) {
    auto start_info = glyph_info(ctx, _text.data() + _select_start);
    rect &r1 = start_info.bounds;
    r1.right = ctx.bounds.right;

    auto end_info = glyph_info(ctx, _text.data() + _select_end);
    rect &r2 = end_info.bounds;
    r2.right = r2.left;
    r2.left = ctx.bounds.left;

    auto color = theme.text_box_hilite_color;
    if (!_is_focus)
      color = color.opacity(0.15);
    canvas.fill_style(color);
    if (r1.top == r2.top) {
      canvas.fill_rect({r1.left, r1.top, r2.right, r1.bottom});
    } else {
      canvas.begin_path();
      canvas.move_to(r1.top_left());
      canvas.line_to(r1.top_right());
      canvas.line_to({r1.right, r2.top});
      canvas.line_to(r2.top_right());
      canvas.line_to(r2.bottom_right());
      canvas.line_to(r2.bottom_left());
      canvas.line_to({r2.left, r1.bottom});
      canvas.line_to(r1.bottom_left());
      canvas.close_path();
      canvas.fill();
    }
  }
}

char const *selectable_text_box::caret_position(context const &ctx, point p) {
  auto x = ctx.bounds.left;
  auto y = ctx.bounds.top;
  auto metrics = _layout.metrics();
  auto line_height = metrics.ascent + metrics.descent + metrics.leading;

  char const *found = nullptr;
  for (auto &row : _rows) {
    // Check if p is within this row
    if ((p.y >= y) && (p.y < y + line_height)) {
      // Check if we are at the very start of the row or beyond
      if (p.x <= x) {
        found = row.begin();
        break;
      }

      // Get the actual coordinates of the glyph
      row.for_each([p, x, &found](char const *utf8, float left, float right) {
        if ((p.x >= (x + left)) && (p.x < (x + right))) {
          found = utf8;
          return false;
        }
        return true;
      });
      // Assume it's at the end of the row if we haven't found a hit
      if (!found)
        found = row.end();
      break;
    }
    y += line_height;
  }
  return found;
}

selectable_text_box::glyph_metrics
selectable_text_box::glyph_info(context const &ctx, char const *s) {
  auto metrics = _layout.metrics();
  auto x = ctx.bounds.left;
  auto y = ctx.bounds.top + metrics.ascent;
  auto descent = metrics.descent;
  auto ascent = metrics.ascent;
  auto leading = metrics.leading;
  auto line_height = ascent + descent + leading;

  glyph_metrics info;
  info.str = nullptr;
  info.line_height = line_height;

  // Check if s is at the very end
  if (s == _text.data() + _text.size()) {
    auto const &last_row = _rows.back();
    auto rightmost = x + last_row.width();
    auto bottom_y = y + (line_height * (_rows.size() - 1));

    info.pos = {rightmost, bottom_y};
    info.bounds = {rightmost, bottom_y - ascent, rightmost + 10,
                   bottom_y + descent};
    info.str = s;
    return info;
  }

  glyphs *prev_row = nullptr;
  for (auto &row : _rows) {
    // Check if s is within this row
    if (s >= row.begin() && s < row.end()) {
      // Get the actual coordinates of the glyph
      row.for_each([s, &info, x, y, ascent, descent](char const *utf8,
                                                     float left, float right) {
        if (utf8 >= s) {
          info.pos = {x + left, y};
          info.bounds = {x + left, y - ascent, x + right, y + descent};
          info.str = utf8;
          return false;
        }
        return true;
      });
      break;
    }
    // This handles the case where s is in between the start of the
    // current row and the end of the previous.
    else if (s < row.begin() && prev_row) {
      auto rightmost = x + prev_row->width();
      auto prev_y = y - line_height;
      info.pos = {rightmost, prev_y};
      info.bounds = {rightmost, prev_y - ascent, rightmost + 10,
                     prev_y + descent};
      info.str = s;
      break;
    }
    y += line_height;
    prev_row = &row;
  }

  return info;
}

void selectable_text_box::delete_(bool forward) {
  // auto start = std::min(_select_end, _select_start);
  // auto end = std::max(_select_end, _select_start);
  // if (start != -1) {
  //  if (start == end) {
  //    if (forward) {
  //      char const *start_p = &_text[start];
  //      char const *end_p = &_text[0] + _text.size();
  //      char const *p = next_utf8(end_p, start_p);
  //      start = int(start_p - &_text[0]);
  //      _text.erase(start, p - start_p);
  //    } else if (start > 0) {
  //      char const *start_p = &_text[0];
  //      char const *end_p = &_text[start];
  //      char const *p = prev_utf8(start_p, end_p);
  //      start = int(p - &_text[0]);
  //      _text.erase(start, end_p - p);
  //    }
  //  } else {
  //    _text.erase(start, end - start);
  //  }
  //  _select_end = _select_start = start;
  //}
}

void selectable_text_box::cut(view & /* v */, int start, int end) {
  if (start != -1 && start != end) {
    auto end_ = std::max(start, end);
    auto start_ = std::min(start, end);
    clipboard(_text.substr(start, end_ - start_));
    delete_(false);
  }
}

void selectable_text_box::copy(view & /* v */, int start, int end) {
  if (start != -1 && start != end) {
    auto end_ = std::max(start, end);
    auto start_ = std::min(start, end);
    clipboard(_text.substr(start, end_ - start_));
  }
}

void selectable_text_box::paste(view & /* v */, int start, int end) {
  // if (start != -1) {
  //  auto end_ = std::max(start, end);
  //  auto start_ = std::min(start, end);
  //  std::string ins = clipboard();
  //  _text.replace(start, end_ - start_, ins);
  //  start += ins.size();
  //  _select_end = _select_start = start;
  //}
}

struct selectable_text_box::state_saver {
  state_saver(selectable_text_box *this_)
      : text(this_->_text), select_start(this_->_select_start),
        select_end(this_->_select_end), save_text(this_->_text),
        save_select_start(this_->_select_start),
        save_select_end(this_->_select_end) {}

  void operator()() {
    text = save_text;
    select_start = save_select_start;
    select_end = save_select_end;
  }

  std::string &text;
  int &select_start;
  int &select_end;

  std::string save_text;
  int save_select_start;
  int save_select_end;
};

std::function<void()> selectable_text_box::capture_state() {
  return state_saver(this);
}

void selectable_text_box::scroll_into_view(context const &ctx, bool save_x) {
  if (_text.empty()) {
    auto caret = rect{ctx.bounds.left - 1, ctx.bounds.top, ctx.bounds.left + 1,
                      ctx.bounds.bottom};
    scrollable::find(ctx).scroll_into_view(caret);
    ctx.view.refresh(ctx);
    if (save_x)
      _current_x = 0;
    return;
  }

  if (_select_end == -1)
    return;

  auto info = glyph_info(ctx, &_text[_select_end]);
  if (info.str) {
    auto caret = rect{info.bounds.left - 1, info.bounds.top,
                      info.bounds.left + 1, info.bounds.bottom};
    if (!scrollable::find(ctx).scroll_into_view(caret))
      ctx.view.refresh(ctx);

    if (save_x)
      _current_x = info.pos.x - ctx.bounds.left;
  }
}

bool selectable_text_box::wants_focus() const { return true; }

void selectable_text_box::begin_focus() {
  _is_focus = true;
  if (_select_start == -1)
    _select_start = _select_end = 0;
}

void selectable_text_box::end_focus() { _is_focus = false; }

void selectable_text_box::select_start(int pos) {
  if (pos == -1 || (pos >= 0 && pos <= static_cast<int>(_text.size())))
    _select_start = pos;
}

void selectable_text_box::select_end(int pos) {
  if (pos == -1 || (pos >= 0 && pos <= static_cast<int>(_text.size())))
    _select_end = pos;
}

void selectable_text_box::select_all() {
  _select_start = 0;
  _select_end = int(_text.size());
}

void selectable_text_box::select_none() { _select_start = _select_end = -1; }

bool selectable_text_box::word_break(char const *utf8) const {
  auto cp = codepoint(utf8);
  return is_space(cp) || is_punctuation(cp);
}

bool selectable_text_box::line_break(char const *utf8) const {
  auto cp = codepoint(utf8);
  return is_newline(cp);
}

} // namespace cycfi::elements

#include "text_window.hpp"

void TextWindow::init(lv_obj_t *parent, size_t width, size_t height) {
  Window::init(parent, width, height);
  log_container_ = lv_label_create(parent_);
  lv_label_set_recolor(log_container_, true);

  // wrap text on long lines
  lv_label_set_long_mode(log_container_, LV_LABEL_LONG_WRAP);

  // log should span the width of the screen
  lv_obj_set_width(log_container_, lv_pct(100));
  lv_obj_set_height(log_container_, lv_pct(100));
}

void TextWindow::clear_logs(void) {
  // set the string to the display
  lv_label_set_text(log_container_, "");
  // now empty the string
  log_text_.clear();
  // invalidate
  invalidate();
}

void TextWindow::add_log(const std::string &log_text) {
  // now add to our string for storage
  log_text_ += "\n" + log_text;
  // set the string to the display
  lv_label_set_text(log_container_, log_text_.c_str());
  // make sure the most recent logs are shown
  lv_obj_scroll_to_y(log_container_, LV_COORD_MAX, LV_ANIM_OFF);
}

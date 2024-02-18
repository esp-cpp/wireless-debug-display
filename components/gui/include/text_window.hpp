#pragma once

#include <deque>
#include <string>

#include "window.hpp"

class TextWindow : public Window {
public:
  TextWindow() = default;

  void init(lv_obj_t *parent, size_t width, size_t height) override;

  void clear_logs(void);
  void add_log(const std::string &log_text);

private:
  std::string log_text_{""};
  lv_obj_t *log_container_{nullptr};
};

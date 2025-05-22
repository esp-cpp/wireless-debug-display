#pragma once

#include <lvgl.h>

class Window {
public:
  Window() = default;

  virtual void init(lv_obj_t *parent, size_t width, size_t height) {
    parent_ = parent;
    width_ = width;
    height_ = height;
  }
  virtual void clear(void) {}
  virtual void update(void) {}

protected:
  size_t width_{0};
  size_t height_{0};
  lv_obj_t *parent_{nullptr};
};

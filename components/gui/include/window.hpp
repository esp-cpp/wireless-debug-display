#pragma once

#include "lvgl.h"

class Window {
public:
  virtual void init   ( lv_obj_t *parent, size_t width, size_t height ) {
    parent_ = parent;
    width_ = width;
    height_ = height;
  }
  virtual void clear  ( void ) { }
  virtual void update ( void ) { }

protected:
  size_t width_;
  size_t height_;
  lv_obj_t* parent_;
};

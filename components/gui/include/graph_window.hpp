#pragma once

#include <cstring>
#include <string>
#include <unordered_map>

#include "window.hpp"

class GraphWindow : public Window {
public:
  GraphWindow() = default;

  void init(lv_obj_t *parent, size_t width, size_t height) override;
  void update() override;
  void set_max_point_count(size_t max_point_count);

  void clear_plots(void);
  void add_data(const std::string &plot_name, int new_data);
  void remove_plot(const std::string &plot_name);

  lv_obj_t *get_lv_obj(void) { return wrapper_; }

  void invalidate() {
    if (wrapper_) {
      lv_obj_invalidate(wrapper_);
    }
  }

protected:
  lv_chart_series_t *create_plot(const std::string &plotName);
  lv_chart_series_t *get_plot(const std::string &plotName);

  void update_ticks(void);

private:
  lv_obj_t *wrapper_{nullptr};
  lv_obj_t *y_scale_{nullptr};
  lv_obj_t *chart_{nullptr};
  lv_obj_t *legend_{nullptr};
  std::unordered_map<std::string, lv_chart_series_t *> plot_map_{};
};

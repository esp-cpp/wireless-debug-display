#pragma once

#include <string>
#include <cstring>
#include <unordered_map>

#include "window.hpp"

class GraphWindow : public Window {
public:
  void init(lv_obj_t *parent, size_t width, size_t height) override;
  void update() override;

  void clear_plots   ( void );
  void add_data      ( const std::string& plot_name, int new_data );
  void remove_plot   ( const std::string& plot_name );

protected:
  lv_chart_series_t*  create_plot  ( const std::string& plotName );
  lv_chart_series_t*  get_plot     ( const std::string& plotName );

  void update_ticks ( void );

private:
  lv_obj_t *chart_;
  lv_obj_t *legend_;
  std::string y_ticks_;
  std::unordered_map<std::string, lv_chart_series_t*> plot_map_;
};

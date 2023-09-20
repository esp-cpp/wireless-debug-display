#include "graph_window.hpp"

void GraphWindow::init(lv_obj_t *parent, size_t width, size_t height) {
  Window::init(parent, width, height);
  // Create a chart
  chart_ = lv_chart_create(parent_);

  // we need to make the width of the chart less than the parent full width to
  // leave room for tick labels
  lv_obj_set_width(chart_, lv_pct(90));
  lv_obj_set_height(chart_, lv_pct(100));

  // the y axis labels are on the left of the chart, so align the chart on the
  // right side of the parent to leave room for tick labels
  lv_obj_align(chart_, LV_ALIGN_RIGHT_MID, 0, 0);

  // Show lines and points too
  lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);
  
  // update the tick values
  size_t major_tick_length = 5;
  size_t minor_tick_length = 2;
  size_t major_tick_count = 5;
  size_t minor_tick_count = 2;
  bool label_enabled = true;
  size_t draw_size = 50;
  lv_chart_set_axis_tick(chart_, LV_CHART_AXIS_PRIMARY_Y,
                         major_tick_length,
                         minor_tick_length,
                         major_tick_count,
                         minor_tick_count,
                         label_enabled,
                         draw_size);

  // create the legend
  legend_ = lv_label_create(chart_);
  lv_obj_align(legend_, LV_ALIGN_TOP_RIGHT, - 7, 5);
  lv_label_set_text(legend_, "");

  // create a style for the chart
  // give some padding to the chart - especially on the left where we
  // have the y axis labels / ticks.
  lv_obj_set_style_border_width(chart_, 0, 0);
  lv_obj_set_style_pad_left(chart_, 60, 0);
  lv_obj_set_style_pad_bottom(chart_, 36, 0);
  lv_obj_set_style_pad_right(chart_, 25, 0);
  lv_obj_set_style_pad_top(chart_, 20, 0);
}

void GraphWindow::update_ticks() {

  // get the minimum and maximum
  lv_coord_t min=0, max=0;
  auto num_points = lv_chart_get_point_count(chart_);
  for (auto e : plot_map_) {
    auto series = e.second;
    for (size_t i=0; i < num_points; i++) {
      auto point = series->y_points[i];
      if (point < min) min = point;
      if (point > max) max = point;
    }
  }

  // update the chart range
  lv_chart_set_range(chart_, LV_CHART_AXIS_PRIMARY_Y, min, max);
}

void GraphWindow::update() {
  // make sure if we have new data we update the range & tick values
  update_ticks();
  // update the chart if we use our own data arrays or modify the data
  // ourselves
  lv_chart_refresh(chart_);
}

void GraphWindow::clear_plots( void ) {
  // remove all the series from the chart
  for (auto e : plot_map_) {
    lv_chart_remove_series(chart_, e.second);
  }
  // now clear the map
  plot_map_.clear();
  // clear the legend
  lv_label_set_text(legend_, "");
}

void GraphWindow::add_data( const std::string& plotName, int newData ) {
  auto plot = get_plot( plotName );
  // couldn't find the plot so create a new one
  if ( plot == nullptr )
    plot = create_plot( plotName );
  // now add the data
  lv_chart_set_next_value(chart_, plot, newData);
}

lv_chart_series_t* GraphWindow::create_plot( const std::string& plotName ) {
  // make a random color
  uint8_t red = rand() % 256;
  uint8_t green = rand() % 256;
  uint8_t blue = rand() % 256;
  auto color = lv_color_make(red, green, blue);
  // now make the plot
  auto plot = lv_chart_add_series(chart_, color, LV_CHART_AXIS_PRIMARY_Y);

  //Add text to legend, #XXX <string># will be a colored string, if we set the
  //recolor property to true on the label
  char label_buf[50];
  sprintf(label_buf, "#%02X%02X%02X - %s#\n", red, green, blue, plotName.c_str());
  char *current_legend = lv_label_get_text(legend_);
  auto new_text = std::string(current_legend);
  new_text += label_buf;
  lv_label_set_text(legend_, new_text.c_str());
  lv_label_set_recolor(legend_, true);

  // add it to the map
  plot_map_[plotName] = plot;
  // and return it
  return plot;
}

void GraphWindow::remove_plot( const std::string& plotName ) {
  auto plot = get_plot(plotName);
  if (plot != nullptr) {
    // we should remove it from the display
    lv_chart_remove_series(chart_, plot);
    // and delete the value from the map
    plot_map_.erase(plotName);
  }
}

lv_chart_series_t* GraphWindow::get_plot( const std::string& plotName ) {
  lv_chart_series_t* plot = nullptr;
  auto search = plot_map_.find(plotName);
  if (search != plot_map_.end()) {
    // set the plot to be the value of the element
    plot = search->second;
  }
  return plot;
}

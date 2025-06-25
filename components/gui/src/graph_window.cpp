#include "graph_window.hpp"

#include "format.hpp"

#include <widgets/chart/lv_chart_private.h>

void GraphWindow::init(lv_obj_t *parent, size_t width, size_t height) {
  Window::init(parent, width, height);
  // create a transparent wrapper for the chart and the scale.
  wrapper_ = lv_obj_create(parent_);
  lv_obj_remove_style_all(wrapper_);
  lv_obj_set_size(wrapper_, lv_pct(100), lv_pct(100));

  // Create a chart
  chart_ = lv_chart_create(wrapper_);
  lv_chart_set_update_mode(chart_, LV_CHART_UPDATE_MODE_SHIFT);

  // we need to make the width of the chart less than the parent full width to
  // leave room for tick labels
  lv_obj_set_width(chart_, lv_pct(90));
  lv_obj_set_height(chart_, lv_pct(95));

  // the y axis labels are on the left of the chart, so align the chart on the
  // right side of the parent to leave room for tick labels
  lv_obj_align(chart_, LV_ALIGN_RIGHT_MID, 0, 0);

  // Show lines and points too
  lv_chart_set_type(chart_, LV_CHART_TYPE_LINE);

  // update the tick values
  size_t major_tick_length = 6;
  size_t minor_tick_length = 3;
  size_t major_tick_count = 5;
  size_t minor_tick_count = 2;
  size_t total_tick_count = major_tick_count * minor_tick_count + 1;
  bool label_enabled = true;

  // create the scale for the y-axis
  y_scale_ = lv_scale_create(wrapper_);
  lv_obj_set_size(y_scale_, lv_pct(10), lv_pct(95));
  lv_obj_align(y_scale_, LV_ALIGN_LEFT_MID, 0, 0);
  lv_scale_set_mode(y_scale_, LV_SCALE_MODE_VERTICAL_LEFT);
  lv_scale_set_label_show(y_scale_, label_enabled);
  lv_scale_set_total_tick_count(y_scale_, total_tick_count);
  lv_scale_set_major_tick_every(y_scale_, major_tick_count);
  lv_obj_set_style_length(y_scale_, minor_tick_length, LV_PART_ITEMS);
  lv_obj_set_style_length(y_scale_, major_tick_length, LV_PART_INDICATOR);

  // create the legend
  legend_ = lv_spangroup_create(chart_);
  lv_obj_set_width(legend_, lv_pct(100));
  lv_obj_set_height(legend_, LV_SIZE_CONTENT);
  lv_obj_align(legend_, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_spangroup_set_align(legend_, LV_TEXT_ALIGN_RIGHT);
  lv_spangroup_set_mode(legend_, LV_SPAN_MODE_BREAK);
  lv_spangroup_set_max_lines(legend_, -1); // no limit
  lv_spangroup_set_indent(legend_, 0);

  // create a style for the chart
  // give some padding to the chart - especially on the left where we
  // have the y axis labels / ticks.
  lv_chart_set_div_line_count(chart_, 5, 7);
  lv_obj_set_style_border_width(chart_, 0, 0);
  lv_obj_set_style_pad_all(chart_, 0, 0);
}

void GraphWindow::set_max_point_count(size_t max_point_count) {
  // set the maximum number of points to be displayed on the chart
  lv_chart_set_point_count(chart_, max_point_count);
}

void GraphWindow::update_ticks() {
  if (plot_map_.empty()) {
    // if we have no plots, then we don't need to do anything
    return;
  }
  // get the minimum and maximum
  std::vector<lv_coord_t> y_points;

  auto num_points = lv_chart_get_point_count(chart_);
  for (const auto &e : plot_map_) {
    auto series = e.second;
    for (size_t i = 0; i < num_points; i++) {
      auto point = series->y_points[i];
      if (point == LV_CHART_POINT_NONE) {
        // skip this point
        continue;
      }
      y_points.push_back(series->y_points[i]);
    }
  }

  // get the min/max values
  auto min = *std::min_element(y_points.begin(), y_points.end());
  auto max = *std::max_element(y_points.begin(), y_points.end());

  // update the chart range
  lv_chart_set_range(chart_, LV_CHART_AXIS_PRIMARY_Y, min, max);
  lv_scale_set_range(y_scale_, min, max);
}

void GraphWindow::update() {
  // make sure if we have new data we update the range & tick values
  update_ticks();
  // update the chart if we use our own data arrays or modify the data
  // ourselves
  lv_chart_refresh(chart_);
}

void GraphWindow::clear_plots(void) {
  // remove all the series from the chart
  for (auto e : plot_map_) {
    lv_chart_remove_series(chart_, e.second);
  }
  // now clear the map
  plot_map_.clear();
  // clear the legend
  while (lv_spangroup_get_child(legend_, 0)) {
    lv_spangroup_delete_span(legend_, lv_spangroup_get_child(legend_, 0));
  }
  // invalidate
  invalidate();
}

void GraphWindow::add_data(const std::string &plotName, int newData) {
  auto plot = get_plot(plotName);
  // couldn't find the plot so create a new one
  if (plot == nullptr)
    plot = create_plot(plotName);
  // now add the data
  lv_chart_set_next_value(chart_, plot, newData);
}

lv_chart_series_t *GraphWindow::create_plot(const std::string &plotName) {
  // make a random color
  uint8_t red = rand() % 256;
  uint8_t green = rand() % 256;
  uint8_t blue = rand() % 256;
  auto color = lv_color_make(red, green, blue);
  // now make the plot
  auto plot = lv_chart_add_series(chart_, color, LV_CHART_AXIS_PRIMARY_Y);

  // create a new span for the new legend text
  auto span = lv_spangroup_new_span(legend_);
  // set the text to be the color of the plot
  std::string legend_text = fmt::format("{} -\n", plotName);
  lv_span_set_text(span, legend_text.c_str());
  lv_style_set_text_color(lv_span_get_style(span), color);

  lv_spangroup_refr_mode(legend_);

  // add it to the map
  plot_map_[plotName] = plot;
  // and return it
  return plot;
}

void GraphWindow::remove_plot(const std::string &plotName) {
  auto plot = get_plot(plotName);
  if (plot != nullptr) {
    // we should remove it from the display
    lv_chart_remove_series(chart_, plot);
    // and delete the value from the map
    plot_map_.erase(plotName);
  }
}

lv_chart_series_t *GraphWindow::get_plot(const std::string &plotName) {
  lv_chart_series_t *plot = nullptr;
  auto search = plot_map_.find(plotName);
  if (search != plot_map_.end()) {
    // set the plot to be the value of the element
    plot = search->second;
  }
  return plot;
}

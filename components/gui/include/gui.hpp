#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <sstream>

#include <sdkconfig.h>

#include "converter.hpp"
#include "display.hpp"
#include "graph_window.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "text_window.hpp"

class Gui {
public:
  using Pixel = lv_color16_t;
  using Display = espp::Display<Pixel>;

  const std::string delimeter_data = "::"; ///< Delimeter indicating this contains plottable data
  const std::string delimeter_command = "+++";   ///< Delimeter indicating this contains a command
  const std::string command_remove_plot = "RP:"; ///< Command: remove plot
  const std::string command_clear_plots = "CP";  ///< Command: clear plots
  const std::string command_clear_logs = "CL";   ///< Command: clear logs

  struct Config {
    std::shared_ptr<Display> display; ///< Display to use
    size_t max_chart_point_count{30}; ///< Max number of points to show on the chart
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN}; ///< Log level
  };

  explicit Gui(const Config &config)
      : display_(config.display)
      , logger_({.tag = "Gui", .level = config.log_level}) {
    init_ui();
    plot_window_.set_max_point_count(config.max_chart_point_count);
    plot_window_.clear_plots();
    // now start the gui updater task
    using namespace std::placeholders;
    task_ = espp::Task::make_unique(espp::Task::Config{
        .callback = [this](auto &m, auto &cv) -> bool { return this->update(m, cv); },
        .task_config = {.name = "Gui Task", .stack_size_bytes = 6 * 1024}});
    task_->start();
  }

  ~Gui() {
    task_->stop();
    deinit_ui();
  }

  void switch_tab();

  void push_data(const std::string &data);
  std::string pop_data();

  void clear_info();
  void add_info(const std::string &info);

  bool handle_data();

  void set_chart_max_point_count(size_t count) { plot_window_.set_max_point_count(count); }

protected:
  void init_ui();
  void deinit_ui();

  bool update(std::mutex &m, std::condition_variable &cv) {
    {
      std::lock_guard<std::recursive_mutex> lk(mutex_);
      lv_task_handler();
    }
    {
      using namespace std::chrono_literals;
      std::unique_lock<std::mutex> lk(m);
      cv.wait_for(lk, 16ms);
    }
    // don't want to stop the task
    return false;
  }

  static void event_callback(lv_event_t *e) {
    lv_event_code_t event_code = lv_event_get_code(e);
    auto user_data = lv_event_get_user_data(e);
    auto gui = static_cast<Gui *>(user_data);
    if (!gui) {
      return;
    }
    switch (event_code) {
    case LV_EVENT_SHORT_CLICKED:
      break;
    case LV_EVENT_PRESSED:
      gui->on_pressed(e);
      break;
    case LV_EVENT_VALUE_CHANGED:
      // gui->on_value_changed(e);
      break;
    case LV_EVENT_LONG_PRESSED:
      break;
    case LV_EVENT_KEY:
      break;
    default:
      break;
    }
  }

  void on_pressed(lv_event_t *e);

  GraphWindow plot_window_;
  TextWindow log_window_;
  TextWindow info_window_;
  lv_obj_t *tabview_;

  std::mutex data_queue_mutex_;
  std::queue<std::string> data_queue_;

  std::shared_ptr<Display> display_;
  std::unique_ptr<espp::Task> task_;

  espp::Logger logger_;
  std::recursive_mutex mutex_;
};

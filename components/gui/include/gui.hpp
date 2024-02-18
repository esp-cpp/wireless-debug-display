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
  const std::string delimeter_data = "::"; ///< Delimeter indicating this contains plottable data
  const std::string delimeter_command = "+++";   ///< Delimeter indicating this contains a command
  const std::string command_remove_plot = "RP:"; ///< Command: remove plot
  const std::string command_clear_plots = "CP";  ///< Command: clear plots
  const std::string command_clear_logs = "CL";   ///< Command: clear logs

  struct Config {
    std::shared_ptr<espp::Display> display;
    espp::Logger::Verbosity log_level{espp::Logger::Verbosity::WARN};
  };

  explicit Gui(const Config &config)
      : display_(config.display)
      , logger_({.tag = "Gui", .level = config.log_level}) {
    init_ui();
    // now start the gui updater task
    using namespace std::placeholders;
    task_ = espp::Task::make_unique({.name = "Gui Task",
                                     .callback = std::bind(&Gui::update, this, _1, _2),
                                     .stack_size_bytes = 6 * 1024});
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

  std::shared_ptr<espp::Display> display_;
  std::unique_ptr<espp::Task> task_;

  espp::Logger logger_;
  std::recursive_mutex mutex_;
};

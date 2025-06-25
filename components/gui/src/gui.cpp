#include "gui.hpp"

using namespace espp;
using namespace std::chrono_literals;

void Gui::deinit_ui() { lv_obj_del(tabview_); }

void Gui::init_ui() {
  // Initialize the GUI
  const auto tab_location = LV_DIR_TOP;
  const size_t tab_height = 20;
  tabview_ = lv_tabview_create(lv_scr_act());
  lv_tabview_set_tab_bar_position(tabview_, tab_location);
  lv_tabview_set_tab_bar_size(tabview_, tab_height);

  // create the plotting tab and hide the scrollbars
  auto plot_tab = lv_tabview_add_tab(tabview_, "Plots");
  // lv_obj_set_scrollbar_mode(plot_tab, LV_SCROLLBAR_MODE_OFF);
  plot_window_.init(plot_tab, display_->width(), display_->height());

  // create the logging tab and hide the scrollbars
  auto log_tab = lv_tabview_add_tab(tabview_, "Logs");
  // lv_obj_set_scrollbar_mode(log_tab, LV_SCROLLBAR_MODE_OFF);
  log_window_.init(log_tab, display_->width(), display_->height());

  // create the info tab and hide the scrollbars
  auto info_tab = lv_tabview_add_tab(tabview_, "Info");
  // lv_obj_set_scrollbar_mode(info_tab, LV_SCROLLBAR_MODE_OFF);
  info_window_.init(info_tab, display_->width(), display_->height());

  // rom screen navigation
  // lv_obj_add_event_cb(ui_settingsbutton, &Gui::event_callback, LV_EVENT_PRESSED,
  // static_cast<void*>(this)); lv_obj_add_event_cb(ui_playbutton, &Gui::event_callback,
  // LV_EVENT_PRESSED, static_cast<void*>(this));
}

void Gui::switch_tab() {
  std::lock_guard<std::recursive_mutex> lk{mutex_};
  auto num_tabs = lv_tabview_get_tab_count(tabview_);
  auto active_tab = lv_tabview_get_tab_act(tabview_);
  auto next_tab = (active_tab + 1) % num_tabs;
  lv_tabview_set_act(tabview_, next_tab, LV_ANIM_ON);
}

void Gui::push_data(const std::string &data) {
  std::unique_lock<std::mutex> lock{data_queue_mutex_};
  data_queue_.push(data);
}

std::string Gui::pop_data() {
  std::unique_lock<std::mutex> lock{data_queue_mutex_};
  std::string data{""};
  if (!data_queue_.empty()) {
    data = data_queue_.front();
    data_queue_.pop();
  }
  return data;
}

void Gui::clear_info() {
  std::lock_guard<std::recursive_mutex> lk{mutex_};
  info_window_.clear_logs();
}

void Gui::clear_plots() {
  std::lock_guard<std::recursive_mutex> lk{mutex_};
  plot_window_.clear_plots();
}

void Gui::clear_logs() {
  std::lock_guard<std::recursive_mutex> lk{mutex_};
  log_window_.clear_logs();
}

void Gui::add_info(const std::string &info) {
  std::lock_guard<std::recursive_mutex> lk{mutex_};
  info_window_.add_log(info);
}

bool Gui::handle_data() {
  // lock the display
  std::lock_guard<std::recursive_mutex> lk{mutex_};

  bool hasNewPlotData = false;
  bool hasNewTextData = false;

  std::string newData = pop_data();
  int len = newData.length();
  if (len > 0) {
    size_t num_lines = 0;
    // logger_.info("parsing input '{}'", newData);
    // have data, parse here
    std::stringstream ss(newData);
    std::string line;
    while (std::getline(ss, line, '\n')) {
      num_lines++;
      size_t pos = 0;
      // parse for commands
      if ((pos = line.find(delimeter_command)) != std::string::npos) {
        std::string command;
        command = line.substr(pos + delimeter_command.length(), line.length());
        if (command == command_clear_logs) {
          log_window_.clear_logs();
          // make sure we transition to the next state
          hasNewTextData = true;
        } else if (command == command_clear_plots) {
          plot_window_.clear_plots();
          // make sure we transition to the next state
          hasNewPlotData = true;
        } else if ((pos = line.find(command_remove_plot)) != std::string::npos) {
          std::string plotName = line.substr(pos + command_remove_plot.length(), line.length());
          plot_window_.remove_plot(plotName);
          // make sure we transition to the next state
          hasNewPlotData = true;
        }
      } else {
        // parse for data
        if ((pos = line.find(delimeter_data)) != std::string::npos) {
          // found "::" so we have a plot data
          std::string plotName;
          plotName = line.substr(0, pos);
          pos = pos + delimeter_data.length();
          if (pos < line.length()) {
            int iValue;
            auto value = line.substr(pos, line.length());
            if (Converter::str2int(iValue, value.c_str()) == Converter::Status::Success) {
              // make sure we transition to the next state
              plot_window_.add_data(plotName, iValue);
              hasNewPlotData = true;
            } else {
              logger_.warn("has '::', but could not convert to number, adding log '{}'", line);
              // couldn't find that, so we just have text data
              log_window_.add_log(line);
              // make sure we transition to the next state
              hasNewTextData = true;
            }
          } else {
            // couldn't find that, so we just have text data
            log_window_.add_log(line);
            // make sure we transition to the next state
            hasNewTextData = true;
          }
        } else {
          // couldn't find that, so we just have text data
          log_window_.add_log(line);
          // make sure we transition to the next state
          hasNewTextData = true;
        }
      }
    }
    // logger_.info("parsed {} lines", num_lines);
  }
  if (hasNewPlotData) {
    plot_window_.update();
  }
  return hasNewPlotData || hasNewTextData;
}

void Gui::on_pressed(lv_event_t *e) {
  lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
  logger_.info("PRESSED: {}", fmt::ptr(target));
}

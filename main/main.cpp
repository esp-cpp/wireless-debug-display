#include <sdkconfig.h>

#include <chrono>
#include <thread>

#include <mdns.h>

#if CONFIG_HARDWARE_WROVER_KIT
#include "wrover-kit.hpp"
#define HAS_TOUCH 0
using Hal = espp::WroverKit;
#elif CONFIG_HARDWARE_BOX
#include "esp-box.hpp"
#define HAS_TOUCH 1
using Hal = espp::EspBox;
#elif CONFIG_HARDWARE_TDECK
#include "t-deck.hpp"
#define HAS_TOUCH 1
using Hal = espp::TDeck;
#else
#error "Misconfigured hardware!"
#endif

#if CONFIG_ESP32_WIFI_NVS_ENABLED
#include "nvs.hpp"
#endif

#include "button.hpp"
#include "cli.hpp"
#include "gui.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "tcp_socket.hpp"
#include "udp_socket.hpp"
#include "wifi_sta.hpp"
#include "wifi_sta_menu.hpp"

using namespace std::chrono_literals;

static espp::Logger logger({.tag = "WirelessDebugDisplay", .level = espp::Logger::Verbosity::INFO});

static std::recursive_mutex gui_mutex;
static std::shared_ptr<Gui> gui;

static constexpr size_t server_port = CONFIG_DEBUG_SERVER_PORT;
static std::recursive_mutex server_mutex;
static std::unique_ptr<espp::Task> start_server_task;
static std::string server_address = "";
static std::shared_ptr<espp::UdpSocket> server_socket;

static std::shared_ptr<espp::WifiSta> wifi_sta;

bool start_server(std::mutex &m, std::condition_variable &cv, bool &task_notified);
std::optional<std::vector<uint8_t>> on_data_received(const std::vector<uint8_t> &data,
                                                     const espp::Socket::Info &sender_info);

extern "C" void app_main(void) {
  logger.info("Bootup");

#if CONFIG_ESP32_WIFI_NVS_ENABLED
  // initialize NVS, needed for WiFi
  std::error_code ec;
  espp::Nvs nvs;
  nvs.init(ec);
#endif

  // hardware specific configuration
  auto &hal = Hal::get();

  hal.set_log_level(espp::Logger::Verbosity::INFO);

  if (!hal.initialize_lcd()) {
    logger.error("Could not initialize LCD");
    return;
  }
  // initialize the display, using a pixel buffer of 50 lines
  static constexpr size_t pixel_buffer_size = hal.lcd_width() * 50;
  if (!hal.initialize_display(pixel_buffer_size)) {
    logger.error("Could not initialize display");
    return;
  }

  auto display = hal.display();

  // create the gui
  gui = std::make_shared<Gui>(
      Gui::Config{.display = display, .log_level = espp::Logger::Verbosity::DEBUG});

  // initialize the input system
#if !HAS_TOUCH
  espp::Button button({
      .interrupt_config =
          {
              .gpio_num = GPIO_NUM_0,
              .callback =
                  [](const espp::Button::Event &event) {
                    std::lock_guard<std::recursive_mutex> lock(gui_mutex);
                    gui->switch_tab();
                  },
              .active_level = espp::Button::ActiveLevel::LOW,
              .interrupt_type = espp::Button::InterruptType::RISING_EDGE,
              .pullup_enabled = false,
              .pulldown_enabled = false,
          },
      .log_level = espp::Logger::Verbosity::WARN,
  });
#else
  if (!hal.initialize_touch()) {
    logger.error("Could not initialize touch");
    return;
  }
#endif

  // initialize WiFi
  logger.info("Initializing WiFi");
  wifi_sta = std::make_shared<espp::WifiSta>(espp::WifiSta::Config{
      .ssid = CONFIG_ESP_WIFI_SSID,
      .password = CONFIG_ESP_WIFI_PASSWORD,
      .num_connect_retries = CONFIG_ESP_MAXIMUM_RETRY,
      .on_connected = []() { logger.info("WiFi connected, waiting for IP"); },
      .on_disconnected =
          []() {
            logger.info("WiFi disconnected, stopping server task and freeing resources");
            std::lock_guard<std::recursive_mutex> lock(server_mutex);
            // stop the server task
            start_server_task.reset();
            logger.info("server task stopped");
            // free the medns resources
            mdns_free();
            logger.info("mdns resources freed");
            // delete the socket
            server_socket.reset();
            logger.info("Socket resources freed");
          },
      .on_got_ip =
          [](ip_event_got_ip_t *eventdata) {
            server_address = fmt::format("{}.{}.{}.{}", IP2STR(&eventdata->ip_info.ip));
            logger.info("got IP: {}.{}.{}.{}", IP2STR(&eventdata->ip_info.ip));
            // update the info page
            {
              std::lock_guard<std::recursive_mutex> lock(gui_mutex);
              gui->clear_info();
              gui->add_info(std::string("#FF0000 WiFi: #") + wifi_sta->get_ssid());
              gui->add_info(std::string("#00FF00 IP: #") + server_address + ":" +
                            std::to_string(server_port));
            }
            // start the server task
            {
              std::lock_guard<std::recursive_mutex> lock(server_mutex);
              start_server_task = espp::Task::make_unique(
                  espp::Task::Config{.callback = start_server,
                                     .task_config = {.name = "Start Server Task", .priority = 10}});
              start_server_task->start();
            }
          }});

  espp::WifiStaMenu sta_menu(*wifi_sta);
  auto root_menu = sta_menu.get();
  root_menu->Insert(
      "memory",
      [](std::ostream &out) {
        out << "Minimum free memory: " << heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT)
            << std::endl;
      },
      "Display minimum free memory.");

  // add a command to switch tabs
  root_menu->Insert(
      "switch_tab",
      [](std::ostream &out) {
        std::lock_guard<std::recursive_mutex> lock(gui_mutex);
        gui->switch_tab();
        out << "Switched tab.\n";
      },
      "Switch to the next tab in the display.");

  // add a command to clear the info
  root_menu->Insert(
      "clear_info",
      [](std::ostream &out) {
        std::lock_guard<std::recursive_mutex> lock(gui_mutex);
        gui->clear_info();
        out << "Info cleared.\n";
      },
      "Clear the Info display.");

  // add a command to clear the plots
  root_menu->Insert(
      "clear_plots",
      [](std::ostream &out) {
        std::lock_guard<std::recursive_mutex> lock(gui_mutex);
        gui->clear_plots();
        out << "Plots cleared.\n";
      },
      "Clear the Plot display.");

  // add a command to clear the logs
  root_menu->Insert(
      "clear_logs",
      [](std::ostream &out) {
        std::lock_guard<std::recursive_mutex> lock(gui_mutex);
        gui->clear_logs();
        out << "Logs cleared.\n";
      },
      "Clear the Log display.");

  // add a command to push data into the display
  root_menu->Insert("push_data",
                    [](std::ostream &out, const std::string &data) {
                      std::lock_guard<std::recursive_mutex> lock(gui_mutex);
                      gui->push_data(data);
                      gui->handle_data();
                      out << "Data pushed to display.\n";
                    },
                    "Push data to the display.", {"data"});

  // add a command to push info into the display
  root_menu->Insert("push_info",
                    [](std::ostream &out, const std::string &info) {
                      std::lock_guard<std::recursive_mutex> lock(gui_mutex);
                      gui->add_info(info);
                      out << "Info pushed to display.\n";
                    },
                    "Push info to the display.", {"info"});

  cli::Cli cli(std::move(root_menu));
  cli::SetColor();
  cli.ExitAction([](auto &out) { out << "Goodbye and thanks for all the fish.\n"; });

  espp::Cli input(cli);
  input.SetInputHistorySize(10);
  input.Start();
}

bool start_server(std::mutex &m, std::condition_variable &cv, bool &task_notified) {
  // create the debug display socket
  logger.info("Creating debug server at {}:{}", server_address, server_port);
  // create the socket
  server_socket = std::make_shared<espp::UdpSocket>(
      espp::UdpSocket::Config{.log_level = espp::Logger::Verbosity::WARN});
  espp::Task::BaseConfig server_task_config{
      .name = "UdpServer",
      .stack_size_bytes = 6 * 1024,
  };
  espp::UdpSocket::ReceiveConfig server_config{
      .port = server_port,
      .buffer_size = 1024,
      .on_receive_callback = on_data_received,
  };

  // set a timeout on the socket, so that it doesn't block indefinitely. This is
  // required to allow us to gracefully shutdown the socket.
  server_socket->set_receive_timeout(1000ms);

  // now actually start the socket receiving task
  server_socket->start_receiving(server_task_config, server_config);

  // initialize mDNS, so that other embedded devices on the network can find us
  // without having to be hardcoded / configured with our IP address and port
  logger.info("Initializing mDNS");
  auto err = mdns_init();
  if (err != ESP_OK) {
    logger.error("Could not initialize mDNS: {}", err);
    return true; // stop the task
  }

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  std::string hostname = fmt::format("wireless-debug-display-{:x}{:x}{:x}", mac[3], mac[4], mac[5]);
  err = mdns_hostname_set(hostname.c_str());
  if (err != ESP_OK) {
    logger.error("Could not set mDNS hostname: {}", err);
    return true; // stop the task
  }
  logger.info("mDNS hostname set to '{}'", hostname);
  err = mdns_instance_name_set("Wireless Debug Display");
  if (err != ESP_OK) {
    logger.error("Could not set mDNS instance name: {}", err);
    return true; // stop the task
  }
  err = mdns_service_add("Wireless Debug Display", "_debugdisplay", "_udp", server_port, NULL, 0);
  if (err != ESP_OK) {
    logger.error("Could not add mDNS service: {}", err);
    return true; // stop the task
  }
  logger.info("mDNS initialized");

  return true; // stop the task
}

std::optional<std::vector<uint8_t>> on_data_received(const std::vector<uint8_t> &data,
                                                     const espp::Socket::Info &sender_info) {
  std::string data_str(data.begin(), data.end());
  fmt::print("Server received: '{}'\n"
             "    from source: {}\n",
             data_str, sender_info);
  std::lock_guard<std::recursive_mutex> lock(gui_mutex);
  gui->push_data(data_str);
  gui->handle_data();
  return std::nullopt;
}

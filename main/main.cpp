#include <sdkconfig.h>

#include <chrono>
#include <thread>

#include <mdns.h>

#if CONFIG_HARDWARE_WROVER_KIT
#include "wrover-kit.hpp"
#elif CONFIG_HARDWARE_BOX
#include "esp-box.hpp"
#elif CONFIG_HARDWARE_TDECK
#include "t-deck.hpp"
#else
#error "Misconfigured hardware!"
#endif

#if CONFIG_ESP32_WIFI_NVS_ENABLED
#include "nvs.hpp"
#endif

#include "button.hpp"
#include "gui.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "tcp_socket.hpp"
#include "udp_socket.hpp"
#include "wifi_sta.hpp"

using namespace std::chrono_literals;

extern "C" void app_main(void) {
  espp::Logger logger({.tag = "WirelessDebugDisplay", .level = espp::Logger::Verbosity::INFO});
  logger.info("Bootup");

#if CONFIG_ESP32_WIFI_NVS_ENABLED
  // initialize NVS, needed for WiFi
  std::error_code ec;
  espp::Nvs nvs;
  nvs.init(ec);
#endif

  // hardware specific configuration
#if CONFIG_HARDWARE_WROVER_KIT
  auto &hal = espp::WroverKit::get();
#endif
#if CONFIG_HARDWARE_BOX
  auto &hal = espp::EspBox::get();
#endif
#if CONFIG_HARDWARE_TDECK
  auto &hal = espp::TDeck::get();
#endif

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
  Gui gui({.display = display, .log_level = espp::Logger::Verbosity::DEBUG});

  // initialize the input system
#if CONFIG_HARDWARE_WROVER_KIT
  espp::Button button({
      .interrupt_config =
          {
              .gpio_num = GPIO_NUM_0,
              .callback = [&](const espp::Button::Event &event) { gui.switch_tab(); },
              .active_level = espp::Button::ActiveLevel::LOW,
              .interrupt_type = espp::Button::InterruptType::RISING_EDGE,
              .pullup_enabled = false,
              .pulldown_enabled = false,
          },
      .log_level = espp::Logger::Verbosity::WARN,
  });
#endif
#if CONFIG_HARDWARE_BOX || CONFIG_HARDWARE_TDECK
  if (!hal.initialize_touch()) {
    logger.error("Could not initialize touch");
    return;
  }
  // make a task to run the touch update
  auto touch_task_config = espp::Task::Config{
      .name = "TouchTask",
      .callback =
          [&](auto &m, auto &cv) {
            hal.update_touch();
            std::this_thread::sleep_for(10ms);
            return false;
          },
      .stack_size_bytes = 6 * 1024,
  };
  espp::Task touch_task(touch_task_config);
  touch_task.start();
#endif

  // initialize WiFi
  logger.info("Initializing WiFi");
  std::string server_address;
  espp::WifiSta wifi_sta({.ssid = CONFIG_ESP_WIFI_SSID,
                          .password = CONFIG_ESP_WIFI_PASSWORD,
                          .num_connect_retries = CONFIG_ESP_MAXIMUM_RETRY,
                          .on_connected = nullptr,
                          .on_disconnected = nullptr,
                          .on_got_ip = [&server_address, &logger](ip_event_got_ip_t *eventdata) {
                            server_address =
                                fmt::format("{}.{}.{}.{}", IP2STR(&eventdata->ip_info.ip));
                            logger.info("got IP: {}.{}.{}.{}", IP2STR(&eventdata->ip_info.ip));
                          }});

  // wait for network
  while (!wifi_sta.is_connected()) {
    logger.info("waiting for wifi connection...");
    std::this_thread::sleep_for(1s);
  }

  // create the debug display socket
  size_t server_port = CONFIG_DEBUG_SERVER_PORT;
  logger.info("Creating debug server at {}:{}", server_address, server_port);
  // create the socket
  espp::UdpSocket server_socket({.log_level = espp::Logger::Verbosity::WARN});
  auto server_task_config = espp::Task::Config{
      .name = "UdpServer",
      .callback = nullptr,
      .stack_size_bytes = 6 * 1024,
  };
  auto server_config = espp::UdpSocket::ReceiveConfig{
      .port = server_port,
      .buffer_size = 1024,
      .on_receive_callback = [&gui](
          auto &data, auto &source) -> auto{// turn the vector<uint8_t> into a string
                                            std::string data_str(data.begin(), data.end());
  fmt::print("Server received: '{}'\n"
             "    from source: {}\n",
             data_str, source);
  gui.push_data(data_str);
  gui.handle_data();
  return std::nullopt;
}
}
;

server_socket.start_receiving(server_task_config, server_config);

// initialize mDNS, so that other embedded devices on the network can find us
// without having to be hardcoded / configured with our IP address and port
logger.info("Initializing mDNS");
auto err = mdns_init();
if (err != ESP_OK) {
  logger.error("Could not initialize mDNS: {}", err);
  return;
}

uint8_t mac[6];
esp_read_mac(mac, ESP_MAC_WIFI_STA);
std::string hostname = fmt::format("wireless-debug-display-{:x}{:x}{:x}", mac[3], mac[4], mac[5]);
err = mdns_hostname_set(hostname.c_str());
if (err != ESP_OK) {
  logger.error("Could not set mDNS hostname: {}", err);
  return;
}
logger.info("mDNS hostname set to '{}'", hostname);
err = mdns_instance_name_set("Wireless Debug Display");
if (err != ESP_OK) {
  logger.error("Could not set mDNS instance name: {}", err);
  return;
}
err = mdns_service_add("Wireless Debug Display", "_debugdisplay", "_udp", server_port, NULL, 0);
if (err != ESP_OK) {
  logger.error("Could not add mDNS service: {}", err);
  return;
}
logger.info("mDNS initialized");

// update the info page
gui.clear_info();
gui.add_info(std::string("#FF0000 WiFi: #") + CONFIG_ESP_WIFI_SSID);
gui.add_info(std::string("#00FF00 IP: #") + server_address + ":" + std::to_string(server_port));

// loop forever
while (true) {
  std::this_thread::sleep_for(1s);
}
}

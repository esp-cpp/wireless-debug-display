#include <sdkconfig.h>

#include <chrono>
#include <thread>

#include <driver/spi_master.h>
#include <hal/spi_types.h>
#include <mdns.h>
#if CONFIG_ESP32_WIFI_NVS_ENABLED
#include <nvs_flash.h>
#endif

#include "display.hpp"

#if CONFIG_HARDWARE_WROVER_KIT
#include "wrover_kit.hpp"
#elif CONFIG_HARDWARE_BOX
#include "box.hpp"
#elif CONFIG_HARDWARE_BOX_3
#include "box_3.hpp"
#elif CONFIG_HARDWARE_TDECK
#include "tdeck.hpp"
#else
#error "Misconfigured hardware!"
#endif

#include "gui.hpp"
#include "logger.hpp"
#include "task.hpp"
#include "tcp_socket.hpp"
#include "udp_socket.hpp"
#include "wifi_sta.hpp"

using namespace std::chrono_literals;

static spi_device_handle_t spi;
static const int spi_queue_size = 7;
static size_t num_queued_trans = 0;

// the user flag for the callbacks does two things:
// 1. Provides the GPIO level for the data/command pin, and
// 2. Sets some bits for other signaling (such as LVGL FLUSH)
static constexpr int FLUSH_BIT = (1 << (int)espp::display_drivers::Flags::FLUSH_BIT);
static constexpr int DC_LEVEL_BIT = (1 << (int)espp::display_drivers::Flags::DC_LEVEL_BIT);

// This function is called (in irq context!) just before a transmission starts.
// It will set the D/C line to the value indicated in the user field
// (DC_LEVEL_BIT).
static void IRAM_ATTR lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
  uint32_t user_flags = (uint32_t)(t->user);
  bool dc_level = user_flags & DC_LEVEL_BIT;
  gpio_set_level((gpio_num_t)DC_PIN_NUM, dc_level);
}

// This function is called (in irq context!) just after a transmission ends. It
// will indicate to lvgl that the next flush is ready to be done if the
// FLUSH_BIT is set.
static void IRAM_ATTR lcd_spi_post_transfer_callback(spi_transaction_t *t) {
  uint16_t user_flags = (uint32_t)(t->user);
  bool should_flush = user_flags & FLUSH_BIT;
  if (should_flush) {
    lv_disp_t *disp = _lv_refr_get_disp_refreshing();
    lv_disp_flush_ready(disp->driver);
  }
}

extern "C" void IRAM_ATTR lcd_write(const uint8_t *data, size_t length, uint32_t user_data) {
  if (length == 0) {
    return;
  }
  static spi_transaction_t t;
  memset(&t, 0, sizeof(t));
  t.length = length * 8;
  t.tx_buffer = data;
  t.user = (void *)user_data;
  spi_device_polling_transmit(spi, &t);
}

static void lcd_wait_lines() {
  spi_transaction_t *rtrans;
  esp_err_t ret;
  // Wait for all transactions to be done and get back the results.
  while (num_queued_trans) {
    // fmt::print("Waiting for {} lines\n", num_queued_trans);
    ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
    if (ret != ESP_OK) {
      fmt::print("Could not get trans result: {} '{}'\n", ret, esp_err_to_name(ret));
    }
    num_queued_trans--;
    // We could inspect rtrans now if we received any info back. The LCD is treated as write-only,
    // though.
  }
}

void IRAM_ATTR lcd_send_lines(int xs, int ys, int xe, int ye, const uint8_t *data,
                              uint32_t user_data) {
  // if we haven't waited by now, wait here...
  lcd_wait_lines();
  esp_err_t ret;
  // Transaction descriptors. Declared static so they're not allocated on the stack; we need this
  // memory even when this function is finished because the SPI driver needs access to it even while
  // we're already calculating the next line.
  static spi_transaction_t trans[6];
  // In theory, it's better to initialize trans and data only once and hang on to the initialized
  // variables. We allocate them on the stack, so we need to re-init them each call.
  for (int i = 0; i < 6; i++) {
    memset(&trans[i], 0, sizeof(spi_transaction_t));
    if ((i & 1) == 0) {
      // Even transfers are commands
      trans[i].length = 8;
      trans[i].user = (void *)0;
    } else {
      // Odd transfers are data
      trans[i].length = 8 * 4;
      trans[i].user = (void *)DC_LEVEL_BIT;
    }
    trans[i].flags = SPI_TRANS_USE_TXDATA;
  }
  size_t length = (xe - xs + 1) * (ye - ys + 1) * 2;
  trans[0].tx_data[0] = (uint8_t)DisplayDriver::Command::caset;
  trans[1].tx_data[0] = (xs) >> 8;
  trans[1].tx_data[1] = (xs)&0xff;
  trans[1].tx_data[2] = (xe) >> 8;
  trans[1].tx_data[3] = (xe)&0xff;
  trans[2].tx_data[0] = (uint8_t)DisplayDriver::Command::raset;
  trans[3].tx_data[0] = (ys) >> 8;
  trans[3].tx_data[1] = (ys)&0xff;
  trans[3].tx_data[2] = (ye) >> 8;
  trans[3].tx_data[3] = (ye)&0xff;
  trans[4].tx_data[0] = (uint8_t)DisplayDriver::Command::ramwr;
  trans[5].tx_buffer = data;
  trans[5].length = length * 8;
  // undo SPI_TRANS_USE_TXDATA flag
  trans[5].flags = 0;
  // we need to keep the dc bit set, but also add our flags
  trans[5].user = (void *)(DC_LEVEL_BIT | user_data);
  // Queue all transactions.
  for (int i = 0; i < 6; i++) {
    ret = spi_device_queue_trans(spi, &trans[i], portMAX_DELAY);
    if (ret != ESP_OK) {
      fmt::print("Couldn't queue trans: {} '{}'\n", ret, esp_err_to_name(ret));
    } else {
      num_queued_trans++;
    }
  }
  // When we are here, the SPI driver is busy (in the background) getting the
  // transactions sent. That happens mostly using DMA, so the CPU doesn't have
  // much to do here. We're not going to wait for the transaction to finish
  // because we may as well spend the time calculating the next line. When that
  // is done, we can call send_line_finish, which will wait for the transfers
  // to be done and check their status.
}

extern "C" void app_main(void) {
  static auto start = std::chrono::high_resolution_clock::now();
  static auto elapsed = [&]() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float>(now - start).count();
  };

  espp::Logger logger({.tag = "WirelessDebugDisplay", .level = espp::Logger::Verbosity::INFO});

  logger.info("Bootup");

#if CONFIG_ESP32_WIFI_NVS_ENABLED
  // initialize NVS, needed for WiFi
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    logger.warn("Erasing NVS flash...");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
#endif

  // hardware specific configuration
#if CONFIG_HARDWARE_WROVER_KIT
#endif
#if CONFIG_HARDWARE_BOX
#endif
#if CONFIG_HARDWARE_TDECK
  // peripheral power on t-deck requires the power pin to be set (gpio 10)
  // so set up gpio output and set it high
  gpio_num_t BOARD_POWER_ON_PIN = GPIO_NUM_10;
  gpio_set_direction(BOARD_POWER_ON_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(BOARD_POWER_ON_PIN, 1);

  gpio_num_t KEYBOARD_INTERRUPT_PIN = GPIO_NUM_46;
  gpio_set_direction(KEYBOARD_INTERRUPT_PIN, GPIO_MODE_INPUT);
#endif

  logger.info("Initializing display drivers for {}", dev_kit);
  // create the spi host
  spi_bus_config_t buscfg;
  memset(&buscfg, 0, sizeof(buscfg));
  buscfg.mosi_io_num = mosi;
  buscfg.miso_io_num = -1;
  buscfg.sclk_io_num = sclk;
  buscfg.quadwp_io_num = -1;
  buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = (int)(pixel_buffer_size * sizeof(lv_color_t));
  // create the spi device
  spi_device_interface_config_t devcfg;
  memset(&devcfg, 0, sizeof(devcfg));
  devcfg.mode = 0;
  devcfg.clock_speed_hz = clock_speed;
  devcfg.input_delay_ns = 0;
  devcfg.spics_io_num = spics;
  devcfg.queue_size = spi_queue_size;
  devcfg.pre_cb = lcd_spi_pre_transfer_callback;
  devcfg.post_cb = lcd_spi_post_transfer_callback;

  // Initialize the SPI bus
  ret = spi_bus_initialize(spi_num, &buscfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);
  // Attach the LCD to the SPI bus
  ret = spi_bus_add_device(spi_num, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);
  // initialize the controller
  DisplayDriver::initialize(espp::display_drivers::Config{
      .lcd_write = lcd_write,
      .lcd_send_lines = lcd_send_lines,
      .reset_pin = reset,
      .data_command_pin = dc_pin,
      .reset_value = reset_value,
      .invert_colors = invert_colors,
      .mirror_x = mirror_x,
      .mirror_y = mirror_y,
  });
  // initialize the display / lvgl
  auto display = std::make_shared<espp::Display>(
      espp::Display::AllocatingConfig{.width = width,
                                      .height = height,
                                      .pixel_buffer_size = pixel_buffer_size,
                                      .flush_callback = DisplayDriver::flush,
                                      .backlight_pin = backlight,
                                      .backlight_on_value = backlight_value,
                                      .rotation = rotation,
                                      .software_rotation_enabled = true});

  // create the gui
  Gui gui({.display = display, .log_level = espp::Logger::Verbosity::DEBUG});

  // initialize the input system
#if CONFIG_HARDWARE_WROVER_KIT
  espp::Button button({
      .gpio_num = GPIO_NUM_0,
      .callback = [&](const espp::Button::Event &event) { gui.switch_tab(); },
      .active_level = espp::Button::ActiveLevel::LOW,
      .interrupt_type = espp::Button::InterruptType::RISING_EDGE,
      .pullup_enabled = false,
      .pulldown_enabled = false,
      .log_level = espp::Logger::Verbosity::WARN,
  });
#endif
#if CONFIG_HARDWARE_BOX || CONFIG_HARDWARE_BOX_3 || CONFIG_HARDWARE_TDECK
  // initialize the i2c bus to read the touchpad driver
  espp::I2c i2c({
      .port = I2C_NUM_0,
      .sda_io_num = i2c_sda,
      .scl_io_num = i2c_scl,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .clk_speed = 400 * 1000,
  });

  // probe for the touch devices:
  // tt21100, gt911
  bool has_tt21100 = false;
  bool has_gt911 = false;
  has_tt21100 = i2c.probe_device(0x24);
  has_gt911 = i2c.probe_device(0x5d) | i2c.probe_device(0x14);
  logger.info("Touchpad probe results: tt21100: {}, gt911: {}", has_tt21100, has_gt911);

#if CONFIG_HARDWARE_BOX
  logger.info("Initializing Tt21100");
  using TouchDriver = espp::Tt21100;
#endif
#if CONFIG_HARDWARE_TDECK || CONFIG_HARDWARE_BOX_3
  logger.info("Initializing GT911");
  using TouchDriver = espp::Gt911;
  // implement GT911
#endif
  TouchDriver touch({.write = std::bind(&espp::I2c::write, &i2c, std::placeholders::_1,
                                        std::placeholders::_2, std::placeholders::_3),
                     .read = std::bind(&espp::I2c::read, &i2c, std::placeholders::_1,
                                       std::placeholders::_2, std::placeholders::_3)});

  auto touchpad_read = [&touch](uint8_t *num_touch_points, uint16_t *x, uint16_t *y,
                                uint8_t *btn_state) {
    std::error_code ec;
    *num_touch_points = 0;
    // get the latest data from the device
    if (touch.update(ec) && !ec) {
      touch.get_touch_point(num_touch_points, x, y);
    }
    *btn_state = touch.get_home_button_state();
  };
  logger.info("Initializing touchpad");
  auto touchpad =
      espp::TouchpadInput(espp::TouchpadInput::Config{.touchpad_read = touchpad_read,
                                                      .swap_xy = touch_swap_xy,
                                                      .invert_x = touch_invert_x,
                                                      .invert_y = touch_invert_y,
                                                      .log_level = espp::Logger::Verbosity::WARN});
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

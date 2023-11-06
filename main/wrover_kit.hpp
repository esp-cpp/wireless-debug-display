#pragma once

#include "ili9341.hpp"
#include "button.hpp"
static constexpr int DC_PIN_NUM = 21;

static constexpr std::string_view dev_kit = "ESP-WROVER-DevKit";
static constexpr int clock_speed = 20 * 1000 * 1000;
static constexpr auto spi_num = SPI2_HOST;
static constexpr gpio_num_t mosi = GPIO_NUM_23;
static constexpr gpio_num_t sclk = GPIO_NUM_19;
static constexpr gpio_num_t spics = GPIO_NUM_22;
static constexpr gpio_num_t reset = GPIO_NUM_18;
static constexpr gpio_num_t dc_pin = (gpio_num_t)DC_PIN_NUM;
static constexpr gpio_num_t backlight = GPIO_NUM_5;
static constexpr size_t width = 320;
static constexpr size_t height = 240;
static constexpr size_t pixel_buffer_size = 16384;
static constexpr bool invert_colors = false;
static auto flush_cb = espp::Ili9341::flush;
static auto rotation = espp::Display::Rotation::LANDSCAPE;

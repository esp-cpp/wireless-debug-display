#pragma once

#include "i2c.hpp"
#include "st7789.hpp"
#include "touchpad_input.hpp"
#include "gt911.hpp"
static constexpr int DC_PIN_NUM = 11;

static constexpr std::string_view dev_kit = "LILYGO T-DECK";
static constexpr gpio_num_t i2c_sda = GPIO_NUM_18;
static constexpr gpio_num_t i2c_scl = GPIO_NUM_8;
static constexpr bool touch_swap_xy = true;
static constexpr int clock_speed = 40 * 1000 * 1000;
static constexpr auto spi_num = SPI2_HOST;
static constexpr gpio_num_t mosi = GPIO_NUM_41;
static constexpr gpio_num_t sclk = GPIO_NUM_40;
static constexpr gpio_num_t spics = GPIO_NUM_12;
static constexpr gpio_num_t reset = GPIO_NUM_NC; // not connected according to Setup210_LilyGo_T_Deck.h
static constexpr gpio_num_t dc_pin = (gpio_num_t)DC_PIN_NUM;
static constexpr gpio_num_t backlight = GPIO_NUM_42;
static constexpr size_t width = 320;
static constexpr size_t height = 240;
static constexpr size_t pixel_buffer_size = width * 50;
static constexpr bool backlight_value = true;
static constexpr bool reset_value = false;
static constexpr bool invert_colors = false;
static constexpr auto rotation = espp::Display::Rotation::LANDSCAPE_INVERTED;
static constexpr bool mirror_x = true;
static constexpr bool mirror_y = true;
static constexpr bool touch_invert_x = true;
static constexpr bool touch_invert_y = false;

using DisplayDriver = espp::St7789;

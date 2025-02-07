#pragma once

#include "driver/gpio.h"

struct Pins {
  // LED
  static constexpr gpio_num_t LED = GPIO_NUM_22;

  // SPI for sensor
  static constexpr gpio_num_t SCK = GPIO_NUM_13;
  static constexpr gpio_num_t MOSI = GPIO_NUM_2;
  static constexpr gpio_num_t MISO = GPIO_NUM_15;
  static constexpr gpio_num_t ICMCS = GPIO_NUM_4;
  static constexpr gpio_num_t LPSCS = GPIO_NUM_12;

  // SPI for microSD card
  static constexpr gpio_num_t SD_CS = GPIO_NUM_16;
  static constexpr gpio_num_t SD_MOSI = GPIO_NUM_23;
  static constexpr gpio_num_t SD_MISO = GPIO_NUM_19;
  static constexpr gpio_num_t SD_SCK = GPIO_NUM_18;

  // CAN
  static constexpr gpio_num_t CAN_RX = GPIO_NUM_5;
  static constexpr gpio_num_t CAN_TX = GPIO_NUM_17;

  // Servo
  static constexpr gpio_num_t SERVO = GPIO_NUM_26;
};

// グローバルな設定namespace
namespace config {
extern const Pins pins; // どこからでもconfig::pinsでアクセス可能
}

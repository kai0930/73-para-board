#pragma once

#include "driver/gpio.h"

struct Pins {
    // LED
    static constexpr gpio_num_t LED = GPIO_NUM_5;

    // SPI for sensor
    static constexpr gpio_num_t SCK = GPIO_NUM_3;
    static constexpr gpio_num_t MOSI = GPIO_NUM_8;
    static constexpr gpio_num_t MISO = GPIO_NUM_18;
    static constexpr gpio_num_t ICMCS = GPIO_NUM_9;
    static constexpr gpio_num_t LPSCS = GPIO_NUM_17;

    // CAN
    static constexpr gpio_num_t CAN_RX = GPIO_NUM_47;
    static constexpr gpio_num_t CAN_TX = GPIO_NUM_21;

    // Servo
    static constexpr gpio_num_t SERVO = GPIO_NUM_42;
};

// グローバルな設定namespace
namespace config {
extern const Pins pins;  // どこからでもconfig::pinsでアクセス可能
}

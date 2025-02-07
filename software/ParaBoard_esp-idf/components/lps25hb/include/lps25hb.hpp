#pragma once

#include "create_spi.hpp"
#include "driver/spi_master.h"

struct Lps25hbConfig {
    static constexpr uint8_t WHO_AM_I_VALUE = 0xBD;        // 期待される値
    static constexpr uint32_t DEFAULT_SPI_FREQ = 8000000;  // 8MHz
    static constexpr uint8_t READ_BIT = 0x80;              // 読み取り時の最上位ビット

    struct Registers {
        static constexpr uint8_t WHO_AM_I = 0x0F;
        static constexpr uint8_t CTRL_REG1 = 0x20;
        static constexpr uint8_t CTRL_REG2 = 0x21;
        static constexpr uint8_t PRESS_OUT_XL = 0x28;
        static constexpr uint8_t PRESS_OUT_L = 0x29;
        static constexpr uint8_t PRESS_OUT_H = 0x2A;
    };

    struct Settings {
        static constexpr uint8_t POWER_UP = 0xC0;    // Power up + ODR=25Hz
        static constexpr uint8_t BDU_ENABLE = 0x04;  // Block Data Update
    };
};

struct Lps25hbData {
    float pressure;  // hPa (mbar)
    uint32_t raw;    // 生のセンサー値
};

class Lps25hb {
   private:
    int cs_pin;
    int device_handle_id;
    CreateSpi *create_spi;
    static const char *TAG;

   public:
    IRAM_ATTR bool begin(CreateSpi *create_spi, gpio_num_t cs_pin, uint32_t frequency = 8000000);
    IRAM_ATTR uint8_t whoAmI();
    IRAM_ATTR void get(Lps25hbData *data);

    bool isInitialized() const {
        return device_handle_id >= 0;
    }
};
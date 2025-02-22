#pragma once

#include "create_spi.hpp"
#include "driver/spi_master.h"

namespace Lps {

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
        static constexpr uint8_t TEMP_OUT_L = 0x2B;
        static constexpr uint8_t TEMP_OUT_H = 0x2C;
    };

    struct Settings {
        static constexpr uint8_t POWER_UP = 0b10000000;     // Power up
        static constexpr uint8_t ODR_25 = 0b01000000;       // ODR=25Hz
        static constexpr uint8_t I2C_DISABLE = 0b00001000;  // I2C disable
    };
};

struct PressureData {
    uint8_t xl_p, l_p, h_p;
};

struct TempData {
    uint8_t l_t, h_t;
};

class Lps25hb {
   private:
    int cs_pin;
    int device_handle_id;
    CreateSpi *create_spi;
    static const char *TAG;

   public:
    bool begin(CreateSpi *create_spi, gpio_num_t cs_pin, uint32_t frequency = Lps25hbConfig::DEFAULT_SPI_FREQ);
    bool whoAmI(uint8_t *data);
    bool getPressure(PressureData *pressure);
    bool getTemp(TempData *temp);
    bool getPressureAndTemp(PressureData *pressure, TempData *temp);

    bool isInitialized() const {
        return device_handle_id >= 0;
    }
};

}  // namespace Lps

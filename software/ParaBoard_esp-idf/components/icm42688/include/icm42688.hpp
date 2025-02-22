#pragma once

#include "create_spi.hpp"
#include "driver/spi_master.h"
#include "math.h"

namespace Icm {

struct Icm42688Config {
    static constexpr uint8_t WHO_AM_I_VALUE = 0x47;        // 期待される値
    static constexpr uint32_t DEFAULT_SPI_FREQ = 8000000;  // 8MHz
    static constexpr uint8_t READ_BIT = 0x80;              // 読み取り時の最上位ビット

    struct Registers {
        static constexpr uint8_t PWR_MGMT0 = 0x4E;
        static constexpr uint8_t WHO_AM_I = 0x75;
        static constexpr uint8_t TEMP_DATA = 0x1D;
        static constexpr uint8_t ACCEL_DATA = 0x1F;
        static constexpr uint8_t GYRO_DATA = 0x25;
        static constexpr uint8_t GYRO_CONFIG0 = 0x4F;
        static constexpr uint8_t ACCEL_CONFIG0 = 0x50;
    };

    struct GyroScale {
        static constexpr uint8_t DPS2000 = 0b00000000;
        static constexpr uint8_t DPS1000 = 0b00100000;
        static constexpr uint8_t DPS500 = 0b01000000;
        static constexpr uint8_t DPS250 = 0b01100000;
        static constexpr uint8_t DPS125 = 0b10000000;
        static constexpr uint8_t DPS62_5 = 0b10100000;
        static constexpr uint8_t DPS31_25 = 0b11000000;
        static constexpr uint8_t DPS15_625 = 0b11100000;
    };

    struct AccelScale {
        static constexpr uint8_t G16 = 0b00000000;
        static constexpr uint8_t G8 = 0b00100000;
        static constexpr uint8_t G4 = 0b01000000;
        static constexpr uint8_t G2 = 0b01100000;
    };

    struct ODR {
        static constexpr uint8_t ODR32k = 0b00000001;     // LN mode
        static constexpr uint8_t ODR16k = 0b00000010;     // LN mode
        static constexpr uint8_t ODR8k = 0b00000011;      // LN mode
        static constexpr uint8_t ODR4k = 0b00000100;      // LN mode
        static constexpr uint8_t ODR2k = 0b00000101;      // LN mode
        static constexpr uint8_t ODR1k = 0b00000110;      // LN mode
        static constexpr uint8_t ODR200 = 0b00000111;     // LN or LP mode
        static constexpr uint8_t ODR100 = 0b00001000;     // LN or LP mode
        static constexpr uint8_t ODR50 = 0b00001001;      // LN or LP mode
        static constexpr uint8_t ODR25 = 0b00001010;      // LN or LP mode
        static constexpr uint8_t ODR12_5 = 0b00001011;    // LN or LP mode
        static constexpr uint8_t ODR6_25 = 0b00001100;    // LP mode
        static constexpr uint8_t ODR3_125 = 0b00001101;   // LP mode
        static constexpr uint8_t ODR1_5625 = 0b00001110;  // LP mode
        static constexpr uint8_t ODR500 = 0b00001111;     // LN or LP mode
    };
};

struct AccelData {
    uint8_t u_x, d_x, u_y, d_y, u_z, d_z;
};

struct GyroData {
    uint8_t u_x, d_x, u_y, d_y, u_z, d_z;
};

struct TempData {
    uint8_t u_t, d_t;
};

class Icm42688 {
   private:
    int cs_pin;
    int device_handle_id;
    CreateSpi *create_spi;
    static const char *TAG;

   public:
    bool begin(CreateSpi *create_spi, gpio_num_t cs_pin, uint32_t frequency = Icm42688Config::DEFAULT_SPI_FREQ);
    bool whoAmI(uint8_t *data);
    bool getAccel(AccelData *data);
    bool getGyro(GyroData *data);
    bool getTemp(TempData *data);
    bool getAccelAndGyro(AccelData *accel, GyroData *gyro);

    // エラー状態の管理を追加
    bool isInitialized() const {
        return device_handle_id >= 0;
    }
};

}  // namespace Icm

#pragma once

#include "create_spi.hpp"
#include "driver/spi_master.h"
#include "math.h"

struct Icm20602Config {
    static constexpr uint8_t WHO_AM_I_VALUE = 0x12;        // 期待される値
    static constexpr uint32_t DEFAULT_SPI_FREQ = 8000000;  // 8MHz
    static constexpr uint8_t READ_BIT = 0x80;              // 読み取り時の最上位ビット

    struct Registers {
        static constexpr uint8_t CONFIG = 0x1A;
        static constexpr uint8_t PWR_MGMT_1 = 0x6B;
        static constexpr uint8_t GYRO_CONFIG = 0x1B;
        static constexpr uint8_t ACC_CONFIG = 0x1C;
        static constexpr uint8_t WHO_AM_I = 0x75;
        static constexpr uint8_t DATA = 0x3B;
        static constexpr uint8_t I2C_IF = 0x70;
        static constexpr uint8_t FIFO_EN = 0x23;            // FIFOの設定
        static constexpr uint8_t INT_ENABLE = 0x38;         // 割り込み設定
        static constexpr uint8_t SIGNAL_PATH_RESET = 0x68;  // リセット用
    };

    struct AccelScale {
        static constexpr uint8_t G16 = 0b00011000;
        static constexpr uint8_t G8 = 0b00010000;
        static constexpr uint8_t G4 = 0b00001000;
        static constexpr uint8_t G2 = 0b00000000;
    };

    struct GyroScale {
        static constexpr uint8_t DPS2000 = 0b00011000;
        static constexpr uint8_t DPS1000 = 0b00010000;
        static constexpr uint8_t DPS500 = 0b00001000;
        static constexpr uint8_t DPS250 = 0b00000000;
    };
};

struct Icm20602Data {
    struct Vector3D {
        float x, y, z;  // 変換後の値を格納するためfloatに変更
    };

    Vector3D accel;    // G単位
    Vector3D gyro;     // dps単位
    float accel_norm;  // G単位
};

class Icm20602 {
   private:
    int cs_pin;
    int device_handle_id;
    CreateSpi *create_spi;
    static const char *TAG;

   public:
    // IRAM_ATTR属性の追加
    IRAM_ATTR bool begin(CreateSpi *create_spi, gpio_num_t cs_pin, uint32_t frequency = 8000000);
    IRAM_ATTR uint8_t whoAmI();
    IRAM_ATTR void get(Icm20602Data *data);

    // エラー状態の管理を追加
    bool isInitialized() const {
        return device_handle_id >= 0;
    }

    // スケール設定用のメソッドを追加
    void setAccelScale(uint8_t scale);
    void setGyroScale(uint8_t scale);
};

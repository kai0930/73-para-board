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

// 条件判定用の閾値設定
namespace ConditionConfig {
// 加速度による離床検知の設定
/** 判定に必要な加速度データの数 */
static constexpr int16_t NUMBER_OF_ACCEL_DATA_FOR_LAUNCH = 20;
/** 判定に必要な加速度の二乗和の閾値(G^2) */
static constexpr int16_t ACCEL_SQUARE_SUM_THRESHOLD = 4;

// 気圧による離床検知の設定
/** 判定に必要な気圧データの数 */
static constexpr uint32_t NUMBER_OF_PRESSURE_DATA_FOR_LAUNCH = 5;
/** 判定に必要な気圧の和の閾値(Pa) */
static constexpr uint32_t PRESSURE_AV_THRESHOLD_FOR_LAUNCH = 10;
/** 気圧が減少した回数の閾値 */
static constexpr int8_t PRESSURE_DECREASE_COUNT_THRESHOLD_FOR_LAUNCH = 5;

// 気圧による頂点検知の設定
/** 判定に必要な気圧データの数 */
static constexpr int8_t NUMBER_OF_PRESSURE_DATA_FOR_APOGEE = 5;
/** 判定に必要な気圧の差の閾値(Pa) */
static constexpr int16_t PRESSURE_AV_DIFFERENCE_THRESHOLD_FOR_APOGEE = 0;
/** 気圧が増加した回数の閾値 */
static constexpr int8_t PRESSURE_INCREASE_COUNT_THRESHOLD_FOR_APOGEE = 5;

// タイマーによる頂点検知の設定
/** 離床検知から何秒立ったら頂点とするか(ms) */
static constexpr uint32_t TIME_THRESHOLD_FOR_APOGEE_FROM_LAUNCH = 9000;
}  // namespace ConditionConfig

struct AccelData {
  uint8_t u_x, d_x, u_y, d_y, u_z, d_z;
};

struct GyroData {
  uint8_t u_x, d_x, u_y, d_y, u_z, d_z;
};

struct IcmTempData {
  uint8_t u_t, d_t;
};

struct PressureData {
  uint8_t xl_p, l_p, h_p;
};

struct TempData {
  uint8_t l_t, h_t;
};

struct SensorData {
  uint64_t timestamp_us;
  AccelData accel;
  GyroData gyro;
  PressureData pressure;
  TempData temperature;
};

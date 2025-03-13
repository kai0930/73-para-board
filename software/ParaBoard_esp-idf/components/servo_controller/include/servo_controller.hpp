#pragma once

#include <stdio.h>

#include "config.hpp"
#include "driver/ledc.h"
#include "esp_log.h"

class ServoController {
 public:
  ServoController();
  ~ServoController();

  /**
   * @brief サーボコントローラーを初期化する
   * @param servo_pin サーボ制御用のGPIOピン
   * @return 初期化が成功したかどうか
   */
  bool init(gpio_num_t servo_pin = config::pins.SERVO);

  /**
   * @brief サーボを指定した角度に設定する
   * @param angle_deg 角度（度）
   * @return 設定が成功したかどうか
   */
  bool setAngle(int angle_deg);

  /**
   * @brief サーボを開く（指定した角度に設定する）
   * @param open_angle 開く角度（度）
   * @return 設定が成功したかどうか
   */
  bool openServo(int open_angle);

  /**
   * @brief サーボを閉じる（指定した角度に設定する）
   * @param close_angle 閉じる角度（度）
   * @return 設定が成功したかどうか
   */
  bool closeServo(int close_angle);

 private:
  static constexpr const char* TAG = "SERVO_CONTROLLER";

  // サーボの設定
  static constexpr int MIN_PULSE_WIDTH_US = 500;   // 最小パルス幅（マイクロ秒）
  static constexpr int MAX_PULSE_WIDTH_US = 2500;  // 最大パルス幅（マイクロ秒）
  static constexpr int MIN_ANGLE_DEG = 0;          // 最小角度（度）
  static constexpr int MAX_ANGLE_DEG = 180;        // 最大角度（度）

  // LEDCの設定
  static constexpr ledc_timer_t LEDC_TIMER = LEDC_TIMER_0;
  static constexpr ledc_channel_t LEDC_CHANNEL = LEDC_CHANNEL_0;
  static constexpr ledc_mode_t LEDC_MODE = LEDC_LOW_SPEED_MODE;
  static constexpr uint32_t LEDC_FREQUENCY =
      50;  // 50Hz（サーボモーターの標準）
  static constexpr ledc_timer_bit_t LEDC_DUTY_RES =
      LEDC_TIMER_13_BIT;  // 13ビット分解能

  gpio_num_t servo_pin;
  bool is_initialized = false;

  /**
   * @brief 角度をデューティサイクルに変換する
   * @param angle_deg 角度（度）
   * @return デューティサイクル値
   */
  uint32_t angleToDuty(int angle_deg);
};
#pragma once

#include <stdint.h>

#include "config.hpp"
#include "esp_log.h"

class ConditionChecker {
 public:
  ConditionChecker();
  ~ConditionChecker();

  /**
   * @brief 初期化
   */
  void begin();

  /**
   * @brief 加速度による離床検知
   * @param accel_x X軸加速度
   * @param accel_y Y軸加速度
   * @param accel_z Z軸加速度
   * @return 離床検知したかどうか
   * @note 1kHzで呼び出すことを想定
   */
  bool checkLaunchByAccel(float accel_x, float accel_y, float accel_z);

  /**
   * @brief 気圧による離床検知
   * @param pressure 気圧値
   * @return 離床検知したかどうか
   * @note 25Hzで呼び出すことを想定
   */
  bool checkLaunchByPressure(uint32_t pressure);

  /**
   * @brief 気圧による頂点検知
   * @param pressure 気圧値
   * @return 頂点検知したかどうか
   * @note 離床検知をしていないときはfalseを返す
   * @note 25Hzで呼び出すことを想定
   */
  bool checkApogeeByPressure(uint32_t pressure);

  /**
   * @brief タイマーによる頂点検知
   * @return 頂点検知したかどうか
   * @note 離床検知をしていないときはfalseを返す
   * @note 1kHzで呼び出すことを想定
   */
  bool checkApogeeByTimer();

  /**
   * @brief 離床検知をしているか
   * @return 離床検知をしているかどうか
   */
  bool getIsLaunched() const;

  /**
   * @brief 頂点検知をしているか
   * @return 頂点検知をしているかどうか
   */
  bool getHasReachedApogee() const;

  /**
   * @brief 離床検知時刻を取得
   * @return 離床検知時刻（ミリ秒）
   */
  int64_t getLaunchTime() const;

 private:
  static constexpr const char* TAG = "CONDITION_CHECKER";

  bool is_launched;
  bool has_reached_apogee;

  int64_t launch_time;

  float accel_sum_for_check_launch[3];
  uint8_t pressure_decrease_count_for_check_launch;
  uint8_t pressure_increase_count_for_check_apogee;
  int16_t accel_data_count_for_check_launch;
  uint32_t pressure_sum_for_check_launch;
  uint32_t pressure_sum_for_check_apogee;
  uint32_t last_pressure_av_for_check_launch;
  uint32_t last_pressure_av_for_check_apogee;
  uint32_t pressure_data_count_for_check_launch;
  uint32_t pressure_data_count_for_check_apogee;
};
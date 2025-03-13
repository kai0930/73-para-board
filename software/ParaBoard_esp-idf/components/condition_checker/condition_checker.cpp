#include "condition_checker.hpp"

#include <cmath>

#include "esp_timer.h"

ConditionChecker::ConditionChecker()
    : is_launched(false),
      has_reached_apogee(false),
      launch_time(0),
      pressure_decrease_count_for_check_launch(0),
      pressure_increase_count_for_check_apogee(0),
      accel_data_count_for_check_launch(0),
      pressure_sum_for_check_launch(0),
      pressure_sum_for_check_apogee(0),
      last_pressure_av_for_check_launch(0),
      last_pressure_av_for_check_apogee(0),
      pressure_data_count_for_check_launch(0),
      pressure_data_count_for_check_apogee(0) {
  accel_sum_for_check_launch[0] = 0.0f;
  accel_sum_for_check_launch[1] = 0.0f;
  accel_sum_for_check_launch[2] = 0.0f;
}

ConditionChecker::~ConditionChecker() {}

void ConditionChecker::begin() {
  is_launched = false;
  has_reached_apogee = false;
  launch_time = 0;
  pressure_decrease_count_for_check_launch = 0;
  pressure_increase_count_for_check_apogee = 0;
  accel_sum_for_check_launch[0] = 0.0f;
  accel_sum_for_check_launch[1] = 0.0f;
  accel_sum_for_check_launch[2] = 0.0f;
  accel_data_count_for_check_launch = 0;
  pressure_sum_for_check_launch = 0;
  pressure_data_count_for_check_launch = 0;
  pressure_data_count_for_check_apogee = 0;
  last_pressure_av_for_check_launch = 0;
  last_pressure_av_for_check_apogee = 0;

  ESP_LOGI(TAG, "ConditionChecker initialized");
}

bool ConditionChecker::checkLaunchByAccel(float accel_x, float accel_y,
                                          float accel_z) {
  // 加速度データを蓄積
  accel_sum_for_check_launch[0] += accel_x;
  accel_sum_for_check_launch[1] += accel_y;
  accel_sum_for_check_launch[2] += accel_z;

  if (accel_data_count_for_check_launch ==
      ConditionConfig::NUMBER_OF_ACCEL_DATA_FOR_LAUNCH) {
    // 離床判断
    float accel_av_x = accel_sum_for_check_launch[0] /
                       ConditionConfig::NUMBER_OF_ACCEL_DATA_FOR_LAUNCH;
    float accel_av_y = accel_sum_for_check_launch[1] /
                       ConditionConfig::NUMBER_OF_ACCEL_DATA_FOR_LAUNCH;
    float accel_av_z = accel_sum_for_check_launch[2] /
                       ConditionConfig::NUMBER_OF_ACCEL_DATA_FOR_LAUNCH;

    float accel_av_square_sum = accel_av_x * accel_av_x +
                                accel_av_y * accel_av_y +
                                accel_av_z * accel_av_z;

    ESP_LOGI(TAG, "Accel average square sum: %.4f", accel_av_square_sum);

    if (accel_av_square_sum >= ConditionConfig::ACCEL_SQUARE_SUM_THRESHOLD) {
      is_launched = true;
      // ESP-IDFではmillis()の代わりにesp_timer_get_time()を使用（マイクロ秒単位）
      launch_time = esp_timer_get_time() / 1000;  // マイクロ秒からミリ秒に変換
      ESP_LOGI(TAG, "Launch detected by accelerometer at %lld ms", launch_time);
    }

    // データをリセット
    accel_sum_for_check_launch[0] = 0.0f;
    accel_sum_for_check_launch[1] = 0.0f;
    accel_sum_for_check_launch[2] = 0.0f;
    accel_data_count_for_check_launch = 0;
  }

  accel_data_count_for_check_launch++;
  return is_launched;
}

bool ConditionChecker::checkLaunchByPressure(uint32_t pressure) {
  pressure_sum_for_check_launch += pressure;

  if (pressure_data_count_for_check_launch ==
      ConditionConfig::NUMBER_OF_PRESSURE_DATA_FOR_LAUNCH) {
    uint32_t pressure_av = pressure_sum_for_check_launch /
                           ConditionConfig::NUMBER_OF_PRESSURE_DATA_FOR_LAUNCH;

    // 初回は前回の平均値がないので、現在の平均値を設定して終了
    if (last_pressure_av_for_check_launch == 0) {
      last_pressure_av_for_check_launch = pressure_av;
      pressure_sum_for_check_launch = 0;
      pressure_data_count_for_check_launch = 0;
      return is_launched;
    }

    int32_t pressure_diff = last_pressure_av_for_check_launch - pressure_av;
    ESP_LOGI(TAG, "Pressure diff for launch check: %ld", pressure_diff);

    if (pressure_diff > 0 &&
        pressure_diff > ConditionConfig::PRESSURE_AV_THRESHOLD_FOR_LAUNCH) {
      pressure_decrease_count_for_check_launch++;
      ESP_LOGI(TAG, "Pressure decrease count: %d",
               pressure_decrease_count_for_check_launch);
    } else {
      pressure_decrease_count_for_check_launch = 0;
    }

    last_pressure_av_for_check_launch = pressure_av;

    if (pressure_decrease_count_for_check_launch >=
        ConditionConfig::PRESSURE_DECREASE_COUNT_THRESHOLD_FOR_LAUNCH) {
      is_launched = true;
      launch_time = esp_timer_get_time() / 1000;  // マイクロ秒からミリ秒に変換
      ESP_LOGI(TAG, "Launch detected by pressure sensor at %lld ms",
               launch_time);
    }

    pressure_sum_for_check_launch = 0;
    pressure_data_count_for_check_launch = 0;
  }

  pressure_data_count_for_check_launch++;
  return is_launched;
}

bool ConditionChecker::checkApogeeByPressure(uint32_t pressure) {
  if (!is_launched) {
    return false;
  }

  pressure_sum_for_check_apogee += pressure;

  if (pressure_data_count_for_check_apogee ==
      ConditionConfig::NUMBER_OF_PRESSURE_DATA_FOR_APOGEE) {
    uint32_t pressure_av = pressure_sum_for_check_apogee /
                           ConditionConfig::NUMBER_OF_PRESSURE_DATA_FOR_APOGEE;

    // 初回は前回の平均値がないので、現在の平均値を設定して終了
    if (last_pressure_av_for_check_apogee == 0) {
      last_pressure_av_for_check_apogee = pressure_av;
      pressure_sum_for_check_apogee = 0;
      pressure_data_count_for_check_apogee = 0;
      return has_reached_apogee;
    }

    int32_t pressure_diff = pressure_av - last_pressure_av_for_check_apogee;
    ESP_LOGI(TAG, "Pressure diff for apogee check: %ld", pressure_diff);

    if (pressure_diff > 0 &&
        pressure_diff >
            ConditionConfig::PRESSURE_AV_DIFFERENCE_THRESHOLD_FOR_APOGEE) {
      pressure_increase_count_for_check_apogee++;
      ESP_LOGI(TAG, "Pressure increase count: %d",
               pressure_increase_count_for_check_apogee);
    } else {
      pressure_increase_count_for_check_apogee = 0;
    }

    last_pressure_av_for_check_apogee = pressure_av;

    if (pressure_increase_count_for_check_apogee >=
        ConditionConfig::PRESSURE_INCREASE_COUNT_THRESHOLD_FOR_APOGEE) {
      has_reached_apogee = true;
      ESP_LOGI(TAG, "Apogee detected by pressure sensor at %lld ms",
               esp_timer_get_time() / 1000);
    }

    pressure_sum_for_check_apogee = 0;
    pressure_data_count_for_check_apogee = 0;
  }

  pressure_data_count_for_check_apogee++;
  return has_reached_apogee;
}

bool ConditionChecker::checkApogeeByTimer() {
  if (!is_launched) {
    return false;
  }

  int64_t current_time =
      esp_timer_get_time() / 1000;  // マイクロ秒からミリ秒に変換

  if (current_time - launch_time >=
      ConditionConfig::TIME_THRESHOLD_FOR_APOGEE_FROM_LAUNCH) {
    has_reached_apogee = true;
    ESP_LOGI(TAG, "Apogee detected by timer at %lld ms", current_time);
  }

  return has_reached_apogee;
}

bool ConditionChecker::getIsLaunched() const { return is_launched; }

bool ConditionChecker::getHasReachedApogee() const {
  return has_reached_apogee;
}

int64_t ConditionChecker::getLaunchTime() const { return launch_time; }
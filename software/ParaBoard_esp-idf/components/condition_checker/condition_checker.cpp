#include "condition_checker.hpp"

#include <cmath>

#include "esp_timer.h"

ConditionChecker::ConditionChecker()
    : is_launched(false),
      has_reached_apogee(false),
      launch_time(0),
      accel_data_count_for_check_launch(0),
      accel_increase_count_for_check_launch(0),
      pressure_decrease_count_for_check_launch(0),
      pressure_sum_for_check_launch(0),
      pressure_data_count_for_check_launch(0),
      last_pressure_av_for_check_launch(0),
      pressure_increase_count_for_check_apogee(0),
      pressure_sum_for_check_apogee(0),
      pressure_data_count_for_check_apogee(0),
      last_pressure_av_for_check_apogee(0) {
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
  accel_increase_count_for_check_launch = 0;
  accel_sum_for_check_launch[0] = 0.0f;
  accel_sum_for_check_launch[1] = 0.0f;
  accel_sum_for_check_launch[2] = 0.0f;
  accel_data_count_for_check_launch = 0;
  pressure_sum_for_check_launch = 0;
  pressure_sum_for_check_apogee = 0;
  pressure_data_count_for_check_launch = 0;
  pressure_data_count_for_check_apogee = 0;
  last_pressure_av_for_check_launch = 0;
  last_pressure_av_for_check_apogee = 0;

  ESP_LOGI(TAG, "ConditionChecker initialized");
}

bool ConditionChecker::checkLaunchByAccel(float accel_x, float accel_y,
                                          float accel_z) {
  // すでに離床検知している場合
  if (is_launched) {
    return true;
  }
  // 加速度データを蓄積
  accel_sum_for_check_launch[0] += accel_x;
  accel_sum_for_check_launch[1] += accel_y;
  accel_sum_for_check_launch[2] += accel_z;

  // 既定の回数を取得した場合
  if (accel_data_count_for_check_launch ==
      ConditionConfig::NUMBER_OF_ACCEL_DATA_FOR_LAUNCH) {
    // 各軸の加速度を求める
    float accel_av_x = accel_sum_for_check_launch[0] /
                       ConditionConfig::NUMBER_OF_ACCEL_DATA_FOR_LAUNCH;
    float accel_av_y = accel_sum_for_check_launch[1] /
                       ConditionConfig::NUMBER_OF_ACCEL_DATA_FOR_LAUNCH;
    float accel_av_z = accel_sum_for_check_launch[2] /
                       ConditionConfig::NUMBER_OF_ACCEL_DATA_FOR_LAUNCH;

    ESP_LOGI(TAG, "Waiting for launch: Accel av x: %f, y: %f, z: %f",
             accel_av_x, accel_av_y, accel_av_z);

    // 各軸の加速度の2乗の和を計算
    float accel_av_square_sum = accel_av_x * accel_av_x +
                                accel_av_y * accel_av_y +
                                accel_av_z * accel_av_z;

    // 平均加速度が既定の回数を超えた場合
    if (accel_av_square_sum >= ConditionConfig::ACCEL_SQUARE_SUM_THRESHOLD) {
      // 加速度増加回数カウントを増加
      accel_increase_count_for_check_launch++;
    } else {
      accel_increase_count_for_check_launch = 0;
    }

    // 加速度が増加した回数
    if (accel_increase_count_for_check_launch >
        ConditionConfig::ACCEL_INCREASE_COUNT_THRESHOLD_FOR_LAUNCH) {
      ESP_LOGI(TAG, "Launch detected by accel");
      is_launched = true;
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

bool ConditionChecker::checkLaunchByPressure(float pressure) {
  if (is_launched) {
    return true;
  }
  pressure_sum_for_check_launch += pressure;

  if (pressure_data_count_for_check_launch ==
      ConditionConfig::NUMBER_OF_PRESSURE_DATA_FOR_LAUNCH) {
    float pressure_av = pressure_sum_for_check_launch /
                        ConditionConfig::NUMBER_OF_PRESSURE_DATA_FOR_LAUNCH;

    // 初回は前回の平均値がないので、現在の平均値を設定して終了
    if (last_pressure_av_for_check_launch == 0) {
      last_pressure_av_for_check_launch = pressure_av;
      pressure_sum_for_check_launch = 0;
      pressure_data_count_for_check_launch = 0;
      return is_launched;
    }

    float pressure_diff = last_pressure_av_for_check_launch - pressure_av;

    ESP_LOGI(
        TAG,
        "Waiting for launch: Pressure av: %.2f hPa, Pressure diff: %.2f hPa",
        pressure_av, pressure_diff);

    if (pressure_diff > 0 &&
        pressure_diff > ConditionConfig::PRESSURE_AV_THRESHOLD_FOR_LAUNCH) {
      pressure_decrease_count_for_check_launch++;
    } else {
      pressure_decrease_count_for_check_launch = 0;
    }

    last_pressure_av_for_check_launch = pressure_av;

    if (pressure_decrease_count_for_check_launch >=
        ConditionConfig::PRESSURE_DECREASE_COUNT_THRESHOLD_FOR_LAUNCH) {
      ESP_LOGI(TAG, "Launch detected by pressure");
      is_launched = true;
      launch_time = esp_timer_get_time() / 1000;  // マイクロ秒からミリ秒に変換
    }

    pressure_sum_for_check_launch = 0;
    pressure_data_count_for_check_launch = 0;
  }

  pressure_data_count_for_check_launch++;
  return is_launched;
}

bool ConditionChecker::checkApogeeByPressure(float pressure) {
  if (!is_launched) {
    return false;
  }

  if (has_reached_apogee) {
    return true;
  }

  pressure_sum_for_check_apogee += pressure;

  if (pressure_data_count_for_check_apogee ==
      ConditionConfig::NUMBER_OF_PRESSURE_DATA_FOR_APOGEE) {
    float pressure_av = pressure_sum_for_check_apogee /
                        ConditionConfig::NUMBER_OF_PRESSURE_DATA_FOR_APOGEE;

    // 初回は前回の平均値がないので、現在の平均値を設定して終了
    if (last_pressure_av_for_check_apogee == 0) {
      last_pressure_av_for_check_apogee = pressure_av;
      pressure_sum_for_check_apogee = 0;
      pressure_data_count_for_check_apogee = 0;
      return has_reached_apogee;
    }

    float pressure_diff = pressure_av - last_pressure_av_for_check_apogee;

    ESP_LOGI(TAG,
             "Waiting for apogee: Pressure av: %f hPa, Pressure diff: %f hPa",
             pressure_av, pressure_diff);

    if (pressure_diff > 0 &&
        pressure_diff >
            ConditionConfig::PRESSURE_AV_DIFFERENCE_THRESHOLD_FOR_APOGEE) {
      pressure_increase_count_for_check_apogee++;
    } else {
      pressure_increase_count_for_check_apogee = 0;
    }

    last_pressure_av_for_check_apogee = pressure_av;

    if (pressure_increase_count_for_check_apogee >=
        ConditionConfig::PRESSURE_INCREASE_COUNT_THRESHOLD_FOR_APOGEE) {
      ESP_LOGI(TAG, "Apogee detected by pressure");
      has_reached_apogee = true;
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

  if (has_reached_apogee) {
    return true;
  }

  int64_t current_time =
      esp_timer_get_time() / 1000;  // マイクロ秒からミリ秒に変換

  if (current_time - launch_time >=
      ConditionConfig::TIME_THRESHOLD_FOR_APOGEE_FROM_LAUNCH) {
    ESP_LOGI(TAG, "Apogee reached by timer");
    has_reached_apogee = true;
  }

  return has_reached_apogee;
}

bool ConditionChecker::getIsLaunched() const { return is_launched; }

bool ConditionChecker::getHasReachedApogee() const {
  return has_reached_apogee;
}

int64_t ConditionChecker::getLaunchTime() const { return launch_time; }
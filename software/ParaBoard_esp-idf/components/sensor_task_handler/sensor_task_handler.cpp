#include "sensor_task_handler.hpp"

#include <stdio.h>

SensorTaskHandler::SensorTaskHandler()
    : sensor_task_handle(nullptr),
      icm(nullptr),
      lps(nullptr),
      log_handler(nullptr) {}

SensorTaskHandler::~SensorTaskHandler() {
  // タスクの停止
  stopTask();
}

bool SensorTaskHandler::init(Icm::Icm42688* icm_ptr, Lps::Lps25hb* lps_ptr,
                             LogTaskHandler* log_handler_ptr) {
  if (icm_ptr == nullptr) {
    ESP_LOGE(TAG, "ICM pointer is null");
    return false;
  }

  if (lps_ptr == nullptr) {
    ESP_LOGE(TAG, "LPS pointer is null");
    return false;
  }

  if (log_handler_ptr == nullptr) {
    ESP_LOGE(TAG, "Log handler pointer is null");
    return false;
  }

  icm = icm_ptr;
  lps = lps_ptr;
  log_handler = log_handler_ptr;

  // ConditionCheckerの初期化
  condition_checker.begin();
  ESP_LOGI(TAG, "ConditionChecker initialized");

  return true;
}

void SensorTaskHandler::startTask() {
  if (sensor_task_handle != nullptr) {
    ESP_LOGW(TAG, "Sensor task already running");
    return;
  }

  // タスクの作成
  BaseType_t result =
      xTaskCreate(sensorTask, "sensor_task", TASK_STACK_SIZE,
                  this,  // 自身のインスタンスをパラメータとして渡す
                  TASK_PRIORITY, &sensor_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create sensor task");
    return;
  }

  // 初期状態では停止状態にする
  vTaskSuspend(sensor_task_handle);

  ESP_LOGI(TAG, "Sensor task created (suspended)");
}

void SensorTaskHandler::stopTask() {
  if (sensor_task_handle == nullptr) {
    return;
  }

  // タスクの削除
  vTaskDelete(sensor_task_handle);
  sensor_task_handle = nullptr;
  ESP_LOGI(TAG, "Sensor task stopped");
}

bool SensorTaskHandler::notifyFromISR() {
  if (sensor_task_handle == nullptr) {
    return false;
  }

  BaseType_t taskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(sensor_task_handle, &taskWoken);
  return (taskWoken == pdTRUE);
}

void SensorTaskHandler::sensorTask(void* pvParameters) {
  SensorTaskHandler* self = static_cast<SensorTaskHandler*>(pvParameters);

  if (self == nullptr || self->icm == nullptr || self->lps == nullptr ||
      self->log_handler == nullptr) {
    ESP_LOGE("SENSOR_TASK", "Invalid parameters");
    vTaskDelete(nullptr);
    return;
  }

  int32_t count = 0;
  AccelData accel;
  GyroData gyro;
  PressureData pressure;
  TempData temperature;

  while (true) {
    // タイマー割り込みからの通知を待つ
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // センサーからデータを取得する
    // ICMは1kHz（タイマーも1kHz）でデータを取得する
    self->icm->getAccelAndGyro(&accel, &gyro);

    // 加速度データを使用して離床検知
    float accel_x =
        (accel.u_x << 8 | accel.d_x) / 32768.0f * 16.0f;  // ±16gレンジを仮定
    float accel_y = (accel.u_y << 8 | accel.d_y) / 32768.0f * 16.0f;
    float accel_z = (accel.u_z << 8 | accel.d_z) / 32768.0f * 16.0f;
    self->condition_checker.checkLaunchByAccel(accel_x, accel_y, accel_z);

    // タイマーによる頂点検知
    self->condition_checker.checkApogeeByTimer();

    // LPSは25Hzでデータを取得する（40回に1回）
    if (count % SensorTaskHandler::LPS_SAMPLE_DIVIDER == 0) {
      self->lps->getPressureAndTemp(&pressure, &temperature);

      // 気圧データを使用して離床検知と頂点検知
      uint32_t pressure_value =
          (pressure.h_p << 16) | (pressure.l_p << 8) | pressure.xl_p;
      self->condition_checker.checkLaunchByPressure(pressure_value);
      self->condition_checker.checkApogeeByPressure(pressure_value);
    }

    // ログタスクにデータを送信する
    SensorData data;
    data.timestamp_us = esp_timer_get_time();
    data.accel = accel;
    data.gyro = gyro;
    data.pressure = pressure;
    data.temperature = temperature;
    self->log_handler->sendToQueue(data);

    count++;
  }
}

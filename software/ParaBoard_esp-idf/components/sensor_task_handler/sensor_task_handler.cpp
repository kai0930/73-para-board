#include "sensor_task_handler.hpp"

#include <stdio.h>

SensorTaskHandler::SensorTaskHandler()
    : sensor_task_handle(nullptr),
      icm(nullptr),
      lps(nullptr),
      log_handler(nullptr),
      servo(nullptr),
      sd_controller(nullptr) {}

SensorTaskHandler::~SensorTaskHandler() {
  // タスクの停止
  stopTask();
}

bool SensorTaskHandler::init(Icm::Icm42688* icm_ptr, Lps::Lps25hb* lps_ptr,
                             LogTaskHandler* log_handler_ptr,
                             ServoController* servo_ptr,
                             SdController* sd_controller_ptr) {
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

  if (servo_ptr == nullptr) {
    ESP_LOGE(TAG, "Servo controller pointer is null");
    return false;
  }

  if (sd_controller_ptr == nullptr) {
    ESP_LOGE(TAG, "SD controller pointer is null");
    return false;
  }

  icm = icm_ptr;
  lps = lps_ptr;
  log_handler = log_handler_ptr;
  servo = servo_ptr;
  sd_controller = sd_controller_ptr;

  // ConditionCheckerの初期化
  condition_checker = new ConditionChecker();
  condition_checker->begin();

  is_servo_open = false;
  ESP_LOGI(TAG, "ConditionChecker initialized");

  return true;
}

void SensorTaskHandler::startTask() {
  // タスクが既に存在する場合は、状態をチェックして適切に処理
  if (sensor_task_handle != nullptr) {
    eTaskState task_state = eTaskGetState(sensor_task_handle);

    if (task_state == eSuspended) {
      // タスクが一時停止状態なら再開
      ESP_LOGI(TAG, "Resuming existing sensor task");
      vTaskResume(sensor_task_handle);
      return;
    } else if (task_state != eDeleted && task_state != eInvalid) {
      // タスクが実行中または準備完了状態なら何もしない
      ESP_LOGW(TAG, "Sensor task already running or ready");
      return;
    } else {
      // タスクが削除されているか無効な状態なら、ハンドルをクリア
      ESP_LOGW(TAG, "Sensor task handle is invalid, recreating task");
      sensor_task_handle = nullptr;
    }
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

  // タスクの状態をチェック
  eTaskState task_state = eTaskGetState(sensor_task_handle);

  if (task_state != eDeleted && task_state != eInvalid) {
    // タスクが有効な状態なら削除
    ESP_LOGI(TAG, "Deleting sensor task");
    vTaskDelete(sensor_task_handle);
  } else {
    ESP_LOGW(TAG, "Sensor task is already deleted or invalid");
  }

  sensor_task_handle = nullptr;
  ESP_LOGI(TAG, "Sensor task stopped");
}

bool SensorTaskHandler::notifyFromISR() {
  if (sensor_task_handle == nullptr) {
    return false;
  }

  // タスクの状態をチェック（ISR内では最小限のチェックのみ）
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // タスクに通知を送る
  vTaskNotifyGiveFromISR(sensor_task_handle, &xHigherPriorityTaskWoken);

  // 高優先度タスクの切り替えが必要かどうかを返す
  return (xHigherPriorityTaskWoken == pdTRUE);
}

void SensorTaskHandler::sensorTask(void* pvParameters) {
  SensorTaskHandler* self = static_cast<SensorTaskHandler*>(pvParameters);

  // condition_checkerの初期化
  self->condition_checker->begin();

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
    float accel_x = (int16_t)(accel.u_x << 8 | accel.d_x) / 32768.0f *
                    16.0f;  // ±16gレンジを仮定
    float accel_y = (int16_t)(accel.u_y << 8 | accel.d_y) / 32768.0f * 16.0f;
    float accel_z = (int16_t)(accel.u_z << 8 | accel.d_z) / 32768.0f * 16.0f;
    self->condition_checker->checkLaunchByAccel(accel_x, accel_y, accel_z);

    // タイマーによる頂点検知
    self->condition_checker->checkApogeeByTimer();

    // LPSは25Hzでデータを取得する（40回に1回）
    if (count % SensorTaskHandler::LPS_SAMPLE_DIVIDER == 0) {
      self->lps->getPressureAndTemp(&pressure, &temperature);

      // 気圧データを使用して離床検知と頂点検知
      float pressure_value =
          (pressure.h_p << 16) | (pressure.l_p << 8) | pressure.xl_p;
      self->condition_checker->checkLaunchByPressure(pressure_value / 4096.0f);
      self->condition_checker->checkApogeeByPressure(pressure_value / 4096.0f);
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

    // サーボの制御
    if (self->is_servo_open == false &&
        self->condition_checker->getHasReachedApogee()) {
      int open_angle = self->sd_controller->getIntSetting("open-angle", 10);
      self->servo->openServo(open_angle);
      self->is_servo_open = true;
    } else if (self->is_servo_open == true &&
               !self->condition_checker->getHasReachedApogee()) {
      int close_angle = self->sd_controller->getIntSetting("close-angle", 10);
      self->servo->closeServo(close_angle);
      self->is_servo_open = false;
    }
  }
}

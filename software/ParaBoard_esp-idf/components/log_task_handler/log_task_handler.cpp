#include "log_task_handler.hpp"

LogTaskHandler::LogTaskHandler() {
  // キューの作成
  log_queue = xQueueCreate(QUEUE_SIZE, sizeof(SensorData));
  if (log_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create log queue");
  }
}

LogTaskHandler::~LogTaskHandler() {
  // タスクの停止
  stopTask();

  // キューの削除
  if (log_queue != nullptr) {
    vQueueDelete(log_queue);
    log_queue = nullptr;
  }
}

bool LogTaskHandler::init(SdController* logger_ptr) {
  if (logger_ptr == nullptr) {
    ESP_LOGE(TAG, "Logger pointer is null");
    return false;
  }

  logger = logger_ptr;
  return true;
}

void LogTaskHandler::startTask() {
  if (log_task_handle != nullptr) {
    ESP_LOGW(TAG, "Log task already running");
    return;
  }

  // タスクの作成
  BaseType_t result =
      xTaskCreate(logTask, "log_task", TASK_STACK_SIZE,
                  this,  // 自身のインスタンスをパラメータとして渡す
                  TASK_PRIORITY, &log_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create log task");
    return;
  }

  ESP_LOGI(TAG, "Log task started");
}

void LogTaskHandler::stopTask() {
  if (log_task_handle == nullptr) {
    return;
  }

  // タスクの削除
  vTaskDelete(log_task_handle);
  log_task_handle = nullptr;
  ESP_LOGI(TAG, "Log task stopped");
}

bool LogTaskHandler::sendToQueue(const SensorData& data) {
  if (log_queue == nullptr) {
    ESP_LOGE(TAG, "Log queue is null");
    return false;
  }

  // キューにデータを送信
  BaseType_t result = xQueueSend(log_queue, &data, 0);
  return (result == pdPASS);
}

void LogTaskHandler::logTask(void* pvParameters) {
  LogTaskHandler* self = static_cast<LogTaskHandler*>(pvParameters);

  if (self == nullptr || self->logger == nullptr ||
      self->log_queue == nullptr) {
    ESP_LOGE("LOG_TASK", "Invalid parameters");
    vTaskDelete(nullptr);
    return;
  }

  int16_t count = 0;

  while (true) {
    // log_queueからデータを取得する
    SensorData data;
    if (xQueueReceive(self->log_queue, &data, portMAX_DELAY) != pdPASS) {
      ESP_LOGE("LOG_TASK", "Failed to receive data from queue");
      continue;
    }

    // ログを書き込む
    self->logger->writeLog(data);

    // 一定回数ごとにフラッシュする
    count++;
    if (count % self->flush_count == 0) {
      self->logger->flush();
      count = 0;
    }

    // // 一定回数でログを出力
    // if (count % 100 == 0) {
    //   // 加速度・角速度・気圧をログに変換してから表示
    //   float accel_x =
    //       ((int16_t)(data.accel.u_x << 8 | data.accel.d_x)) / 32768.0f
    //       * 16.0f;
    //   float accel_y =
    //       ((int16_t)(data.accel.u_y << 8 | data.accel.d_y)) / 32768.0f
    //       * 16.0f;
    //   float accel_z =
    //       ((int16_t)(data.accel.u_z << 8 | data.accel.d_z)) / 32768.0f
    //       * 16.0f;
    //
    //   float gyro_x =
    //       ((int16_t)(data.gyro.u_x << 8 | data.gyro.d_x)) / 32768.0f *
    //       2000.0f;
    //   float gyro_y =
    //       ((int16_t)(data.gyro.u_y << 8 | data.gyro.d_y)) / 32768.0f *
    //       2000.0f;
    //   float gyro_z =
    //       ((int16_t)(data.gyro.u_z << 8 | data.gyro.d_z)) / 32768.0f *
    //       2000.0f;
    //
    //   uint32_t pressure_value = (data.pressure.h_p << 16 |
    //                              data.pressure.l_p << 8 | data.pressure.xl_p)
    //                              /
    //                             4096.0f;
    //
    //   ESP_LOGI("LOG_TASK",
    //            "Time: %lld us, Accel: [%.2f, %.2f, %.2f] m/s^2, Gyro: [%.2f,
    //            "
    //            "%.2f, %.2f] dps, Pressure: %lu hPa",
    //            data.timestamp_us, accel_x, accel_y, accel_z, gyro_x, gyro_y,
    //            gyro_z, pressure_value);
    // }
  }
}

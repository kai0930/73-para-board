#pragma once

#include <stdio.h>

#include "config.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sd_controller.hpp"

class LogTaskHandler {
 public:
  LogTaskHandler();
  ~LogTaskHandler();

  /**
   * @brief ログタスクハンドラを初期化する
   * @param logger SDカードコントローラへのポインタ
   * @return 初期化が成功したかどうか
   */
  bool init(SdController* logger);

  /**
   * @brief ログタスクを開始する
   */
  void startTask();

  /**
   * @brief ログタスクを停止する
   */
  void stopTask();

  /**
   * @brief ログキューにデータを送信する
   * @param data センサーデータ
   * @return 送信が成功したかどうか
   */
  bool sendToQueue(const SensorData& data);

  /**
   * @brief ログタスクハンドルを取得する
   * @return ログタスクハンドル
   */
  TaskHandle_t getTaskHandle() const { return log_task_handle; }

  /**
   * @brief ログキューを取得する
   * @return ログキュー
   */
  QueueHandle_t getQueue() const { return log_queue; }

 private:
  static constexpr const char* TAG = "LOG_TASK_HANDLER";
  static constexpr int QUEUE_SIZE = 10;
  static constexpr int TASK_STACK_SIZE = 4096;
  static constexpr int TASK_PRIORITY = 5;
  static constexpr int DEFAULT_FLUSH_COUNT = 40;

  TaskHandle_t log_task_handle = nullptr;
  QueueHandle_t log_queue = nullptr;
  SdController* logger = nullptr;
  int flush_count = DEFAULT_FLUSH_COUNT;

  /**
   * @brief ログタスク関数
   * @param pvParameters タスクパラメータ
   */
  static void logTask(void* pvParameters);
};

#pragma once

#include <stdio.h>

#include "condition_checker.hpp"
#include "config.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "icm42688.hpp"
#include "log_task_handler.hpp"
#include "lps25hb.hpp"
#include "sd_controller.hpp"
#include "servo_controller.hpp"

class SensorTaskHandler {
 public:
  SensorTaskHandler();
  ~SensorTaskHandler();

  /**
   * @brief センサータスクハンドラを初期化する
   * @param icm ICMセンサーへのポインタ
   * @param lps LPSセンサーへのポインタ
   * @param log_handler ログタスクハンドラへのポインタ
   * @param servo サーボコントローラへのポインタ
   * @param sd_controller SDカードコントローラへのポインタ
   * @return 初期化が成功したかどうか
   */
  bool init(Icm::Icm42688* icm, Lps::Lps25hb* lps, LogTaskHandler* log_handler,
            ServoController* servo, SdController* sd_controller);

  /**
   * @brief センサータスクを開始する
   */
  void startTask();

  /**
   * @brief センサータスクを停止する
   */
  void stopTask();

  /**
   * @brief タイマー割り込みからタスクに通知を送る
   * @return 高優先度タスクの切り替えが必要かどうか
   */
  bool notifyFromISR();

  /**
   * @brief センサータスクハンドルを取得する
   * @return センサータスクハンドル
   */
  TaskHandle_t getTaskHandle() const { return sensor_task_handle; }

  /**
   * @brief 離床検知状態を取得する
   * @return 離床検知状態
   */
  bool getIsLaunched() const { return condition_checker->getIsLaunched(); }

  /**
   * @brief 頂点検知状態を取得する
   * @return 頂点検知状態
   */
  bool getHasReachedApogee() const {
    return condition_checker->getHasReachedApogee();
  }

  /**
   * @brief 離床検知時刻を取得する
   * @return 離床検知時刻（ミリ秒）
   */
  uint32_t getLaunchTime() const { return condition_checker->getLaunchTime(); }

 private:
  static constexpr const char* TAG = "SENSOR_TASK_HANDLER";
  static constexpr int TASK_STACK_SIZE = 4096;
  static constexpr int TASK_PRIORITY = 5;
  static constexpr int LPS_SAMPLE_DIVIDER =
      40;  // LPSは25Hzでサンプリング（ICMの1kHzの1/40）
  bool is_servo_open = false;

  TaskHandle_t sensor_task_handle = nullptr;
  Icm::Icm42688* icm = nullptr;
  Lps::Lps25hb* lps = nullptr;
  LogTaskHandler* log_handler = nullptr;
  ServoController* servo = nullptr;
  SdController* sd_controller = nullptr;
  ConditionChecker* condition_checker = nullptr;

  /**
   * @brief センサータスク関数
   * @param pvParameters タスクパラメータ
   */
  static void sensorTask(void* pvParameters);
};

#pragma once

#include <stdio.h>

#include "CanComm.hpp"
#include "config.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "log_task_handler.hpp"
#include "mode_manager.hpp"
#include "sd_controller.hpp"
#include "sensor_task_handler.hpp"
#include "servo_controller.hpp"

class CommandHandler {
 public:
  CommandHandler();
  ~CommandHandler();

  /**
   * @brief コマンドハンドラを初期化する
   * @param can_comm CANコミュニケーションへのポインタ
   * @param logger SDカードコントローラへのポインタ
   * @param sensor_handler センサータスクハンドラへのポインタ
   * @param log_handler ログタスクハンドラへのポインタ
   * @param servo_controller サーボコントローラーへのポインタ
   * @return 初期化が成功したかどうか
   */
  bool init(CanComm* can_comm, SdController* logger,
            SensorTaskHandler* sensor_handler, LogTaskHandler* log_handler,
            ServoController* servo_controller);

  /**
   * @brief コマンド受信タスクを開始する
   */
  void startTask();

  /**
   * @brief コマンド受信タスクを停止する
   */
  void stopTask();

  /**
   * @brief モードマネージャーを取得する
   * @return モードマネージャーへのポインタ
   */
  ModeManager* getModeManager() const { return mode_manager; }

  /**
   * @brief コマンド受信タスクハンドルを取得する
   * @return コマンド受信タスクハンドル
   */
  TaskHandle_t getTaskHandle() const { return command_task_handle; }

 private:
  static constexpr const char* TAG = "COMMAND_HANDLER";
  static constexpr int TASK_STACK_SIZE = 4096;
  static constexpr int TASK_PRIORITY = 5;
  static constexpr int UART_DELAY_MS = 10;

  TaskHandle_t command_task_handle = nullptr;
  CanComm* can_comm = nullptr;
  SdController* logger = nullptr;
  SensorTaskHandler* sensor_handler = nullptr;
  LogTaskHandler* log_handler = nullptr;
  ServoController* servo_controller = nullptr;
  ModeManager* mode_manager = nullptr;

  /**
   * @brief モードマネージャーを設定する
   */
  void setupModeManager();

  /**
   * @brief CANからのコマンドを処理する
   * @param frame 受信したCANフレーム
   */
  void processCanCommand(const CanRxFrame& frame);

  /**
   * @brief UARTからのコマンドを処理する
   * @param cmd_uart 受信したUARTコマンド
   */
  void processUartCommand(int cmd_uart);

  /**
   * @brief サーボコマンドを処理する
   * @param servo_command サーボコマンド
   */
  void processServoCommand(ServoCommand servo_command);

  /**
   * @brief コマンド受信タスク関数
   * @param pvParameters タスクパラメータ
   */
  static void commandTask(void* pvParameters);
};
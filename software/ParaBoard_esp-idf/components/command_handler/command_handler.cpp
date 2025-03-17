#include "command_handler.hpp"

CommandHandler::CommandHandler()
    : command_task_handle(nullptr),
      can_comm(nullptr),
      logger(nullptr),
      sensor_handler(nullptr),
      log_handler(nullptr),
      servo_controller(nullptr),
      led_controller(nullptr),
      mode_manager(nullptr) {}

CommandHandler::~CommandHandler() {
  // タスクの停止
  stopTask();

  // モードマネージャーの解放
  if (mode_manager != nullptr) {
    delete mode_manager;
    mode_manager = nullptr;
  }
}

bool CommandHandler::init(CanComm* can_comm_ptr, SdController* logger_ptr,
                          SensorTaskHandler* sensor_handler_ptr,
                          LogTaskHandler* log_handler_ptr,
                          ServoController* servo_controller_ptr,
                          LedController* led_controller_ptr) {
  // UARTモードの場合はcan_commがnullptrでも許容する
  if (can_comm_ptr == nullptr) {
    ESP_LOGW(
        TAG,
        "CAN communication pointer is null, will operate in UART mode only");
  }

  if (logger_ptr == nullptr) {
    ESP_LOGE(TAG, "Logger pointer is null");
    return false;
  }

  if (sensor_handler_ptr == nullptr) {
    ESP_LOGE(TAG, "Sensor handler pointer is null");
    return false;
  }

  if (log_handler_ptr == nullptr) {
    ESP_LOGE(TAG, "Log handler pointer is null");
    return false;
  }

  if (servo_controller_ptr == nullptr) {
    ESP_LOGE(TAG, "Servo controller pointer is null");
    return false;
  }

  if (led_controller_ptr == nullptr) {
    ESP_LOGE(TAG, "LED controller pointer is null");
    return false;
  }

  can_comm = can_comm_ptr;
  logger = logger_ptr;
  sensor_handler = sensor_handler_ptr;
  log_handler = log_handler_ptr;
  servo_controller = servo_controller_ptr;
  led_controller = led_controller_ptr;

  // SDカードから通信モードを読み込む
  std::string comm_mode_str = logger->getStringSetting("comm_mode", "can");
  if (comm_mode_str == "uart" || can_comm == nullptr) {
    comm_mode = CommMode::UART;
    ESP_LOGI(TAG, "Communication mode set to UART");
  } else {
    comm_mode = CommMode::CAN;
    ESP_LOGI(TAG, "Communication mode set to CAN");
  }

  // モードマネージャーの設定
  setupModeManager();

  return true;
}

void CommandHandler::setupModeManager() {
  mode_manager = new ModeManager();

  // STARTモードの設定
  mode_manager->setupMode(
      ModeCommand::START,
      [this](ModeCommand previous_mode, ModeCommand next_mode) {
        ESP_LOGI(TAG, "Changing to START mode");

        // ログバッファをフラッシュして、ファイル操作の競合を減らす
        if (log_handler != nullptr && logger != nullptr) {
          logger->flush();
        }

        // STARTモードに移行したらis_logging_modeをfalseに設定
        logger->setBoolSetting("is_logging_mode", false);
        ESP_LOGI(TAG, "is_logging_mode set to false");

        // 設定の保存を試みる（改善版）
        int retry_count = 0;
        const int max_retries = 3;
        bool save_success = false;

        while (retry_count < max_retries && !save_success) {
          // 少し待機してから再試行
          if (retry_count > 0) {
            ESP_LOGI(TAG, "Retrying settings save (attempt %d/%d)...",
                     retry_count + 1, max_retries);
            vTaskDelay(
                pdMS_TO_TICKS(500 * retry_count));  // 待機時間を徐々に増やす
          }

          save_success = logger->saveSettings();

          if (!save_success) {
            ESP_LOGW(TAG, "Failed to save settings (attempt %d/%d)",
                     retry_count + 1, max_retries);
            retry_count++;
          }
        }

        if (save_success) {
          ESP_LOGI(TAG, "Settings saved to SD card successfully");
        } else {
          ESP_LOGE(
              TAG,
              "Failed to save settings after %d attempts, continuing anyway",
              max_retries);
        }

        // STARTモードのLED点滅パターンを設定
        if (led_controller->getBlinkTaskHandle() == nullptr) {
          // タスクが存在しない場合は新規作成
          led_controller->startBlinking(START_MODE_LED_ON_TIME_MS,
                                        START_MODE_LED_OFF_TIME_MS);
        } else {
          // タスクが存在する場合はパターンのみ変更
          led_controller->changeBlinkPattern(START_MODE_LED_ON_TIME_MS,
                                             START_MODE_LED_OFF_TIME_MS);
        }

        printf("Mode changed to %s\n",
               ModeManager::getModeString(next_mode).c_str());
      },
      [](ModeCommand previous_mode, ModeCommand next_mode) {
        printf("Mode changed from %s\n",
               ModeManager::getModeString(previous_mode).c_str());
      });

  // LOGGINGモードの設定
  mode_manager->setupMode(
      ModeCommand::LOGGING,
      [this](ModeCommand previous_mode, ModeCommand next_mode) {
        ESP_LOGI(TAG, "Changing to LOGGING mode");

        // ログバッファをフラッシュして、ファイル操作の競合を減らす
        if (log_handler != nullptr && logger != nullptr) {
          logger->flush();
        }

        // LOGGINGモードに移行したらis_logging_modeをtrueに設定
        logger->setBoolSetting("is_logging_mode", true);
        ESP_LOGI(TAG, "is_logging_mode set to true");

        // 設定の保存を試みる（改善版）
        int retry_count = 0;
        const int max_retries = 3;
        bool save_success = false;

        while (retry_count < max_retries && !save_success) {
          // 少し待機してから再試行
          if (retry_count > 0) {
            ESP_LOGI(TAG, "Retrying settings save (attempt %d/%d)...",
                     retry_count + 1, max_retries);
            vTaskDelay(
                pdMS_TO_TICKS(500 * retry_count));  // 待機時間を徐々に増やす
          }

          save_success = logger->saveSettings();

          if (!save_success) {
            ESP_LOGW(TAG, "Failed to save settings (attempt %d/%d)",
                     retry_count + 1, max_retries);
            retry_count++;
          }
        }

        if (save_success) {
          ESP_LOGI(TAG, "Settings saved to SD card successfully");
        } else {
          ESP_LOGE(
              TAG,
              "Failed to save settings after %d attempts, continuing anyway",
              max_retries);
        }

        // センサータスクとログタスクを開始
        if (sensor_handler->getTaskHandle() != nullptr) {
          // タスクの状態をチェック
          eTaskState task_state =
              eTaskGetState(sensor_handler->getTaskHandle());
          if (task_state == eSuspended) {
            ESP_LOGI(TAG, "Resuming sensor task");
            vTaskResume(sensor_handler->getTaskHandle());
          } else {
            ESP_LOGW(TAG, "Sensor task is not suspended, current state: %d",
                     task_state);
            // タスクが停止していない場合は再起動を試みる
            if (task_state == eDeleted || task_state == eInvalid) {
              ESP_LOGI(TAG, "Restarting sensor task");
              sensor_handler->startTask();
            }
          }
        } else {
          ESP_LOGI(TAG, "Starting sensor task");
          sensor_handler->startTask();
        }

        log_handler->startTask();

        // LOGGINGモードのLED点滅パターンを設定
        if (led_controller->getBlinkTaskHandle() == nullptr) {
          // タスクが存在しない場合は新規作成
          led_controller->startBlinking(LOGGING_MODE_LED_ON_TIME_MS,
                                        LOGGING_MODE_LED_OFF_TIME_MS);
        } else {
          // タスクが存在する場合はパターンのみ変更
          led_controller->changeBlinkPattern(LOGGING_MODE_LED_ON_TIME_MS,
                                             LOGGING_MODE_LED_OFF_TIME_MS);
        }
      },
      [this](ModeCommand previous_mode, ModeCommand next_mode) {
        // センサータスクとログタスクを停止
        if (sensor_handler->getTaskHandle() != nullptr) {
          // タスクの状態をチェック
          eTaskState task_state =
              eTaskGetState(sensor_handler->getTaskHandle());
          if (task_state != eSuspended) {
            ESP_LOGI(TAG, "Suspending sensor task");
            vTaskSuspend(sensor_handler->getTaskHandle());
          } else {
            ESP_LOGW(TAG, "Sensor task is already suspended");
          }
        }
        log_handler->stopTask();
        printf("Mode changed from %s\n",
               ModeManager::getModeString(previous_mode).c_str());
      });

  // モードマネージャーの初期化
  mode_manager->begin();

  // 初期モードに応じたLED点滅パターンを設定（初期化時は新しいタスクを作成）
  if (mode_manager->getMode() == ModeCommand::START) {
    led_controller->startBlinking(START_MODE_LED_ON_TIME_MS,
                                  START_MODE_LED_OFF_TIME_MS);
  } else if (mode_manager->getMode() == ModeCommand::LOGGING) {
    led_controller->startBlinking(LOGGING_MODE_LED_ON_TIME_MS,
                                  LOGGING_MODE_LED_OFF_TIME_MS);
  }
}

void CommandHandler::startTask() {
  if (command_task_handle != nullptr) {
    ESP_LOGW(TAG, "Command task already running");
    return;
  }

  // タスクの作成
  BaseType_t result =
      xTaskCreate(commandTask, "command_task", TASK_STACK_SIZE,
                  this,  // 自身のインスタンスをパラメータとして渡す
                  TASK_PRIORITY, &command_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create command task");
    return;
  }

  ESP_LOGI(TAG, "Command task started");
}

void CommandHandler::stopTask() {
  if (command_task_handle == nullptr) {
    return;
  }

  // タスクの削除
  vTaskDelete(command_task_handle);
  command_task_handle = nullptr;
  ESP_LOGI(TAG, "Command task stopped");
}

void CommandHandler::processCanCommand(const CanRxFrame& frame) {
  // モード遷移コマンドの処理
  if (frame.content_id == ContentID::MODE_TRANSITION) {
    ModeCommand new_mode = static_cast<ModeCommand>(frame.data[0]);
    // 現在のモードと異なる場合のみモード変更を行う
    if (new_mode != mode_manager->getMode()) {
      mode_manager->changeMode(new_mode);
    }
  }

  // 開放コマンドの処理
  if (frame.content_id == ContentID::KAIHOU_COMMAND) {
    ServoCommand servo_command = static_cast<ServoCommand>(frame.data[0]);
    processServoCommand(servo_command);
  }
}

void CommandHandler::processUartCommand(int cmd_uart) {
  // モード遷移コマンドの処理
  if (cmd_uart == static_cast<int>(ModeCommand::START)) {
    // 現在のモードと異なる場合のみモード変更を行う
    if (mode_manager->getMode() != ModeCommand::START) {
      mode_manager->changeMode(ModeCommand::START);
    } else {
      printf("Already in START mode\n");
    }
  } else if (cmd_uart == static_cast<int>(ModeCommand::LOGGING)) {
    // 現在のモードと異なる場合のみモード変更を行う
    if (mode_manager->getMode() != ModeCommand::LOGGING) {
      mode_manager->changeMode(ModeCommand::LOGGING);
    } else {
      printf("Already in LOGGING mode\n");
    }
  }
  // サーボコマンドの処理
  else if (cmd_uart == static_cast<int>(ServoCommand::OPEN_SERVO) ||
           cmd_uart == static_cast<int>(ServoCommand::CLOSE_SERVO) ||
           cmd_uart == static_cast<int>(ServoCommand::OPEN_ANGLE_MINUS_1) ||
           cmd_uart == static_cast<int>(ServoCommand::OPEN_ANGLE_PLUS_1) ||
           cmd_uart == static_cast<int>(ServoCommand::CLOSE_ANGLE_PLUS) ||
           cmd_uart == static_cast<int>(ServoCommand::CLOSE_ANGLE_MINUS)) {
    processServoCommand(static_cast<ServoCommand>(cmd_uart));
  }
  // 離床検知と頂点検知の状態確認コマンド
  else if (cmd_uart == 'L' || cmd_uart == 'l') {
    // 離床検知状態の確認
    bool is_launched = sensor_handler->getIsLaunched();
    printf("Launch detection: %s\n", is_launched ? "DETECTED" : "NOT DETECTED");
    if (is_launched) {
      int64_t launch_time = sensor_handler->getLaunchTime();
      int64_t current_time =
          esp_timer_get_time() / 1000;  // マイクロ秒からミリ秒に変換
      printf("Launch time: %lld ms (elapsed: %lld ms)\n", launch_time,
             current_time - launch_time);
    }
  } else if (cmd_uart == 'A' || cmd_uart == 'a') {
    // 頂点検知状態の確認
    bool has_reached_apogee = sensor_handler->getHasReachedApogee();
    printf("Apogee detection: %s\n",
           has_reached_apogee ? "DETECTED" : "NOT DETECTED");
    if (has_reached_apogee) {
      int64_t launch_time = sensor_handler->getLaunchTime();
      int64_t current_time =
          esp_timer_get_time() / 1000;  // マイクロ秒からミリ秒に変換
      printf("Time since launch: %lld ms\n", current_time - launch_time);
    }
  } else if (cmd_uart == 'S' || cmd_uart == 's') {
    // センサー状態の概要表示
    bool is_launched = sensor_handler->getIsLaunched();
    bool has_reached_apogee = sensor_handler->getHasReachedApogee();
    printf("Sensor status:\n");
    printf("- Launch detection: %s\n",
           is_launched ? "DETECTED" : "NOT DETECTED");
    printf("- Apogee detection: %s\n",
           has_reached_apogee ? "DETECTED" : "NOT DETECTED");
    if (is_launched) {
      int64_t launch_time = sensor_handler->getLaunchTime();
      int64_t current_time =
          esp_timer_get_time() / 1000;  // マイクロ秒からミリ秒に変換
      printf("- Launch time: %lld ms (elapsed: %lld ms)\n", launch_time,
             current_time - launch_time);
    }
  }
}

void CommandHandler::processServoCommand(ServoCommand servo_command) {
  // STARTモードの時のみサーボコマンドを実行する
  if (mode_manager->getMode() != ModeCommand::START) {
    ESP_LOGW(TAG, "Servo command ignored: Not in START mode");
    printf("Servo commands are only available in START mode\n");
    return;
  }

  int open_angle = logger->getIntSetting("open-angle", 10);
  int close_angle = logger->getIntSetting("close-angle", 55);
  bool settings_changed = false;

  switch (servo_command) {
    case ServoCommand::OPEN_SERVO:
      printf("OPEN_SERVO\n");
      // サーボを開く
      servo_controller->openServo(open_angle);
      break;
    case ServoCommand::CLOSE_SERVO:
      printf("CLOSE_SERVO\n");
      // サーボを閉じる
      servo_controller->closeServo(close_angle);
      break;
    case ServoCommand::OPEN_ANGLE_MINUS_1:
      printf("OPEN_ANGLE_MINUS_1\n");
      // 開く角度を1度減らす
      open_angle = open_angle - 1;
      logger->setIntSetting("open-angle", open_angle);
      settings_changed = true;
      // 現在のモードがSTARTの場合は、サーボの角度も更新する
      servo_controller->openServo(open_angle);
      break;
    case ServoCommand::OPEN_ANGLE_PLUS_1:
      printf("OPEN_ANGLE_PLUS_1\n");
      // 開く角度を1度増やす
      open_angle = open_angle + 1;
      logger->setIntSetting("open-angle", open_angle);
      settings_changed = true;
      // 現在のモードがSTARTの場合は、サーボの角度も更新する
      servo_controller->openServo(open_angle);
      break;
    case ServoCommand::CLOSE_ANGLE_PLUS:
      printf("CLOSE_ANGLE_PLUS\n");
      // 閉じる角度を1度増やす
      close_angle = close_angle + 1;
      logger->setIntSetting("close-angle", close_angle);
      settings_changed = true;
      break;
    case ServoCommand::CLOSE_ANGLE_MINUS:
      printf("CLOSE_ANGLE_MINUS\n");
      // 閉じる角度を1度減らす
      close_angle = close_angle - 1;
      logger->setIntSetting("close-angle", close_angle);
      settings_changed = true;
      break;
    default:
      break;
  }

  // 設定が変更された場合、SDカードに保存する
  if (settings_changed) {
    // 設定の保存を試みるが、失敗しても続行
    if (logger->saveSettings()) {
      ESP_LOGI(TAG, "Servo settings saved to SD card");
      // 変更後の値をログに出力
      ESP_LOGI(TAG, "Open angle: %d, Close angle: %d",
               logger->getIntSetting("open-angle"),
               logger->getIntSetting("close-angle"));
    } else {
      ESP_LOGW(TAG,
               "Failed to save servo settings to SD card, but continuing with "
               "updated values in memory");
      // 変更後の値をログに出力（メモリ上の値）
      ESP_LOGI(TAG, "Open angle (in memory): %d, Close angle (in memory): %d",
               open_angle, close_angle);
    }
  }
}

void CommandHandler::commandTask(void* pvParameters) {
  CommandHandler* self = static_cast<CommandHandler*>(pvParameters);

  if (self == nullptr || self->mode_manager == nullptr) {
    ESP_LOGE("COMMAND_TASK", "Invalid parameters");
    vTaskDelete(nullptr);
    return;
  }

  // CANモードの場合、CANが初期化されているか確認
  if (self->comm_mode == CommMode::CAN && self->can_comm == nullptr) {
    ESP_LOGE("COMMAND_TASK", "CAN is not initialized but CAN mode is selected");
    vTaskDelete(nullptr);
    return;
  }

  CanRxFrame receive_frame;

  while (true) {
    if (self->comm_mode == CommMode::CAN) {
      // CANからのコマンド受信
      if (self->can_comm->readFrameNoWait(receive_frame) == ESP_OK) {
        self->processCanCommand(receive_frame);
      }
    } else {
      // UARTからのコマンド受信
      int cmd_uart = getchar();
      if (cmd_uart != EOF) {
        self->processUartCommand(cmd_uart);
      }
    }
  }
}
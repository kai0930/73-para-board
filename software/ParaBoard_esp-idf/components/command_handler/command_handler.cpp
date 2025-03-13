#include "command_handler.hpp"

CommandHandler::CommandHandler()
    : command_task_handle(nullptr),
      can_comm(nullptr),
      logger(nullptr),
      sensor_handler(nullptr),
      log_handler(nullptr),
      servo_controller(nullptr),
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
                          ServoController* servo_controller_ptr) {
  if (can_comm_ptr == nullptr) {
    ESP_LOGE(TAG, "CAN communication pointer is null");
    return false;
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

  can_comm = can_comm_ptr;
  logger = logger_ptr;
  sensor_handler = sensor_handler_ptr;
  log_handler = log_handler_ptr;
  servo_controller = servo_controller_ptr;

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
        printf("Mode changed to %s\n",
               ModeManager::getModeString(next_mode).c_str());
        // STARTモードに移行したらis_logging_modeをfalseに設定
        logger->setBoolSetting("is_logging_mode", false);
        logger->saveSettings();
      },
      [](ModeCommand previous_mode, ModeCommand next_mode) {
        printf("Mode changed from %s\n",
               ModeManager::getModeString(previous_mode).c_str());
      });

  // LOGGINGモードの設定
  mode_manager->setupMode(
      ModeCommand::LOGGING,
      [this](ModeCommand previous_mode, ModeCommand next_mode) {
        printf("Mode changed to %s\n",
               ModeManager::getModeString(next_mode).c_str());
        // LOGGINGモードに移行したらis_logging_modeをtrueに設定
        logger->setBoolSetting("is_logging_mode", true);
        logger->saveSettings();
        // センサータスクとログタスクを開始
        vTaskResume(sensor_handler->getTaskHandle());
        log_handler->startTask();
      },
      [this](ModeCommand previous_mode, ModeCommand next_mode) {
        printf("Mode changed from %s\n",
               ModeManager::getModeString(previous_mode).c_str());
        // センサータスクとログタスクを停止
        vTaskSuspend(sensor_handler->getTaskHandle());
        log_handler->stopTask();
      });

  // モードマネージャーの初期化
  mode_manager->begin();
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
  int open_angle = logger->getIntSetting("open-angle", 10);
  int close_angle = logger->getIntSetting("close-angle", 10);
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
      if (mode_manager->getMode() == ModeCommand::START) {
        servo_controller->openServo(open_angle);
      }
      break;
    case ServoCommand::OPEN_ANGLE_PLUS_1:
      printf("OPEN_ANGLE_PLUS_1\n");
      // 開く角度を1度増やす
      open_angle = open_angle + 1;
      logger->setIntSetting("open-angle", open_angle);
      settings_changed = true;
      // 現在のモードがSTARTの場合は、サーボの角度も更新する
      if (mode_manager->getMode() == ModeCommand::START) {
        servo_controller->openServo(open_angle);
      }
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
    if (logger->saveSettings()) {
      ESP_LOGI(TAG, "Servo settings saved to SD card");
      // 変更後の値をログに出力
      ESP_LOGI(TAG, "Open angle: %d, Close angle: %d",
               logger->getIntSetting("open-angle"),
               logger->getIntSetting("close-angle"));
    } else {
      ESP_LOGE(TAG, "Failed to save servo settings to SD card");
    }
  }
}

void CommandHandler::commandTask(void* pvParameters) {
  CommandHandler* self = static_cast<CommandHandler*>(pvParameters);

  if (self == nullptr || self->can_comm == nullptr ||
      self->mode_manager == nullptr) {
    ESP_LOGE("COMMAND_TASK", "Invalid parameters");
    vTaskDelete(nullptr);
    return;
  }

  CanRxFrame receive_frame;

  while (true) {
    // CANからのコマンド受信
    self->can_comm->readFrameNoWait(receive_frame);
    self->processCanCommand(receive_frame);

    // UARTからのコマンド受信
    int cmd_uart = getchar();
    if (cmd_uart != EOF) {
      self->processUartCommand(cmd_uart);
    }

    // 少し待機して次のループへ
    vTaskDelay(pdMS_TO_TICKS(self->UART_DELAY_MS));
  }
}
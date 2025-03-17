#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>

#include "CanComm.hpp"
#include "command_handler.hpp"
#include "config.hpp"
#include "create_spi.hpp"
#include "driver/usb_serial_jtag.h"
#include "esp_timer.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gptimer.hpp"
#include "icm42688.hpp"
#include "led_controller.hpp"
#include "log_task_handler.hpp"
#include "lps25hb.hpp"
#include "sd_controller.hpp"
#include "sensor_task_handler.hpp"
#include "servo_controller.hpp"

TaskHandle_t process_task_handle;
QueueHandle_t process_queue;

CreateSpi *spi = nullptr;
Icm::Icm42688 *icm = nullptr;
Lps::Lps25hb *lps = nullptr;
GPTimer *gptimer = nullptr;
SdController *logger = nullptr;
CanComm *can_comm = nullptr;
LogTaskHandler *log_task_handler = nullptr;
SensorTaskHandler *sensor_task_handler = nullptr;
CommandHandler *command_handler = nullptr;
ConditionChecker *condition_checker = nullptr;
ServoController *servo_controller = nullptr;
LedController *led_controller = nullptr;

constexpr const char *TAG = "MAIN";

void initUart() {
  /* Disable buffering on stdin */
  setvbuf(stdin, NULL, _IONBF, 0);

  /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
  esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
  /* Move the caret to the beginning of the next line on '\n' */
  esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

  /* Enable non-blocking mode on stdin and stdout */
  fcntl(fileno(stdout), F_SETFL, 0);
  fcntl(fileno(stdin), F_SETFL, 0);

  usb_serial_jtag_driver_config_t usb_serial_jtag_config;
  usb_serial_jtag_config.rx_buffer_size = 1024;
  usb_serial_jtag_config.tx_buffer_size = 1024;

  esp_err_t ret = ESP_OK;
  /* Install USB-SERIAL-JTAG driver for interrupt-driven reads and writes */
  ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
  if (ret != ESP_OK) {
    return;
  }

  /* Tell vfs to use usb-serial-jtag driver */
  esp_vfs_usb_serial_jtag_use_driver();
}

static bool IRAM_ATTR onTimer(gptimer_handle_t timer,
                              const gptimer_alarm_event_data_t *edata,
                              void *user_ctx) {
  // センサータスクハンドラが初期化されていれば、通知を送る
  if (sensor_task_handler != nullptr) {
    return sensor_task_handler->notifyFromISR();
  }
  return false;
}

extern "C" void app_main(void) {
  // SPIデバイスの初期化
  spi = new CreateSpi();
  spi->begin(SPI2_HOST, config::pins.SCK, config::pins.MISO, config::pins.MOSI);

  icm = new Icm::Icm42688();
  if (!icm->begin(spi, config::pins.ICMCS)) {
    ESP_LOGE(TAG, "Failed to initialize ICM42688");
  }

  lps = new Lps::Lps25hb();
  if (!lps->begin(spi, config::pins.LPSCS)) {
    ESP_LOGE(TAG, "Failed to initialize LPS25HB");
  }

  // SDカードの初期化
  logger = new SdController();
  if (!logger->begin()) {
    ESP_LOGE(TAG, "Failed to initialize SDMMC");
  }

  // 通信モードの設定を読み込む
  std::string comm_mode_str = logger->getStringSetting("comm_mode", "can");
  bool use_can = (comm_mode_str != "uart");
  ESP_LOGI(TAG, "Communication mode: %s", use_can ? "CAN" : "UART");

  // CANの初期化（CANモードの場合のみ）
  if (use_can) {
    can_comm =
        new CanComm(BoardID::PARA, config::pins.CAN_TX, config::pins.CAN_RX);
    if (can_comm->begin() == ESP_OK) {
      ESP_LOGI(TAG, "CAN driver started.");
    } else {
      ESP_LOGE(TAG, "can_comm->begin() failed.");
      return;
    }
  } else {
    can_comm = nullptr;
  }

  // UARTの初期化（UARTモードの場合のみ）
  if (!use_can) {
    initUart();
    ESP_LOGI(TAG, "UART initialized");
  }

  // タイマーの初期化
  gptimer = new GPTimer();
  if (!gptimer->init(40000000, 40000)) {
    ESP_LOGE(TAG, "Failed to initialize GPTimer");
  }
  gptimer->registerCallback(onTimer);
  gptimer->start();

  // LogTaskHandlerの初期化
  log_task_handler = new LogTaskHandler();
  if (!log_task_handler->init(logger)) {
    ESP_LOGE(TAG, "Failed to initialize LogTaskHandler");
    return;
  }
  ESP_LOGI(TAG, "LogTaskHandler initialized");

  // ServoControllerの初期化
  servo_controller = new ServoController();
  if (!servo_controller->init(config::pins.SERVO)) {
    ESP_LOGE(TAG, "Failed to initialize ServoController");
    return;
  }
  ESP_LOGI(TAG, "ServoController initialized");

  // SensorTaskHandlerの初期化
  sensor_task_handler = new SensorTaskHandler();
  if (!sensor_task_handler->init(icm, lps, log_task_handler, servo_controller,
                                 logger)) {
    ESP_LOGE(TAG, "Failed to initialize SensorTaskHandler");
    return;
  }
  ESP_LOGI(TAG, "SensorTaskHandler initialized");
  sensor_task_handler->startTask();

  // LEDコントローラーの初期化
  led_controller = new LedController();
  if (!led_controller->init(config::pins.LED)) {
    ESP_LOGE(TAG, "Failed to initialize LEDController");
    return;
  }
  ESP_LOGI(TAG, "LEDController initialized");

  // CommandHandlerの初期化
  command_handler = new CommandHandler();
  if (!command_handler->init(can_comm, logger, sensor_task_handler,
                             log_task_handler, servo_controller,
                             led_controller)) {
    ESP_LOGE(TAG, "Failed to initialize CommandHandler");
    return;
  }
  ESP_LOGI(TAG, "CommandHandler initialized");
  command_handler->startTask();

  // SDカードの設定でis_logging_modeがtrueの場合、LOGGINGモードで開始・他基板にCANで通知
  bool is_logging_mode = logger->getBoolSetting("is_logging_mode", false);
  if (is_logging_mode) {
    ESP_LOGI(TAG, "is_logging_mode is true, starting in LOGGING mode");
    // 現在のモードがSTARTの場合のみ、LOGGINGモードに変更
    if (command_handler->getModeManager()->getMode() == ModeCommand::START) {
      command_handler->getModeManager()->changeMode(ModeCommand::LOGGING);
      uint8_t mode = static_cast<uint8_t>(ModeCommand::LOGGING);
      can_comm->send(ContentID::BOARD_STATE, &mode, 1);
    } else {
      ESP_LOGI(TAG, "is_logging_mode is false, starting in START mode");
    }

    // データ格納用キュー（処理用）
    process_queue = xQueueCreate(10, sizeof(SensorData));
  }
}
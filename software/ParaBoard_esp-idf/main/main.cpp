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
ServoController *servo_controller = nullptr;

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
    printf("Failed to initialize ICM42688\n");
  }

  lps = new Lps::Lps25hb();
  if (!lps->begin(spi, config::pins.LPSCS)) {
    printf("Failed to initialize LPS25HB\n");
  }

  // SDカードの初期化
  logger = new SdController();
  if (!logger->begin()) {
    printf("Failed to initialize SDMMC\n");
  }

  // SDカードのsettings.jsonから、"test-text"を読み込み表示
  std::string test_text = logger->getStringSetting("test-text", "default");
  printf("test-text: %s\n", test_text.c_str());

  // CANの初期化
  can_comm =
      new CanComm(BoardID::PARA, config::pins.CAN_TX, config::pins.CAN_RX);
  if (can_comm->begin() == ESP_OK) {
    printf("[MAIN] CAN driver started.\n");
  } else {
    printf("[MAIN] can_comm->begin() failed.\n");
    return;
  }

  // タイマーの初期化
  gptimer = new GPTimer();
  if (!gptimer->init(40000000, 40000)) {
    printf("Failed to initialize GPTimer\n");
  }
  gptimer->registerCallback(onTimer);
  gptimer->start();

  // UARTの初期化
  initUart();

  // LogTaskHandlerの初期化
  log_task_handler = new LogTaskHandler();
  if (!log_task_handler->init(logger)) {
    printf("Failed to initialize LogTaskHandler\n");
    return;
  }

  // SensorTaskHandlerの初期化
  sensor_task_handler = new SensorTaskHandler();
  if (!sensor_task_handler->init(icm, lps, log_task_handler)) {
    printf("Failed to initialize SensorTaskHandler\n");
    return;
  }
  sensor_task_handler->startTask();

  // ServoControllerの初期化
  servo_controller = new ServoController();
  if (!servo_controller->init(config::pins.SERVO)) {
    printf("Failed to initialize ServoController\n");
    return;
  }

  // CommandHandlerの初期化
  command_handler = new CommandHandler();
  if (!command_handler->init(can_comm, logger, sensor_task_handler,
                             log_task_handler, servo_controller)) {
    printf("Failed to initialize CommandHandler\n");
    return;
  }
  command_handler->startTask();

  // SDカードの設定でis_logging_modeがtrueの場合、LOGGINGモードで開始
  bool is_logging_mode = logger->getBoolSetting("is_logging_mode", false);
  if (is_logging_mode) {
    printf("is_logging_mode is true, starting in LOGGING mode\n");
    // 現在のモードがSTARTの場合のみ、LOGGINGモードに変更
    if (command_handler->getModeManager()->getMode() == ModeCommand::START) {
      command_handler->getModeManager()->changeMode(ModeCommand::LOGGING);
      printf("Changed to LOGGING mode\n");
    }
  } else {
    printf("is_logging_mode is false, starting in START mode\n");
  }

  // データ格納用キュー（処理用）
  process_queue = xQueueCreate(10, sizeof(SensorData));
}

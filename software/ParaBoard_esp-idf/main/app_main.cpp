#include <stdio.h>

#include "config.hpp"
#include "create_spi.hpp"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "icm42688.hpp"
#include "linenoise/linenoise.h"
#include "lps25hb.hpp"
#include "sdmmc_logger.hpp"
#include "servo.hpp"

static const char *TAG = "Main";

// コマンド定義
enum Command {
  OPEN = 'o',
  CLOSE = 'c',
  QUIT = 'q',
  INCREASE_OPEN_ANGLE = 'i',
  DECREASE_OPEN_ANGLE = 'd',
  INCREASE_CLOSE_ANGLE = 'u',
  DECREASE_CLOSE_ANGLE = 'l'
};

// サーボの角度定義
struct ServoAngles {
  static float OPEN_ANGLE;
  static float CLOSE_ANGLE;
};

class ServoController {
 public:
  ServoController() : servo_(Pins::SERVO) {
    gpio_reset_pin(Pins::LED);
    gpio_set_direction(Pins::LED, GPIO_MODE_OUTPUT);
    ServoAngles::OPEN_ANGLE = 15.0f;
    ServoAngles::CLOSE_ANGLE = 165.0f;
  }

  void open() {
    ESP_LOGI(TAG, "cmd: オープン");
    servo_.setAngle(ServoAngles::OPEN_ANGLE);
    gpio_set_level(Pins::LED, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  void close() {
    ESP_LOGI(TAG, "cmd: クローズ");
    servo_.setAngle(ServoAngles::CLOSE_ANGLE);
    gpio_set_level(Pins::LED, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  void increaseOpenAngle() {
    ESP_LOGI(TAG, "cmd: オープン角度を増加");
    ServoAngles::OPEN_ANGLE += 5.0f;
  }

  void decreaseOpenAngle() {
    ESP_LOGI(TAG, "cmd: オープン角度を減少");
    ServoAngles::OPEN_ANGLE -= 5.0f;
  }

  void increaseCloseAngle() {
    ESP_LOGI(TAG, "cmd: クローズ角度を増加");
    ServoAngles::CLOSE_ANGLE += 5.0f;
  }

  void decreaseCloseAngle() {
    ESP_LOGI(TAG, "cmd: クローズ角度を減少");
    ServoAngles::CLOSE_ANGLE -= 5.0f;
  }

 private:
  Servo servo_;
};

void initialize_console() {
  /* コンソールの初期化 */
  esp_console_config_t console_config = {.max_cmdline_length = 256,
                                         .max_cmdline_args = 8,
                                         .hint_color = 37,
                                         .hint_bold = 0};
  ESP_ERROR_CHECK(esp_console_init(&console_config));

  /* linenoise初期化 */
  linenoiseSetMultiLine(1);
  linenoiseHistorySetMaxLen(10);

  /* USB-CDC初期化 */
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  /* USB-CDCのライン設定 */
  esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
  esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);
}

extern "C" void app_main(void) {
  ServoController controller;
  initialize_console();

  printf("コマンドを入力してください（o: オープン, c: クローズ, q: 終了）\n");

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));

    int c = getchar();
    if (c == EOF) continue;

    switch (c) {
      case Command::OPEN:
        controller.open();
        break;
      case Command::CLOSE:
        controller.close();
        break;
      case Command::INCREASE_OPEN_ANGLE:
        controller.increaseOpenAngle();
        break;
      case Command::DECREASE_OPEN_ANGLE:
        controller.decreaseOpenAngle();
        break;
      case Command::INCREASE_CLOSE_ANGLE:
        controller.increaseCloseAngle();
        break;
      case Command::DECREASE_CLOSE_ANGLE:
        controller.decreaseCloseAngle();
        break;
      case Command::QUIT:
        printf("プログラムを終了します\n");
        return;
    }
  }

  // GPIOの設定
  // gpio_reset_pin(Pins::LED);
  // gpio_set_direction(Pins::LED, GPIO_MODE_OUTPUT);

  // CreateSpi spi;
  // Icm::Icm42688 icm;
  // Lps::Lps25hb lps;
  // SdmmcLogger sdmmc_logger;

  // if (!spi.begin(SPI2_HOST, Pins::SCK, Pins::MISO, Pins::MOSI)) {
  //     ESP_LOGE(TAG, "SPIの初期化に失敗しました");
  //     return;
  // }

  // if (!icm.begin(&spi, Pins::ICMCS)) {
  //     ESP_LOGE(TAG, "ICM42688の初期化に失敗しました");
  //     return;
  // }

  // if (!lps.begin(&spi, Pins::LPSCS)) {
  //     ESP_LOGE(TAG, "LPS25HBの初期化に失敗しました");
  //     return;
  // }

  // // ロガーの初期化
  // if (!sdmmc_logger.begin()) {
  //     ESP_LOGE(TAG, "SDMMCの初期化に失敗しました");
  //     return;
  // }

  // // データ構造体を完全修飾名で宣言
  // Icm::AccelData accel_data;
  // Icm::GyroData gyro_data;
  // Icm::TempData temp_data_icm;
  // Lps::PressureData pressure_data;
  // Lps::TempData temp_data_lps;

  // while (1) {
  //     icm.getAccelAndGyro(&accel_data, &gyro_data);
  //     icm.getTemp(&temp_data_icm);
  //     lps.getPressureAndTemp(&pressure_data, &temp_data_lps);

  //     // accel, gyro, tempを変換する
  //     float accel[3];
  //     float gyro[3];
  //     float temperature_icm;
  //     float temperature_lps;
  //     float pressure;

  //     accel[0] = (int16_t)(accel_data.u_x << 8 | accel_data.d_x) / 2054.0f;
  //     accel[1] = (int16_t)(accel_data.u_y << 8 | accel_data.d_y) / 2054.0f;
  //     accel[2] = (int16_t)(accel_data.u_z << 8 | accel_data.d_z) / 2054.0f;

  //     gyro[0] = (int16_t)(gyro_data.u_x << 8 | gyro_data.d_x) / 16.4f;
  //     gyro[1] = (int16_t)(gyro_data.u_y << 8 | gyro_data.d_y) / 16.4f;
  //     gyro[2] = (int16_t)(gyro_data.u_z << 8 | gyro_data.d_z) / 16.4f;

  //     temperature_icm = (int16_t)(temp_data_icm.u_t << 8 |
  //     temp_data_icm.d_t) / 132.48f + 25.0f; temperature_lps =
  //     (int16_t)(temp_data_lps.h_t << 8 | temp_data_lps.l_t) / 480.0f
  //     + 42.5f;

  //     pressure = (int32_t)(pressure_data.h_p << 16 | pressure_data.l_p << 8
  //     | pressure_data.xl_p) * 100.0f / 4096.0f;

  //     ESP_LOGI(TAG, "accel: %f, %f, %f, gyro: %f, %f, %f, temp_icm: %f,
  //     temp_lps: %f, pressure: %f", accel[0], accel[1], accel[2], gyro[0],
  //     gyro[1], gyro[2],
  //              temperature_icm, temperature_lps, pressure);

  //     sdmmc_logger.writeLog(esp_timer_get_time(), accel[0], accel[1],
  //     accel[2], gyro[0], gyro[1], gyro[2], pressure); sdmmc_logger.flush();

  //     // 1秒ごとにデータを取得
  //     vTaskDelay(pdMS_TO_TICKS(1000));
  // }
}
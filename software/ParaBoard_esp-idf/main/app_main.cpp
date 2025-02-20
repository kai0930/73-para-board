#include "config.hpp"
#include "create_spi.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "icm42688.hpp"
#include "lps25hb.hpp"
#include "sdmmc_logger.hpp"
#include "servo.hpp"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

static const char *TAG = "Main";

static void cdc_rx_callback(int itf, cdcacm_event_t *event) {
    char buf[64];
    int len = tusb_cdc_acm_read(itf, buf, sizeof(buf));
    if (len > 0) {
        buf[len] = '\0';  // 文字列終端
        ESP_LOGI("USB-CDC", "Received: %s", buf);
        // 受信データをPCに返す
        tusb_cdc_acm_write_queue(itf, buf, len);
        tusb_cdc_acm_write_flush(itf);
    }
}

extern "C" void app_main(void) {
    // Servoの初期化
    Servo servo(Pins::SERVO);
    // LEDの初期化
    gpio_reset_pin(Pins::LED);
    gpio_set_direction(Pins::LED, GPIO_MODE_OUTPUT);

    // TinyUSBの初期化
    tinyusb_config_t tusb_cfg = {0};  // デフォルト設定
    tinyusb_driver_install(&tusb_cfg);

    // USB-CDCを有効化
    tusb_cdc_acm_init();
    tusb_cdc_acm_register_callback(0, CDC_EVENT_RX, cdc_rx_callback);

    // while (1) {
    //     servo.setAngle(15);
    //     gpio_set_level(Pins::LED, 1);
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    //     servo.setAngle(165);
    //     gpio_set_level(Pins::LED, 0);
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }

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

    //     temperature_icm = (int16_t)(temp_data_icm.u_t << 8 | temp_data_icm.d_t) / 132.48f + 25.0f;
    //     temperature_lps = (int16_t)(temp_data_lps.h_t << 8 | temp_data_lps.l_t) / 480.0f + 42.5f;

    //     pressure = (int32_t)(pressure_data.h_p << 16 | pressure_data.l_p << 8 | pressure_data.xl_p) * 100.0f / 4096.0f;

    //     ESP_LOGI(TAG, "accel: %f, %f, %f, gyro: %f, %f, %f, temp_icm: %f, temp_lps: %f, pressure: %f", accel[0], accel[1], accel[2], gyro[0], gyro[1],
    //     gyro[2],
    //              temperature_icm, temperature_lps, pressure);

    //     sdmmc_logger.writeLog(esp_timer_get_time(), accel[0], accel[1], accel[2], gyro[0], gyro[1], gyro[2], pressure);
    //     sdmmc_logger.flush();

    //     // 1秒ごとにデータを取得
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}
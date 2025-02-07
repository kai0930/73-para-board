#include "config.hpp"
#include "create_spi.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "icm20602.hpp"
#include "lps25hb.hpp"

static const char *TAG = "Main";

extern "C" void app_main(void) {
  CreateSpi spi;
  Icm20602 icm;
  Lps25hb lps;

  // SPIの初期化
  if (!spi.begin(SPI2_HOST, config::pins.SCK, config::pins.MISO,
                 config::pins.MOSI)) {
    ESP_LOGE(TAG, "Failed to initialize SPI");
    return;
  }

  ESP_LOGI(TAG, "SPI initialized successfully");

  // ICM20602の初期化
  if (!icm.begin(&spi, config::pins.ICMCS)) {
    ESP_LOGE(TAG, "Failed to initialize ICM20602");
    return;
  }

  if (!icm.isInitialized()) {
    ESP_LOGE(TAG, "Failed to initialize ICM20602");
    return;
  }

  // WHO_AM_Iの確認
  uint8_t who_am_i = icm.whoAmI();
  ESP_LOGI(TAG, "WHO_AM_I value: 0x%02x (Expected: 0x%02x)", who_am_i,
           Icm20602Config::WHO_AM_I_VALUE);

  // LPS25HBの初期化
  if (!lps.begin(&spi, config::pins.LPSCS)) {
    ESP_LOGE(TAG, "Failed to initialize LPS25HB");
    return;
  }

  // メインループ
  Icm20602Data data;
  Lps25hbData pressure_data;
  while (1) {
    icm.get(&data);
    lps.get(&pressure_data);

    ESP_LOGI(TAG,
             "Accel[G]: X=%.2f Y=%.2f Z=%.2f | "
             "Gyro[dps]: X=%.2f Y=%.2f Z=%.2f | "
             "Norm[G]: %.2f",
             data.accel.x, data.accel.y, data.accel.z, data.gyro.x, data.gyro.y,
             data.gyro.z, data.accel_norm);
    ESP_LOGI(TAG, "Pressure: %.2f hPa (raw: %lu)", pressure_data.pressure,
             pressure_data.raw);

    vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz
  }
}
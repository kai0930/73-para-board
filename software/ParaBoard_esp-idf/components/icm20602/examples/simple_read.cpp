#include "create_spi.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "icm20602.hpp"

static const char *TAG = "ICM20602_EXAMPLE";

extern "C" void app_main(void) {
  // SPIの初期化
  CreateSpi spi;
  if (!spi.begin(SPI2_HOST, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23)) {
    ESP_LOGE(TAG, "Failed to initialize SPI");
    return;
  }

  // ICM20602の初期化
  Icm20602 icm;
  if (!icm.begin(&spi, GPIO_NUM_5)) {
    ESP_LOGE(TAG, "Failed to initialize ICM20602");
    return;
  }

  // メインループ
  Icm20602Data data;
  while (1) {
    icm.get(&data);

    // データの表示
    ESP_LOGI(TAG, "Acceleration (G): X=%.2f Y=%.2f Z=%.2f", data.accel.x,
             data.accel.y, data.accel.z);
    ESP_LOGI(TAG, "Gyroscope (dps): X=%.2f Y=%.2f Z=%.2f", data.gyro.x,
             data.gyro.y, data.gyro.z);
    ESP_LOGI(TAG, "Acceleration magnitude: %.2f G", data.accel_norm);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
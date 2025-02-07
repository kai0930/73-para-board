#include "create_spi.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lps25hb.hpp"

static const char *TAG = "LPS25HB_EXAMPLE";

extern "C" void app_main(void) {
  // SPIの初期化
  CreateSpi spi;
  if (!spi.begin(SPI2_HOST, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23)) {
    ESP_LOGE(TAG, "Failed to initialize SPI");
    return;
  }

  // LPS25HBの初期化
  Lps25hb lps;
  if (!lps.begin(&spi, GPIO_NUM_5)) {
    ESP_LOGE(TAG, "Failed to initialize LPS25HB");
    return;
  }

  // メインループ
  Lps25hbData data;
  while (1) {
    lps.get(&data);

    // データの表示
    ESP_LOGI(TAG, "Pressure: %.2f hPa (raw: %lu)", data.pressure, data.raw);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
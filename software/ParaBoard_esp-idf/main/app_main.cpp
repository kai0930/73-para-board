#include "config.hpp"
#include "create_spi.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "icm20602.hpp"
#include "lps25hb.hpp"
#include "test_liftoff_accel.hpp"

static const char *TAG = "Main";
static QueueHandle_t log_queue;

// ログ出力タスク
void log_task(void *pvParameters) {
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
  ESP_LOGI(TAG, "log_task started");
  static const char *LOGTAG = "LiftoffTest";
  LogData log_data;

  while (1) {
    if (xQueueReceive(log_queue, &log_data, pdMS_TO_TICKS(100))) {
      ESP_LOGI(LOGTAG, "t=%lu ms, Norm2=%.3f, Count=%d", log_data.timestamp,
               log_data.norm2, log_data.count);
    }
    esp_task_wdt_reset();
  }
}

// メインの処理タスク
void liftoff_detection_task(void *pvParameters) {
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

  CreateSpi spi;
  Icm20602 icm;
  Lps25hb lps;
  LiftoffAccelTest test(log_queue);

  // SPIの初期化
  if (!spi.begin(SPI2_HOST, config::pins.SCK, config::pins.MISO,
                 config::pins.MOSI)) {
    ESP_LOGE(TAG, "Failed to initialize SPI");
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "SPI initialized successfully");

  // ICM20602の初期化
  if (!icm.begin(&spi, config::pins.ICMCS)) {
    ESP_LOGE(TAG, "Failed to initialize ICM20602");
    vTaskDelete(NULL);
    return;
  }

  if (!icm.isInitialized()) {
    ESP_LOGE(TAG, "Failed to initialize ICM20602");
    vTaskDelete(NULL);
    return;
  }

  // WHO_AM_Iの確認
  uint8_t who_am_i = icm.whoAmI();
  ESP_LOGI(TAG, "WHO_AM_I value: 0x%02x (Expected: 0x%02x)", who_am_i,
           Icm20602Config::WHO_AM_I_VALUE);

  // LPS25HBの初期化
  if (!lps.begin(&spi, config::pins.LPSCS)) {
    ESP_LOGE(TAG, "Failed to initialize LPS25HB");
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "Starting liftoff detection test...");
  ESP_LOGI(TAG, "Please shake the board to simulate liftoff...");

  // メインループ
  Icm20602Data data;
  Lps25hbData pressure_data;
  uint32_t last_sample = 0;
  const uint32_t SAMPLE_INTERVAL_MS = 5; // 5ミリ秒ごとにサンプリング (200Hz)

  while (!test.isDetected()) {
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (current_time - last_sample >= SAMPLE_INTERVAL_MS) {
      icm.get(&data);
      lps.get(&pressure_data);
      test.processData(data.accel.x, data.accel.y, data.accel.z);
      last_sample = current_time;
      ESP_LOGI(TAG, "sensor loop, current_time: %lu", current_time);
    }
    esp_task_wdt_reset();
    taskYIELD();
  }

  ESP_LOGI(TAG, "Test completed!");
  vTaskDelete(NULL);
}

extern "C" void app_main(void) {
  // キューの作成
  log_queue = xQueueCreate(10, sizeof(LogData));

  // ログタスクの作成（CPU0で実行）
  xTaskCreatePinnedToCore(log_task, "log_task", 4096, NULL, 1, NULL, 0);

  // 検知タスクの作成（CPU1で実行）
  xTaskCreatePinnedToCore(liftoff_detection_task, "liftoff_task", 8192, NULL, 1,
                          NULL, 1);
}
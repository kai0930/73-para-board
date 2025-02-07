#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "create_spi.hpp"

static const char* MAIN_TAG = "APP_MAIN";

CreateSpi spi;

extern "C" void app_main(void)
{
  spi.begin();
  while (true) {
    ESP_LOGI(MAIN_TAG, "Hello, world!");
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
} 
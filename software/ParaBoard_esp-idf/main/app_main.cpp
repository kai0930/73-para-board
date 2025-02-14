#include "config.hpp"
#include "create_spi.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "icm42688.hpp"
#include "lps25hb.hpp"

static const char *TAG = "Main";

extern "C" void app_main(void) {
    // GPIOの設定
    gpio_reset_pin(Pins::LED);
    gpio_set_direction(Pins::LED, GPIO_MODE_OUTPUT);

    CreateSpi spi;
    Icm42688 icm;
    Lps25hb lps;

    if (!spi.begin(SPI2_HOST, Pins::SCK, Pins::MISO, Pins::MOSI)) {
        ESP_LOGE(TAG, "SPIの初期化に失敗しました");
        return;
    }

    if (!icm.begin(&spi, Pins::ICMCS)) {
        ESP_LOGE(TAG, "ICM42688の初期化に失敗しました");
        return;
    }

    if (!lps.begin(&spi, Pins::LPSCS)) {
        ESP_LOGE(TAG, "LPS25HBの初期化に失敗しました");
        return;
    }

    Icm42688Data data;
    Lps25hbData pressure_data;

    while (1) {
        icm.get(&data);
        lps.get(&pressure_data);

        ESP_LOGI(TAG, "ICM42688: %.3f, %.3f, %.3f, %.3f, %.3f, %.3f", data.accel.x, data.accel.y, data.accel.z, data.gyro.x, data.gyro.y, data.gyro.z);
        ESP_LOGI(TAG, "LPS25HB: %.3f", pressure_data.pressure);

        // 1秒ごとにデータを取得
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
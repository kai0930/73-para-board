#include "config.hpp"
#include "create_spi.hpp"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gptimer.hpp"
#include "lps25hb.hpp"
#include "servo.hpp"
#include "test_liftoff_pressure.hpp"

static const char *TAG = "Main";
static QueueHandle_t log_queue;
static Lps25hb *lps_ptr = nullptr;

static LiftoffPressureTest *test_ptr = nullptr;
static Servo *servo_ptr = nullptr;

// データ構造体の定義を追加
struct SensorReadRequest {
    bool pending;
};

// キューハンドルを追加
static QueueHandle_t sensor_queue;

// ログ出力タスク
void log_task(void *pvParameters) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI(TAG, "log_task started");
    static const char *LOGTAG = "LiftoffTest";
    LogData log_data;

    while (1) {
        if (xQueueReceive(log_queue, &log_data, pdMS_TO_TICKS(100))) {
            ESP_LOGI(LOGTAG, "%lu,%.3f,%d", log_data.timestamp, log_data.pressure, log_data.count);
        }
        esp_task_wdt_reset();
    }
}

// タイマーコールバック関数を修正
static bool IRAM_ATTR timer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    static SensorReadRequest req = {true};
    xQueueSendFromISR(sensor_queue, &req, NULL);
    return true;
}

// メインの処理タスクを修正
void liftoff_detection_task(void *pvParameters) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    CreateSpi spi;
    Lps25hb lps;
    LiftoffPressureTest test(log_queue);
    Servo servo(config::pins.SERVO);

    // グローバルポインタを設定
    lps_ptr = &lps;
    test_ptr = &test;
    servo_ptr = &servo;

    // サーボの初期化
    servo.setAngle(30);

    // SPIの初期化
    if (!spi.begin(SPI2_HOST, config::pins.SCK, config::pins.MISO, config::pins.MOSI)) {
        ESP_LOGE(TAG, "Failed to initialize SPI");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "SPI initialized successfully");

    // LPS25HBの初期化
    if (!lps.begin(&spi, config::pins.LPSCS)) {
        ESP_LOGE(TAG, "Failed to initialize LPS25HB");
        vTaskDelete(NULL);
        return;
    }

    // whoAmIの確認
    uint8_t who_am_i = lps.whoAmI();
    ESP_LOGI(TAG, "LPS25HB WHO_AM_I value: 0x%02x", who_am_i);

    ESP_LOGI(TAG, "Starting liftoff detection test...");
    ESP_LOGI(TAG, "Please lift the board to simulate liftoff...");

    // センサー読み取り用キューの作成
    sensor_queue = xQueueCreate(1, sizeof(SensorReadRequest));

    // タイマーの設定
    GPTimer timer;
    const uint32_t TIMER_RESOLUTION_HZ = 1000000;           // 1MHz
    const uint64_t ALARM_COUNT = TIMER_RESOLUTION_HZ / 25;  // 25Hz (10ms)

    if (!timer.init(TIMER_RESOLUTION_HZ, ALARM_COUNT)) {
        ESP_LOGE(TAG, "Failed to initialize timer");
        vTaskDelete(NULL);
        return;
    }

    if (!timer.registerCallback(timer_callback)) {
        ESP_LOGE(TAG, "Failed to register timer callback");
        vTaskDelete(NULL);
        return;
    }

    if (!timer.start()) {
        ESP_LOGE(TAG, "Failed to start timer");
        vTaskDelete(NULL);
        return;
    }

    // メインループの修正
    SensorReadRequest req;
    while (!test.isDetected()) {
        if (xQueueReceive(sensor_queue, &req, 0)) {
            Lps25hbData pressure_data;

            lps.get(&pressure_data);
            test.processData(pressure_data.pressure);
        }
        esp_task_wdt_reset();
    }

    // 検知後、サーボモータを180度回転
    ESP_LOGI(TAG, "Liftoff detected! Moving servo...");
    servo.setAngle(180);

    // タイマーを停止
    timer.stop();

    // WDTを無効化
    ESP_ERROR_CHECK(esp_task_wdt_delete(NULL));

    ESP_LOGI(TAG, "Test completed!");
    vTaskDelete(NULL);
}

extern "C" void app_main(void) {
    // キューの作成
    log_queue = xQueueCreate(10, sizeof(LogData));

    // ログタスクの作成（CPU0で実行）
    xTaskCreatePinnedToCore(log_task, "log_task", 4096, NULL, 1, NULL, 0);

    // 検知タスクの作成（CPU1で実行）
    xTaskCreatePinnedToCore(liftoff_detection_task, "liftoff_task", 8192, NULL, 1, NULL, 1);
}
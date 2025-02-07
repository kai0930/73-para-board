#include "config.hpp"
#include "create_spi.hpp"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gptimer.hpp"
#include "icm20602.hpp"
#include "lps25hb.hpp"
#include "test_liftoff_accel.hpp"

static const char *TAG = "Main";
static QueueHandle_t log_queue;
static Icm20602 *icm_ptr = nullptr;
static Lps25hb *lps_ptr = nullptr;
static LiftoffAccelTest *test_ptr = nullptr;

// データ構造体の定義を追加
struct SensorReadRequest {
    bool pending;
};

// キューハンドルを追加
static QueueHandle_t sensor_queue;

// サーボモータの設定用の定数を追加
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT  // 13ビット分解能
#define LEDC_FREQUENCY 50                // 50Hz
#define SERVO_MIN_PULSEWIDTH 500         // 最小パルス幅（マイクロ秒）
#define SERVO_MAX_PULSEWIDTH 2500        // 最大パルス幅（マイクロ秒）

// サーボモータの初期化関数を追加
static void init_servo() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE, .duty_resolution = LEDC_DUTY_RES, .timer_num = LEDC_TIMER, .freq_hz = LEDC_FREQUENCY, .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = config::pins.SERVO, .speed_mode = LEDC_MODE, .channel = LEDC_CHANNEL, .timer_sel = LEDC_TIMER, .duty = 0, .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// サーボモータの角度設定関数を追加
static void set_servo_angle(uint32_t angle) {
    // 角度を0-180度の範囲に制限
    if (angle > 180) angle = 180;

    // 角度をパルス幅に変換
    uint32_t pulse_width = SERVO_MIN_PULSEWIDTH + (((SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH) * angle) / 180);

    // パルス幅をデューティ比に変換
    uint32_t duty = (pulse_width * ((1 << LEDC_DUTY_RES) - 1)) / (1000000 / LEDC_FREQUENCY);

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

// ログ出力タスク
void log_task(void *pvParameters) {
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    ESP_LOGI(TAG, "log_task started");
    static const char *LOGTAG = "LiftoffTest";
    LogData log_data;

    while (1) {
        if (xQueueReceive(log_queue, &log_data, pdMS_TO_TICKS(100))) {
            ESP_LOGI(LOGTAG, "%lu,%.3f,%d", log_data.timestamp, log_data.norm2, log_data.count);
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

    // サーボモータの初期化
    init_servo();
    // 初期位置を設定（0度）
    set_servo_angle(90);

    CreateSpi spi;
    Icm20602 icm;
    Lps25hb lps;
    LiftoffAccelTest test(log_queue);

    // グローバルポインタを設定
    icm_ptr = &icm;
    lps_ptr = &lps;
    test_ptr = &test;

    // SPIの初期化
    if (!spi.begin(SPI2_HOST, config::pins.SCK, config::pins.MISO, config::pins.MOSI)) {
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
    ESP_LOGI(TAG, "WHO_AM_I value: 0x%02x (Expected: 0x%02x)", who_am_i, Icm20602Config::WHO_AM_I_VALUE);

    // LPS25HBの初期化
    if (!lps.begin(&spi, config::pins.LPSCS)) {
        ESP_LOGE(TAG, "Failed to initialize LPS25HB");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Starting liftoff detection test...");
    ESP_LOGI(TAG, "Please shake the board to simulate liftoff...");

    // センサー読み取り用キューの作成
    sensor_queue = xQueueCreate(1, sizeof(SensorReadRequest));

    // タイマーの設定
    GPTimer timer;
    const uint32_t TIMER_RESOLUTION_HZ = 1000000;             // 1MHz
    const uint64_t ALARM_COUNT = TIMER_RESOLUTION_HZ / 1000;  // 1kHz (1ms)

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
            Icm20602Data data;
            Lps25hbData pressure_data;

            icm.get(&data);
            lps.get(&pressure_data);
            test.processData(data.accel.x, data.accel.y, data.accel.z);
        }
        esp_task_wdt_reset();
    }

    // 検知後、サーボモータを180度回転
    ESP_LOGI(TAG, "Liftoff detected! Moving servo...");
    set_servo_angle(180);

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
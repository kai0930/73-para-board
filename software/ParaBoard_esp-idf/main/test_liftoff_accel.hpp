#pragma once

#include <array>
#include <vector>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// ログデータの構造体
struct LogData {
    uint32_t timestamp;  // ミリ秒
    float norm2;         // 加速度の2乗和
    int count;           // 連続検出回数
};

class LiftoffAccelTest {
   private:
    static constexpr int SAMPLE_RATE = 1000;     // 1000Hz
    static constexpr int AVERAGE_SAMPLES = 20;   // 0.02秒ごとの平均
    static constexpr float THRESHOLD_G2 = 4.0f;  // 4G^2
    static constexpr int DETECTION_COUNT = 3;    // テスト用に3回に設定（通常は50回）

    std::vector<float> accel_buffer_x;
    std::vector<float> accel_buffer_y;
    std::vector<float> accel_buffer_z;

    bool is_detected;
    int continuous_count;
    QueueHandle_t log_queue;
    static const char *TAG;
    uint32_t start_time;

   public:
    LiftoffAccelTest(QueueHandle_t queue) : is_detected(false), continuous_count(0), log_queue(queue) {
        accel_buffer_x.reserve(AVERAGE_SAMPLES);
        accel_buffer_y.reserve(AVERAGE_SAMPLES);
        accel_buffer_z.reserve(AVERAGE_SAMPLES);
        start_time = esp_timer_get_time() / 1000;
    }

    void processData(float ax, float ay, float az) {
        accel_buffer_x.push_back(ax);
        accel_buffer_y.push_back(ay);
        accel_buffer_z.push_back(az);

        if (accel_buffer_x.size() >= AVERAGE_SAMPLES) {
            // 平均値を計算
            float avg_x = 0, avg_y = 0, avg_z = 0;
            for (int i = 0; i < AVERAGE_SAMPLES; i++) {
                avg_x += accel_buffer_x[i];
                avg_y += accel_buffer_y[i];
                avg_z += accel_buffer_z[i];
            }
            avg_x /= AVERAGE_SAMPLES;
            avg_y /= AVERAGE_SAMPLES;
            avg_z /= AVERAGE_SAMPLES;

            // 2乗和を計算
            float norm2 = avg_x * avg_x + avg_y * avg_y + avg_z * avg_z;

            // 現在の時刻を取得（ミリ秒）
            uint32_t current_time = esp_timer_get_time() / 1000 - start_time;

            // ログデータを送信

            // 閾値判定
            if (norm2 > THRESHOLD_G2) {
                continuous_count++;
                if (continuous_count >= DETECTION_COUNT && !is_detected) {
                    is_detected = true;
                }
            } else {
                continuous_count = 0;
            }

            LogData log_data = {.timestamp = current_time, .norm2 = norm2, .count = continuous_count};
            xQueueSend(log_queue, &log_data, 0);

            // バッファをクリア
            accel_buffer_x.clear();
            accel_buffer_y.clear();
            accel_buffer_z.clear();
        }
    }

    bool isDetected() const {
        return is_detected;
    }
};

const char *LiftoffAccelTest::TAG = "LiftoffTest";
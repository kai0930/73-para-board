#pragma once

#include <array>
#include <numeric>
#include <vector>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

struct PressureLogData {
    uint32_t timestamp;
    float pressure;
    int count;
};

class PeakPressureTest {
   private:
    static constexpr int SAMPLE_RATE = 25;
    static constexpr int AVERAGE_SAMPLES = 5;
    static constexpr int DETECTION_COUNT = 5;

    std::vector<float> pressure_buffer;
    bool is_detected;
    int continuous_count;
    QueueHandle_t log_queue;
    static const char *TAG;
    uint32_t start_time;
    float prev_average_pressure;

   public:
    PeakPressureTest(QueueHandle_t queue) : is_detected(false), continuous_count(0), log_queue(queue) {
        pressure_buffer.reserve(AVERAGE_SAMPLES);
        start_time = esp_timer_get_time() / 1000;
        prev_average_pressure = 0.0f;
    }

    void processData(float pressure) {
        if (is_detected) {
            return;
        }
        pressure_buffer.push_back(pressure);
        if (pressure_buffer.size() >= AVERAGE_SAMPLES) {
            // 平均値を計算
            float average_pressure = std::accumulate(pressure_buffer.begin(), pressure_buffer.end(), 0.0f) / AVERAGE_SAMPLES;

            // 現在の時刻を取得（ミリ秒）
            uint32_t current_time = esp_timer_get_time() / 1000 - start_time;

            // 閾値判定
            if (prev_average_pressure != 0.0f && prev_average_pressure < average_pressure) {
                continuous_count++;
                if (continuous_count >= DETECTION_COUNT) {
                    is_detected = true;
                }
            } else {
                continuous_count = 0;
            }

            // ログデータを送信
            PressureLogData log_data = {.timestamp = current_time, .pressure = average_pressure, .count = continuous_count};
            xQueueSend(log_queue, &log_data, 0);

            prev_average_pressure = average_pressure;
            pressure_buffer.clear();
        }
    }

    bool isDetected() const {
        return is_detected;
    }
};

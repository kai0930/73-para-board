#pragma once

#include "driver/ledc.h"
#include "esp_log.h"

class Servo {
   public:
    // サーボモータの設定用の定数
    static constexpr ledc_mode_t MODE = LEDC_LOW_SPEED_MODE;
    static constexpr ledc_timer_t TIMER = LEDC_TIMER_0;
    static constexpr ledc_channel_t CHANNEL = LEDC_CHANNEL_0;
    static constexpr ledc_timer_bit_t DUTY_RES = LEDC_TIMER_13_BIT;
    static constexpr uint32_t FREQUENCY = 50;         // 50Hz
    static constexpr uint32_t MIN_PULSEWIDTH = 500;   // 最小パルス幅（マイクロ秒）
    static constexpr uint32_t MAX_PULSEWIDTH = 2500;  // 最大パルス幅（マイクロ秒）

    /**
     * @brief サーボモータを初期化する
     * @param pin サーボモータのピン番号
     */
    explicit Servo(gpio_num_t pin);

    /**
     * @brief サーボモータの角度を設定する
     * @param angle 角度（0-180度）
     */
    void setAngle(uint32_t angle);

   private:
    static const char *TAG;
    gpio_num_t pin;
};
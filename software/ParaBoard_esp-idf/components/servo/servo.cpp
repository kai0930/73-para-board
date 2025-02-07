#include "servo.hpp"

const char *Servo::TAG = "Servo";

Servo::Servo(gpio_num_t pin) : pin(pin) {
    // タイマーの設定
    ledc_timer_config_t timer_config = {
        .speed_mode = MODE, .duty_resolution = DUTY_RES, .timer_num = TIMER, .freq_hz = FREQUENCY, .clk_cfg = LEDC_AUTO_CLK, .deconfigure = false};

    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    // チャンネルの設定
    ledc_channel_config_t channel_config = {.gpio_num = pin,
                                            .speed_mode = MODE,
                                            .channel = CHANNEL,
                                            .intr_type = LEDC_INTR_DISABLE,
                                            .timer_sel = TIMER,
                                            .duty = 0,
                                            .hpoint = 0,
                                            .sleep_mode = LEDC_SLEEP_MODE_KEEP_ALIVE,
                                            .flags = 0};

    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
    ESP_LOGI(TAG, "Servo initialized on pin %d", pin);
}

void Servo::setAngle(uint32_t angle) {
    // 角度を0-180度の範囲に制限
    if (angle > 180U) {
        angle = 180U;
    }

    // 角度をパルス幅に変換
    uint32_t pulse_width = MIN_PULSEWIDTH + (((MAX_PULSEWIDTH - MIN_PULSEWIDTH) * angle) / 180);

    // パルス幅をデューティ比に変換
    uint32_t duty = (pulse_width * ((1 << DUTY_RES) - 1)) / (1000000 / FREQUENCY);

    ESP_ERROR_CHECK(ledc_set_duty(MODE, CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(MODE, CHANNEL));
    ESP_LOGI(TAG, "Angle set to %d degrees", angle);
}

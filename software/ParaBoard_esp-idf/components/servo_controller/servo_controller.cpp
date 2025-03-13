#include "servo_controller.hpp"

ServoController::ServoController()
    : servo_pin(GPIO_NUM_NC), is_initialized(false) {}

ServoController::~ServoController() {
  // LEDCチャンネルを停止
  if (is_initialized) {
    ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
  }
}

bool ServoController::init(gpio_num_t pin) {
  servo_pin = pin;

  // LEDCタイマーの設定
  ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_MODE,
                                    .duty_resolution = LEDC_DUTY_RES,
                                    .timer_num = LEDC_TIMER,
                                    .freq_hz = LEDC_FREQUENCY,
                                    .clk_cfg = LEDC_AUTO_CLK};

  esp_err_t ret = ledc_timer_config(&ledc_timer);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LEDC timer: %d", ret);
    return false;
  }

  // LEDCチャンネルの設定
  ledc_channel_config_t ledc_channel = {.gpio_num = servo_pin,
                                        .speed_mode = LEDC_MODE,
                                        .channel = LEDC_CHANNEL,
                                        .intr_type = LEDC_INTR_DISABLE,
                                        .timer_sel = LEDC_TIMER,
                                        .duty = 0,
                                        .hpoint = 0};

  ret = ledc_channel_config(&ledc_channel);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LEDC channel: %d", ret);
    return false;
  }

  is_initialized = true;
  ESP_LOGI(TAG, "Servo controller initialized on GPIO %d", servo_pin);

  // 初期位置として中間位置に設定
  setAngle(90);

  return true;
}

bool ServoController::setAngle(int angle_deg) {
  if (!is_initialized) {
    ESP_LOGE(TAG, "Servo controller not initialized");
    return false;
  }

  // 角度を有効範囲内に制限
  if (angle_deg < MIN_ANGLE_DEG) {
    angle_deg = MIN_ANGLE_DEG;
    ESP_LOGW(TAG, "Angle limited to minimum: %d", MIN_ANGLE_DEG);
  } else if (angle_deg > MAX_ANGLE_DEG) {
    angle_deg = MAX_ANGLE_DEG;
    ESP_LOGW(TAG, "Angle limited to maximum: %d", MAX_ANGLE_DEG);
  }

  // 角度をデューティサイクルに変換
  uint32_t duty = angleToDuty(angle_deg);

  // デューティサイクルを設定
  esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set LEDC duty: %d", ret);
    return false;
  }

  // 設定を適用
  ret = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to update LEDC duty: %d", ret);
    return false;
  }

  ESP_LOGI(TAG, "Servo angle set to %d degrees (duty: %lu)", angle_deg, duty);
  return true;
}

bool ServoController::openServo(int open_angle) {
  ESP_LOGI(TAG, "Opening servo to %d degrees", open_angle);
  return setAngle(open_angle);
}

bool ServoController::closeServo(int close_angle) {
  ESP_LOGI(TAG, "Closing servo to %d degrees", close_angle);
  return setAngle(close_angle);
}

uint32_t ServoController::angleToDuty(int angle_deg) {
  // 角度をパルス幅（マイクロ秒）に変換
  float pulse_width_us = MIN_PULSE_WIDTH_US +
                         (MAX_PULSE_WIDTH_US - MIN_PULSE_WIDTH_US) *
                             (float)angle_deg / (MAX_ANGLE_DEG - MIN_ANGLE_DEG);

  // パルス幅をデューティサイクルに変換
  // LEDC周波数は50Hz = 20ms周期
  // デューティサイクルは2^13 = 8192の分解能
  uint32_t duty =
      (uint32_t)((pulse_width_us / 20000.0f) * (1 << LEDC_DUTY_RES));

  return duty;
}
#include "led_controller.hpp"

LedController::LedController()
    : led_pin(GPIO_NUM_NC), blink_task_handle(nullptr) {}

LedController::~LedController() {
  // タスクの停止
  stopBlinking();
}

bool LedController::init(gpio_num_t led_pin) {
  this->led_pin = led_pin;

  // GPIOの設定
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_DISABLE;
  io_conf.mode = GPIO_MODE_OUTPUT;
  io_conf.pin_bit_mask = (1ULL << led_pin);
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  esp_err_t ret = gpio_config(&io_conf);

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure LED GPIO");
    return false;
  }

  // 初期状態は消灯
  turnOff();

  ESP_LOGI(TAG, "LED controller initialized");
  return true;
}

void LedController::turnOn() {
  if (led_pin != GPIO_NUM_NC) {
    gpio_set_level(led_pin, 1);
  }
}

void LedController::turnOff() {
  if (led_pin != GPIO_NUM_NC) {
    gpio_set_level(led_pin, 0);
  }
}

void LedController::startBlinking(uint32_t on_time_ms, uint32_t off_time_ms) {
  // すでに点滅タスクが実行中の場合は停止
  stopBlinking();

  // 点滅パラメータの設定
  this->on_time_ms = on_time_ms;
  this->off_time_ms = off_time_ms;
  this->should_blink = true;

  // タスクの作成
  BaseType_t result =
      xTaskCreate(blinkTask, "led_blink_task", TASK_STACK_SIZE,
                  this,  // 自身のインスタンスをパラメータとして渡す
                  TASK_PRIORITY, &blink_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create LED blink task");
    return;
  }

  ESP_LOGI(TAG, "LED blink task started (on: %lu ms, off: %lu ms)", on_time_ms,
           off_time_ms);
}

void LedController::stopBlinking() {
  if (blink_task_handle == nullptr) {
    return;
  }

  // 点滅フラグをオフに
  should_blink = false;

  // タスクの削除
  vTaskDelete(blink_task_handle);
  blink_task_handle = nullptr;

  // LEDを消灯
  turnOff();

  ESP_LOGI(TAG, "LED blink task stopped");
}

void LedController::changeBlinkPattern(uint32_t on_time_ms,
                                       uint32_t off_time_ms) {
  // 点滅パラメータの更新
  this->on_time_ms = on_time_ms;
  this->off_time_ms = off_time_ms;

  ESP_LOGI(TAG, "LED blink pattern changed (on: %lu ms, off: %lu ms)",
           on_time_ms, off_time_ms);
}

void LedController::blinkTask(void* pvParameters) {
  LedController* self = static_cast<LedController*>(pvParameters);

  if (self == nullptr) {
    ESP_LOGE("LED_BLINK_TASK", "Invalid parameters");
    vTaskDelete(nullptr);
    return;
  }

  while (self->should_blink) {
    // LED点灯
    self->turnOn();
    vTaskDelay(pdMS_TO_TICKS(self->on_time_ms));

    // LED消灯
    self->turnOff();
    vTaskDelay(pdMS_TO_TICKS(self->off_time_ms));
  }

  vTaskDelete(nullptr);
}
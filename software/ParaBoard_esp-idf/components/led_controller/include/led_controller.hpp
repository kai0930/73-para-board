#pragma once

#include <stdio.h>

#include "config.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class LedController {
 public:
  LedController();
  ~LedController();

  /**
   * @brief LEDコントローラーを初期化する
   * @param led_pin LEDのピン番号
   * @return 初期化が成功したかどうか
   */
  bool init(gpio_num_t led_pin = config::pins.LED);

  /**
   * @brief LEDを点灯する
   */
  void turnOn();

  /**
   * @brief LEDを消灯する
   */
  void turnOff();

  /**
   * @brief LEDの点滅タスクを開始する
   * @param on_time_ms 点灯時間（ミリ秒）
   * @param off_time_ms 消灯時間（ミリ秒）
   */
  void startBlinking(uint32_t on_time_ms, uint32_t off_time_ms);

  /**
   * @brief LEDの点滅タスクを停止する
   */
  void stopBlinking();

  /**
   * @brief LEDの点滅パターンを変更する
   * @param on_time_ms 点灯時間（ミリ秒）
   * @param off_time_ms 消灯時間（ミリ秒）
   */
  void changeBlinkPattern(uint32_t on_time_ms, uint32_t off_time_ms);

  /**
   * @brief LEDの点滅タスクハンドルを取得する
   * @return 点滅タスクハンドル
   */
  TaskHandle_t getBlinkTaskHandle() const { return blink_task_handle; }

 private:
  static constexpr const char* TAG = "LED_CONTROLLER";
  static constexpr int TASK_STACK_SIZE = 2048;
  static constexpr int TASK_PRIORITY = 5;

  gpio_num_t led_pin;
  TaskHandle_t blink_task_handle = nullptr;
  uint32_t on_time_ms = 500;   // デフォルト点灯時間: 500ms
  uint32_t off_time_ms = 500;  // デフォルト消灯時間: 500ms
  bool should_blink = false;

  /**
   * @brief LEDの点滅タスク関数
   * @param pvParameters タスクパラメータ
   */
  static void blinkTask(void* pvParameters);
};
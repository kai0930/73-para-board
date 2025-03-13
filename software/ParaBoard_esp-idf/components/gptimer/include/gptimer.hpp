#pragma once

#include "driver/gptimer.h"
#include "esp_attr.h"

// APBクロック80Mhzを使用
class GPTimer {
 private:
  gptimer_handle_t timer;
  gptimer_alarm_config_t alarm_config;
  gptimer_alarm_cb_t userCallback;
  static IRAM_ATTR bool internalCallback(
      gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata,
      void *user_ctx);

 public:
  GPTimer();

  bool init(uint32_t resolution_hz, uint64_t alarm_count);
  bool registerCallback(gptimer_alarm_cb_t callback);
  bool start();
  bool stop();
};
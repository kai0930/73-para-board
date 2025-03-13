#include "gptimer.hpp"

GPTimer::GPTimer() : timer(nullptr), userCallback(nullptr) {}

bool GPTimer::init(uint32_t resolution_hz, uint64_t alarm_count) {
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = resolution_hz,
  };
  if (gptimer_new_timer(&timer_config, &timer) != ESP_OK) {
    return false;
  }

  alarm_config = {.alarm_count = alarm_count,
                  .reload_count = 0,
                  .flags = {.auto_reload_on_alarm = true}};
  if (gptimer_set_alarm_action(timer, &alarm_config) != ESP_OK) {
    return false;
  }

  return true;
}

bool GPTimer::registerCallback(gptimer_alarm_cb_t callback) {
  userCallback = callback;
  gptimer_event_callbacks_t cbs = {.on_alarm = internalCallback};
  return (gptimer_register_event_callbacks(timer, &cbs, this) == ESP_OK);
}

bool GPTimer::start() {
  return (gptimer_enable(timer) == ESP_OK && gptimer_start(timer) == ESP_OK);
}

bool GPTimer::stop() {
  bool stopped = (gptimer_stop(timer) == ESP_OK);
  return (stopped && gptimer_disable(timer) == ESP_OK);
}

IRAM_ATTR bool GPTimer::internalCallback(
    gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata,
    void *user_ctx) {
  GPTimer *gpTimer = static_cast<GPTimer *>(user_ctx);
  if (gpTimer->userCallback) {
    return gpTimer->userCallback(timer, edata, user_ctx);
  }
  return false;
}

#include <stdio.h>
#include <string.h>

#include "CanComm.hpp"
#include "config.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CAN_TEST";

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "CANテスト開始 - 受信のみ");

  // CANコントローラの初期化
  CanComm can(BoardID::PARA, config::pins.CAN_TX, config::pins.CAN_RX);

  // CANドライバの開始
  esp_err_t res = can.begin();
  if (res != ESP_OK) {
    ESP_LOGE(TAG, "CANドライバの初期化に失敗: %d", res);
    return;
  }
  ESP_LOGI(TAG, "CANドライバ初期化成功");

  // 受信用の変数
  CanRxFrame rx_frame;

  // メインループ - 受信のみ
  while (1) {
    // 非ブロッキングでメッセージを受信
    res = can.readFrameNoWait(rx_frame);

    if (res == ESP_OK) {
      // メッセージ受信成功
      ESP_LOGI(TAG, "受信: 送信元=%d, コンテンツID=%d, データ長=%d",
               static_cast<int>(rx_frame.sender_board_id),
               static_cast<int>(rx_frame.content_id), rx_frame.dlc);

      // データ内容を16進数で表示
      printf("データ: ");
      for (int i = 0; i < rx_frame.dlc; i++) {
        printf("%02X ", rx_frame.data[i]);
      }
      printf("\n");

      // テキストとして表示（可能な場合）
      if (rx_frame.dlc > 0) {
        char text[9] = {0};  // 最大8バイト + 終端文字
        memcpy(text, rx_frame.data, rx_frame.dlc);
        printf("テキスト: %s\n", text);
      }
    } else if (res != ESP_ERR_TIMEOUT) {
      // タイムアウト以外のエラー
      ESP_LOGE(TAG, "CAN受信エラー: %d", res);
    }

    // 少し待機
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

#pragma once

#include "esp_log.h"

// グローバル変数の代わりにクラスの静的メンバーとして定義
class CreateSpi {
  public:
    bool begin();
  private:
    static const char* TAG; // ヘッダーでは宣言のみ
};

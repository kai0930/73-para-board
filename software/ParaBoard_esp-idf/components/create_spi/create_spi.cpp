#include "create_spi.hpp"

// クラスの静的メンバーの定義
const char* CreateSpi::TAG = "CREATE SPI";

bool CreateSpi::begin() {
  ESP_LOGI(TAG, "begin");
  return true;
}

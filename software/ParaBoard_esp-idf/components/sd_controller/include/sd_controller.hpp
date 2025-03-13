#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "cJSON.h"
#include "config.hpp"
#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sdmmc_cmd.h"

// 設定項目の型定義
enum class SettingType { INTEGER, FLOAT, BOOLEAN, STRING };

// 設定項目値のユニオン型
union SettingValue {
  int int_value;
  float float_value;
  bool bool_value;
  char* string_value;
};

// 設定項目の構造体
struct SettingItem {
  SettingType type;
  SettingValue value;
  SettingValue default_value;
};

class SdController {
 private:
  static constexpr const char* TAG = "SD_CONTROLLER";
  sdmmc_card_t* card = nullptr;
  FILE* log_file_pointer = nullptr;
  FILE* setting_file_pointer = nullptr;
  bool mounted = false;
  bool high_speed = false;
  std::string mount_point = "/sdcard";
  std::string log_file_prefix = "log-";
  std::string setting_file_name = "setting.json";
  std::string log_file_name = "";
  uint32_t freq_khz = SDMMC_FREQ_DEFAULT;

  static constexpr size_t LOG_BUFFER_SIZE = 4 * 1024;
  char* dmaBuffer = nullptr;

  // 設定項目を格納するマップ
  std::map<std::string, SettingItem> settings;

  // JSONファイル読み込み処理
  bool loadSettingsFromFile();

  // JSONファイル書き込み処理
  bool saveSettingsToFile();

  // デフォルト設定の初期化
  void initDefaultSettings();

 public:
  SdController();
  ~SdController();

  bool begin(bool useHighSpeed = false,
             // 追加: ピンをユーザーが指定したい場合
             int gpio_clk = GPIO_NUM_14, int gpio_cmd = GPIO_NUM_15,
             int gpio_d0 = GPIO_NUM_2, int gpio_d1 = GPIO_NUM_4,
             int gpio_d2 = GPIO_NUM_12, int gpio_d3 = GPIO_NUM_13);

  /**
   * ログファイルを閉じる
   */
  void end();

  // ログ書き込み
  void writeLog(SensorData data);

  // fflush()のラッパ (必要に応じて呼び出し)
  void flush();

  // 設定ファイル操作関連の新規メソッド

  // int型の設定値取得/設定
  int getIntSetting(const std::string& key, int default_value = 0);
  void setIntSetting(const std::string& key, int value);

  // float型の設定値取得/設定
  float getFloatSetting(const std::string& key, float default_value = 0.0f);
  void setFloatSetting(const std::string& key, float value);

  // bool型の設定値取得/設定
  bool getBoolSetting(const std::string& key, bool default_value = false);
  void setBoolSetting(const std::string& key, bool value);

  // string型の設定値取得/設定
  std::string getStringSetting(const std::string& key,
                               const std::string& default_value = "");
  void setStringSetting(const std::string& key, const std::string& value);

  // 全設定の保存
  bool saveSettings();

  // 全設定の再読み込み
  bool reloadSettings();
};
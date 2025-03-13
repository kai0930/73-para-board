#include "sd_controller.hpp"

SdController::SdController() {
  // デフォルト設定の初期化
  initDefaultSettings();
}

SdController::~SdController() { end(); }

void SdController::initDefaultSettings() {
  // デフォルト設定値の初期化
  // サーボオープン角度（整数型）
  SettingItem open_angle;
  open_angle.type = SettingType::INTEGER;
  open_angle.value.int_value = 10;  // 10度
  open_angle.default_value.int_value = 10;
  settings["open-angle"] = open_angle;

  // サーボクローズ角度（整数型）
  SettingItem close_angle;
  close_angle.type = SettingType::INTEGER;
  close_angle.value.int_value = 10;  // 10度
  close_angle.default_value.int_value = 10;
  settings["close-angle"] = close_angle;

  // 真偽値型の設定例
  SettingItem is_logging_mode;
  is_logging_mode.type = SettingType::BOOLEAN;
  is_logging_mode.value.bool_value = true;
  is_logging_mode.default_value.bool_value = true;
  settings["is_logging_mode"] = is_logging_mode;
}

bool SdController::begin(bool useHighSpeed, int gpio_clk, int gpio_cmd,
                         int gpio_d0, int gpio_d1, int gpio_d2, int gpio_d3) {
  if (mounted) {
    ESP_LOGW("SDMMC", "Already mounted");
    return true;
  }

  high_speed = useHighSpeed;
  freq_khz = high_speed ? SDMMC_FREQ_HIGHSPEED : SDMMC_FREQ_DEFAULT;

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = freq_khz;

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  // ユーザーが指定したピンを設定
  slot_config.clk = (gpio_num_t)gpio_clk;
  slot_config.cmd = (gpio_num_t)gpio_cmd;
  slot_config.d0 = (gpio_num_t)gpio_d0;
  slot_config.d1 = (gpio_num_t)gpio_d1;
  slot_config.d2 = (gpio_num_t)gpio_d2;
  slot_config.d3 = (gpio_num_t)gpio_d3;
  slot_config.width = 4;
  slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 10,
      .allocation_unit_size = 16 * 1024,
      .disk_status_check_enable = false};

  ESP_LOGI("SDMMC", "Mounting SD card at %s (freq=%.2f MHz)...",
           mount_point.c_str(), freq_khz / 1000.0f);

  esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point.c_str(), &host,
                                          &slot_config, &mount_config, &card);
  if (ret != ESP_OK) {
    ESP_LOGE("SDMMC", "Mount failed (0x%x)", ret);
    return false;
  }
  mounted = true;
  sdmmc_card_print_info(stdout, card);

  // 設定ファイルを確認し、存在すれば読み込む
  if (access((mount_point + "/" + setting_file_name).c_str(), F_OK) != -1) {
    // 設定ファイルが存在する場合、読み込み
    if (!loadSettingsFromFile()) {
      ESP_LOGW("SDMMC", "Failed to load settings, using defaults");
    }
  } else {
    // 設定ファイルが存在しない場合、デフォルト設定を保存
    if (!saveSettingsToFile()) {
      ESP_LOGW("SDMMC", "Failed to save default settings");
    }
  }

  // ログファイルを開く
  // ログファイルの名前は、log-1.csv, log-2.csv, ...
  // というように連番で作成される
  int file_count = 0;
  while (true) {
    log_file_name = log_file_prefix + std::to_string(file_count + 1) + ".csv";
    if (access((mount_point + "/" + log_file_name).c_str(), F_OK) == -1) {
      break;
    }
    file_count++;
  }

  log_file_pointer = fopen((mount_point + "/" + log_file_name).c_str(), "w");
  if (!log_file_pointer) {
    ESP_LOGE("SDMMC", "Failed to open log file: %s",
             (mount_point + "/" + log_file_name).c_str());
    return false;
  }
  ESP_LOGI("SDMMC", "Log file opened: %s",
           (mount_point + "/" + log_file_name).c_str());

  // DMA対応領域へ大きめのバッファを確保し、setvbuf() に設定
  dmaBuffer = (char*)heap_caps_malloc(LOG_BUFFER_SIZE, MALLOC_CAP_DMA);
  if (dmaBuffer) {
    // _IOFBF: 完全バッファリング、LOG_BUFFER_SIZE: バッファサイズ
    setvbuf(log_file_pointer, dmaBuffer, _IOFBF, LOG_BUFFER_SIZE);
    ESP_LOGI("SDMMC", "Enabled DMA buffer for file (size=%d)", LOG_BUFFER_SIZE);
  } else {
    ESP_LOGW("SDMMC", "Failed to alloc DMA buffer. Using default buffer.");
  }

  // CSVヘッダ等を書いておく
  fprintf(
      log_file_pointer,
      "timestamp(us),accel-ux,accel-dx,accel-uy,accel-dy,accel-uz,accel-dz,"
      "gyro-ux,gyro-dx,gyro-uy,gyro-dy,gyro-uz,gyro-dz,pressure-h,pressure-l,"
      "pressure-xl,temperature-h,temperature-l\n");

  return true;
}

void SdController::end() {
  if (log_file_pointer) {
    fclose(log_file_pointer);
    log_file_pointer = nullptr;
  }
  if (setting_file_pointer) {
    fclose(setting_file_pointer);
    setting_file_pointer = nullptr;
  }
  if (mounted) {
    esp_vfs_fat_sdcard_unmount(mount_point.c_str(), card);
    mounted = false;
  }
  if (dmaBuffer) {
    heap_caps_free(dmaBuffer);
    dmaBuffer = nullptr;
  }
  ESP_LOGI("SDMMC", "Unmounted SD card");

  // 文字列型の設定値のメモリを解放
  for (auto& item : settings) {
    if (item.second.type == SettingType::STRING) {
      if (item.second.value.string_value) {
        free(item.second.value.string_value);
        item.second.value.string_value = nullptr;
      }
      if (item.second.default_value.string_value) {
        free(item.second.default_value.string_value);
        item.second.default_value.string_value = nullptr;
      }
    }
  }

  settings.clear();
}

void SdController::writeLog(SensorData data) {
  if (!log_file_pointer) return;
  fprintf(log_file_pointer,
          "%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
          (long long unsigned)data.timestamp_us, data.accel.u_x, data.accel.d_x,
          data.accel.u_y, data.accel.d_y, data.accel.u_z, data.accel.d_z,
          data.gyro.u_x, data.gyro.d_x, data.gyro.u_y, data.gyro.d_y,
          data.gyro.u_z, data.gyro.d_z, data.pressure.h_p, data.pressure.l_p,
          data.pressure.xl_p, data.temperature.h_t, data.temperature.l_t);
}

void SdController::flush() {
  if (!log_file_pointer) return;

  // 1) fflushでライブラリバッファをクリア
  fflush(log_file_pointer);

  // 2) fsyncでOSレベルのキャッシュを物理メディアへ書き込み
  int fd = fileno(log_file_pointer);  // ファイルディスクリプタ取得
  if (fd >= 0) {
    fsync(fd);
  }
}

bool SdController::loadSettingsFromFile() {
  if (!mounted) {
    ESP_LOGW("SDMMC", "Cannot load settings, SD card not mounted");
    return false;
  }

  FILE* fp = fopen((mount_point + "/" + setting_file_name).c_str(), "r");
  if (!fp) {
    ESP_LOGE("SDMMC", "Failed to open settings file for reading");
    return false;
  }

  // ファイルサイズを取得
  fseek(fp, 0, SEEK_END);
  long fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (fileSize <= 0) {
    ESP_LOGE("SDMMC", "Settings file is empty");
    fclose(fp);
    return false;
  }

  // ファイル内容を読み込むバッファを確保
  char* buffer = (char*)malloc(fileSize + 1);
  if (!buffer) {
    ESP_LOGE("SDMMC", "Failed to allocate memory for settings file");
    fclose(fp);
    return false;
  }

  // ファイル内容を読み込む
  size_t readSize = fread(buffer, 1, fileSize, fp);
  fclose(fp);

  if (readSize != (size_t)fileSize) {
    ESP_LOGE("SDMMC", "Failed to read entire settings file");
    free(buffer);
    return false;
  }

  buffer[readSize] = '\0';  // JSON解析のために終端を追加

  // JSONをパース
  cJSON* root = cJSON_Parse(buffer);
  free(buffer);

  if (!root) {
    ESP_LOGE("SDMMC", "Failed to parse settings JSON: %s", cJSON_GetErrorPtr());
    return false;
  }

  // 各設定項目を読み込む
  for (auto& item : settings) {
    const std::string& key = item.first;
    SettingItem& setting = item.second;

    cJSON* json_item = cJSON_GetObjectItem(root, key.c_str());
    if (!json_item) {
      ESP_LOGW("SDMMC", "Setting '%s' not found in file, using default",
               key.c_str());
      continue;
    }

    switch (setting.type) {
      case SettingType::INTEGER:
        if (cJSON_IsNumber(json_item)) {
          setting.value.int_value = json_item->valueint;
        }
        break;

      case SettingType::FLOAT:
        if (cJSON_IsNumber(json_item)) {
          setting.value.float_value = (float)json_item->valuedouble;
        }
        break;

      case SettingType::BOOLEAN:
        if (cJSON_IsBool(json_item)) {
          setting.value.bool_value = cJSON_IsTrue(json_item);
        }
        break;

      case SettingType::STRING:
        if (cJSON_IsString(json_item)) {
          // 既存の文字列を解放
          if (setting.value.string_value) {
            free(setting.value.string_value);
          }
          // 新しい文字列を複製
          setting.value.string_value = strdup(json_item->valuestring);
        }
        break;
    }
  }

  cJSON_Delete(root);
  ESP_LOGI("SDMMC", "Settings loaded successfully from %s",
           (mount_point + "/" + setting_file_name).c_str());
  return true;
}

bool SdController::saveSettingsToFile() {
  if (!mounted) {
    ESP_LOGW("SDMMC", "Cannot save settings, SD card not mounted");
    return false;
  }

  // JSON構造体を作成
  cJSON* root = cJSON_CreateObject();
  if (!root) {
    ESP_LOGE("SDMMC", "Failed to create JSON object");
    return false;
  }

  // 各設定項目をJSONに追加
  for (const auto& item : settings) {
    const std::string& key = item.first;
    const SettingItem& setting = item.second;

    switch (setting.type) {
      case SettingType::INTEGER:
        cJSON_AddNumberToObject(root, key.c_str(), setting.value.int_value);
        break;

      case SettingType::FLOAT:
        cJSON_AddNumberToObject(root, key.c_str(), setting.value.float_value);
        break;

      case SettingType::BOOLEAN:
        cJSON_AddBoolToObject(root, key.c_str(), setting.value.bool_value);
        break;

      case SettingType::STRING:
        if (setting.value.string_value) {
          cJSON_AddStringToObject(root, key.c_str(),
                                  setting.value.string_value);
        } else {
          cJSON_AddStringToObject(root, key.c_str(), "");
        }
        break;
    }
  }

  // JSONをフォーマットされた文字列に変換
  char* json_str = cJSON_Print(root);
  cJSON_Delete(root);

  if (!json_str) {
    ESP_LOGE("SDMMC", "Failed to print JSON to string");
    return false;
  }

  // ファイルに書き込み
  FILE* fp = fopen((mount_point + "/" + setting_file_name).c_str(), "w");
  if (!fp) {
    ESP_LOGE("SDMMC", "Failed to open settings file for writing");
    free(json_str);
    return false;
  }

  fprintf(fp, "%s", json_str);
  fclose(fp);
  free(json_str);

  ESP_LOGI("SDMMC", "Settings saved successfully to %s",
           (mount_point + "/" + setting_file_name).c_str());
  return true;
}

// 各型の設定値取得・設定メソッド

int SdController::getIntSetting(const std::string& key, int default_value) {
  auto it = settings.find(key);
  if (it != settings.end() && it->second.type == SettingType::INTEGER) {
    return it->second.value.int_value;
  }
  return default_value;
}

void SdController::setIntSetting(const std::string& key, int value) {
  auto it = settings.find(key);
  if (it != settings.end() && it->second.type == SettingType::INTEGER) {
    it->second.value.int_value = value;
  } else {
    // キーが存在しない場合は新規作成
    SettingItem setting;
    setting.type = SettingType::INTEGER;
    setting.value.int_value = value;
    setting.default_value.int_value = value;
    settings[key] = setting;
  }
}

float SdController::getFloatSetting(const std::string& key,
                                    float default_value) {
  auto it = settings.find(key);
  if (it != settings.end() && it->second.type == SettingType::FLOAT) {
    return it->second.value.float_value;
  }
  return default_value;
}

void SdController::setFloatSetting(const std::string& key, float value) {
  auto it = settings.find(key);
  if (it != settings.end() && it->second.type == SettingType::FLOAT) {
    it->second.value.float_value = value;
  } else {
    // キーが存在しない場合は新規作成
    SettingItem setting;
    setting.type = SettingType::FLOAT;
    setting.value.float_value = value;
    setting.default_value.float_value = value;
    settings[key] = setting;
  }
}

bool SdController::getBoolSetting(const std::string& key, bool default_value) {
  auto it = settings.find(key);
  if (it != settings.end() && it->second.type == SettingType::BOOLEAN) {
    return it->second.value.bool_value;
  }
  return default_value;
}

void SdController::setBoolSetting(const std::string& key, bool value) {
  auto it = settings.find(key);
  if (it != settings.end() && it->second.type == SettingType::BOOLEAN) {
    it->second.value.bool_value = value;
  } else {
    // キーが存在しない場合は新規作成
    SettingItem setting;
    setting.type = SettingType::BOOLEAN;
    setting.value.bool_value = value;
    setting.default_value.bool_value = value;
    settings[key] = setting;
  }
}

std::string SdController::getStringSetting(const std::string& key,
                                           const std::string& default_value) {
  auto it = settings.find(key);
  if (it != settings.end() && it->second.type == SettingType::STRING &&
      it->second.value.string_value) {
    return std::string(it->second.value.string_value);
  }
  return default_value;
}

void SdController::setStringSetting(const std::string& key,
                                    const std::string& value) {
  auto it = settings.find(key);
  if (it != settings.end() && it->second.type == SettingType::STRING) {
    // 既存の文字列を解放
    if (it->second.value.string_value) {
      free(it->second.value.string_value);
    }
    // 新しい文字列を複製
    it->second.value.string_value = strdup(value.c_str());
  } else {
    // キーが存在しない場合は新規作成
    SettingItem setting;
    setting.type = SettingType::STRING;
    setting.value.string_value = strdup(value.c_str());
    setting.default_value.string_value = strdup(value.c_str());
    settings[key] = setting;
  }
}

bool SdController::saveSettings() { return saveSettingsToFile(); }

bool SdController::reloadSettings() { return loadSettingsFromFile(); }
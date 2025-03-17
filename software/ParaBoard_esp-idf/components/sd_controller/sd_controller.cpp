#include "sd_controller.hpp"

SdController::SdController() {
  // ファイル操作用ミューテックスの作成
  file_mutex = xSemaphoreCreateMutex();
  if (!file_mutex) {
    ESP_LOGE(TAG, "Failed to create file mutex");
  }

  // デフォルト設定の初期化
  initDefaultSettings();
}

SdController::~SdController() {
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

  // 設定バックアップのメモリも解放
  for (auto& item : settings_backup) {
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

  end();

  // ミューテックスの解放
  if (file_mutex) {
    vSemaphoreDelete(file_mutex);
    file_mutex = nullptr;
  }
}

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

  // ログモード（真偽値型）
  SettingItem is_logging_mode;
  is_logging_mode.type = SettingType::BOOLEAN;
  is_logging_mode.value.bool_value = true;
  is_logging_mode.default_value.bool_value = true;
  settings["is_logging_mode"] = is_logging_mode;

  // 通信モード（文字列型）
  SettingItem comm_mode;
  comm_mode.type = SettingType::STRING;
  comm_mode.value.string_value = strdup("can");
  comm_mode.default_value.string_value = strdup("can");
  settings["comm_mode"] = comm_mode;
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

  // ミューテックスで保護
  bool mutex_taken = false;
  if (file_mutex) {
    mutex_taken = (xSemaphoreTake(file_mutex, pdMS_TO_TICKS(1000)) == pdTRUE);
    if (!mutex_taken) {
      ESP_LOGE(TAG, "Failed to take file mutex, aborting settings load");
      return false;
    }
  }

  std::string file_path = mount_point + "/" + setting_file_name;
  FILE* fp = fopen(file_path.c_str(), "r");
  if (!fp) {
    logFileError("open for reading", file_path.c_str());
    if (mutex_taken) xSemaphoreGive(file_mutex);
    return false;
  }

  // ファイルサイズを取得
  fseek(fp, 0, SEEK_END);
  long fileSize = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  if (fileSize <= 0) {
    ESP_LOGE(TAG, "Settings file is empty");
    fclose(fp);
    if (mutex_taken) xSemaphoreGive(file_mutex);
    return false;
  }

  // ファイル内容を読み込むバッファを確保
  char* buffer = (char*)malloc(fileSize + 1);
  if (!buffer) {
    ESP_LOGE(TAG, "Failed to allocate memory for settings file");
    fclose(fp);
    if (mutex_taken) xSemaphoreGive(file_mutex);
    return false;
  }

  // ファイル内容を読み込む
  size_t readSize = fread(buffer, 1, fileSize, fp);
  fclose(fp);

  // ミューテックスを解放（ファイル操作完了）
  if (mutex_taken) {
    xSemaphoreGive(file_mutex);
  }

  if (readSize != (size_t)fileSize) {
    ESP_LOGE(TAG, "Failed to read entire settings file");
    free(buffer);
    return false;
  }

  buffer[readSize] = '\0';  // JSON解析のために終端を追加

  // JSONをパース
  cJSON* root = cJSON_Parse(buffer);
  free(buffer);

  if (!root) {
    ESP_LOGE(TAG, "Failed to parse settings JSON: %s", cJSON_GetErrorPtr());
    return false;
  }

  // 設定のバックアップを作成
  backupSettings();

  // 各設定項目を読み込む
  for (auto& item : settings) {
    const std::string& key = item.first;
    SettingItem& setting = item.second;

    cJSON* json_item = cJSON_GetObjectItem(root, key.c_str());
    if (!json_item) {
      ESP_LOGW(TAG, "Setting '%s' not found in file, using default",
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
  ESP_LOGI(TAG, "Settings loaded successfully from %s", file_path.c_str());
  return true;
}

bool SdController::saveSettingsToFile() {
  if (!mounted) {
    ESP_LOGW("SDMMC", "Cannot save settings, SD card not mounted");
    return false;
  }

  // 安全な実装を使用
  return safeWriteSettingsToFile();
}

// 新しいメソッド: ファイルシステム状態チェック
bool SdController::checkFileSystemStatus() {
  if (!mounted) {
    ESP_LOGW(TAG, "Cannot check file system, SD card not mounted");
    return false;
  }

  // SDカードの状態を確認
  if (!card) {
    ESP_LOGE(TAG, "SD card not initialized");
    return false;
  }

  // SDカードの容量と使用可能な空き容量を確認
  FATFS* fs;
  DWORD free_clusters;
  FRESULT res = f_getfree("0:", &free_clusters, &fs);

  if (res != FR_OK) {
    ESP_LOGE(TAG, "Failed to get filesystem info (error %d)", res);
    return false;
  }

  // 総容量と空き容量を計算（バイト単位）
  uint64_t total_bytes =
      (uint64_t)(fs->n_fatent - 2) * (uint64_t)fs->csize * (uint64_t)FF_MAX_SS;
  uint64_t free_bytes =
      (uint64_t)free_clusters * (uint64_t)fs->csize * (uint64_t)FF_MAX_SS;

  // 最低限必要な空き容量（例: 1MB）
  const uint64_t min_required = 1024 * 1024;

  if (free_bytes < min_required) {
    ESP_LOGE(TAG,
             "Low disk space: %llu bytes available, %llu required (%llu total)",
             free_bytes, min_required, total_bytes);
    return false;
  }

  ESP_LOGI(TAG, "Filesystem check: %llu bytes free of %llu total", free_bytes,
           total_bytes);
  return true;
}

// 新しいメソッド: エラー情報をログに記録
void SdController::logFileError(const char* operation, const char* filename) {
  ESP_LOGE(TAG, "File operation '%s' failed on '%s': %s (errno=%d)", operation,
           filename, strerror(errno), errno);
}

// 新しいメソッド: 設定のバックアップを作成
void SdController::backupSettings() {
  // 現在の設定をバックアップ
  settings_backup.clear();

  for (const auto& item : settings) {
    const std::string& key = item.first;
    const SettingItem& setting = item.second;

    SettingItem backup_item;
    backup_item.type = setting.type;

    switch (setting.type) {
      case SettingType::INTEGER:
        backup_item.value.int_value = setting.value.int_value;
        backup_item.default_value.int_value = setting.default_value.int_value;
        break;

      case SettingType::FLOAT:
        backup_item.value.float_value = setting.value.float_value;
        backup_item.default_value.float_value =
            setting.default_value.float_value;
        break;

      case SettingType::BOOLEAN:
        backup_item.value.bool_value = setting.value.bool_value;
        backup_item.default_value.bool_value = setting.default_value.bool_value;
        break;

      case SettingType::STRING:
        if (setting.value.string_value) {
          backup_item.value.string_value = strdup(setting.value.string_value);
        } else {
          backup_item.value.string_value = nullptr;
        }

        if (setting.default_value.string_value) {
          backup_item.default_value.string_value =
              strdup(setting.default_value.string_value);
        } else {
          backup_item.default_value.string_value = nullptr;
        }
        break;
    }

    settings_backup[key] = backup_item;
  }

  ESP_LOGI(TAG, "Settings backup created (%zu items)", settings_backup.size());
}

// 新しいメソッド: バックアップから設定を復元
void SdController::restoreSettingsFromBackup() {
  if (settings_backup.empty()) {
    ESP_LOGW(TAG, "No settings backup available to restore");
    return;
  }

  // 現在の設定を解放（特に文字列型）
  for (auto& item : settings) {
    if (item.second.type == SettingType::STRING &&
        item.second.value.string_value) {
      free(item.second.value.string_value);
      item.second.value.string_value = nullptr;
    }
  }

  settings.clear();

  // バックアップから復元
  for (const auto& item : settings_backup) {
    const std::string& key = item.first;
    const SettingItem& backup_item = item.second;

    SettingItem restored_item;
    restored_item.type = backup_item.type;

    switch (backup_item.type) {
      case SettingType::INTEGER:
        restored_item.value.int_value = backup_item.value.int_value;
        restored_item.default_value.int_value =
            backup_item.default_value.int_value;
        break;

      case SettingType::FLOAT:
        restored_item.value.float_value = backup_item.value.float_value;
        restored_item.default_value.float_value =
            backup_item.default_value.float_value;
        break;

      case SettingType::BOOLEAN:
        restored_item.value.bool_value = backup_item.value.bool_value;
        restored_item.default_value.bool_value =
            backup_item.default_value.bool_value;
        break;

      case SettingType::STRING:
        if (backup_item.value.string_value) {
          restored_item.value.string_value =
              strdup(backup_item.value.string_value);
        } else {
          restored_item.value.string_value = nullptr;
        }

        if (backup_item.default_value.string_value) {
          restored_item.default_value.string_value =
              strdup(backup_item.default_value.string_value);
        } else {
          restored_item.default_value.string_value = nullptr;
        }
        break;
    }

    settings[key] = restored_item;
  }

  ESP_LOGI(TAG, "Settings restored from backup (%zu items)", settings.size());
}

// 新しいメソッド: 安全なファイル書き込み処理（一時ファイル使用）
bool SdController::safeWriteSettingsToFile() {
  if (!mounted) {
    ESP_LOGW(TAG, "Cannot save settings, SD card not mounted");
    return false;
  }

  // ファイルシステムの状態チェック
  if (!checkFileSystemStatus()) {
    ESP_LOGE(TAG, "File system check failed, aborting settings save");
    return false;
  }

  // 設定のバックアップを作成
  backupSettings();

  // JSON構造体を作成
  cJSON* root = cJSON_CreateObject();
  if (!root) {
    ESP_LOGE(TAG, "Failed to create JSON object");
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
    ESP_LOGE(TAG, "Failed to print JSON to string");
    return false;
  }

  // 一時ファイル名
  std::string temp_file = mount_point + "/setting.tmp";
  std::string target_file = mount_point + "/" + setting_file_name;

  // ログバッファをフラッシュして、他のファイル操作との競合を減らす
  flush();

  // ミューテックスで保護
  if (file_mutex && xSemaphoreTake(file_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to take file mutex, aborting settings save");
    free(json_str);
    return false;
  }

  bool success = false;

  // 一時ファイルに書き込み
  FILE* fp = fopen(temp_file.c_str(), "w");
  if (!fp) {
    logFileError("open temp file", temp_file.c_str());
    goto cleanup;
  }

  // ファイルに書き込み
  if (fprintf(fp, "%s", json_str) < 0) {
    logFileError("write", temp_file.c_str());
    fclose(fp);
    goto cleanup;
  }

  // ファイルをフラッシュして確実にディスクに書き込む
  fflush(fp);
  fsync(fileno(fp));
  fclose(fp);

  // 既存のファイルがあれば削除
  if (access(target_file.c_str(), F_OK) != -1) {
    if (unlink(target_file.c_str()) != 0) {
      logFileError("unlink", target_file.c_str());
      goto cleanup;
    }
  }

  // 一時ファイルを本ファイルにリネーム
  if (rename(temp_file.c_str(), target_file.c_str()) != 0) {
    logFileError("rename", temp_file.c_str());
    goto cleanup;
  }

  ESP_LOGI(TAG, "Settings saved successfully to %s", target_file.c_str());
  success = true;

cleanup:
  // 失敗した場合、一時ファイルを削除
  if (!success && access(temp_file.c_str(), F_OK) != -1) {
    unlink(temp_file.c_str());
  }

  free(json_str);

  // ミューテックスを解放
  if (file_mutex) {
    xSemaphoreGive(file_mutex);
  }

  return success;
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
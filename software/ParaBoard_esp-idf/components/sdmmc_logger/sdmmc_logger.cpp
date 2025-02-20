#include "sdmmc_logger.hpp"

bool SdmmcLogger::begin(bool useHighSpeed, const char *mountPoint, const char *logFile, int gpio_clk, int gpio_cmd, int gpio_d0, int gpio_d1, int gpio_d2,
                        int gpio_d3) {
    if (mounted) {
        ESP_LOGW("SDMMC", "Already mounted");
        return true;
    }

    mount_point = mountPoint;
    log_path = logFile;
    high_speed = useHighSpeed;
    freq_khz = high_speed ? SDMMC_FREQ_HIGHSPEED : SDMMC_FREQ_DEFAULT;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = freq_khz;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    // ▼ユーザーが指定したピンを設定
    slot_config.clk = (gpio_num_t)gpio_clk;
    slot_config.cmd = (gpio_num_t)gpio_cmd;
    slot_config.d0 = (gpio_num_t)gpio_d0;
    slot_config.d1 = (gpio_num_t)gpio_d1;
    slot_config.d2 = (gpio_num_t)gpio_d2;
    slot_config.d3 = (gpio_num_t)gpio_d3;
    slot_config.width = 4;
    slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024, .disk_status_check_enable = false};

    ESP_LOGI("SDMMC", "Mounting SD card at %s (freq=%.2f MHz)...", mount_point.c_str(), freq_khz / 1000.0f);

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point.c_str(), &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        ESP_LOGE("SDMMC", "Mount failed (0x%x)", ret);
        return false;
    }
    mounted = true;
    sdmmc_card_print_info(stdout, card);

    // ログファイルを開く
    file_pointer = fopen(log_path.c_str(), "w");
    if (!file_pointer) {
        ESP_LOGE("SDMMC", "Failed to open log file: %s", log_path.c_str());
        return false;
    }
    ESP_LOGI("SDMMC", "Log file opened: %s", log_path.c_str());

    // ▼ DMA対応領域へ大きめのバッファを確保し、setvbuf() に設定
    dmaBuffer = (char *)heap_caps_malloc(LOG_BUFFER_SIZE, MALLOC_CAP_DMA);
    if (dmaBuffer) {
        // _IOFBF: 完全バッファリング、LOG_BUFFER_SIZE: バッファサイズ
        setvbuf(file_pointer, dmaBuffer, _IOFBF, LOG_BUFFER_SIZE);
        ESP_LOGI("SDMMC", "Enabled DMA buffer for file (size=%d)", LOG_BUFFER_SIZE);
    } else {
        ESP_LOGW("SDMMC", "Failed to alloc DMA buffer. Using default buffer.");
    }

    // CSVヘッダ等を書いておく
    fprintf(file_pointer, "timestamp(us),accel-x,accel-y,accel-z,gyro-x,gyro-y,gyro-z,pressure\n");

    return true;
}

void SdmmcLogger::end() {
    if (file_pointer) {
        fclose(file_pointer);
        file_pointer = nullptr;
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
}

void SdmmcLogger::writeLog(uint64_t timestampUs, int16_t accel_x_raw, int16_t accel_y_raw, int16_t accel_z_raw, int16_t gyro_x_raw, int16_t gyro_y_raw,
                           int16_t gyro_z_raw, int16_t pressure_row) {
    if (!file_pointer) return;
    fprintf(file_pointer, "%llu,%d,%d,%d,%d,%d,%d,%d\n", (long long unsigned)timestampUs, accel_x_raw, accel_y_raw, accel_z_raw, gyro_x_raw, gyro_y_raw,
            gyro_z_raw, pressure_row);
}

void SdmmcLogger::flush() {
    if (!file_pointer) return;

    // ★ 1) fflushでライブラリバッファをクリア
    fflush(file_pointer);

    // ★ 2) fsyncでOSレベルのキャッシュを物理メディアへ書き込み
    int fd = fileno(file_pointer);  // ファイルディスクリプタ取得
    if (fd >= 0) {
        fsync(fd);
    }
}

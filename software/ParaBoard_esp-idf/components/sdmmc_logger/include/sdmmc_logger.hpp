#pragma once

#include <unistd.h>  // ★追加: fsync用

#include <cstdio>
#include <cstring>
#include <string>

#include "driver/sdmmc_host.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

class SdmmcLogger {
   private:
    sdmmc_card_t *card = nullptr;
    FILE *file_pointer = nullptr;
    bool mounted = false;
    bool high_speed = false;
    std::string mount_point = "/sdcard";
    std::string log_path = "/sdcard/log.csv";
    uint32_t freq_khz = SDMMC_FREQ_DEFAULT;

    static constexpr size_t LOG_BUFFER_SIZE = 4 * 1024;
    char *dmaBuffer = nullptr;

   public:
    bool begin(bool useHighSpeed = false, const char *mountPoint = "/sdcard", const char *logFile = "/sdcard/log.csv",
               // 追加: ピンをユーザーが指定したい場合
               int gpio_clk = GPIO_NUM_14, int gpio_cmd = GPIO_NUM_15, int gpio_d0 = GPIO_NUM_2, int gpio_d1 = GPIO_NUM_4, int gpio_d2 = GPIO_NUM_12,
               int gpio_d3 = GPIO_NUM_13);

    /**
     * ログファイルを閉じる
     */
    void end();

    // ログ書き込み
    void writeLog(uint64_t timestampUs, int16_t accel_x_raw, int16_t accel_y_raw, int16_t accel_z_raw, int16_t gyro_x_raw, int16_t gyro_y_raw,
                  int16_t gyro_z_raw, int16_t pressure_row);

    // fflush()のラッパ (必要に応じて呼び出し)
    void flush();
};

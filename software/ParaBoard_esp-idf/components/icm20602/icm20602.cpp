#include "icm20602.hpp"

const char *Icm20602::TAG = "Icm20602";

IRAM_ATTR bool Icm20602::begin(CreateSpi *spi, gpio_num_t cs_pin,
                               uint32_t frequency) {
  create_spi = spi;
  this->cs_pin = cs_pin;

  // デバイスハンドルのチェックを追加
  if (!create_spi) {
    ESP_LOGE(TAG, "SPI instance is null");
    return false;
  }

  spi_device_interface_config_t device_if_config = {};

  device_if_config.pre_cb = nullptr;
  device_if_config.post_cb = nullptr;
  device_if_config.cs_ena_pretrans = 0;
  device_if_config.cs_ena_posttrans = 0;
  device_if_config.clock_speed_hz = frequency;
  device_if_config.mode = 3;
  device_if_config.queue_size = 1;
  device_if_config.spics_io_num = cs_pin;
  device_if_config.pre_cb = create_spi->csReset;
  device_if_config.post_cb = create_spi->csSet;

  device_handle_id = create_spi->addDevice(&device_if_config, cs_pin);
  ESP_LOGI(TAG, "Device added, handle_id: %d", device_handle_id);
  if (device_handle_id < 0) {
    ESP_LOGE(TAG, "Failed to add device");
    return false;
  }
  ESP_LOGI(TAG, "Device added, handle_id: %d", device_handle_id);

  // I2Cインターフェースを無効化
  create_spi->setReg(Icm20602Config::Registers::I2C_IF, 0x40, device_handle_id);
  // センサーの基本設定
  create_spi->setReg(Icm20602Config::Registers::CONFIG, 0x00, device_handle_id);
  create_spi->setReg(Icm20602Config::Registers::PWR_MGMT_1, 0x80,
                     device_handle_id);
  vTaskDelay(pdMS_TO_TICKS(100)); // リセット待機

  // クロックソースの設定
  create_spi->setReg(Icm20602Config::Registers::PWR_MGMT_1, 0x01,
                     device_handle_id);

  // 加速度センサーとジャイロの設定を追加
  create_spi->setReg(Icm20602Config::Registers::ACC_CONFIG,
                     Icm20602Config::AccelScale::G16, device_handle_id);
  create_spi->setReg(Icm20602Config::Registers::GYRO_CONFIG,
                     Icm20602Config::GyroScale::DPS2000, device_handle_id);

  // レジスタ設定の確認
  uint8_t who_am_i = whoAmI();
  ESP_LOGI(TAG, "WHO_AM_I: 0x%02x", who_am_i);
  if (who_am_i != 0x12) { // ICM20602の場合
    ESP_LOGE(TAG, "Invalid WHO_AM_I value");
    return false;
  }
  return true;
}

IRAM_ATTR uint8_t Icm20602::whoAmI() {
  return create_spi->readByte(0x80 | Icm20602Config::Registers::WHO_AM_I,
                              device_handle_id);
}

IRAM_ATTR void Icm20602::get(Icm20602Data *data) {
  spi_transaction_t transaction = {};
  transaction.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
  transaction.length = (14) * 8;

  uint8_t rx_buffer[14];
  transaction.cmd = 0x80 | Icm20602Config::Registers::DATA;
  transaction.tx_buffer = nullptr;
  transaction.rx_buffer = rx_buffer;
  transaction.user = (void *)cs_pin;

  spi_transaction_ext_t spi_transaction = {};
  spi_transaction.base = transaction;
  spi_transaction.command_bits = 8;
  create_spi->pollTransmit((spi_transaction_t *)&spi_transaction,
                           device_handle_id);

  // 生データを物理単位に変換して格納
  data->accel.x =
      (int16_t)(rx_buffer[0] << 8 | rx_buffer[1]) / 2048.0f; // G単位
  data->accel.y = (int16_t)(rx_buffer[2] << 8 | rx_buffer[3]) / 2048.0f;
  data->accel.z = (int16_t)(rx_buffer[4] << 8 | rx_buffer[5]) / 2048.0f;

  data->gyro.x = (int16_t)(rx_buffer[8] << 8 | rx_buffer[9]) / 16.4f; // dps単位
  data->gyro.y = (int16_t)(rx_buffer[10] << 8 | rx_buffer[11]) / 16.4f;
  data->gyro.z = (int16_t)(rx_buffer[12] << 8 | rx_buffer[13]) / 16.4f;

  data->accel_norm =
      sqrt(data->accel.x * data->accel.x + data->accel.y * data->accel.y +
           data->accel.z * data->accel.z);
}

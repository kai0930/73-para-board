#include "icm42688.hpp"

namespace Icm {

const char *Icm42688::TAG = "Icm42688";

bool Icm42688::begin(CreateSpi *spi, gpio_num_t cs_pin, uint32_t frequency) {
  create_spi = spi;
  this->cs_pin = cs_pin;

  if (!create_spi) {
    ESP_LOGE(TAG, "SPI instance is null");
    return false;
  }

  spi_device_interface_config_t device_if_config = {};

  device_if_config.cs_ena_pretrans = 0;
  device_if_config.cs_ena_posttrans = 0;
  device_if_config.clock_speed_hz = frequency;
  device_if_config.mode = 3;
  device_if_config.queue_size = 1;

  device_handle_id = create_spi->addDevice(&device_if_config, cs_pin);
  if (device_handle_id < 0) {
    ESP_LOGE(TAG, "Failed to add device");
    return false;
  }
  ESP_LOGI(TAG, "Device added, handle_id: %d", device_handle_id);

  // 1. gyroとaccelセンサーをOFF
  if (!create_spi->setReg(Icm42688Config::Registers::PWR_MGMT0, 0x00,
                          device_handle_id)) {
    ESP_LOGE(TAG, "Failed to set PWR_MGMT0");
    return false;
  }
  esp_rom_delay_us(200);  // 200us間はレジスタ変更禁止

  // 2. Gyroセンサーの設定
  if (!create_spi->setReg(
          Icm42688Config::Registers::GYRO_CONFIG0,
          Icm42688Config::GyroScale::DPS2000 | Icm42688Config::ODR::ODR1k,
          device_handle_id)) {
    ESP_LOGE(TAG, "Failed to set GYRO_CONFIG0");
    return false;
  }

  // 3. Accelセンサーの設定
  if (!create_spi->setReg(
          Icm42688Config::Registers::ACCEL_CONFIG0,
          Icm42688Config::AccelScale::G16 | Icm42688Config::ODR::ODR1k,
          device_handle_id)) {
    ESP_LOGE(TAG, "Failed to set ACCEL_CONFIG0");
    return false;
  }

  // 4. センサーをLNモードでON
  if (!create_spi->setReg(Icm42688Config::Registers::PWR_MGMT0, 0x0F,
                          device_handle_id)) {
    ESP_LOGE(TAG, "Failed to set PWR_MGMT0");
    return false;
  }

  // whoAmIを確認
  uint8_t who_am_i;
  if (!whoAmI(&who_am_i)) {
    ESP_LOGE(TAG, "Failed to get WHO_AM_I");
    return false;
  }
  if (who_am_i != Icm42688Config::WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "Invalid WHO_AM_I value: 0x%02x (expected 0x%02x)", who_am_i,
             Icm42688Config::WHO_AM_I_VALUE);
    return false;
  }
  ESP_LOGI(TAG, "WHO_AM_I: 0x%02x", who_am_i);
  return true;
}

bool Icm42688::whoAmI(uint8_t *data) {
  if (!create_spi->readByte(
          Icm42688Config::READ_BIT | Icm42688Config::Registers::WHO_AM_I,
          device_handle_id, data)) {
    ESP_LOGE(TAG, "Failed to read WHO_AM_I");
    return false;
  }
  return true;
}

bool Icm42688::getAccel(AccelData *data) {
  spi_transaction_t transaction = {};
  transaction.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
  transaction.length = (6) * 8;

  uint8_t rx_buffer[6];
  transaction.cmd =
      Icm42688Config::READ_BIT | Icm42688Config::Registers::ACCEL_DATA;
  transaction.tx_buffer = NULL;
  transaction.rx_buffer = rx_buffer;
  transaction.user = (void *)cs_pin;

  spi_transaction_ext_t spi_transaction = {};
  spi_transaction.base = transaction;
  spi_transaction.command_bits = 8;
  bool result = create_spi->pollTransmit((spi_transaction_t *)&spi_transaction,
                                         device_handle_id);
  if (!result) {
    ESP_LOGE(TAG, "Failed to get accel");
    return false;
  }

  data->u_x = rx_buffer[0];
  data->d_x = rx_buffer[1];
  data->u_y = rx_buffer[2];
  data->d_y = rx_buffer[3];
  data->u_z = rx_buffer[4];
  data->d_z = rx_buffer[5];
  return true;
}

bool Icm42688::getGyro(GyroData *data) {
  spi_transaction_t transaction = {};
  transaction.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
  transaction.length = (6) * 8;

  uint8_t rx_buffer[6];
  transaction.cmd =
      Icm42688Config::READ_BIT | Icm42688Config::Registers::GYRO_DATA;
  transaction.tx_buffer = NULL;
  transaction.rx_buffer = rx_buffer;
  transaction.user = (void *)cs_pin;

  spi_transaction_ext_t spi_transaction = {};
  spi_transaction.base = transaction;
  spi_transaction.command_bits = 8;
  bool result = create_spi->pollTransmit((spi_transaction_t *)&spi_transaction,
                                         device_handle_id);
  if (!result) {
    ESP_LOGE(TAG, "Failed to get gyro");
    return false;
  }

  data->u_x = rx_buffer[0];
  data->d_x = rx_buffer[1];
  data->u_y = rx_buffer[2];
  data->d_y = rx_buffer[3];
  data->u_z = rx_buffer[4];
  data->d_z = rx_buffer[5];
  return true;
}

bool Icm42688::getTemp(IcmTempData *data) {
  spi_transaction_t transaction = {};
  transaction.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
  transaction.length = (2) * 8;

  uint8_t rx_buffer[2];
  transaction.cmd =
      Icm42688Config::READ_BIT | Icm42688Config::Registers::TEMP_DATA;
  transaction.tx_buffer = NULL;
  transaction.rx_buffer = rx_buffer;
  transaction.user = (void *)cs_pin;

  spi_transaction_ext_t spi_transaction = {};
  spi_transaction.base = transaction;
  spi_transaction.command_bits = 8;
  bool result = create_spi->pollTransmit((spi_transaction_t *)&spi_transaction,
                                         device_handle_id);
  if (!result) {
    ESP_LOGE(TAG, "Failed to get temp");
    return false;
  }

  data->u_t = rx_buffer[0];
  data->d_t = rx_buffer[1];
  return true;
}

bool Icm42688::getAccelAndGyro(AccelData *accel, GyroData *gyro) {
  spi_transaction_t transaction = {};
  transaction.flags = SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR;
  transaction.length = (6 + 6) * 8;

  uint8_t rx_buffer[12];
  transaction.cmd =
      Icm42688Config::READ_BIT | Icm42688Config::Registers::ACCEL_DATA;
  transaction.tx_buffer = NULL;
  transaction.rx_buffer = rx_buffer;
  transaction.user = (void *)cs_pin;

  spi_transaction_ext_t spi_transaction = {};
  spi_transaction.base = transaction;
  spi_transaction.command_bits = 8;
  bool result = create_spi->pollTransmit((spi_transaction_t *)&spi_transaction,
                                         device_handle_id);
  if (!result) {
    ESP_LOGE(TAG, "Failed to get accel and gyro");
    return false;
  }

  accel->u_x = rx_buffer[0];
  accel->d_x = rx_buffer[1];
  accel->u_y = rx_buffer[2];
  accel->d_y = rx_buffer[3];
  accel->u_z = rx_buffer[4];
  accel->d_z = rx_buffer[5];

  gyro->u_x = rx_buffer[6];
  gyro->d_x = rx_buffer[7];
  gyro->u_y = rx_buffer[8];
  gyro->d_y = rx_buffer[9];
  gyro->u_z = rx_buffer[10];
  gyro->d_z = rx_buffer[11];
  return true;
}

}  // namespace Icm

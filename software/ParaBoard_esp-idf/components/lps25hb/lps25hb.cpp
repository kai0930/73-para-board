#include "lps25hb.hpp"

const char *Lps25hb::TAG = "Lps25hb";

IRAM_ATTR bool Lps25hb::begin(CreateSpi *spi, gpio_num_t cs_pin,
                              uint32_t frequency) {
  create_spi = spi;
  this->cs_pin = cs_pin;

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
  if (device_handle_id < 0) {
    ESP_LOGE(TAG, "Failed to add device");
    return false;
  }

  // センサーの基本設定
  create_spi->setReg(Lps25hbConfig::Registers::CTRL_REG2,
                     Lps25hbConfig::Settings::BDU_ENABLE, device_handle_id);
  create_spi->setReg(Lps25hbConfig::Registers::CTRL_REG1,
                     Lps25hbConfig::Settings::POWER_UP, device_handle_id);

  // WHO_AM_Iの確認
  uint8_t who_am_i = whoAmI();
  ESP_LOGI(TAG, "WHO_AM_I: 0x%02x", who_am_i);
  if (who_am_i != Lps25hbConfig::WHO_AM_I_VALUE) {
    ESP_LOGE(TAG, "Invalid WHO_AM_I value");
    return false;
  }

  return true;
}

IRAM_ATTR uint8_t Lps25hb::whoAmI() {
  return create_spi->readByte(Lps25hbConfig::READ_BIT |
                                  Lps25hbConfig::Registers::WHO_AM_I,
                              device_handle_id);
}

IRAM_ATTR void Lps25hb::get(Lps25hbData *data) {
  uint8_t pressure_bytes[3];

  // 3バイトの気圧データを読み取り
  pressure_bytes[0] = create_spi->readByte(
      Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_XL,
      device_handle_id);
  pressure_bytes[1] = create_spi->readByte(
      Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_L,
      device_handle_id);
  pressure_bytes[2] = create_spi->readByte(
      Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_H,
      device_handle_id);

  // 24ビットの生データを構築
  data->raw = ((uint32_t)pressure_bytes[2] << 16) |
              ((uint32_t)pressure_bytes[1] << 8) | pressure_bytes[0];

  // hPa単位に変換 (データシートによる)
  data->pressure = data->raw * 100.0f / 4096.0f;
}
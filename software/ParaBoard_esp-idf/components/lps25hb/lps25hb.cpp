#include "lps25hb.hpp"

namespace Lps {

const char *Lps25hb::TAG = "Lps25hb";

bool Lps25hb::begin(CreateSpi *spi, gpio_num_t cs_pin, uint32_t frequency) {
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

    if (!create_spi->setReg(Lps25hbConfig::Registers::CTRL_REG2, Lps25hbConfig::Settings::I2C_DISABLE, device_handle_id)) {
        ESP_LOGE(TAG, "Failed to set CTRL_REG2");
        return false;
    }
    if (!create_spi->setReg(Lps25hbConfig::Registers::CTRL_REG1, Lps25hbConfig::Settings::POWER_UP | Lps25hbConfig::Settings::ODR_25, device_handle_id)) {
        ESP_LOGE(TAG, "Failed to set CTRL_REG1");
        return false;
    }

    // WHO_AM_Iの確認
    uint8_t who_am_i;
    if (!whoAmI(&who_am_i)) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I");
        return false;
    }
    if (who_am_i != Lps25hbConfig::WHO_AM_I_VALUE) {
        ESP_LOGE(TAG, "Invalid WHO_AM_I value: 0x%02x (expected 0x%02x)", who_am_i, Lps25hbConfig::WHO_AM_I_VALUE);
        return false;
    }
    ESP_LOGI(TAG, "WHO_AM_I: 0x%02x", who_am_i);

    return true;
}

bool Lps25hb::whoAmI(uint8_t *data) {
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::WHO_AM_I, device_handle_id, data)) {
        ESP_LOGE(TAG, "Failed to read WHO_AM_I");
        return false;
    }
    return true;
}

bool Lps25hb::getPressure(PressureData *data) {
    uint8_t pressure_bytes[3];

    // 3バイトの気圧データを読み取り
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_XL, device_handle_id, &pressure_bytes[0])) {
        ESP_LOGE(TAG, "Failed to read PRESS_OUT_XL");
        return false;
    }
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_L, device_handle_id, &pressure_bytes[1])) {
        ESP_LOGE(TAG, "Failed to read PRESS_OUT_L");
        return false;
    }
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_H, device_handle_id, &pressure_bytes[2])) {
        ESP_LOGE(TAG, "Failed to read PRESS_OUT_H");
        return false;
    }

    // 24ビットの生データを構築
    data->xl_p = pressure_bytes[0];
    data->l_p = pressure_bytes[1];
    data->h_p = pressure_bytes[2];

    return true;
}

bool Lps25hb::getTemp(TempData *data) {
    uint8_t temp_bytes[2];

    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::TEMP_OUT_L, device_handle_id, &temp_bytes[0])) {
        ESP_LOGE(TAG, "Failed to read TEMP_OUT_L");
        return false;
    }
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::TEMP_OUT_H, device_handle_id, &temp_bytes[1])) {
        ESP_LOGE(TAG, "Failed to read TEMP_OUT_H");
        return false;
    }

    data->l_t = temp_bytes[0];
    data->h_t = temp_bytes[1];

    return true;
}

bool Lps25hb::getPressureAndTemp(PressureData *pressure, TempData *temp) {
    uint8_t pressure_bytes[3];
    uint8_t temp_bytes[2];

    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_XL, device_handle_id, &pressure_bytes[0])) {
        ESP_LOGE(TAG, "Failed to read PRESS_OUT_XL");
        return false;
    }
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_L, device_handle_id, &pressure_bytes[1])) {
        ESP_LOGE(TAG, "Failed to read PRESS_OUT_L");
        return false;
    }
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::PRESS_OUT_H, device_handle_id, &pressure_bytes[2])) {
        ESP_LOGE(TAG, "Failed to read PRESS_OUT_H");
        return false;
    }
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::TEMP_OUT_L, device_handle_id, &temp_bytes[0])) {
        ESP_LOGE(TAG, "Failed to read TEMP_OUT_L");
        return false;
    }
    if (!create_spi->readByte(Lps25hbConfig::READ_BIT | Lps25hbConfig::Registers::TEMP_OUT_H, device_handle_id, &temp_bytes[1])) {
        ESP_LOGE(TAG, "Failed to read TEMP_OUT_H");
        return false;
    }

    pressure->xl_p = pressure_bytes[0];
    pressure->l_p = pressure_bytes[1];
    pressure->h_p = pressure_bytes[2];

    temp->l_t = temp_bytes[0];
    temp->h_t = temp_bytes[1];

    return true;
}

}  // namespace Lps

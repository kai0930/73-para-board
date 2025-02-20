#include "create_spi.hpp"

const char *CreateSpi::TAG = "CREATE SPI";

CreateSpi::CreateSpi() : host(SPI2_HOST), frequency(DEFAULT_SPI_FREQUENCY) {
}

CreateSpi::~CreateSpi() {
    end();
}

bool CreateSpi::begin(spi_host_device_t host_id, int8_t sck, int8_t miso, int8_t mosi, uint32_t frequency) {
    this->frequency = frequency;

    bus_cfg.mosi_io_num = mosi;
    bus_cfg.miso_io_num = miso;
    bus_cfg.sclk_io_num = sck;
    bus_cfg.max_transfer_sz = MAX_TRANSFER_SIZE;

    host = host_id == SPI2_HOST ? SPI2_HOST : SPI3_HOST;

    esp_err_t err = spi_bus_initialize(host, &bus_cfg, SPI_DMA_CH_AUTO);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialization failed: %d", err);
        return false;
    }

    return true;
}

bool CreateSpi::end() {
    for (size_t i = 0; i < devices.size(); i++) {
        rmDevice(i);
    }

    esp_err_t err = spi_bus_free(host);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus free failed: %d", err);
        return false;
    }

    return true;
}

int CreateSpi::addDevice(spi_device_interface_config_t *device_if_cfg, gpio_num_t cs) {
    if (devices.size() >= MAX_CS_PINS) {
        ESP_LOGE(TAG, "Too many devices to add spi bus");
        return -1;
    }

    device_if_cfg->spics_io_num = cs;

    spi_device_handle_t device_handle;
    esp_err_t err = spi_bus_add_device(host, device_if_cfg, &device_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus add device failed: %d", err);
        return -1;
    }

    CS_pins.push_back(cs);
    devices.push_back(device_handle);
    return devices.size() - 1;
}

bool CreateSpi::rmDevice(int device_handle_id) {
    if (device_handle_id < 0 || device_handle_id >= devices.size()) {
        ESP_LOGE(TAG, "Invalid device handle");
        return false;
    }

    esp_err_t err = spi_bus_remove_device(devices[device_handle_id]);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus remove device failed: %d", err);
        return false;
    }

    devices.erase(devices.begin() + device_handle_id);
    CS_pins.erase(CS_pins.begin() + device_handle_id);
    return true;
}

bool CreateSpi::sendData(uint8_t data, int device_handle_id) {
    spi_transaction_t transaction = {};
    transaction.flags = SPI_TRANS_USE_TXDATA;
    transaction.length = 8;
    transaction.user = (void *)(intptr_t)CS_pins[device_handle_id];
    transaction.tx_data[0] = data;

    return pollTransmit(&transaction, device_handle_id);
}

bool CreateSpi::readByte(uint8_t addr, int device_handle_id, uint8_t *data) {
    spi_transaction_t transaction = {};
    transaction.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
    transaction.tx_data[0] = addr;
    transaction.length = 16;

    if (!pollTransmit(&transaction, device_handle_id)) {
        return false;
    }

    *data = transaction.rx_data[1];
    return true;
}

bool CreateSpi::setReg(uint8_t addr, uint8_t data, int device_handle_id) {
    spi_transaction_t transaction = {};
    transaction.flags = SPI_TRANS_USE_TXDATA;
    transaction.length = 16;
    transaction.tx_data[0] = addr;
    transaction.tx_data[1] = data;

    return transmit(&transaction, device_handle_id);
}

bool CreateSpi::transmit(spi_transaction_t *transaction, int device_handle_id) {
    esp_err_t err = spi_device_transmit(devices[device_handle_id], transaction);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI device transmit failed: %d", err);
        return false;
    }
    return true;
}

bool CreateSpi::pollTransmit(spi_transaction_t *transaction, int device_handle_id) {
    esp_err_t err = spi_device_polling_transmit(devices[device_handle_id], transaction);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI device polling transmit failed: %d", err);
        return false;
    }
    return true;
}

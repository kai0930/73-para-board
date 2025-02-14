#pragma once

#include <stdio.h>
#include <string.h>

#include <vector>

#include "config.hpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#define MAX_TRANSFER_SIZE 4094
#define DEFAULT_SPI_FREQUENCY SPI_MASTER_FREQ_8M
#define MAX_CS_PINS 3

class CreateSpi {
   public:
    CreateSpi();
    ~CreateSpi();

    /**
     * @brief Initialize the SPI bus
     * @param host_id The host id for the SPI bus
     * @param sck The sck pin for the SPI bus
     * @param miso The miso pin for the SPI bus
     * @param mosi The mosi pin for the SPI bus
     * @param frequency The frequency for the SPI bus
     */
    bool begin(spi_host_device_t host_id, int8_t sck, int8_t miso, int8_t mosi, uint32_t frequency = DEFAULT_SPI_FREQUENCY);

    /**
     * @brief End the SPI bus
     * @return true if the SPI bus was ended, false otherwise
     */
    bool end();

    /**
     * @brief Add a device to the SPI bus
     * @param device_if_cfg spi_device_interface_config_t
     * @param cs gpio_num_t
     * @return The handle id for the device(for rmDevice), -1 if failed
     */
    int addDevice(spi_device_interface_config_t *device_if_cfg, gpio_num_t cs);

    /**
     * @brief Remove a device from the SPI bus
     * @param device_handle_id The handle id for the device
     * @return true if the device was removed, false otherwise
     */
    bool rmDevice(int device_handle_id);

    /**
     * @brief Read a byte from the device
     * @param addr The address to read from
     * @param device_handle_id The handle id for the device
     * @return The byte read from the device
     */
    uint8_t readByte(uint8_t addr, int device_handle_id);

    /**
     * @brief Send a byte to the device
     * @param data The data to send
     * @param device_handle_id The handle id for the device
     */
    void sendData(uint8_t data, int device_handle_id);

    /**
     * @brief Set a register on the device
     * @param addr The address to set
     * @param data The data to set
     * @param device_handle_id The handle id for the device
     */
    void setReg(uint8_t addr, uint8_t data, int device_handle_id);

    /**
     * @brief Transmit data to the device
     * @param transaction The transaction to transmit
     * @param device_handle_id The handle id for the device
     */
    void transmit(spi_transaction_t *transaction, int device_handle_id);

    /**
     * @brief Poll transmit data to the device
     * @param transaction The transaction to transmit
     * @param device_handle_id The handle id for the device
     */
    void pollTransmit(spi_transaction_t *transaction, int device_handle_id);

   private:
    static const char *TAG;
    spi_bus_config_t bus_cfg = {};
    spi_host_device_t host;
    int dma_chan;
    uint32_t frequency;
    std::vector<gpio_num_t> CS_pins;
    std::vector<spi_device_handle_t> devices;
};

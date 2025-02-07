#include "esp_log.h"
#include "icm20602.hpp"
#include "unity.h"

static const char *TAG = "ICM20602_TEST";

TEST_CASE("ICM20602 Initialization Test", "[icm20602]") {
    CreateSpi spi;
    TEST_ASSERT(spi.begin(SPI2_HOST, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23));

    Icm20602 icm;
    TEST_ASSERT(icm.begin(&spi, GPIO_NUM_5));
    TEST_ASSERT(icm.isInitialized());

    uint8_t who_am_i = icm.whoAmI();
    TEST_ASSERT_EQUAL_HEX8(Icm20602Config::WHO_AM_I_VALUE, who_am_i);
}

TEST_CASE("ICM20602 Data Reading Test", "[icm20602]") {
    CreateSpi spi;
    Icm20602 icm;

    TEST_ASSERT(spi.begin(SPI2_HOST, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_23));
    TEST_ASSERT(icm.begin(&spi, GPIO_NUM_5));

    Icm20602Data data;
    icm.get(&data);

    // 基本的な範囲チェック
    TEST_ASSERT_FLOAT_WITHIN(16.0f, 0.0f, data.accel.x);
    TEST_ASSERT_FLOAT_WITHIN(16.0f, 0.0f, data.accel.y);
    TEST_ASSERT_FLOAT_WITHIN(16.0f, 0.0f, data.accel.z);

    TEST_ASSERT_FLOAT_WITHIN(2000.0f, 0.0f, data.gyro.x);
    TEST_ASSERT_FLOAT_WITHIN(2000.0f, 0.0f, data.gyro.y);
    TEST_ASSERT_FLOAT_WITHIN(2000.0f, 0.0f, data.gyro.z);
}
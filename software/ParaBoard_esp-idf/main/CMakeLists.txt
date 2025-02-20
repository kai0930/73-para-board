idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES create_spi icm42688 lps25hb config esp_timer esp_system gptimer sdmmc_logger esp_timer servo tinyusb tusb_cdc_acm
)

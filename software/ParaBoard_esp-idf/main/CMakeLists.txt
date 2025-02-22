idf_component_register(
  SRCS "app_main.cpp"
  INCLUDE_DIRS "."
  REQUIRES "config create_spi icm42688 lps25hb sdmmc_logger servo driver esp_timer freertos esp_system console"
)

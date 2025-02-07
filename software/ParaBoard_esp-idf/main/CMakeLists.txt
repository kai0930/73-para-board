idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES create_spi icm20602 lps25hb config esp_timer esp_system
)

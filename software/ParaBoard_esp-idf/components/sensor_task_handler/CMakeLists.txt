idf_component_register(
    SRCS "sensor_task_handler.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        icm42688
        lps25hb
        log_task_handler
        condition_checker
        config
        esp_common
        log
        esp_timer
)

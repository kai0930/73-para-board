idf_component_register(
    SRCS "log_task_handler.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        sd_controller
        config
        esp_common
        log
)

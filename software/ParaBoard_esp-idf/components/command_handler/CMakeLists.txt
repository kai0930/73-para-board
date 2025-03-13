idf_component_register(
    SRCS "command_handler.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        freertos 
        CanComm
        sd_controller
        log_task_handler
        sensor_task_handler
        servo_controller
        mode_manager
        condition_checker
        config
        esp_common
        log
        esp_timer
) 
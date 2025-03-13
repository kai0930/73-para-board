idf_component_register(
    SRCS "servo_controller.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        driver
        config
        esp_common
        log
) 
idf_component_register(
    SRCS "condition_checker.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        driver
        config
        esp_common
        log
        esp_timer
) 
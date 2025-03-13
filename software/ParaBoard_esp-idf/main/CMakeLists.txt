idf_component_register(
  SRCS "main.cpp"
  INCLUDE_DIRS "."
  REQUIRES "create_spi icm42688 lps25hb gptimer freertos sd_controller CanComm esp_timer log_task_handler sensor_task_handler command_handler servo_controller"
)
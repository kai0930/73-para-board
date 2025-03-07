#include <stdio.h>

extern "C" void app_main(void)
{
    gpio_reset_pin(Pins::LED);
    gpio_set_direction(Pins::LED, GPIO_MODE_OUTPUT);
}
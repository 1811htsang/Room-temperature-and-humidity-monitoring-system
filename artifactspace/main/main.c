#include <stdio.h>
#include "reset_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    printf("\n");
    printf("====================================\n");
    printf("ESP32 TEMPERATURE - HUMIDITY CLOCK\n");
    printf("RESET DRIVER INITIALIZATION\n");
    printf("====================================\n");

    check_reset_reason();

    vTaskDelay(10000 / portTICK_PERIOD_MS);

    request_software_reset();
}
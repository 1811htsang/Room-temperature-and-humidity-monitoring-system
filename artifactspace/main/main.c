#include <stdio.h>
#include "reset_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/rtc.h"
#include "esp_private/periph_ctrl.h"


static const char *TAG = "CLOCK_TASK";

void app_main(void)
{
    printf("\n");
    printf("====================================\n");
    printf("ESP32 TEMPERATURE - HUMIDITY CLOCK\n");
    printf("RESET DRIVER INITIALIZATION\n");
    printf("====================================\n");

    check_reset_reason();

    ESP_LOGI(TAG, "---- BAT DAU THUC HIEN CLOCK TASK ----");

    ESP_LOGI(TAG,"Kiem tra nguyen nhan reset");
    esp_reset_reason_t reason = esp_reset_reason();

    switch(reason)
    {
        case ESP_RST_POWERON:
            ESP_LOGI(TAG,"RESET do POWER ON");
            break;

        case ESP_RST_SW:
            ESP_LOGI(TAG,"Reset do Software");
            break;

        case ESP_RST_PANIC:
            ESP_LOGI(TAG,"Reset do System Panic");
            break;

        case ESP_RST_INT_WDT:
            ESP_LOGI(TAG,"Reset do Watchdog");
            break;

        default:
            ESP_LOGI(TAG,"Reset reason khac");
            break;
    }

    /* 1. Khoi dong CLOCK HE THONG */
    rtc_cpu_freq_config_t config;

    rtc_clk_cpu_freq_mhz_to_config(160, &config);
    rtc_clk_cpu_freq_set_config(&config);

    ESP_LOGI(TAG, "CPU Clock da duoc dat = 160MHz");

    uint32_t apb_freq = rtc_clk_apb_freq_get();
    ESP_LOGI(TAG,"APB Clock = %lu Hz", apb_freq);

    /* 2. Bat clock cho UART1 */
    periph_module_enable(PERIPH_UART1_MODULE);
    ESP_LOGI(TAG,"Clock cho UART1 da duoc bat");

    /* Reset ngoai vi */
    periph_module_reset(PERIPH_UART1_MODULE);
    ESP_LOGI(TAG,"UART1 da duoc reset");

    /* 3. Tat clock ngoai vi */
    periph_module_disable(PERIPH_UART1_MODULE);
    ESP_LOGI(TAG,"Clock cho UART1 da duoc tat");

    ESP_LOGI(TAG, "---- HOAN THANH CLOCK TASK ----");
    
}
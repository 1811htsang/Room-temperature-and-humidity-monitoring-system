#include <stdio.h>
#include "esp_system.h"
#include "esp_log.h"
#include "reset_driver.h"

static const char *TAG = "RESET_DRIVER";

void check_reset_reason()
{
    esp_reset_reason_t reason = esp_reset_reason();

    switch(reason)
    {
        case ESP_RST_POWERON:
            ESP_LOGI(TAG, "Reset reason: Power on reset");
            break;

        case ESP_RST_EXT:
            ESP_LOGI(TAG, "Reset reason: External reset (EN pin)");
            break;

        case ESP_RST_SW:
            ESP_LOGI(TAG, "Reset reason: Software reset");
            break;

        case ESP_RST_PANIC:
            ESP_LOGI(TAG, "Reset reason: Exception / Panic reset");
            break;

        case ESP_RST_INT_WDT:
            ESP_LOGI(TAG, "Reset reason: Interrupt Watchdog");
            break;

        case ESP_RST_TASK_WDT:
            ESP_LOGI(TAG, "Reset reason: Task Watchdog");
            break;

        case ESP_RST_WDT:
            ESP_LOGI(TAG, "Reset reason: Other Watchdog");
            break;

        case ESP_RST_DEEPSLEEP:
            ESP_LOGI(TAG, "Reset reason: Wake from Deep Sleep");
            break;

        case ESP_RST_BROWNOUT:
            ESP_LOGI(TAG, "Reset reason: Brownout reset");
            break;

        default:
            ESP_LOGI(TAG, "Reset reason: Unknown");
            break;
    }
}

void request_software_reset()
{
    ESP_LOGW(TAG, "System will restart now...");
    esp_restart();
}
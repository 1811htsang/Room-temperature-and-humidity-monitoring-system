#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "reset_driver.h"
#include "i2c_hal.h"
#include "ds3231.h"

static const char *TAG = "MAIN";

/* ── Callback báo thức (tuỳ chọn) ──────────────────────────────────────── */
static void on_alarm(uint8_t alarm_num, void *arg)
{
    ESP_LOGW(TAG, ">>> Alarm %d fired! <<<", alarm_num);
}

void app_main(void)
{
    /* ================================================================
     * BANNER KHỞI ĐỘNG
     * ================================================================ */
    printf("\n");
    printf("====================================\n");
    printf("ESP32 TEMPERATURE - HUMIDITY CLOCK\n");
    printf("====================================\n");
    printf("\n");

    /* ================================================================
     * BƯỚC 1: RESET DRIVER (giữ nguyên logic cũ)
     * ================================================================ */
    printf("--- RESET DRIVER ---\n");
    check_reset_reason();

    /* ================================================================
     * BƯỚC 2: KHỞI TẠO I2C
     * ================================================================ */
    printf("--- I2C INITIALIZATION ---\n");
    esp_err_t ret = i2c_hal_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s — Restarting...", esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(2000));
        request_software_reset();   /* Dùng lại hàm reset cũ */
    }
    ESP_LOGI(TAG, "I2C initialized OK");

    /* ================================================================
     * BƯỚC 3: KHỞI TẠO DS3231
     * ================================================================ */
    printf("--- DS3231 INITIALIZATION ---\n");
    ds3231_config_t ds_cfg = {
        .int_pin   = GPIO_NUM_NC,   /* Chưa dùng INT pin */
        .use_mutex = true,
    };
    ret = ds3231_init(&ds_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DS3231 init failed: %s — Check wiring!", esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(2000));
        request_software_reset();
    }
    ESP_LOGI(TAG, "DS3231 initialized OK");

    /* ================================================================
     * BƯỚC 4: KIỂM TRA MẤT NGUỒN — CÀI THỜI GIAN NẾU CẦN
     * ================================================================ */
    bool lost_power = false;
    ds3231_lost_power(&lost_power);

    if (lost_power) {
        ESP_LOGW(TAG, "RTC lost power! Setting default time...");

        ds3231_time_t default_time = {
            .second      = 0,
            .minute      = 0,
            .hour        = 8,
            .day_of_week = 2,       /* 1=CN, 2=T2, ..., 7=T7 */
            .date        = 1,
            .month       = 1,
            .year        = 2025,
        };
        ret = ds3231_set_time(&default_time);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Default time set: 2025-01-01 08:00:00");
        } else {
            ESP_LOGE(TAG, "Failed to set time: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGI(TAG, "RTC time is valid");
    }

    /* ================================================================
     * BƯỚC 5: ĐẶT BÁO THỨC MẪU (tuỳ chọn, xoá nếu chưa cần)
     * ================================================================ */
    ds3231_time_t alarm_time = {
        .hour   = 8,
        .minute = 30,
        .second = 0,
    };
    ds3231_set_alarm(1, &alarm_time, DS3231_ALM1_MATCH_HR_MIN_SEC, on_alarm, NULL);
    ESP_LOGI(TAG, "Alarm 1 set at 08:30:00");

    /* ================================================================
     * BƯỚC 6: VÒNG LẶP CHÍNH — ĐỌC & IN DỮ LIỆU
     * ================================================================ */
    printf("\n");
    printf("====================================\n");
    printf("MAIN LOOP STARTED\n");
    printf("====================================\n");

    while (1) {
        ds3231_time_t now;
        float temp_rtc = 0.0f;

        /* Đọc thời gian */
        if (ds3231_get_time(&now) == ESP_OK) {
            ESP_LOGI(TAG, "[RTC] %04d-%02d-%02d  %02d:%02d:%02d  (Thu %d)",
                     now.year, now.month, now.date,
                     now.hour, now.minute, now.second,
                     now.day_of_week);
        } else {
            ESP_LOGE(TAG, "Failed to read RTC time");
        }

        /* Đọc nhiệt độ nội bộ DS3231 */
        if (ds3231_get_temperature(&temp_rtc) == ESP_OK) {
            ESP_LOGI(TAG, "[RTC] Internal temp: %.2f C", temp_rtc);
        } else {
            ESP_LOGE(TAG, "Failed to read RTC temperature");
        }

        /* Polling báo thức (khi không dùng INT pin) */
        bool alarm_fired = false;
        ds3231_check_alarm(1, &alarm_fired);
        if (alarm_fired) {
            ESP_LOGW(TAG, "Alarm 1 triggered (polling)!");
        }

        printf("------------------------------------\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
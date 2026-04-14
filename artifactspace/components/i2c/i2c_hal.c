/**
 * @file    i2c_hal.c
 * @brief   Hiện thực I2C HAL sử dụng ESP-IDF legacy I2C driver.
 */
#include "i2c_hal.h"
#include "esp_log.h"

static const char *TAG = "I2C_HAL";

/* ── Khởi tạo ──────────────────────────────────────────────────────────── */
esp_err_t i2c_hal_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_HAL_SDA_PIN,
        .scl_io_num       = I2C_HAL_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_HAL_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_HAL_PORT, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(I2C_HAL_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/* ── Ghi một thanh ghi ─────────────────────────────────────────────────── */
esp_err_t i2c_hal_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_HAL_PORT, cmd,
                                          pdMS_TO_TICKS(I2C_HAL_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write_reg 0x%02X -> 0x%02X failed: %s",
                 dev_addr, reg_addr, esp_err_to_name(ret));
    }
    return ret;
}

/* ── Đọc một thanh ghi ─────────────────────────────────────────────────── */
esp_err_t i2c_hal_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_HAL_PORT, cmd, pdMS_TO_TICKS(I2C_HAL_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    return ret;
}
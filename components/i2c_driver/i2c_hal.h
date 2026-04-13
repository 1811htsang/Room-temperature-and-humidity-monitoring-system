/**
 * @file    i2c_hal.h
 * @brief   I2C Hardware Abstraction Layer for ESP32 (ESP-IDF)
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Cấu hình mặc định ─────────────────────────────────────────────────── */
#define I2C_HAL_PORT        I2C_NUM_0
#define I2C_HAL_SDA_PIN     21
#define I2C_HAL_SCL_PIN     22
#define I2C_HAL_FREQ_HZ     400000      /* 400 kHz Fast-mode */
#define I2C_HAL_TIMEOUT_MS  50

/**
 * @brief Khởi tạo I2C master.
 * @return ESP_OK nếu thành công.
 */
esp_err_t i2c_hal_init(void);

/**
 * @brief Ghi một byte vào thanh ghi của slave.
 */
esp_err_t i2c_hal_write_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);

/**
 * @brief Đọc một byte từ thanh ghi của slave.
 */
esp_err_t i2c_hal_read_reg(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);

/**
 * @brief Đọc nhiều byte liên tiếp (burst read).
 */
esp_err_t i2c_hal_read_burst(uint8_t dev_addr, uint8_t reg_addr,
                              uint8_t *buf, size_t len);

/**
 * @brief Ghi nhiều byte liên tiếp (burst write).
 */
esp_err_t i2c_hal_write_burst(uint8_t dev_addr, uint8_t reg_addr,
                               const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif
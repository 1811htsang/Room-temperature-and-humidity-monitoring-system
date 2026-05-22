/**
 * @file    ds3231.h
 * @brief   Driver DS3231 RTC — ESP32 / ESP-IDF
 *
 * Tính năng:
 *   - Đọc / ghi thời gian & ngày tháng (BCD ↔ decimal tự động)
 *   - Đọc nhiệt độ nội bộ (độ phân giải 0.25 °C)
 *   - Cài đặt / xóa Alarm 1 & Alarm 2 + ISR callback
 *   - Phát hiện mất nguồn (Oscillator Stop Flag)
 *   - Bật / tắt ngõ ra 32kHz
 *   - Thread-safe (FreeRTOS mutex)
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Địa chỉ & thanh ghi ───────────────────────────────────────────────── */
#define DS3231_I2C_ADDR         0x68

/* Time / date registers */
#define DS3231_REG_SECONDS      0x00
#define DS3231_REG_MINUTES      0x01
#define DS3231_REG_HOURS        0x02
#define DS3231_REG_DAY          0x03    /* 1=CN, 2=T2 ... 7=T7 */
#define DS3231_REG_DATE         0x04
#define DS3231_REG_MONTH        0x05
#define DS3231_REG_YEAR         0x06

/* Alarm 1 registers (0x07–0x0A) */
#define DS3231_REG_ALM1_SEC     0x07
#define DS3231_REG_ALM1_MIN     0x08
#define DS3231_REG_ALM1_HOUR    0x09
#define DS3231_REG_ALM1_DAY     0x0A

/* Alarm 2 registers (0x0B–0x0D) */
#define DS3231_REG_ALM2_MIN     0x0B
#define DS3231_REG_ALM2_HOUR    0x0C
#define DS3231_REG_ALM2_DAY     0x0D

/* Control & Status */
#define DS3231_REG_CONTROL      0x0E
#define DS3231_REG_STATUS       0x0F
#define DS3231_REG_AGING        0x10

/* Temperature */
#define DS3231_REG_TEMP_MSB     0x11
#define DS3231_REG_TEMP_LSB     0x12

/* Control register bits */
#define DS3231_CTRL_A1IE        BIT(0)  /* Alarm 1 interrupt enable */
#define DS3231_CTRL_A2IE        BIT(1)  /* Alarm 2 interrupt enable */
#define DS3231_CTRL_INTCN       BIT(2)  /* INT/SQW pin → interrupt */
#define DS3231_CTRL_RS1         BIT(3)  /* SQW rate select bit 1    */
#define DS3231_CTRL_RS2         BIT(4)  /* SQW rate select bit 2    */
#define DS3231_CTRL_CONV        BIT(5)  /* Force temperature convert */
#define DS3231_CTRL_BBSQW       BIT(6)  /* SQW on battery backup    */
#define DS3231_CTRL_EOSC        BIT(7)  /* !EOSC: 0 = osc enabled   */

/* Status register bits */
#define DS3231_STAT_A1F         BIT(0)  /* Alarm 1 flag */
#define DS3231_STAT_A2F         BIT(1)  /* Alarm 2 flag */
#define DS3231_STAT_BSY         BIT(2)  /* Busy (temperature convert) */
#define DS3231_STAT_EN32KHZ     BIT(3)  /* 32kHz output enable */
#define DS3231_STAT_OSF         BIT(7)  /* Oscillator Stop Flag */

/* ── Kiểu dữ liệu ──────────────────────────────────────────────────────── */

/** Cấu trúc ngày giờ */
typedef struct {
    uint8_t second;     /*!< 0–59  */
    uint8_t minute;     /*!< 0–59  */
    uint8_t hour;       /*!< 0–23  */
    uint8_t day_of_week;/*!< 1–7 (1 = Chủ nhật) */
    uint8_t date;       /*!< 1–31  */
    uint8_t month;      /*!< 1–12  */
    uint16_t year;      /*!< 2000–2099 */
} ds3231_time_t;

/** Chế độ kích hoạt Alarm */
typedef enum {
    /* Alarm 1 modes */
    DS3231_ALM1_EVERY_SECOND    = 0x0F, /*!< Mỗi giây một lần */
    DS3231_ALM1_MATCH_SECONDS   = 0x0E, /*!< Khi giây khớp    */
    DS3231_ALM1_MATCH_MIN_SEC   = 0x0C, /*!< Khi phút:giây khớp */
    DS3231_ALM1_MATCH_HR_MIN_SEC = 0x08,/*!< Khi giờ:phút:giây khớp */
    DS3231_ALM1_MATCH_DATE      = 0x00, /*!< Khi ngày+giờ:phút:giây khớp */
    DS3231_ALM1_MATCH_DAY       = 0x10, /*!< Khi thứ+giờ:phút:giây khớp */

    /* Alarm 2 modes */
    DS3231_ALM2_EVERY_MINUTE    = 0x07, /*!< Mỗi phút một lần */
    DS3231_ALM2_MATCH_MINUTES   = 0x06, /*!< Khi phút khớp    */
    DS3231_ALM2_MATCH_HR_MIN    = 0x04, /*!< Khi giờ:phút khớp */
    DS3231_ALM2_MATCH_DATE      = 0x00, /*!< Khi ngày+giờ:phút khớp */
    DS3231_ALM2_MATCH_DAY       = 0x10, /*!< Khi thứ+giờ:phút khớp */
} ds3231_alarm_mode_t;

/** Callback khi ngắt báo thức kích hoạt */
typedef void (*ds3231_alarm_cb_t)(uint8_t alarm_num, void *arg);

/** Cấu hình khởi tạo driver */
typedef struct {
    gpio_num_t  int_pin;    /*!< GPIO kết nối chân INT/SQW (-1 nếu không dùng) */
    bool        use_mutex;  /*!< true = dùng FreeRTOS mutex (đa luồng)         */
} ds3231_config_t;

/* ── API ────────────────────────────────────────────────────────────────── */

/**
 * @brief Khởi tạo driver DS3231.
 *        Hàm tự động bật oscillator, xóa OSF flag, cấu hình INT pin nếu có.
 *
 * @param cfg  Con trỏ đến cấu hình (NULL = giá trị mặc định, không dùng INT).
 * @return ESP_OK nếu thành công.
 */
esp_err_t ds3231_init(const ds3231_config_t *cfg);

/**
 * @brief Giải phóng tài nguyên driver (mutex, ISR).
 */
esp_err_t ds3231_deinit(void);

/* ── Thời gian ─────────────────────────────────────────────────────────── */

/**
 * @brief Đọc toàn bộ thời gian từ DS3231.
 * @param[out] t  Kết quả thời gian.
 */
esp_err_t ds3231_get_time(ds3231_time_t *t);

/**
 * @brief Cài đặt thời gian cho DS3231.
 *        Hàm tự động xóa OSF flag sau khi ghi thành công.
 * @param[in] t  Thời gian muốn cài đặt.
 */
esp_err_t ds3231_set_time(const ds3231_time_t *t);

/* ── Nhiệt độ ──────────────────────────────────────────────────────────── */

/**
 * @brief Đọc nhiệt độ nội bộ DS3231.
 *        DS3231 cập nhật nhiệt độ mỗi 64 giây hoặc khi gọi force_convert.
 *
 * @param[out] temp_c  Nhiệt độ tính bằng °C (độ phân giải 0.25°C).
 */
esp_err_t ds3231_get_temperature(float *temp_c);

/**
 * @brief Yêu cầu DS3231 chuyển đổi nhiệt độ ngay lập tức.
 *        Hàm block tối đa ~2ms đến khi BSY bit = 0.
 */
esp_err_t ds3231_force_temp_convert(void);

/* ── Báo thức ──────────────────────────────────────────────────────────── */

/**
 * @brief Cài đặt báo thức.
 *
 * @param alarm_num  1 hoặc 2.
 * @param t          Thời gian báo thức (chỉ dùng các trường phù hợp với mode).
 * @param mode       Chế độ so sánh.
 * @param cb         Callback khi báo thức (NULL = không dùng).
 * @param cb_arg     Tham số truyền vào callback.
 */
esp_err_t ds3231_set_alarm(uint8_t alarm_num, const ds3231_time_t *t,
                            ds3231_alarm_mode_t mode,
                            ds3231_alarm_cb_t cb, void *cb_arg);

/**
 * @brief Vô hiệu hóa và xóa cờ báo thức.
 * @param alarm_num  1, 2, hoặc 0 (cả hai).
 */
esp_err_t ds3231_clear_alarm(uint8_t alarm_num);

/**
 * @brief Kiểm tra cờ báo thức (polling, không cần INT pin).
 * @param alarm_num   1 hoặc 2.
 * @param[out] fired  true nếu báo thức đã kích hoạt.
 */
esp_err_t ds3231_check_alarm(uint8_t alarm_num, bool *fired);

/* ── Tiện ích ──────────────────────────────────────────────────────────── */

/**
 * @brief Kiểm tra DS3231 có bị mất nguồn/lỗi oscillator không (OSF flag).
 * @param[out] lost  true nếu thời gian không còn chính xác.
 */
esp_err_t ds3231_lost_power(bool *lost);

/**
 * @brief Bật / tắt ngõ ra 32kHz.
 */
esp_err_t ds3231_enable_32khz(bool enable);

/**
 * @brief Đọc / ghi trực tiếp thanh ghi (debug).
 */
esp_err_t ds3231_read_raw(uint8_t reg, uint8_t *val);
esp_err_t ds3231_write_raw(uint8_t reg, uint8_t val);

#ifdef __cplusplus
}
#endif
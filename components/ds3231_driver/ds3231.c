/**
 * @file    ds3231.c
 * @brief   Hiện thực đầy đủ driver DS3231 cho ESP32.
 */
#include "ds3231.h"
#include "i2c_hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "DS3231";

/* ── Trạng thái nội bộ ─────────────────────────────────────────────────── */
typedef struct {
    bool               initialized;
    gpio_num_t         int_pin;
    SemaphoreHandle_t  mutex;
    ds3231_alarm_cb_t  alarm_cb[2];     /* cb[0] = alarm1, cb[1] = alarm2 */
    void              *alarm_cb_arg[2];
} ds3231_dev_t;

static ds3231_dev_t s_dev = { 0 };

/* ── Macro bảo vệ mutex ────────────────────────────────────────────────── */
#define LOCK()   do { if (s_dev.mutex) xSemaphoreTake(s_dev.mutex, portMAX_DELAY); } while(0)
#define UNLOCK() do { if (s_dev.mutex) xSemaphoreGive(s_dev.mutex); } while(0)

/* ── Tiện ích chuyển đổi BCD ───────────────────────────────────────────── */
static inline uint8_t bcd2dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static inline uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/* ── ISR handler cho INT pin ───────────────────────────────────────────── */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    /*
     * Không đọc I2C trong ISR. Dùng task notification để xử lý ngoài ISR.
     * Ở đây chỉ ghi nhận — task alarm sẽ đọc flag và gọi callback.
     */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    /* Notify task (nếu bạn muốn tạo task riêng — xem phần mở rộng) */
    (void)xHigherPriorityTaskWoken;
}

/* ── Kiểm tra & xóa alarm flag, gọi callback ───────────────────────────── */
static void ds3231_process_alarms(void)
{
    uint8_t status = 0;
    if (i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status) != ESP_OK)
        return;

    for (int i = 0; i < 2; i++) {
        uint8_t flag = (i == 0) ? DS3231_STAT_A1F : DS3231_STAT_A2F;
        if (status & flag) {
            /* Xóa flag */
            uint8_t new_status = status & ~flag;
            i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, new_status);
            status = new_status;

            if (s_dev.alarm_cb[i]) {
                s_dev.alarm_cb[i]((uint8_t)(i + 1), s_dev.alarm_cb_arg[i]);
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  KHỞI TẠO
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t ds3231_init(const ds3231_config_t *cfg)
{
    if (s_dev.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    /* Cấu hình mặc định */
    s_dev.int_pin = (cfg && cfg->int_pin >= 0) ? cfg->int_pin : GPIO_NUM_NC;
    bool use_mutex = (cfg == NULL) ? true : cfg->use_mutex;

    /* Tạo mutex */
    if (use_mutex) {
        s_dev.mutex = xSemaphoreCreateMutex();
        if (!s_dev.mutex) {
            ESP_LOGE(TAG, "Failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    /* Kiểm tra kết nối DS3231 */
    uint8_t ctrl = 0;
    esp_err_t ret = i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, &ctrl);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DS3231 not found on I2C bus! Check wiring.");
        goto fail;
    }

    /* Bật oscillator (xóa bit EOSC = 0), bật INTCN */
    ctrl &= ~DS3231_CTRL_EOSC;
    ctrl |=  DS3231_CTRL_INTCN;
    ret = i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, ctrl);
    if (ret != ESP_OK) goto fail;

    /* Đọc status, xóa alarm flags (không xóa OSF — để người dùng kiểm tra) */
    uint8_t status = 0;
    i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status);
    status &= ~(DS3231_STAT_A1F | DS3231_STAT_A2F);
    i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, status);

    /* Cấu hình INT pin GPIO nếu được chỉ định */
    if (s_dev.int_pin != GPIO_NUM_NC) {
        gpio_config_t io_cfg = {
            .pin_bit_mask = (1ULL << s_dev.int_pin),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,    /* DS3231 INT là active-low open-drain */
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_NEGEDGE,
        };
        gpio_config(&io_cfg);
        gpio_install_isr_service(0);
        gpio_isr_handler_add(s_dev.int_pin, gpio_isr_handler, NULL);
    }

    s_dev.initialized = true;
    ESP_LOGI(TAG, "DS3231 initialized. OSF=%d", (status & DS3231_STAT_OSF) ? 1 : 0);
    return ESP_OK;

fail:
    if (s_dev.mutex) { vSemaphoreDelete(s_dev.mutex); s_dev.mutex = NULL; }
    return ret;
}

esp_err_t ds3231_deinit(void)
{
    if (!s_dev.initialized) return ESP_OK;

    if (s_dev.int_pin != GPIO_NUM_NC) {
        gpio_isr_handler_remove(s_dev.int_pin);
    }
    if (s_dev.mutex) {
        vSemaphoreDelete(s_dev.mutex);
        s_dev.mutex = NULL;
    }
    memset(&s_dev, 0, sizeof(s_dev));
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  ĐỌC / GHI THỜI GIAN
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t ds3231_get_time(ds3231_time_t *t)
{
    if (!t) return ESP_ERR_INVALID_ARG;
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    uint8_t buf[7];
    LOCK();
    esp_err_t ret = i2c_hal_read_burst(DS3231_I2C_ADDR, DS3231_REG_SECONDS, buf, 7);
    UNLOCK();

    if (ret != ESP_OK) return ret;

    t->second      = bcd2dec(buf[0] & 0x7F);
    t->minute      = bcd2dec(buf[1] & 0x7F);
    t->hour        = bcd2dec(buf[2] & 0x3F);   /* Giả định 24h mode */
    t->day_of_week = buf[3] & 0x07;
    t->date        = bcd2dec(buf[4] & 0x3F);
    t->month       = bcd2dec(buf[5] & 0x1F);
    t->year        = bcd2dec(buf[6]) + ((buf[5] & 0x80) ? 2100 : 2000);

    return ESP_OK;
}

esp_err_t ds3231_set_time(const ds3231_time_t *t)
{
    if (!t) return ESP_ERR_INVALID_ARG;
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    /* Kiểm tra giá trị hợp lệ */
    if (t->second > 59 || t->minute > 59 || t->hour > 23 ||
        t->date < 1 || t->date > 31 || t->month < 1 || t->month > 12 ||
        t->year < 2000 || t->year > 2099 ||
        t->day_of_week < 1 || t->day_of_week > 7) {
        ESP_LOGE(TAG, "Invalid time values");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[7];
    uint8_t century = (t->year >= 2100) ? 0x80 : 0x00;

    buf[0] = dec2bcd(t->second);
    buf[1] = dec2bcd(t->minute);
    buf[2] = dec2bcd(t->hour);             /* 24h mode: bit 6 = 0 */
    buf[3] = t->day_of_week & 0x07;
    buf[4] = dec2bcd(t->date);
    buf[5] = dec2bcd(t->month) | century;
    buf[6] = dec2bcd((uint8_t)(t->year % 100));

    LOCK();
    esp_err_t ret = i2c_hal_write_burst(DS3231_I2C_ADDR, DS3231_REG_SECONDS, buf, 7);

    if (ret == ESP_OK) {
        /* Xóa OSF flag sau khi cài thời gian mới */
        uint8_t status = 0;
        i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status);
        status &= ~DS3231_STAT_OSF;
        i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, status);
    }
    UNLOCK();

    return ret;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  NHIỆT ĐỘ
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t ds3231_get_temperature(float *temp_c)
{
    if (!temp_c) return ESP_ERR_INVALID_ARG;
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    uint8_t buf[2];
    LOCK();
    esp_err_t ret = i2c_hal_read_burst(DS3231_I2C_ADDR, DS3231_REG_TEMP_MSB, buf, 2);
    UNLOCK();

    if (ret != ESP_OK) return ret;

    /*
     * MSB: số nguyên có dấu (complement 2, bit 7 = dấu âm)
     * LSB: bit[7:6] là phần thập phân, bước 0.25°C
     */
    int8_t  integer = (int8_t)buf[0];
    uint8_t frac    = (buf[1] >> 6) & 0x03;
    *temp_c = (float)integer + frac * 0.25f;

    return ESP_OK;
}

esp_err_t ds3231_force_temp_convert(void)
{
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    LOCK();
    uint8_t ctrl = 0;
    esp_err_t ret = i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, &ctrl);
    if (ret != ESP_OK) { UNLOCK(); return ret; }

    /* Kiểm tra BSY — không set CONV khi đang convert */
    uint8_t status = 0;
    i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status);
    if (status & DS3231_STAT_BSY) {
        UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }

    ctrl |= DS3231_CTRL_CONV;
    ret = i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, ctrl);
    UNLOCK();

    if (ret != ESP_OK) return ret;

    /* Chờ BSY = 0, timeout ~10ms */
    for (int i = 0; i < 20; i++) {
        vTaskDelay(pdMS_TO_TICKS(1));
        LOCK();
        i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status);
        UNLOCK();
        if (!(status & DS3231_STAT_BSY)) return ESP_OK;
    }

    ESP_LOGW(TAG, "force_temp_convert timeout");
    return ESP_ERR_TIMEOUT;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  BÁO THỨC
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Xây dựng byte thanh ghi alarm với bit AxMx (mask) và DY/DT.
 * Alarm 1: sec, min, hour, day/date (4 thanh ghi)
 * Alarm 2: min, hour, day/date      (3 thanh ghi)
 */
esp_err_t ds3231_set_alarm(uint8_t alarm_num, const ds3231_time_t *t,
                            ds3231_alarm_mode_t mode,
                            ds3231_alarm_cb_t cb, void *cb_arg)
{
    if (alarm_num < 1 || alarm_num > 2 || !t) return ESP_ERR_INVALID_ARG;
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    /*
     * Bit A1M1..A1M4 (hoặc A2M2..A2M4) nằm ở bit 7 của mỗi thanh ghi.
     * mode được encode như sau (4-bit cho alarm1, 3-bit cho alarm2):
     *   bit[3] = A1M4/A2M4, bit[2] = A1M3/A2M3, bit[1] = A1M2/A2M2,
     *   bit[0] = A1M1 (chỉ alarm1), bit[4] = DY/DT.
     */
    bool dy_dt = (mode & 0x10) != 0;   /* 1 = so sánh thứ, 0 = so sánh ngày */

    esp_err_t ret = ESP_OK;
    LOCK();

    if (alarm_num == 1) {
        uint8_t buf[4];
        buf[0] = dec2bcd(t->second)  | (((mode >> 0) & 1) << 7);  /* A1M1 */
        buf[1] = dec2bcd(t->minute)  | (((mode >> 1) & 1) << 7);  /* A1M2 */
        buf[2] = dec2bcd(t->hour)    | (((mode >> 2) & 1) << 7);  /* A1M3 */
        buf[3] = (dy_dt ? dec2bcd(t->day_of_week) : dec2bcd(t->date))
                 | (((mode >> 3) & 1) << 7)                        /* A1M4 */
                 | (dy_dt ? 0x40 : 0x00);                          /* DY/DT */

        ret = i2c_hal_write_burst(DS3231_I2C_ADDR, DS3231_REG_ALM1_SEC, buf, 4);

        if (ret == ESP_OK) {
            /* Bật Alarm 1 interrupt */
            uint8_t ctrl = 0;
            i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, &ctrl);
            ctrl |= DS3231_CTRL_A1IE;
            i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, ctrl);

            s_dev.alarm_cb[0]     = cb;
            s_dev.alarm_cb_arg[0] = cb_arg;
        }
    } else {
        uint8_t buf[3];
        buf[0] = dec2bcd(t->minute) | (((mode >> 1) & 1) << 7);   /* A2M2 */
        buf[1] = dec2bcd(t->hour)   | (((mode >> 2) & 1) << 7);   /* A2M3 */
        buf[2] = (dy_dt ? dec2bcd(t->day_of_week) : dec2bcd(t->date))
                 | (((mode >> 3) & 1) << 7)                        /* A2M4 */
                 | (dy_dt ? 0x40 : 0x00);

        ret = i2c_hal_write_burst(DS3231_I2C_ADDR, DS3231_REG_ALM2_MIN, buf, 3);

        if (ret == ESP_OK) {
            uint8_t ctrl = 0;
            i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, &ctrl);
            ctrl |= DS3231_CTRL_A2IE;
            i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, ctrl);

            s_dev.alarm_cb[1]     = cb;
            s_dev.alarm_cb_arg[1] = cb_arg;
        }
    }

    UNLOCK();
    return ret;
}

esp_err_t ds3231_clear_alarm(uint8_t alarm_num)
{
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    LOCK();
    uint8_t ctrl = 0, status = 0;
    i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, &ctrl);
    i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status);

    if (alarm_num == 0 || alarm_num == 1) {
        ctrl   &= ~DS3231_CTRL_A1IE;
        status &= ~DS3231_STAT_A1F;
        s_dev.alarm_cb[0]     = NULL;
        s_dev.alarm_cb_arg[0] = NULL;
    }
    if (alarm_num == 0 || alarm_num == 2) {
        ctrl   &= ~DS3231_CTRL_A2IE;
        status &= ~DS3231_STAT_A2F;
        s_dev.alarm_cb[1]     = NULL;
        s_dev.alarm_cb_arg[1] = NULL;
    }

    i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_CONTROL, ctrl);
    esp_err_t ret = i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, status);
    UNLOCK();
    return ret;
}

esp_err_t ds3231_check_alarm(uint8_t alarm_num, bool *fired)
{
    if (!fired || alarm_num < 1 || alarm_num > 2) return ESP_ERR_INVALID_ARG;
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    /* Gọi xử lý callback nếu flag được set */
    ds3231_process_alarms();

    LOCK();
    uint8_t status = 0;
    esp_err_t ret = i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status);
    UNLOCK();

    if (ret != ESP_OK) return ret;
    *fired = (status & (alarm_num == 1 ? DS3231_STAT_A1F : DS3231_STAT_A2F)) != 0;
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  TIỆN ÍCH
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t ds3231_lost_power(bool *lost)
{
    if (!lost) return ESP_ERR_INVALID_ARG;
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    LOCK();
    uint8_t status = 0;
    esp_err_t ret = i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status);
    UNLOCK();

    if (ret == ESP_OK) *lost = (status & DS3231_STAT_OSF) != 0;
    return ret;
}

esp_err_t ds3231_enable_32khz(bool enable)
{
    if (!s_dev.initialized) return ESP_ERR_INVALID_STATE;

    LOCK();
    uint8_t status = 0;
    esp_err_t ret = i2c_hal_read_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, &status);
    if (ret == ESP_OK) {
        if (enable) status |=  DS3231_STAT_EN32KHZ;
        else        status &= ~DS3231_STAT_EN32KHZ;
        ret = i2c_hal_write_reg(DS3231_I2C_ADDR, DS3231_REG_STATUS, status);
    }
    UNLOCK();
    return ret;
}

esp_err_t ds3231_read_raw(uint8_t reg, uint8_t *val)
{
    if (!val) return ESP_ERR_INVALID_ARG;
    LOCK();
    esp_err_t ret = i2c_hal_read_reg(DS3231_I2C_ADDR, reg, val);
    UNLOCK();
    return ret;
}

esp_err_t ds3231_write_raw(uint8_t reg, uint8_t val)
{
    LOCK();
    esp_err_t ret = i2c_hal_write_reg(DS3231_I2C_ADDR, reg, val);
    UNLOCK();
    return ret;
}
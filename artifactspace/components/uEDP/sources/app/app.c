/**
 * @file app.c
 * @author Shang Huang
 * @brief Application main source file
 * @version 0.1
 * @date 2026-05-07
 * @copyright MIT License
 */

/**
 * @brief Khai báo thư viện sử dụng
 */

#include <inttypes.h>
#include "ciedpc_core.h"
#include "ciedpc_task.h"
#include "ciedpc_msg.h"
#include "ciedpc_timer.h"
#include "pal_memrp.h"
#include "app_decl.h"
#include "app_cfg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lcd.h"

#define DS3231_UPDATE_PERIOD_MS   (1000)
#define SHT30_SAMPLE_PERIOD_MS    (20000)
#define SHT30_CONVERSION_WAIT_MS  (30)
#define DS3231_DISPLAY_OFFSET_SEC (17)
#define LED_BLINK_PERIOD_MS       (500)
#define TEMP_LED_THRESHOLD_C      (30.0f)

/**
 * @brief Ủy quyền sử dụng biến đếm hành động của hệ thống
 */

ui32 system_action_count = 0x0u;

/**
 * @brief Ủy quyền sử dụng message queue cho các tác vụ đã khai báo trong app_decl.h
 * @attention Các hàng đợi tin nhắn này phải được khai báo đúng tên 
 *            và kích thước trong app_decl.h để tránh lỗi
 *            multiple definition hoặc thiếu định nghĩa khi biên dịch
 */

// Điền các khai báo tại đây
ciedpc_msg_t* usr_q_mem[8] = {0};
ciedpc_msg_t* ds3231_q_mem[8] = {0};
ciedpc_msg_t* sht30_q_mem[8] = {0};
ciedpc_msg_t* lcd_q_mem[8] = {0};
ciedpc_msg_t* led_q_mem[4] = {0};

/**
 * @brief Định nghĩa các buffer dữ liệu để truyền các tin nhắn kích thước lớn
 *        như dạng chuỗi hoặc struct phức tạp. 
 *        Việc này giúp giảm tải bộ nhớ bằng cách không sao chép dữ liệu 
 *        mà chỉ truyền địa chỉ của biến chứa dữ liệu.
 */

// Điền các khai báo tại đây
static char ds3231_time[32] = {0};
static char sht30_data[32] = {0};
static bool led_output_state = false;
static bool led_blink_enabled = false;

/**
 * @brief Định nghĩa bảng task 
 */

// Điền các khai báo tại đây
task_norm_t app_task_table[] = {
  { CIEDPC_TASK_NORM_USR_ID,  CIEDPC_TASK_PRI_LEVEL_8, task_norm_usr_handler,     {0}, usr_q_mem    },
  { TASK_NORM_DS3231_ID,      CIEDPC_TASK_PRI_LEVEL_6, task_norm_ds3231_handler,  {0}, ds3231_q_mem },
  { TASK_NORM_SHT30_ID,       CIEDPC_TASK_PRI_LEVEL_7, task_norm_sht30_handler,   {0}, sht30_q_mem  },
  { TASK_NORM_LCD_ID,         CIEDPC_TASK_PRI_LEVEL_5, task_norm_lcd_handler,     {0}, lcd_q_mem    },
  { TASK_NORM_LED_ID,         CIEDPC_TASK_PRI_LEVEL_4, task_norm_led_handler,    {0}, led_q_mem    },
  { CIEDPC_TASK_NORM_EOT_ID,  CIEDPC_TASK_PRI_LEVEL_0, NULL,                      {0}, NULL         }
};

task_poll_t app_poll_table[] = {
  { CIEDPC_TASK_POLL_MEMRP_ID , 0, task_poll_memrp_handler },
  { CIEDPC_TASK_POLL_SYSLF_ID , 0, task_poll_syslf_handler },
  { CIEDPC_TASK_POLL_EOT_ID   , 0, NULL }
};

/**
 * @brief Định nghĩa các cấu hình bus I2C
 */

i2c_master_bus_config_t i2c_bus_config = {
  .clk_source = I2C_CLK_SRC_DEFAULT,
  .i2c_port = I2C_NUM_0,
  .scl_io_num = GPIO_NUM_22,
  .sda_io_num = GPIO_NUM_21,
  .glitch_ignore_cnt = 7
};
i2c_master_bus_handle_t bus_handle;

/**
 * @brief Cấu hình dev I2C
 */

i2c_device_config_t sht30_cfg = {
  .dev_addr_length = I2C_ADDR_BIT_LEN_7,
  .scl_speed_hz = 200000, // 200 kHz
  .device_address = SHT30_ADDR
};
i2c_master_dev_handle_t sht30_handle;

i2c_device_config_t rtc_cfg = {
  .dev_addr_length = I2C_ADDR_BIT_LEN_7,
  .scl_speed_hz = 100000,
  .device_address = RTC_ADDR
};
i2c_master_dev_handle_t rtc_handle;

i2c_device_config_t lcd_cfg = {
  .dev_addr_length = I2C_ADDR_BIT_LEN_7,
  .scl_speed_hz = 100000, // 200 kHz
  .device_address = LCD_ADDR
};
i2c_master_dev_handle_t lcd_handle;

/**
 * @brief Khai báo các hàm phụ trợ
 */

esp_err_t ds3231_read(i2c_master_dev_handle_t rtc_handle, ds3231_time_t *now);
static bool is_leap_year(ui16 year);
static ui8 days_in_month(ui16 year, ui8 month);
static void ds3231_apply_display_offset(ds3231_time_t *time, int offset_seconds);
static void led_hw_init(void);
static void led_hw_set(bool on);
static void led_hw_toggle(void);

/**
 * @brief Định nghĩa handler cho task USR, task DS3231, task SHT30 và task LCD
 */

// Điền các khai báo tại đây

void task_norm_usr_handler(ciedpc_msg_t* msg) {
  if (msg->sig == SIG_USR_START) {
    printf("[USR] Received START signal. Init I2C module...\n");
    // Khởi tạo bus I2C
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));
    printf("[USR] I2C master bus initialized. Scanning for devices...\n");
    // Quét bus I2C để tìm các thiết bị có kết nối
    for (int i = 1; i < 127; i++) {
      esp_err_t res = i2c_master_probe(bus_handle, i, -1);
      if (res == ESP_OK) {
          printf("[SCAN] Found device at address: 0x%02x\n", i);
      }
    }
    printf("[USR] Adding SHT30 device to I2C bus...\n");
    // Thêm thiết bị SHT30 vào bus I2C
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &sht30_cfg, &sht30_handle));
    printf("[USR] Adding DS3231 device to I2C bus...\n");
    // Thêm thiết bị DS3231 vào bus I2C
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &rtc_cfg, &rtc_handle));
    printf("[USR] Adding LCD device to I2C bus...\n");
    // Thêm thiết bị LCD vào bus I2C
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &lcd_cfg, &lcd_handle));
    led_hw_init();
    printf("[USR] I2C devices added. Initializing LCD...\n");
    // Khởi tạo LCD
    lcd_init(lcd_handle);
    lcd_set_cursor(lcd_handle, 0, 0);
    lcd_clear(lcd_handle);
    printf("[USR] LCD initialized. Sending initial signal to Task DS3231 to read time...\n");
    // Gửi tín hiệu đến Task DS3231 để bắt đầu đọc thời gian
    ciedpc_msg_t* msg_to_ds3231 = ciedpc_msg_alloc(TASK_NORM_DS3231_ID, SIG_READ_DS3231, 0);
    ciedpc_task_norm_post_msg(TASK_NORM_DS3231_ID, msg_to_ds3231);
    printf("[USR] Sending initial signal to Task SHT30 to start periodic sampling...\n");
    // Gửi tín hiệu đầu tiên để khởi động vòng lặp đo SHT30 độc lập với vòng lặp DS3231
    ciedpc_msg_t* msg_to_sht30 = ciedpc_msg_alloc(TASK_NORM_SHT30_ID, SIG_READ_SHT30, 0);
    ciedpc_task_norm_post_msg(TASK_NORM_SHT30_ID, msg_to_sht30);
  } else if (msg->sig == SIG_USR_STOP) {
    printf("[USR] Received STOP signal. Stopping the system...\n");
  }
}

void task_norm_ds3231_handler(ciedpc_msg_t* msg) {
  switch (msg->sig) {
    case SIG_READ_DS3231:
      printf("[Task DS3231] Received READ signal. Getting data to send Task LCD...\n");
      ds3231_time_t now;
      bool rtc_ok = (ds3231_read(rtc_handle, &now) == ESP_OK);
      if (rtc_ok) {
        printf("[Task DS3231] Current time: %02u:%02u:%02u %02u/%02u/%04u\n", 
          now.hour, now.minute, now.second, now.date, now.month, now.year);
        ds3231_apply_display_offset(&now, -(int)DS3231_DISPLAY_OFFSET_SEC);
        printf("[Task DS3231] Display time after offset: %02u:%02u:%02u %02u/%02u/%04u\n",
          now.hour, now.minute, now.second, now.date, now.month, now.year);
        // Chuyển đổi thời gian thành chuỗi để gửi đến Task LCD
        snprintf(ds3231_time, sizeof(ds3231_time), "D%02u:%02u:%02u %02u/%02u", 
                now.hour, now.minute, now.second, now.date, now.month);
        // Gửi tín hiệu đến Task LCD để cập nhật hiển thị
        ciedpc_msg_t* msg_to_lcd = ciedpc_msg_alloc(TASK_NORM_LCD_ID, SIG_UPDATE_LCD, sizeof(char*));
        ciedpc_msg_set_data_ref(msg_to_lcd, ds3231_time);
        ciedpc_task_norm_post_msg(TASK_NORM_LCD_ID, msg_to_lcd);
        // Lên lịch lần đọc tiếp theo để đồng hồ luôn cập nhật đều, không phụ thuộc SHT30
        ciedpc_timer_set(TASK_NORM_DS3231_ID, SIG_READ_DS3231, DS3231_UPDATE_PERIOD_MS, CIEDPC_TIMER_ONE_SHOT);
        printf("[Task DS3231] Time data sent to Task LCD. Waiting for next signal...\n");
      } else {
        printf("[Task DS3231] Failed to read time from RTC.\n");
        // Gửi tín hiệu đến task USR để thông báo lỗi nếu cần thiết
        ciedpc_msg_t* msg_to_usr = ciedpc_msg_alloc(CIEDPC_TASK_NORM_USR_ID, SIG_USR_STOP, 0);
        ciedpc_task_norm_post_msg(CIEDPC_TASK_NORM_USR_ID, msg_to_usr);
        printf("[Task DS3231] Sent STOP signal to USR due to RTC read failure. Exiting...\n");
      }
      break;
    default:
      printf("[Task DS3231] Received unknown signal: 0x%04X\n", msg->sig);
      break;
  }
}

void task_norm_sht30_handler(ciedpc_msg_t* msg) {
  uint8_t cmd[2] = {0x2C, 0x0D}; // Command: High Repeatability
  switch (msg->sig) {
    case SIG_READ_SHT30:
      printf("[Task SHT30] Received READ signal. Triggering sensor conversion...\n");
      // Đợi chuyển đổi hoàn thành trong thời gian ngắn rồi mới đọc dữ liệu
      if (i2c_master_transmit(sht30_handle, cmd, 2, 1000) == ESP_OK) {
        ciedpc_timer_set(TASK_NORM_SHT30_ID, SIG_RECALL_SHT30, SHT30_CONVERSION_WAIT_MS, CIEDPC_TIMER_ONE_SHOT);
      } else {
        printf("[Task SHT30] Failed to start sensor conversion.\n");
        ciedpc_msg_t* msg_to_usr = ciedpc_msg_alloc(CIEDPC_TASK_NORM_USR_ID, SIG_USR_STOP, 0);
        ciedpc_task_norm_post_msg(CIEDPC_TASK_NORM_USR_ID, msg_to_usr);
      }
      break;
    case SIG_RECALL_SHT30:
      uint8_t data[6];
      printf("[Task SHT30] Timer expired. Reading sensor data...\n");
      if (i2c_master_receive(sht30_handle, data, 6, 1000) == ESP_OK) {
        uint16_t raw_temp = (data[0] << 8) | data[1];
        uint16_t raw_humi = (data[3] << 8) | data[4];
        // Chuyển đổi giá trị thô thành nhiệt độ và độ ẩm theo công thức của SHT30
        float temp_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
        float humi_rh = 100.0f * ((float)raw_humi / 65535.0f);
        printf("[Task SHT30] Temperature: %.2f °C, Humidity: %.2f %%\n", temp_c, humi_rh);
        if (temp_c > TEMP_LED_THRESHOLD_C) {
          led_blink_enabled = true;
          ciedpc_timer_set(TASK_NORM_LED_ID, SIG_LED_ON, LED_BLINK_PERIOD_MS, CIEDPC_TIMER_PERIODIC);
        } else {
          led_blink_enabled = false;
          ciedpc_timer_remove(TASK_NORM_LED_ID, SIG_LED_ON);
          ciedpc_msg_t* msg_to_led = ciedpc_msg_alloc(TASK_NORM_LED_ID, SIG_LED_OFF, 0);
          ciedpc_task_norm_post_msg(TASK_NORM_LED_ID, msg_to_led);
        }
        // Chuyển đổi dữ liệu cảm biến thành chuỗi để gửi đến Task LCD
        snprintf(sht30_data, sizeof(sht30_data), "S%.2fC %.2f%%", temp_c, humi_rh);
        // Gửi tín hiệu đến Task LCD để cập nhật hiển thị
        ciedpc_msg_t* msg_to_lcd = ciedpc_msg_alloc(TASK_NORM_LCD_ID, SIG_UPDATE_LCD, sizeof(char*));
        ciedpc_msg_set_data_ref(msg_to_lcd, sht30_data);
        ciedpc_task_norm_post_msg(TASK_NORM_LCD_ID, msg_to_lcd);
        // Lấy mẫu định kỳ theo chu kỳ cấu hình, độc lập với DS3231
        ciedpc_timer_set(TASK_NORM_SHT30_ID, SIG_READ_SHT30, SHT30_SAMPLE_PERIOD_MS, CIEDPC_TIMER_ONE_SHOT);
        printf("[Task SHT30] Sensor data sent to Task LCD. Waiting for next signal...\n");
      } else {
        printf("[Task SHT30] Failed to read data from sensor.\n");
        // Gửi tín hiệu đến task USR để thông báo lỗi nếu cần thiết
        ciedpc_msg_t* msg_to_usr = ciedpc_msg_alloc(CIEDPC_TASK_NORM_USR_ID, SIG_USR_STOP, 0);
        ciedpc_task_norm_post_msg(CIEDPC_TASK_NORM_USR_ID, msg_to_usr);
        printf("[Task SHT30] Sent STOP signal to USR due to sensor read failure. Exiting...\n");
      }
      break;
    default:
      printf("[Task SHT30] Received unknown signal: 0x%04X\n", msg->sig);
      break;
  }
}

void task_norm_lcd_handler(ciedpc_msg_t* msg) {
  switch (msg->sig) {
    case SIG_UPDATE_LCD:
      printf("[Task LCD] Received UPDATE signal. Updating display...\n");
      char* lcd_content = (msg->data != NULL) ? (char*)(*(char**)(msg->data)) : "No data";
      // Kiểm tra ký tự đầu tiên là "D" hay "S" để xác định loại dữ liệu và hiển thị phù hợp
      if (lcd_content[0] == 'D') { // DS3231
        printf("[Task LCD] Displaying time: %s\n", lcd_content);
        // Gọi hàm hiển thị thời gian lên LCD
        lcd_set_cursor(lcd_handle, 0, 0);
        lcd_put_str(lcd_handle, lcd_content + 1); // Bỏ ký tự "D" ở đầu chuỗi
        return;
      } else if (lcd_content[0] == 'S') { // SHT30
        printf("[Task LCD] Displaying sensor data: %s\n", lcd_content);
        // Gọi hàm hiển thị dữ liệu cảm biến lên LCD
        lcd_clear(lcd_handle);
        lcd_set_cursor(lcd_handle, 0, 1);
        lcd_put_str(lcd_handle, lcd_content + 1); // Bỏ ký tự "S" ở đầu chuỗi
        return;
      } else {
        printf("[Task LCD] Received unknown data format: %s\n", lcd_content);
      }
      break;
    default:
      printf("[Task LCD] Received unknown signal: 0x%04X\n", msg->sig);
      break;
  }
}

void task_norm_led_handler(ciedpc_msg_t* msg) {
  switch (msg->sig) {
    case SIG_LED_ON:
      if (led_blink_enabled) {
        printf("[Task LED] Blinking LED...\n");
        led_hw_toggle();
      }
      break;
    case SIG_LED_OFF:
      printf("[Task LED] Received LED OFF signal. Turning off LED...\n");
      led_hw_set(false);
      break;
    default:
      printf("[Task LED] Received unknown signal: 0x%04X\n", msg->sig);
      break;
  }
}

void task_poll_memrp_handler() {
  pal_memrp_report(&(pal_memrp_info_t){ .type = CIEDPC_MSG_TYPE_BLANK});
  pal_memrp_report(&(pal_memrp_info_t){ .type = CIEDPC_MSG_TYPE_ALLOC});
  pal_memrp_report(&(pal_memrp_info_t){ .type = CIEDPC_MSG_TYPE_EXTAL});
  pal_memrp_report(&(pal_memrp_info_t){ .type = CIEDPC_MSG_TYPE_ISR});
  ciedpc_task_poll_set_ability(CIEDPC_TASK_POLL_MEMRP_ID, false);
}

void task_poll_syslf_handler() {
  printf("[SYSLF] System is alive. Action count: %" PRIu32 "\n", system_action_count);
  system_action_count++;

  if (system_action_count >= 10) {
    printf("[SYSLF] Action count reached 10. Stopping SYSLF polling...\n");
    ciedpc_task_poll_set_ability(CIEDPC_TASK_POLL_SYSLF_ID, false);
  }
}

/**
 * @brief Định nghĩa các hàm on-entry/exit cho các trạng thái của TSM (nếu có)
 */

// Điền các khai báo tại đây

/**
 * @brief Định nghĩa các state handler cho FSM của task USR, task DS3231, task SHT30 và task LCD (nếu có)
 */

// Điền các khai báo tại đây

/**
 * @brief Định nghĩa các hàm phụ trợ khác (nếu có)
 */

ui8 crc8(ui8 *data, int len) {
  ui8 crc = 0xFF;
  for (int j = 0; j < len; j++) {
    crc ^= data[j];
    for (int i = 0; i < 8; i++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x31;
      else crc <<= 1;
    }
  }
  return crc;
}

ui8 bcd_to_dec(ui8 value) {
  return (ui8)(((value >> 4) * 10U) + (value & 0x0F));
}

static bool is_leap_year(ui16 year) {
  return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

static ui8 days_in_month(ui16 year, ui8 month) {
  static const ui8 days_per_month[] = {
    31, 28, 31, 30, 31, 30,
    31, 31, 30, 31, 30, 31
  };

  if (month == 0U || month > 12U) {
    return 31U;
  }

  if (month == 2U && is_leap_year(year)) {
    return 29U;
  }

  return days_per_month[month - 1U];
}

static void ds3231_apply_display_offset(ds3231_time_t *time, int offset_seconds) {
  if (!time || offset_seconds == 0) {
    return;
  }

  int second = (int)time->second + offset_seconds;
  int minute = (int)time->minute;
  int hour = (int)time->hour;
  int date = (int)time->date;
  int month = (int)time->month;
  int year = (int)time->year;

  while (second < 0) {
    second += 60;
    minute--;
  }

  while (second >= 60) {
    second -= 60;
    minute++;
  }

  while (minute < 0) {
    minute += 60;
    hour--;
  }

  while (minute >= 60) {
    minute -= 60;
    hour++;
  }

  while (hour < 0) {
    hour += 24;
    date--;
  }

  while (hour >= 24) {
    hour -= 24;
    date++;
  }

  while (date < 1) {
    month--;
    if (month < 1) {
      month = 12;
      year--;
    }
    date += days_in_month((ui16)year, (ui8)month);
  }

  while (date > days_in_month((ui16)year, (ui8)month)) {
    date -= days_in_month((ui16)year, (ui8)month);
    month++;
    if (month > 12) {
      month = 1;
      year++;
    }
  }

  time->second = (ui8)second;
  time->minute = (ui8)minute;
  time->hour = (ui8)hour;
  time->date = (ui8)date;
  time->month = (ui8)month;
  time->year = (ui16)year;
}

esp_err_t ds3231_read(i2c_master_dev_handle_t rtc_handle, ds3231_time_t *now) {
  if (!rtc_handle || !now) {
    return ESP_ERR_INVALID_ARG;
  }

  ui8 reg = 0x00;
  ui8 raw[7] = {0};
  esp_err_t ret = i2c_master_transmit_receive(rtc_handle, &reg, 1, raw, sizeof(raw), 1000);
  if (ret != ESP_OK) {
    return ret;
  }

  now->second = bcd_to_dec(raw[0] & 0x7F);
  now->minute = bcd_to_dec(raw[1] & 0x7F);

  if (raw[2] & 0x40) {
    ui8 hour = bcd_to_dec(raw[2] & 0x1F);
    bool is_pm = (raw[2] & 0x20) != 0;
    if (hour == 12) {
      hour = 0;
    }
    if (is_pm) {
      hour = (ui8)(hour + 12U);
    }
    now->hour = hour;
  } else {
    now->hour = bcd_to_dec(raw[2] & 0x3F);
  }

  now->day_of_week = raw[3] & 0x07;
  now->date = bcd_to_dec(raw[4] & 0x3F);
  now->month = bcd_to_dec(raw[5] & 0x1F);
  now->year = (ui16)(2000U + bcd_to_dec(raw[6]));

  return ESP_OK;
}

static void led_hw_init(void) {
  gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << LED_GPIO_NUM),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };

  gpio_config(&io_conf);
  led_hw_set(false);
}

static void led_hw_set(bool on) {
  led_output_state = on;
  gpio_set_level((gpio_num_t)LED_GPIO_NUM, on ? 1 : 0);
}

static void led_hw_toggle(void) {
  led_hw_set(!led_output_state);
}
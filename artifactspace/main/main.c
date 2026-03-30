// Khai báo các thư viện sử dụng

#include <stdio.h>
#include "reset_driver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/rtc.h"
#include "esp_private/periph_ctrl.h"
#include "sdkconfig.h"
#include "driver/i2c_master.h"
#include "rom/ets_sys.h"


// Khai báo các hằng số sử dụng
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define SHT30_ADDR 0x44 // Địa chỉ I2C mặc định của cảm biến SHT30
#define LCD_ADDR 0x27 // Địa chỉ I2C ghi mặc định của màn LCD16X2
#define LCD_RS_BIT      0x01    // P0
#define LCD_RW_BIT      0x02    // P1 (Thường nối GND)
#define LCD_EN_BIT      0x04    // P2
#define LCD_BL_BIT      0x08    // P3
static const char *TAG = "CLOCK_TASK";

// Khai báo hàm tính CRC8 cho dữ liệu nhận được từ cảm biến SHT30
uint8_t crc8(uint8_t *data, int len) {
  uint8_t crc = 0xFF;
  for (int j = 0; j < len; j++) {
    crc ^= data[j];
    for (int i = 0; i < 8; i++) {
      if (crc & 0x80) crc = (crc << 1) ^ 0x31;
      else crc <<= 1;
    }
  }
  return crc;
}

void lcd_send_cmd(i2c_master_dev_handle_t dev_handle, uint8_t cmd) {
  uint8_t data_u = (cmd & 0xF0);
  uint8_t data_l = ((cmd << 4) & 0xF0);
  uint8_t data_t[4];

  data_t[0] = data_u | 0x0C;  // EN=1, RS=0, BL=1
  data_t[1] = data_u | 0x08;  // EN=0, RS=0, BL=1
  data_t[2] = data_l | 0x0C;  // EN=1, RS=0, BL=1
  data_t[3] = data_l | 0x08;  // EN=0, RS=0, BL=1

  i2c_master_transmit(dev_handle, data_t, 4, 1000);
}

void lcd_send_data(i2c_master_dev_handle_t dev_handle, uint8_t data) {
  uint8_t data_u = (data & 0xF0);
  uint8_t data_l = ((data << 4) & 0xF0);
  uint8_t data_t[4];

  data_t[0] = data_u | 0x0D;  // EN=1, RS=1, BL=1
  data_t[1] = data_u | 0x09;  // EN=0, RS=1, BL=1
  data_t[2] = data_l | 0x0D;  // EN=1, RS=1, BL=1
  data_t[3] = data_l | 0x09;  // EN=0, RS=1, BL=1

  i2c_master_transmit(dev_handle, data_t, 4, 1000);
}

void lcd_init(i2c_master_dev_handle_t dev_handle) {
    // 1. Chờ LCD ổn định điện áp (>40ms)
    vTaskDelay(pdMS_TO_TICKS(50));

    lcd_send_cmd(dev_handle, 0x30); // Gửi 0x30 lần 1
    vTaskDelay(pdMS_TO_TICKS(5));

    // Gửi 0x30 lần 2
    lcd_send_cmd(dev_handle, 0x30); // Gửi 0x30 lần 2
    vTaskDelay(pdMS_TO_TICKS(5));

    // Gửi 0x30 lần 3
    lcd_send_cmd(dev_handle, 0x30); // Gửi 0x30 lần 3
    vTaskDelay(pdMS_TO_TICKS(1));

    // 3. Thiết lập chế độ 4-bit (Gửi 0x20)
    lcd_send_cmd(dev_handle, 0x20);
    vTaskDelay(pdMS_TO_TICKS(1));

    // 4. Các thiết lập chức năng
    lcd_send_cmd(dev_handle, 0x28); // Function Set: 4-bit, 2 dòng, font 5x8
    vTaskDelay(pdMS_TO_TICKS(10));
    lcd_send_cmd(dev_handle, 0x08); // Display Off
    vTaskDelay(pdMS_TO_TICKS(10));
    lcd_send_cmd(dev_handle, 0x01); // Clear Display
    vTaskDelay(pdMS_TO_TICKS(10));
    lcd_send_cmd(dev_handle, 0x06); // Entry Mode: Tăng con trỏ, không dịch màn hình
    vTaskDelay(pdMS_TO_TICKS(10));
    lcd_send_cmd(dev_handle, 0x0C); // Display On: Hiện màn hình, tắt con trỏ/nháy
    vTaskDelay(pdMS_TO_TICKS(10));
}

void lcd_clear(i2c_master_dev_handle_t dev_handle) {
  lcd_send_cmd(dev_handle, 0x01); // Clear Display
  vTaskDelay(pdMS_TO_TICKS(100)); // Clear Display cần thời gian lâu
}

void lcd_put_cur(i2c_master_dev_handle_t dev_handle, uint8_t row, uint8_t col) {
  switch (row) {
    case 0:
      col |= 0x80;
      break;
    case 1:
      col |= 0xC0;
      break;
    default:
      col |= 0x80;
      break;
  }

  lcd_send_cmd(dev_handle, col);
}

void lcd_send_string(i2c_master_dev_handle_t dev_handle, const char *str) {
  while (*str) {
    lcd_send_data(dev_handle, (uint8_t)(*str++));
  }
}

void lcd_put_str(i2c_master_dev_handle_t dev_handle, const char *str) {
  lcd_send_string(dev_handle, str);
}

void lcd_set_cursor(i2c_master_dev_handle_t dev_handle, uint8_t col, uint8_t row) {
  lcd_put_cur(dev_handle, row, col);
}

// Hàm main của ứng dụng
/**
 * Ghi chú:
 * Cấu hình chân trên mạch thực tế như sau:
 * - SCL chân D22
 * - SDA chân D21
 * - VCC chân 3.3V
 * - GND chân GND
 */
void app_main(void) {
  // In ra thông báo khi ứng dụng bắt đầu chạy
  ESP_LOGI("main", "Firmware starts running");

  printf("\n");
  printf("====================================\n");
  printf("ESP32 TEMPERATURE - HUMIDITY CLOCK\n");
  printf("RESET DRIVER INITIALIZATION\n");
  printf("====================================\n");

  check_reset_reason();

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

  ESP_LOGI(TAG, "---- BAT DAU THUC HIEN CLOCK TASK ----");

  /**
   * Ghi chú:
   * Phần cấu hình của clock có thể được cấu hình sẵn trong menuconfig,
   * nên có thể sẽ cần chuyển đổi chức năng sang kiểm tra xung clock hoạt động ở bao nhiêu MHz.
   */

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

  // Cấu hình chân I2C cho master
  i2c_master_bus_config_t i2c_bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .scl_io_num = GPIO_NUM_22,
    .sda_io_num = GPIO_NUM_21,
    .glitch_ignore_cnt = 7
  };
  i2c_master_bus_handle_t bus_handle;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

  for (int i = 1; i < 127; i++) {
    esp_err_t res = i2c_master_probe(bus_handle, i, -1);
    if (res == ESP_OK) {
      ESP_LOGI("SCAN", "Found device at address: 0x%02x", i);
    }
  }

  // Cấu hình thiết bị I2C cho cảm biến SHT30
  i2c_device_config_t sht30_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .scl_speed_hz = 200000, // 200 kHz
    .device_address = SHT30_ADDR
  };
  i2c_master_dev_handle_t sht30_handle;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &sht30_cfg, &sht30_handle));

  // Cấu hình thiết bị I2C cho màn LCD16X2
  i2c_device_config_t lcd_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .scl_speed_hz = 100000, // 200 kHz
    .device_address = LCD_ADDR
  };
  i2c_master_dev_handle_t lcd_handle;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &lcd_cfg, &lcd_handle));
  
  lcd_init(lcd_handle);
  lcd_set_cursor(lcd_handle, 0, 0);
  lcd_put_str(lcd_handle, "System start");
  vTaskDelay(pdMS_TO_TICKS(2000));

  uint8_t cmd[2] = {0x2C, 0x0D}; // Command: High Repeatability
  uint8_t data[6];

  // Gửi lệnh đọc dữ liệu từ cảm biến SHT30
  ESP_ERROR_CHECK(i2c_master_transmit(sht30_handle, cmd, 2, 1000));

  // Đợi một khoảng thời gian để cảm biến có thể trả về dữ liệu
  vTaskDelay(pdMS_TO_TICKS(20));

  // Đọc dữ liệu từ cảm biến SHT30
  ESP_ERROR_CHECK(i2c_master_receive(sht30_handle, data, 6, 1000));

  // Vòng lặp chính của ứng dụng
  while (1) {
    ESP_LOGI("main", "----------------------------");
    ESP_LOGI("main", "Requesting data from SHT30...");

    // Gửi lệnh đo (Dùng transmit thay vì ESP_ERROR_CHECK để tránh crash nếu lỏng dây)
    esp_err_t ret = i2c_master_transmit(sht30_handle, cmd, 2, 1000);

    if (ret == ESP_OK) {
      // Đợi cảm biến hoàn thành phép đo (High repeatability cần tối đa 15ms)
      vTaskDelay(pdMS_TO_TICKS(20)); 

      // Đọc 6 bytes dữ liệu
      ret = i2c_master_receive(sht30_handle, data, 6, 1000);

      if (ret == ESP_OK) {
        // Kiểm tra CRC cho Nhiệt độ và Độ ẩm
        if (crc8(data, 2) == data[2] && crc8(data + 3, 2) == data[5]) {
          // Chuyển đổi giá trị Raw sang Vật lý theo công thức Datasheet
          uint16_t raw_temp = (data[0] << 8) | data[1];
          uint16_t raw_humi = (data[3] << 8) | data[4];

          float temp = -45.0 + (175.0 * (float)raw_temp / 65535.0);
          float humi = 100.0 * ((float)raw_humi / 65535.0);

          ESP_LOGI("main", "Temperature: %.2f °C", temp);
          ESP_LOGI("main", "Humidity: %.2f %%", humi);

          // Hiển thị lên LCD
          lcd_clear(lcd_handle);
          lcd_set_cursor(lcd_handle, 0, 0);
          char line1[17];
          snprintf(line1, sizeof(line1), "Temp: %.2f C", temp);
          lcd_put_str(lcd_handle, line1);
          lcd_set_cursor(lcd_handle, 0, 1);
          char line2[17];
          snprintf(line2, sizeof(line2), "Humi: %.2f %%", humi);
          lcd_put_str(lcd_handle, line2);

        } else {
          ESP_LOGE("main", "CRC Check Failed!");
        }
      } else {
        ESP_LOGE("main", "I2C Receive Failed!");
      }
    } else {
      ESP_LOGE("main", "I2C Transmit Failed! Check connection.");
    }

    // Đợi 30 giây cho lần đọc tiếp theo
    ESP_LOGI("main", "Sleeping for 30 seconds...");
    vTaskDelay(pdMS_TO_TICKS(30000)); 
  }    
       
}
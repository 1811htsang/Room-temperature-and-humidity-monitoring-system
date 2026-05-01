/**
 * @file main.C
 * @author Huynh Thanh Sang
 * @brief Firmware chính của dự án, thực hiện các chức năng sau:
 * @version 0.1
 * @date 2026-05-01
 * 
 * @copyright GNU GENERAL PUBLIC LICENSE Version 3 (GPLv3) - https://www.gnu.org/licenses/gpl-3.0.en.html
 * 
 */

/**
 * @brief Khai báo các thư viện sử dụng
 */

#include <stdio.h>
#include <string.h>
#include "reset.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "soc/rtc.h"
#include "esp_private/periph_ctrl.h"
#include "sdkconfig.h"
#include "driver/i2c_master.h"
#include "rom/ets_sys.h"
#include "esp_clk_tree.h"
#include "lcd.h"

/**
 * @brief Định nghĩa các hằng số và cấu trúc dữ liệu cần thiết cho ứng dụng
 */

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define SHT30_ADDR 0x44 // Địa chỉ I2C mặc định của cảm biến SHT30
#define LCD_ADDR 0x27 // Địa chỉ I2C ghi mặc định của màn LCD16X2
#define LCD_RS_BIT      0x01    // P0
#define LCD_RW_BIT      0x02    // P1 (Thường nối GND)
#define LCD_EN_BIT      0x04    // P2
#define LCD_BL_BIT      0x08    // P3

/**
 * @brief Hàm tính CRC8
 * @param data Dữ liệu cần tính CRC
 * @param len Độ dài của dữ liệu
 * @return uint8_t Giá trị CRC8 tính được
 */
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

/**
 * @brief Hàm lấy thông tin tần số clock của CPU
 */
void get_clock(void) {
    uint32_t cpu_freq_mhz = 0;
    ESP_ERROR_CHECK(esp_clk_tree_src_get_freq_hz(
        SOC_MOD_CLK_CPU, 
        ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT, 
        &cpu_freq_mhz)
    );
    ESP_LOGI("main", "CPU frequency: %u Hz (~%u MHz)", cpu_freq_mhz, cpu_freq_mhz / 1000000U);
}

/**
 * @brief Hàm chính của ứng dụng
 */
void app_main(void) {
  ESP_LOGI("main", "Firmware starts running");

  check_reset_reason();

  get_clock();

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
  
  // Initialize LCD
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
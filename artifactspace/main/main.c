// Khai báo các thư viện sử dụng

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

// Khai báo các hằng số sử dụng
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define SHT30_ADDR 0x44 // Địa chỉ I2C mặc định của cảm biến SHT30

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

  // Cấu hình thiết bị I2C cho cảm biến SHT30
  i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x44, // SHT3x Address
    .scl_speed_hz = 200000 // 200 kHz
  };
  i2c_master_dev_handle_t sht30_handle;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &sht30_handle));

  uint8_t cmd[2] = {0x2C, 0x0D}; // Command: High Repeatability
  uint8_t data[6];

  // Gửi lệnh đọc dữ liệu từ cảm biến SHT30
  ESP_ERROR_CHECK(i2c_master_transmit(sht30_handle, cmd, 2, -1));

  // Đợi một khoảng thời gian để cảm biến có thể trả về dữ liệu
  vTaskDelay(20 / portTICK_PERIOD_MS); 

  // Đọc dữ liệu từ cảm biến SHT30
  ESP_ERROR_CHECK(i2c_master_receive(sht30_handle, data, 6, -1));

  // Vòng lặp chính của ứng dụng
  while (1) {
    ESP_LOGI("main", "----------------------------");
    ESP_LOGI("main", "Requesting data from SHT30...");

    // Gửi lệnh đo (Dùng transmit thay vì ESP_ERROR_CHECK để tránh crash nếu lỏng dây)
    esp_err_t ret = i2c_master_transmit(sht30_handle, cmd, 2, -1);
    
    if (ret == ESP_OK) {
      // Đợi cảm biến hoàn thành phép đo (High repeatability cần tối đa 15ms)
      vTaskDelay(pdMS_TO_TICKS(20)); 

      // Đọc 6 bytes dữ liệu
      ret = i2c_master_receive(sht30_handle, data, 6, -1);

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

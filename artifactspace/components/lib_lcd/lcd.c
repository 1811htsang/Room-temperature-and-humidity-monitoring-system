#include "lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
  vTaskDelay(pdMS_TO_TICKS(50));
  lcd_send_cmd(dev_handle, 0x30);
  vTaskDelay(pdMS_TO_TICKS(5));
  lcd_send_cmd(dev_handle, 0x30);
  vTaskDelay(pdMS_TO_TICKS(5));
  lcd_send_cmd(dev_handle, 0x30);
  vTaskDelay(pdMS_TO_TICKS(1));
  lcd_send_cmd(dev_handle, 0x20);
  vTaskDelay(pdMS_TO_TICKS(1));
  lcd_send_cmd(dev_handle, 0x28);
  vTaskDelay(pdMS_TO_TICKS(10));
  lcd_send_cmd(dev_handle, 0x08);
  vTaskDelay(pdMS_TO_TICKS(10));
  lcd_send_cmd(dev_handle, 0x01);
  vTaskDelay(pdMS_TO_TICKS(10));
  lcd_send_cmd(dev_handle, 0x06);
  vTaskDelay(pdMS_TO_TICKS(10));
  lcd_send_cmd(dev_handle, 0x0C);
  vTaskDelay(pdMS_TO_TICKS(10));
}

void lcd_clear(i2c_master_dev_handle_t dev_handle) {
  lcd_send_cmd(dev_handle, 0x01);
  vTaskDelay(pdMS_TO_TICKS(100));
}

void lcd_set_cursor(i2c_master_dev_handle_t dev_handle, uint8_t col, uint8_t row) {
  uint8_t address = (row == 0) ? col : col + 0x40;
  lcd_send_cmd(dev_handle, 0x80 | address);
}

void lcd_put_str(i2c_master_dev_handle_t dev_handle, const char *str) {
  while (*str) {
      lcd_send_data(dev_handle, (uint8_t)(*str++));
  }
}
#ifndef LCD_H
#define LCD_H

#include "driver/i2c_master.h"

void lcd_init(i2c_master_dev_handle_t dev_handle);
void lcd_clear(i2c_master_dev_handle_t dev_handle);
void lcd_set_cursor(i2c_master_dev_handle_t dev_handle, uint8_t col, uint8_t row);
void lcd_put_str(i2c_master_dev_handle_t dev_handle, const char *str);

#endif // LCD_H
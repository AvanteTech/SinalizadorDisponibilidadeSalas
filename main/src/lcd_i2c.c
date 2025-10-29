#include "lcd_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define TAG "I2C_LCD_LIB"

#define LCD_CMD_FUNCTION_SET 0x20
#define LCD_CMD_DISPLAY_CONTROL 0x08
#define LCD_CMD_ENTRY_MODE_SET 0x04
#define LCD_CMD_CLEAR_DISPLAY 0x01
#define LCD_CMD_SET_DDRAM_ADDR 0x80

#define LCD_FLAG_4_BIT_MODE 0x00
#define LCD_FLAG_2_LINE 0x08
#define LCD_FLAG_DISPLAY_ON 0x04
#define LCD_FLAG_ENTRY_LEFT 0x02

#define LCD_BIT_RS (1 << 0)
#define LCD_BIT_EN (1 << 2)
#define LCD_BIT_BACKLIGHT (1 << 3)

esp_err_t i2c_lcd_driver_init(void) {
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = LCD_I2C_SDA_PIN,
      .scl_io_num = LCD_I2C_SCL_PIN,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = LCD_I2C_CLK_SPEED,
  };
  i2c_param_config(LCD_I2C_PORT, &conf);
  return i2c_driver_install(LCD_I2C_PORT, conf.mode, 0, 0, 0);
}

static esp_err_t lcd_send_internal(uint8_t value, uint8_t flags) {
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (LCD_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);

  uint8_t high_nibble = value & 0xF0;
  uint8_t low_nibble = (value << 4) & 0xF0;

  i2c_master_write_byte(cmd, high_nibble | flags | LCD_BIT_EN | LCD_BIT_BACKLIGHT, true);
  i2c_master_write_byte(cmd, high_nibble | flags | LCD_BIT_BACKLIGHT, true);
  i2c_master_write_byte(cmd, low_nibble | flags | LCD_BIT_EN | LCD_BIT_BACKLIGHT, true);
  i2c_master_write_byte(cmd, low_nibble | flags | LCD_BIT_BACKLIGHT, true);

  i2c_master_stop(cmd);
  esp_err_t res = i2c_master_cmd_begin(LCD_I2C_PORT, cmd, pdMS_TO_TICKS(100));
  i2c_cmd_link_delete(cmd);
  return res;
}

void lcd_send_cmd(uint8_t cmd) { lcd_send_internal(cmd, 0); }
void lcd_send_data(uint8_t data) { lcd_send_internal(data, LCD_BIT_RS); }

void lcd_init(void) {
  vTaskDelay(pdMS_TO_TICKS(50));
  lcd_send_cmd(0x30); vTaskDelay(pdMS_TO_TICKS(5));
  lcd_send_cmd(0x30); vTaskDelay(pdMS_TO_TICKS(1));
  lcd_send_cmd(0x30); vTaskDelay(pdMS_TO_TICKS(1));
  lcd_send_cmd(0x20); vTaskDelay(pdMS_TO_TICKS(1));

  lcd_send_cmd(LCD_CMD_FUNCTION_SET | LCD_FLAG_4_BIT_MODE | LCD_FLAG_2_LINE);
  lcd_send_cmd(LCD_CMD_DISPLAY_CONTROL | LCD_FLAG_DISPLAY_ON);
  lcd_send_cmd(LCD_CMD_ENTRY_MODE_SET | LCD_FLAG_ENTRY_LEFT);
  lcd_clear();
}

void lcd_clear(void) {
  lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY);
  vTaskDelay(pdMS_TO_TICKS(5));
}

void lcd_set_cursor(uint8_t col, uint8_t row) {
  const uint8_t row_offsets[] = {0x00, 0x40};
  lcd_send_cmd(LCD_CMD_SET_DDRAM_ADDR | (col + row_offsets[row]));
}

void lcd_send_string(const char *str) {
  while (*str) {
    lcd_send_data(*str++);
  }
}
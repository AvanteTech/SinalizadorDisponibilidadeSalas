#ifndef I2C_LCD_H
#define I2C_LCD_H

#include "driver/i2c.h"
#include "esp_err.h"

// CONFIGURAÇÕES - VERIFIQUE ESTES VALORES
#define LCD_I2C_ADDR        0x27      // Endereço I2C que o scanner encontrou
#define LCD_I2C_PORT        I2C_NUM_0 // Porta I2C a ser usada
#define LCD_I2C_SDA_PIN     13        // Pino para SDA
#define LCD_I2C_SCL_PIN     14        // Pino para SCL
#define LCD_I2C_CLK_SPEED   1000000   // Frequência do Clock I2C

// Funções da biblioteca
esp_err_t i2c_lcd_driver_init(void); 
void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_send_string(const char *str);
void lcd_send_char(char c);
void lcd_reinit();

#endif
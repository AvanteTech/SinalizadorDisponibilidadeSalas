/**
 * @file esp_lcd.h
 * @author Jesus Minjares (https://github.com/jminjares4)
 * @brief Liquid Crystal Display header file
 * @version 0.1
 * @date 2022-08-15
 * @copyright Copyright (c) 2022
 *
 */
#ifndef _ESP_LCD_H_
#define _ESP_LCD_H_

#include "driver/gpio.h"

/* LCD Error */
typedef int lcd_err_t;      /*!< LCD error type */

#define LCD_FAIL            -1  /*!< LCD fail error */
#define LCD_OK               0  /*!< LCD success    */
#define LCD_DATA 0        /*!< LCD data */
#define LCD_CMD 1         /*!< LCD command */
#define GPIO_STATE_LOW 0  /*!< Logic low */
#define GPIO_STATE_HIGH 1 /*!< Logic high */

/* Default pinout  */
#define DATA_0_PIN 14          /*!< DATA 0 */
#define DATA_1_PIN 27          /*!< DATA 0 */
#define DATA_2_PIN 26          /*!< DATA 0 */
#define DATA_3_PIN 25          /*!< DATA 0 */
#define ENABLE_PIN 33          /*!< Enable  */
#define REGISTER_SELECT_PIN 32 /*!< Register Select  */

#define LCD_DATA_LINE 4 /*!< 4-Bit data line */

typedef enum {
    LCD_SCROLL_LEFT,
    LCD_SCROLL_RIGHT
} lcd_scroll_dir_t;

/******************************************************************
 * \enum lcd_state esp_lcd.h 
 * \brief LCD state enumeration
 *******************************************************************/
typedef enum {
    LCD_INACTIVE = 0,   /*!< LCD inactive */
    LCD_ACTIVE = 1,     /*!< LCD active   */
}lcd_state_t;

/******************************************************************
 * \struct lcd_t esp_lcd.h 
 * \brief LCD object
 * 
 * ### Example
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~.c
 * typedef struct {
 *      gpio_num_t data[LCD_DATA_LINE];
 *      gpio_num_t en;
 *      gpio_num_t regSel;
 *      lcd_state_t state;
 * }lcd_t;
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *******************************************************************/
typedef struct
{
    gpio_num_t data[LCD_DATA_LINE]; /*!< LCD data line  */
    gpio_num_t en;                  /*!< LCD enable pin */
    gpio_num_t regSel;              /*!< LCD register select */
    lcd_state_t state;              /*!< LCD state  */
} lcd_t;

void lcdDefault(lcd_t *const lcd);

void lcdInit(lcd_t *const lcd);

void lcdCtor(lcd_t *lcd, gpio_num_t data[LCD_DATA_LINE], gpio_num_t en, gpio_num_t regSel);

lcd_err_t lcdSetText(lcd_t *const lcd, const char *text, int x, int y);

lcd_err_t lcdSetInt(lcd_t *const lcd, int val, int x, int y);

lcd_err_t lcdClear(lcd_t *const lcd);

void lcdFree(lcd_t * const lcd);

void assert_lcd(lcd_err_t lcd_error);

lcd_err_t lcdScrollText(lcd_t *const lcd, const char *text, int y, lcd_scroll_dir_t direction, int speed_ms);

#endif
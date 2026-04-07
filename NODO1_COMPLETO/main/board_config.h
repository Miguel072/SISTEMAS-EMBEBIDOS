#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

#define LCD_HOST               SPI2_HOST

#define LCD_H_RES              240
#define LCD_V_RES              240

/* LCD pins */
#define PIN_LCD_BL             GPIO_NUM_2
#define PIN_LCD_DC             GPIO_NUM_8
#define PIN_LCD_CS             GPIO_NUM_9
#define PIN_LCD_SCLK           GPIO_NUM_10
#define PIN_LCD_MOSI           GPIO_NUM_11
#define PIN_LCD_MISO           GPIO_NUM_12
#define PIN_LCD_RST            GPIO_NUM_14

/* Touch pins */
#define PIN_TOUCH_INT          GPIO_NUM_5
#define PIN_TOUCH_RST          GPIO_NUM_13

/* Shared I2C bus: touch + IMU */
#define PIN_I2C_SDA            GPIO_NUM_6
#define PIN_I2C_SCL            GPIO_NUM_7

#define TOUCH_DOT_SIZE         16

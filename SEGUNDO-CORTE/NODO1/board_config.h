#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

// ============================================================
// Board: Waveshare ESP32-S3 Touch LCD 1.28
// ============================================================

#define LCD_HOST                SPI2_HOST
#define LCD_H_RES               240
#define LCD_V_RES               240

// LCD GC9A01A pins
#define PIN_LCD_BL              GPIO_NUM_2
#define PIN_LCD_DC              GPIO_NUM_8
#define PIN_LCD_CS              GPIO_NUM_9
#define PIN_LCD_SCLK            GPIO_NUM_10
#define PIN_LCD_MOSI            GPIO_NUM_11
#define PIN_LCD_MISO            GPIO_NUM_12
#define PIN_LCD_RST             GPIO_NUM_14

// Internal I2C bus on this board: QMI8658 IMU + touch controller.
// Do not connect external modules here unless the board exposes these pins.
#define PIN_I2C_INTERNAL_SDA    GPIO_NUM_6
#define PIN_I2C_INTERNAL_SCL    GPIO_NUM_7
#define I2C_INTERNAL_PORT_NUM   I2C_NUM_0

// External I2C bus for the MAX30100 through the expansion connector.
// User-confirmed wiring: MAX30100 SDA -> GPIO15, SCL -> GPIO16.
#define PIN_MAX30100_SDA        GPIO_NUM_15
#define PIN_MAX30100_SCL        GPIO_NUM_16
#define I2C_MAX30100_PORT_NUM   I2C_NUM_1

#define I2C_FREQ_HZ             400000

// Optional touch pins. This version does not use touch interaction,
// because the requirement only asks to present the measured data.
#define PIN_TOUCH_INT           GPIO_NUM_5
#define PIN_TOUCH_RST           GPIO_NUM_13

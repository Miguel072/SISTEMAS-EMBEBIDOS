#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "qmi8658_simple.h"
#include "max30100_simple.h"

esp_err_t lcd_ui_init(void);
void lcd_ui_show_data(const qmi8658_sample_t *imu, const max30100_result_t *vitals, bool tx_ok, uint32_t seq);
void lcd_ui_handle_touch(uint16_t x, uint16_t y);

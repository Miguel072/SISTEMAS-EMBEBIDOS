#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

typedef struct {
    float acc_x;
    float acc_y;
    float acc_z;
    float gyr_x;
    float gyr_y;
    float gyr_z;
    bool valid;
} qmi8658_sample_t;

esp_err_t qmi8658_simple_init(i2c_master_bus_handle_t bus);
esp_err_t qmi8658_simple_read(qmi8658_sample_t *out);

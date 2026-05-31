#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// El nombre del modulo se conserva como max30100_simple para no cambiar el resto
// del proyecto. Internamente esta version usa el mapa de registros compatible con
// MAX30102/MAX30105, que es el que funciono en la prueba Arduino con MAX30105.h.
typedef struct {
    float bpm;
    float spo2;
    uint32_t red_raw;
    uint32_t ir_raw;
    bool valid;
    bool device_ok;
    bool finger_detected;
    float ratio_norm;
} max30100_result_t;

esp_err_t max30100_simple_init(i2c_master_bus_handle_t bus);
void max30100_simple_process(void);
max30100_result_t max30100_simple_get(void);

#include "max30100_simple.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "config.h"

static const char *TAG = "MAX3010X";

// ============================================================
// Driver directo para MAX30102 / MAX30105 compatible con la
// configuracion que funciono en Arduino usando MAX30105.h:
//   sensor.setup(0x0A, 1, 2, 400, 411, 16384)
//   red = 0x05, IR = 0x30
//
// Se mantiene el nombre de archivo max30100_simple para no cambiar
// main.c, pero esta version usa registros MAX30102/MAX30105.
// ============================================================

#define MAX3010X_ADDR              0x57

#define REG_INTR_STATUS_1          0x00
#define REG_INTR_STATUS_2          0x01
#define REG_INTR_ENABLE_1          0x02
#define REG_INTR_ENABLE_2          0x03
#define REG_FIFO_WR_PTR            0x04
#define REG_OVF_COUNTER            0x05
#define REG_FIFO_RD_PTR            0x06
#define REG_FIFO_DATA              0x07
#define REG_FIFO_CONFIG            0x08
#define REG_MODE_CONFIG            0x09
#define REG_SPO2_CONFIG            0x0A
#define REG_LED1_PA_RED            0x0C
#define REG_LED2_PA_IR             0x0D
#define REG_MULTI_LED_CTRL1        0x11
#define REG_MULTI_LED_CTRL2        0x12
#define REG_PART_ID                0xFF

#define MODE_RESET                 0x40
#define MODE_RED_IR                0x03

// Valores equivalentes a la configuracion que funciono en Arduino.
#define MAX3010X_FIFO_CONFIG_VALUE 0x10  // promedio 1, rollover habilitado
#define MAX3010X_SPO2_CONFIG_VALUE 0x6F  // ADC 16384nA, 400Hz, pulse width 411us
#define MAX3010X_RED_PA_VALUE      0x05
#define MAX3010X_IR_PA_VALUE       0x30
#define MAX3010X_FINGER_TH         15000UL
#define RATE_SIZE                  8

static i2c_master_dev_handle_t s_dev = NULL;
static bool s_ok = false;

static max30100_result_t s_result = {
    .bpm = 0.0f,
    .spo2 = 0.0f,
    .red_raw = 0,
    .ir_raw = 0,
    .valid = false,
    .device_ok = false,
    .finger_detected = false,
    .ratio_norm = 0.0f,
};

// Estado del algoritmo de pulso.
static float s_ir_dc = 0.0f;
static float s_ir_ac_abs = 1.0f;
static bool s_above_threshold = false;
static int64_t s_last_beat_ms = 0;
static float s_rates[RATE_SIZE] = {0};
static uint8_t s_rate_spot = 0;
static uint8_t s_rate_count = 0;
static float s_beat_avg = 0.0f;
static float s_spo2_filtered = 0.0f;
static float s_last_ratio_norm = 0.0f;

static esp_err_t mx_write(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    return i2c_master_transmit(s_dev, data, sizeof(data), pdMS_TO_TICKS(100));
}

static esp_err_t mx_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static uint8_t fifo_available(uint8_t wr, uint8_t rd)
{
    return (wr - rd) & 0x1F;
}

static void reset_processing_state(void)
{
    s_ir_dc = 0.0f;
    s_ir_ac_abs = 1.0f;
    s_above_threshold = false;
    s_last_beat_ms = 0;
    memset(s_rates, 0, sizeof(s_rates));
    s_rate_spot = 0;
    s_rate_count = 0;
    s_beat_avg = 0.0f;
    s_spo2_filtered = 0.0f;
    s_last_ratio_norm = 0.0f;

    s_result.bpm = 0.0f;
    s_result.spo2 = 0.0f;
    s_result.ratio_norm = 0.0f;
    s_result.red_raw = 0;
    s_result.ir_raw = 0;
    s_result.valid = false;
    s_result.finger_detected = false;
}

static esp_err_t max3010x_hw_reset(void)
{
    esp_err_t ret = mx_write(REG_MODE_CONFIG, MODE_RESET);
    if (ret != ESP_OK) return ret;

    for (int i = 0; i < 25; i++) {
        uint8_t mode = 0xFF;
        vTaskDelay(pdMS_TO_TICKS(20));
        if (mx_read(REG_MODE_CONFIG, &mode, 1) == ESP_OK && ((mode & MODE_RESET) == 0)) {
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "Reset bit did not clear, continuing");
    return ESP_OK;
}

esp_err_t max30100_simple_init(i2c_master_bus_handle_t bus)
{
#if !MAX30100_ENABLE
    return ESP_ERR_NOT_SUPPORTED;
#endif

    reset_processing_state();

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MAX3010X_ADDR,
        .scl_speed_hz = MAX30100_I2C_SPEED_HZ,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Cannot add MAX3010X I2C device: %s", esp_err_to_name(ret));
        return ret;
    }

    uint8_t part_id = 0x00;
    ret = mx_read(REG_PART_ID, &part_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MAX3010X not responding at 0x57. Check VCC=3V3, GND, SDA=GPIO15, SCL=GPIO16");
        return ret;
    }

    // MAX30102/MAX30105 normalmente reporta 0x15. Algunos clones pueden variar.
    ESP_LOGI(TAG, "MAX3010X detected. PART_ID=0x%02X", part_id);
    if (part_id != 0x15) {
        ESP_LOGW(TAG, "PART_ID diferente a 0x15. Si el sensor responde, se continua igual.");
    }

    ret = max3010x_hw_reset();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MAX3010X reset failed: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(MAX30100_WARMUP_MS));

    // Deshabilitar interrupciones. Se trabaja por polling.
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_INTR_ENABLE_1, 0x00));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_INTR_ENABLE_2, 0x00));

    // Limpiar FIFO.
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_FIFO_WR_PTR, 0x00));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_OVF_COUNTER, 0x00));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_FIFO_RD_PTR, 0x00));

    // Configuracion de FIFO y SpO2 equivalente a la prueba Arduino.
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_FIFO_CONFIG, MAX3010X_FIFO_CONFIG_VALUE));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_SPO2_CONFIG, MAX3010X_SPO2_CONFIG_VALUE));

    // Intensidades LED exactas de la prueba que si funciono.
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_LED1_PA_RED, MAX3010X_RED_PA_VALUE));
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_LED2_PA_IR, MAX3010X_IR_PA_VALUE));

    // Modo rojo + infrarrojo.
    ESP_ERROR_CHECK_WITHOUT_ABORT(mx_write(REG_MODE_CONFIG, MODE_RED_IR));

    // Leer status para limpiar banderas pendientes.
    uint8_t dummy[2] = {0};
    mx_read(REG_INTR_STATUS_1, dummy, 2);

    s_ok = true;
    s_result.device_ok = true;

    ESP_LOGI(TAG, "MAX3010X initialized on SDA=GPIO15 SCL=GPIO16 I2C=%d Hz", MAX30100_I2C_SPEED_HZ);
    ESP_LOGI(TAG, "Config: FIFO=0x%02X SPO2=0x%02X RED=0x%02X IR=0x%02X",
             MAX3010X_FIFO_CONFIG_VALUE, MAX3010X_SPO2_CONFIG_VALUE,
             MAX3010X_RED_PA_VALUE, MAX3010X_IR_PA_VALUE);

    return ESP_OK;
}

static void add_bpm_to_average(float bpm)
{
    s_rates[s_rate_spot++] = bpm;
    s_rate_spot %= RATE_SIZE;
    if (s_rate_count < RATE_SIZE) s_rate_count++;

    float sum = 0.0f;
    for (uint8_t i = 0; i < s_rate_count; i++) {
        sum += s_rates[i];
    }
    s_beat_avg = sum / (float)s_rate_count;
}

static void process_sample(uint32_t red, uint32_t ir)
{
    int64_t now_ms = esp_timer_get_time() / 1000;

    s_result.red_raw = red;
    s_result.ir_raw = ir;

    if (ir < MAX3010X_FINGER_TH) {
        s_result.finger_detected = false;
        s_result.valid = false;
        s_result.bpm = 0.0f;
        s_result.spo2 = 0.0f;
        s_result.ratio_norm = 0.0f;
        s_ir_dc = 0.0f;
        s_ir_ac_abs = 1.0f;
        s_above_threshold = false;
        return;
    }

    s_result.finger_detected = true;

    if (s_ir_dc <= 1.0f) {
        s_ir_dc = (float)ir;
        return;
    }

    // Filtro DC y componente AC simple para detectar flancos de pulso.
    s_ir_dc = 0.95f * s_ir_dc + 0.05f * (float)ir;
    float ir_ac = (float)ir - s_ir_dc;
    s_ir_ac_abs = 0.95f * s_ir_ac_abs + 0.05f * fabsf(ir_ac);

    float threshold = fmaxf(120.0f, s_ir_ac_abs * 1.35f);

    if (!s_above_threshold && ir_ac > threshold) {
        s_above_threshold = true;

        if (s_last_beat_ms > 0) {
            int64_t delta = now_ms - s_last_beat_ms;
            float bpm = 60.0f / ((float)delta / 1000.0f);

            if (bpm > 40.0f && bpm < 180.0f) {
                add_bpm_to_average(bpm);
                s_result.bpm = s_beat_avg;
                s_result.valid = true;
            }
        }

        s_last_beat_ms = now_ms;
    }

    if (s_above_threshold && ir_ac < 0.0f) {
        s_above_threshold = false;
    }

    // Estimacion empirica de SpO2 calibrada para la configuracion usada:
    // RED=0x05 e IR=0x30. Si se usa red/ir directo, la relacion queda muy baja
    // y la formula se pega facilmente en 100 %. Por eso se normaliza por la
    // diferencia de corriente LED antes de calcular SpO2.
    if (ir > 0) {
        float ratio_raw = (float)red / (float)ir;
        float ratio_norm = ratio_raw * ((float)MAX3010X_IR_PA_VALUE / (float)MAX3010X_RED_PA_VALUE);
        s_last_ratio_norm = ratio_norm;

        float spo2 = SPO2_CAL_OFFSET - (SPO2_CAL_SLOPE * ratio_norm);

        if (spo2 > SPO2_CAL_MAX) spo2 = SPO2_CAL_MAX;
        if (spo2 < SPO2_CAL_MIN) spo2 = SPO2_CAL_MIN;

        if (s_spo2_filtered <= 1.0f) {
            s_spo2_filtered = spo2;
        } else {
            s_spo2_filtered = (SPO2_FILTER_ALPHA * s_spo2_filtered) +
                              ((1.0f - SPO2_FILTER_ALPHA) * spo2);
        }

        s_result.spo2 = s_spo2_filtered;
        s_result.ratio_norm = s_last_ratio_norm;
    }

    if (s_last_beat_ms > 0 && (now_ms - s_last_beat_ms) > 5000) {
        s_result.valid = false;
        s_result.bpm = 0.0f;
    }
}

void max30100_simple_process(void)
{
    if (!s_ok || !s_dev) return;

    uint8_t wr = 0, rd = 0;
    if (mx_read(REG_FIFO_WR_PTR, &wr, 1) != ESP_OK || mx_read(REG_FIFO_RD_PTR, &rd, 1) != ESP_OK) {
        s_result.device_ok = false;
        ESP_LOGW(TAG, "FIFO pointer read failed");
        return;
    }

    s_result.device_ok = true;

    uint8_t n = fifo_available(wr, rd);
    if (n > 32) n = 32;

    for (uint8_t i = 0; i < n; i++) {
        uint8_t data[6] = {0};
        if (mx_read(REG_FIFO_DATA, data, sizeof(data)) != ESP_OK) {
            ESP_LOGW(TAG, "FIFO data read failed");
            return;
        }

        // En modo Red + IR, el FIFO entrega RED(3 bytes) + IR(3 bytes).
        uint32_t red = (((uint32_t)data[0] & 0x03) << 16) |
                       ((uint32_t)data[1] << 8) |
                       ((uint32_t)data[2]);

        uint32_t ir  = (((uint32_t)data[3] & 0x03) << 16) |
                       ((uint32_t)data[4] << 8) |
                       ((uint32_t)data[5]);

        process_sample(red, ir);
    }
}

max30100_result_t max30100_simple_get(void)
{
    return s_result;
}

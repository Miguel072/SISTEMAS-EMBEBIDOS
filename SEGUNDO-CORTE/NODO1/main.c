#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"

#include "board_config.h"
#include "config.h"
#include "node1_packet.h"
#include "qmi8658_simple.h"
#include "max30100_simple.h"
#include "espnow_node1.h"
#include "lcd_ui.h"

static const char *TAG = "NODO1";

static i2c_master_bus_handle_t s_i2c_bus_internal = NULL;
static i2c_master_bus_handle_t s_i2c_bus_max30100 = NULL;
static esp_lcd_panel_io_handle_t s_touch_io = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;
static bool s_touch_ready = false;

static SemaphoreHandle_t s_data_mutex = NULL;

static qmi8658_sample_t s_imu = {0};
static max30100_result_t s_vitals = {0};
static uint32_t s_seq = 0;
static bool s_tx_ok = false;

static esp_err_t init_i2c_buses(void)
{
    i2c_master_bus_config_t internal_cfg = {
        .i2c_port = I2C_INTERNAL_PORT_NUM,
        .sda_io_num = PIN_I2C_INTERNAL_SDA,
        .scl_io_num = PIN_I2C_INTERNAL_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_LOGI(TAG, "Initializing internal I2C bus QMI8658 SDA=%d SCL=%d",
             PIN_I2C_INTERNAL_SDA, PIN_I2C_INTERNAL_SCL);
    esp_err_t ret = i2c_new_master_bus(&internal_cfg, &s_i2c_bus_internal);
    if (ret != ESP_OK) {
        return ret;
    }

    i2c_master_bus_config_t max_cfg = {
        .i2c_port = I2C_MAX30100_PORT_NUM,
        .sda_io_num = PIN_MAX30100_SDA,
        .scl_io_num = PIN_MAX30100_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_LOGI(TAG, "Initializing external I2C bus MAX30100 SDA=%d SCL=%d",
             PIN_MAX30100_SDA, PIN_MAX30100_SCL);
    return i2c_new_master_bus(&max_cfg, &s_i2c_bus_max30100);
}


static esp_err_t init_touch(void)
{
    ESP_LOGI(TAG, "Initializing touch CST816S on internal I2C bus");

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    esp_err_t ret = esp_lcd_new_panel_io_i2c(s_i2c_bus_internal, &tp_io_config, &s_touch_io);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch I2C IO init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = PIN_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };

    ret = esp_lcd_touch_new_i2c_cst816s(s_touch_io, &tp_cfg, &s_touch);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_touch_ready = true;
    ESP_LOGI(TAG, "Touch ready. Tap left/right to change view, bottom-center for menu.");
    return ESP_OK;
}

static void task_touch(void *arg)
{
    (void)arg;

    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint16_t strength[1] = {0};
    uint8_t touch_cnt = 0;
    int64_t last_touch_ms = 0;

    while (1) {
        if (!s_touch_ready) {
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        esp_lcd_touch_read_data(s_touch);
        bool pressed = esp_lcd_touch_get_coordinates(s_touch, x, y, strength, &touch_cnt, 1);
        int64_t now_ms = esp_timer_get_time() / 1000;

        if (pressed && touch_cnt > 0 && (now_ms - last_touch_ms) > 280) {
            ESP_LOGI(TAG, "TOUCH x=%u y=%u", x[0], y[0]);
            lcd_ui_handle_touch(x[0], y[0]);
            last_touch_ms = now_ms;
        }

        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

static void task_imu(void *arg)
{
    (void)arg;
    qmi8658_sample_t local;

    while (1) {
        memset(&local, 0, sizeof(local));
        if (qmi8658_simple_read(&local) == ESP_OK) {
            if (s_data_mutex && xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                s_imu = local;
                xSemaphoreGive(s_data_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_FAST_PERIOD_MS));
    }
}

static void task_max30100(void *arg)
{
    (void)arg;

    while (1) {
        max30100_simple_process();
        max30100_result_t local = max30100_simple_get();

        if (s_data_mutex && xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            s_vitals = local;
            xSemaphoreGive(s_data_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void make_packet(nodo1_packet_t *p)
{
    qmi8658_sample_t imu_copy = {0};
    max30100_result_t vitals_copy = {0};

    if (s_data_mutex && xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        imu_copy = s_imu;
        vitals_copy = s_vitals;
        xSemaphoreGive(s_data_mutex);
    }

    memset(p, 0, sizeof(*p));

    // Campos esperados por el Nodo 3 FINAL.
    p->pulso = vitals_copy.valid ? vitals_copy.bpm : 0.0f;
    p->spo2  = vitals_copy.valid ? vitals_copy.spo2 : 0.0f;

    p->acc_x = imu_copy.acc_x;
    p->acc_y = imu_copy.acc_y;
    p->acc_z = imu_copy.acc_z;

    p->gyr_x = imu_copy.gyr_x;
    p->gyr_y = imu_copy.gyr_y;
    p->gyr_z = imu_copy.gyr_z;

    // Estas variables son del Nodo 2. En Nodo 1 se envian en cero para que el
    // struct tenga exactamente el mismo tamano y orden que espera Nodo 3.
    p->latitud = 0.0f;
    p->longitud = 0.0f;
    p->altitud = 0.0f;
    p->velocidad = 0.0f;

    // El Nodo 3 calcula movimiento centralizado con IMU de Nodo 1 y Nodo 2.
    p->movimiento = 0;
    p->version = DATA_VERSION;

    s_seq++;
}

static void task_send(void *arg)
{
    (void)arg;
    nodo1_packet_t packet;

    while (1) {
        make_packet(&packet);

        esp_err_t ret = espnow_node1_send(&packet);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "ESP-NOW send request failed: %s", esp_err_to_name(ret));
        }

        // Give the send callback a small time window to update the status flag.
        vTaskDelay(pdMS_TO_TICKS(80));
        s_tx_ok = espnow_node1_last_send_ok();

        max30100_result_t vitals_copy = {0};
        if (s_data_mutex && xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            vitals_copy = s_vitals;
            xSemaphoreGive(s_data_mutex);
        }

        ESP_LOGI(TAG,
                 "SEQ=%lu TX=%s ACC[%.2f %.2f %.2f] GYR[%.2f %.2f %.2f] PULSO=%.1f SPO2=%.1f VALID=%u IR=%lu RED=%lu R=%.2f",
                 (unsigned long)s_seq,
                 s_tx_ok ? "OK" : "--",
                 packet.acc_x, packet.acc_y, packet.acc_z,
                 packet.gyr_x, packet.gyr_y, packet.gyr_z,
                 packet.pulso, packet.spo2, (unsigned int)(packet.pulso > 0.0f),
                 (unsigned long)vitals_copy.ir_raw, (unsigned long)vitals_copy.red_raw, vitals_copy.ratio_norm);

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_SEND_PERIOD_MS));
    }
}

static void task_display(void *arg)
{
    (void)arg;

    while (1) {
        qmi8658_sample_t imu_copy = {0};
        max30100_result_t vitals_copy = {0};
        uint32_t seq_copy = s_seq;
        bool tx_copy = s_tx_ok;

        if (s_data_mutex && xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            imu_copy = s_imu;
            vitals_copy = s_vitals;
            xSemaphoreGive(s_data_mutex);
        }

        lcd_ui_show_data(&imu_copy, &vitals_copy, tx_copy, seq_copy);
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_PERIOD_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "====================================================");
    ESP_LOGI(TAG, "NODO 1 - ESP32-S3 Touch LCD + QMI8658 + MAX3010X");
    ESP_LOGI(TAG, "FreeRTOS + ESP-IDF + ESP-NOW cada 10 segundos");
    ESP_LOGI(TAG, "====================================================");

    s_data_mutex = xSemaphoreCreateMutex();
    if (!s_data_mutex) {
        ESP_LOGE(TAG, "No mutex created");
        return;
    }

    ESP_ERROR_CHECK(lcd_ui_init());
    ESP_ERROR_CHECK(init_i2c_buses());

    if (init_touch() != ESP_OK) {
        ESP_LOGW(TAG, "Continuing without touch. UI will remain on the current view.");
    }

    if (qmi8658_simple_init(s_i2c_bus_internal) != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 not ready. The node will continue and show IMU error.");
    }

    if (max30100_simple_init(s_i2c_bus_max30100) != ESP_OK) {
        ESP_LOGW(TAG, "MAX3010X not ready. Check wiring: VCC=3V3, GND, SDA=GPIO15, SCL=GPIO16.");
        s_vitals.device_ok = false;
    }

    ESP_ERROR_CHECK(espnow_node1_init());

    xTaskCreate(task_imu, "task_imu", 4096, NULL, 5, NULL);
    xTaskCreate(task_touch, "task_touch", 4096, NULL, 4, NULL);
    xTaskCreate(task_max30100, "task_max30100", 4096, NULL, 5, NULL);
    xTaskCreate(task_send, "task_send", 6144, NULL, 4, NULL);
    xTaskCreate(task_display, "task_display", 6144, NULL, 3, NULL);
}

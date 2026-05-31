#include "qmi8658_simple.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "QMI8658";

#define QMI8658_ADDR_LOW       0x6A
#define QMI8658_ADDR_HIGH      0x6B

#define QMI8658_WHO_AM_I       0x00
#define QMI8658_CTRL1          0x02
#define QMI8658_CTRL2          0x03
#define QMI8658_CTRL3          0x04
#define QMI8658_CTRL7          0x08
#define QMI8658_AX_L           0x35

// These scale factors match the configuration used below:
// accelerometer ±8 g and gyroscope ±512 dps.
#define ACC_LSB_PER_G          4096.0f
#define GYR_LSB_PER_DPS        64.0f
#define G_TO_MS2               9.80665f

static i2c_master_dev_handle_t s_dev = NULL;
static uint8_t s_addr = 0;

static esp_err_t qmi_write(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    return i2c_master_transmit(s_dev, data, sizeof(data), pdMS_TO_TICKS(100));
}

static esp_err_t qmi_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static esp_err_t try_add_device(i2c_master_bus_handle_t bus, uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t who = 0xFF;
    ret = qmi_read(QMI8658_WHO_AM_I, &who, 1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "QMI8658 found at 0x%02X, WHO_AM_I=0x%02X", addr, who);
        s_addr = addr;
        return ESP_OK;
    }

    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    return ret;
}

esp_err_t qmi8658_simple_init(i2c_master_bus_handle_t bus)
{
    esp_err_t ret = try_add_device(bus, QMI8658_ADDR_HIGH);
    if (ret != ESP_OK) {
        ret = try_add_device(bus, QMI8658_ADDR_LOW);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 not detected on 0x6A/0x6B");
        return ret;
    }

    // CTRL1: keep default interface behavior, enable address auto-increment on many boards.
    qmi_write(QMI8658_CTRL1, 0x60);

    // CTRL2: accelerometer range ±8g, ODR around 250Hz.
    qmi_write(QMI8658_CTRL2, 0x23);

    // CTRL3: gyroscope range ±512dps, ODR around 250Hz.
    qmi_write(QMI8658_CTRL3, 0x33);

    // CTRL7: enable accelerometer and gyroscope.
    qmi_write(QMI8658_CTRL7, 0x03);

    ESP_LOGI(TAG, "QMI8658 configured at 0x%02X", s_addr);
    return ESP_OK;
}

esp_err_t qmi8658_simple_read(qmi8658_sample_t *out)
{
    if (!out || !s_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t raw[12] = {0};
    esp_err_t ret = qmi_read(QMI8658_AX_L, raw, sizeof(raw));
    if (ret != ESP_OK) {
        out->valid = false;
        return ret;
    }

    int16_t ax = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ay = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t az = (int16_t)((raw[5] << 8) | raw[4]);
    int16_t gx = (int16_t)((raw[7] << 8) | raw[6]);
    int16_t gy = (int16_t)((raw[9] << 8) | raw[8]);
    int16_t gz = (int16_t)((raw[11] << 8) | raw[10]);

    out->acc_x = ((float)ax / ACC_LSB_PER_G) * G_TO_MS2;
    out->acc_y = ((float)ay / ACC_LSB_PER_G) * G_TO_MS2;
    out->acc_z = ((float)az / ACC_LSB_PER_G) * G_TO_MS2;

    // Gyroscope values in degrees per second.
    out->gyr_x = (float)gx / GYR_LSB_PER_DPS;
    out->gyr_y = (float)gy / GYR_LSB_PER_DPS;
    out->gyr_z = (float)gz / GYR_LSB_PER_DPS;
    out->valid = true;

    return ESP_OK;
}

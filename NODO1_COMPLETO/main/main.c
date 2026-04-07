#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_gc9a01.h"

#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"

#include "qmi8658.h"
#include "board_config.h"

static const char *TAG = "NODE1";

static esp_lcd_panel_handle_t s_lcd_panel = NULL;
static esp_lcd_panel_io_handle_t s_lcd_io = NULL;
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static esp_lcd_panel_io_handle_t s_touch_io = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;
static qmi8658_dev_t s_imu;

static bool s_imu_ready = false;
static bool s_touch_ready = false;
static SemaphoreHandle_t s_touch_sem = NULL;

#define COLOR_BG        rgb565(12, 18, 32)
#define COLOR_WHITE     rgb565(255, 255, 255)
#define COLOR_BLACK     rgb565(0, 0, 0)
#define COLOR_GREEN     rgb565(15, 150, 70)
#define COLOR_YELLOW    rgb565(215, 180, 25)
#define COLOR_BLUE      rgb565(30, 100, 220)
#define COLOR_RED       rgb565(220, 40, 40)
#define COLOR_GRAY      rgb565(90, 95, 105)
#define COLOR_CYAN      rgb565(40, 180, 200)

#define G_NOMINAL                   9.81f
#define TH_PARADO_ANGLE             18.0f
#define TH_SENTADO_ANGLE_MIN        18.0f
#define TH_SENTADO_ANGLE_MAX        55.0f
#define TH_CAIDA_ANGLE              65.0f
#define TH_GYRO_QUIETO              0.9f
#define TH_GYRO_CAMINANDO           1.4f
#define TH_GYRO_CAIDA               5.5f
#define TH_ACCEL_DYN_QUIETO         1.2f
#define TH_ACCEL_DYN_CAMINANDO      1.8f
#define TH_ACCEL_LOW_CAIDA          3.5f
#define TH_ACCEL_HIGH_CAIDA         16.5f
#define TH_CAIDA_ANGLE_STRONG       70.0f
#define CNT_CONFIRM_POSTURE         5
#define CNT_CONFIRM_CAMINANDO       4
#define CNT_CONFIRM_CAIDA           2

#define BTN_Y1 170
#define BTN_Y2 206
#define BTN1_X1 28
#define BTN1_X2 84
#define BTN2_X1 92
#define BTN2_X2 148
#define BTN3_X1 156
#define BTN3_X2 212

typedef enum {
    FSM_INIT = 0,
    FSM_CALIBRANDO,
    FSM_PARADO,
    FSM_SENTADO,
    FSM_CAMINANDO,
    FSM_CAIDA
} fsm_state_t;

typedef enum {
    SCREEN_ESTADO = 0,
    SCREEN_DATOS
} screen_mode_t;

typedef struct {
    float ax, ay, az;
    float gx, gy, gz;
    float temperature;
    float accel_mag;
    float gyro_mag;
    float pitch_deg;
    float roll_deg;
    float delta_angle;
    bool strong_motion;
    bool fall_event;
} imu_features_t;

static imu_features_t g_feat = {0};
static fsm_state_t g_state = FSM_INIT;
static fsm_state_t g_candidate_state = FSM_INIT;
static screen_mode_t g_screen_mode = SCREEN_ESTADO;
static bool g_reference_valid = false;
static bool g_fall_latched = false;
static float g_ref_pitch = 0.0f;
static float g_ref_roll = 0.0f;
static int g_stable_counter = 0;
static int g_fall_counter = 0;
static TickType_t g_last_touch_tick = 0;

#define ESPNOW_CHANNEL               1
#define HEART_PACKET_VERSION         1
#define HEART_TIMEOUT_MS             3000U

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t node_id;
    uint8_t finger_ok;
    uint8_t beat_flag;
    uint8_t quality;
    uint16_t bpm_x10;
    uint16_t ibi_ms;
    uint16_t raw_signal;
    uint32_t timestamp_ms;
} heart_packet_t;

typedef struct {
    uint16_t bpm_x10;
    uint16_t ibi_ms;
    uint16_t raw_signal;
    uint8_t quality;
    uint8_t finger_ok;
    uint8_t beat_flag;
    uint32_t rx_ms;
    bool valid;
} heart_rx_state_t;

static volatile heart_rx_state_t g_heart = {0};

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
}

static bool get_glyph(char c, uint8_t out[7])
{
    memset(out, 0, 7);
    switch (c) {
        case 'A': { uint8_t g[7]={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; memcpy(out,g,7); return true; }
        case 'C': { uint8_t g[7]={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; memcpy(out,g,7); return true; }
        case 'D': { uint8_t g[7]={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}; memcpy(out,g,7); return true; }
        case 'E': { uint8_t g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; memcpy(out,g,7); return true; }
        case 'G': { uint8_t g[7]={0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}; memcpy(out,g,7); return true; }
        case 'I': { uint8_t g[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}; memcpy(out,g,7); return true; }
        case 'L': { uint8_t g[7]={0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; memcpy(out,g,7); return true; }
        case 'M': { uint8_t g[7]={0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; memcpy(out,g,7); return true; }
        case 'N': { uint8_t g[7]={0x11,0x19,0x15,0x13,0x11,0x11,0x11}; memcpy(out,g,7); return true; }
        case 'O': { uint8_t g[7]={0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(out,g,7); return true; }
        case 'P': { uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; memcpy(out,g,7); return true; }
        case 'R': { uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; memcpy(out,g,7); return true; }
        case 'S': { uint8_t g[7]={0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; memcpy(out,g,7); return true; }
        case 'T': { uint8_t g[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; memcpy(out,g,7); return true; }
        case 'U': { uint8_t g[7]={0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; memcpy(out,g,7); return true; }
        case 'V': { uint8_t g[7]={0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; memcpy(out,g,7); return true; }
        case 'W': { uint8_t g[7]={0x11,0x11,0x11,0x15,0x15,0x15,0x0A}; memcpy(out,g,7); return true; }
        case '0': { uint8_t g[7]={0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; memcpy(out,g,7); return true; }
        case '1': { uint8_t g[7]={0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; memcpy(out,g,7); return true; }
        case '2': { uint8_t g[7]={0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}; memcpy(out,g,7); return true; }
        case '3': { uint8_t g[7]={0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E}; memcpy(out,g,7); return true; }
        case '4': { uint8_t g[7]={0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; memcpy(out,g,7); return true; }
        case '5': { uint8_t g[7]={0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}; memcpy(out,g,7); return true; }
        case '6': { uint8_t g[7]={0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}; memcpy(out,g,7); return true; }
        case '7': { uint8_t g[7]={0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; memcpy(out,g,7); return true; }
        case '8': { uint8_t g[7]={0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; memcpy(out,g,7); return true; }
        case '9': { uint8_t g[7]={0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}; memcpy(out,g,7); return true; }
        case ':': { uint8_t g[7]={0x00,0x04,0x04,0x00,0x04,0x04,0x00}; memcpy(out,g,7); return true; }
        case '.': { uint8_t g[7]={0x00,0x00,0x00,0x00,0x00,0x06,0x06}; memcpy(out,g,7); return true; }
        case '-': { uint8_t g[7]={0x00,0x00,0x00,0x1F,0x00,0x00,0x00}; memcpy(out,g,7); return true; }
        case ' ': { uint8_t g[7]={0x00,0x00,0x00,0x00,0x00,0x00,0x00}; memcpy(out,g,7); return true; }
        default: return false;
    }
}

static void lcd_fill_rect(int x1, int y1, int x2, int y2, uint16_t color)
{
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > LCD_H_RES) x2 = LCD_H_RES;
    if (y2 > LCD_V_RES) y2 = LCD_V_RES;
    if (x2 <= x1 || y2 <= y1) return;

    int w = x2 - x1;
    int h = y2 - y1;
    int pixels = w * h;

    uint16_t *buf = (uint16_t *)heap_caps_malloc((size_t)pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE(TAG, "No DMA buffer for rect");
        return;
    }

    for (int i = 0; i < pixels; i++) buf[i] = color;
    esp_lcd_panel_draw_bitmap(s_lcd_panel, x1, y1, x2, y2, buf);
    free(buf);
}

static void lcd_fill_screen(uint16_t color)
{
    lcd_fill_rect(0, 0, LCD_H_RES, LCD_V_RES, color);
}

static void lcd_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    uint8_t glyph[7];
    if (!get_glyph(c, glyph)) {
        c = ' ';
        get_glyph(c, glyph);
    }

    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            bool pixel_on = (glyph[row] >> (4 - col)) & 0x01;
            int px1 = x + col * scale;
            int py1 = y + row * scale;
            lcd_fill_rect(px1, py1, px1 + scale, py1 + scale, pixel_on ? fg : bg);
        }
    }
}

static void lcd_draw_text(int x, int y, const char *text, uint16_t fg, uint16_t bg, int scale)
{
    int cursor_x = x;
    while (*text) {
        lcd_draw_char(cursor_x, y, *text, fg, bg, scale);
        cursor_x += 6 * scale;
        text++;
    }
}


static float normalize_angle_deg(float angle)
{
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

static const char *fsm_state_to_text(fsm_state_t s)
{
    switch (s) {
        case FSM_INIT: return "INIT";
        case FSM_CALIBRANDO: return "CALIBRANDO";
        case FSM_PARADO: return "PARADO";
        case FSM_SENTADO: return "SENTADO";
        case FSM_CAMINANDO: return "CAMINANDO";
        case FSM_CAIDA: return "CAIDA";
        default: return "INIT";
    }
}

static uint16_t fsm_state_to_color(fsm_state_t s)
{
    switch (s) {
        case FSM_PARADO: return COLOR_GREEN;
        case FSM_SENTADO: return COLOR_YELLOW;
        case FSM_CAMINANDO: return COLOR_BLUE;
        case FSM_CAIDA: return COLOR_RED;
        case FSM_CALIBRANDO: return COLOR_GRAY;
        default: return COLOR_BG;
    }
}


static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    (void)recv_info;
    if (!data || data_len != sizeof(heart_packet_t)) return;

    heart_packet_t pkt;
    memcpy(&pkt, data, sizeof(pkt));
    if (pkt.version != HEART_PACKET_VERSION || pkt.node_id != 2) return;

    g_heart.bpm_x10 = pkt.bpm_x10;
    g_heart.ibi_ms = pkt.ibi_ms;
    g_heart.raw_signal = pkt.raw_signal;
    g_heart.quality = pkt.quality;
    g_heart.finger_ok = pkt.finger_ok;
    g_heart.beat_flag = pkt.beat_flag;
    g_heart.rx_ms = now_ms();
    g_heart.valid = true;
}

static void wifi_init_for_espnow(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));
    ESP_LOGI(TAG, "MAC nodo 1: %02X:%02X:%02X:%02X:%02X:%02X | Canal ESP-NOW=%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ESPNOW_CHANNEL);
}

static void espnow_init_receiver(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_LOGI(TAG, "ESP-NOW receiver listo");
}

static bool heart_link_is_fresh(void)
{
    return g_heart.valid && ((now_ms() - g_heart.rx_ms) <= HEART_TIMEOUT_MS);
}

static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t hp_task_woken = pdFALSE;
    (void)tp;
    if (s_touch_sem) {
        xSemaphoreGiveFromISR(s_touch_sem, &hp_task_woken);
        if (hp_task_woken) {
            portYIELD_FROM_ISR();
        }
    }
}

static esp_err_t init_backlight(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_BL, 1));
    return ESP_OK;
}

static esp_err_t init_lcd(void)
{
    ESP_LOGI(TAG, "Initializing LCD");
    ESP_ERROR_CHECK(init_backlight());

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &s_lcd_io));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(s_lcd_io, &panel_config, &s_lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_lcd_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_lcd_panel, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_lcd_panel, true));

    lcd_fill_screen(COLOR_BG);
    ESP_LOGI(TAG, "LCD ready");
    return ESP_OK;
}

static esp_err_t init_i2c(void)
{
    ESP_LOGI(TAG, "Initializing I2C bus");
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_i2c_bus));
    return ESP_OK;
}

static esp_err_t init_touch(void)
{
    ESP_LOGI(TAG, "Initializing touch");
    s_touch_sem = xSemaphoreCreateBinary();
    if (!s_touch_sem) {
        return ESP_ERR_NO_MEM;
    }

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    esp_err_t ret = esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_config, &s_touch_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch IO init failed: %s", esp_err_to_name(ret));
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
        .interrupt_callback = touch_callback,
    };

    ret = esp_lcd_touch_new_i2c_cst816s(s_touch_io, &tp_cfg, &s_touch);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch controller init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_touch_ready = true;
    ESP_LOGI(TAG, "Touch ready");
    return ESP_OK;
}

static esp_err_t init_imu(void)
{
    ESP_LOGI(TAG, "Initializing QMI8658");

    esp_err_t ret = qmi8658_init(&s_imu, s_i2c_bus, QMI8658_ADDRESS_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "QMI8658 not found at HIGH address, trying LOW...");
        ret = qmi8658_init(&s_imu, s_i2c_bus, QMI8658_ADDRESS_LOW);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 init failed on both addresses");
        return ret;
    }

    qmi8658_set_accel_range(&s_imu, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&s_imu, QMI8658_ACCEL_ODR_250HZ);
    qmi8658_set_gyro_range(&s_imu, QMI8658_GYRO_RANGE_512DPS);
    qmi8658_set_gyro_odr(&s_imu, QMI8658_GYRO_ODR_250HZ);
    qmi8658_set_accel_unit_mps2(&s_imu, true);
    qmi8658_set_gyro_unit_rads(&s_imu, true);
    qmi8658_set_display_precision(&s_imu, 4);

    s_imu_ready = true;
    ESP_LOGI(TAG, "QMI8658 ready");
    return ESP_OK;
}

static void compute_features(void)
{
    g_feat.accel_mag = sqrtf(g_feat.ax * g_feat.ax + g_feat.ay * g_feat.ay + g_feat.az * g_feat.az);
    g_feat.gyro_mag = sqrtf(g_feat.gx * g_feat.gx + g_feat.gy * g_feat.gy + g_feat.gz * g_feat.gz);
    g_feat.pitch_deg = atan2f(-g_feat.ax, sqrtf(g_feat.ay * g_feat.ay + g_feat.az * g_feat.az)) * 57.2958f;
    g_feat.roll_deg = atan2f(g_feat.ay, g_feat.az) * 57.2958f;

    if (g_reference_valid) {
        float dp = normalize_angle_deg(g_feat.pitch_deg - g_ref_pitch);
        float dr = normalize_angle_deg(g_feat.roll_deg - g_ref_roll);
        g_feat.delta_angle = sqrtf(dp * dp + dr * dr);
    } else {
        g_feat.delta_angle = 0.0f;
    }

    float accel_dyn = fabsf(g_feat.accel_mag - G_NOMINAL);
    bool impact_event = (g_feat.accel_mag < TH_ACCEL_LOW_CAIDA) || (g_feat.accel_mag > TH_ACCEL_HIGH_CAIDA);
    bool strong_rotation = (g_feat.gyro_mag > TH_GYRO_CAIDA);
    bool big_posture_change = (g_feat.delta_angle > TH_CAIDA_ANGLE_STRONG);

    g_feat.strong_motion = (g_feat.gyro_mag > TH_GYRO_CAMINANDO) || (accel_dyn > TH_ACCEL_DYN_CAMINANDO);
    g_feat.fall_event = (impact_event && big_posture_change) ||
                        (strong_rotation && big_posture_change);
}

static void calibrate_reference(void)
{
    g_ref_pitch = g_feat.pitch_deg;
    g_ref_roll = g_feat.roll_deg;
    g_reference_valid = true;
    g_fall_latched = false;
    g_candidate_state = FSM_PARADO;
    g_state = FSM_PARADO;
    g_stable_counter = 0;
    g_fall_counter = 0;
    ESP_LOGI(TAG, "Reference saved: pitch=%.2f roll=%.2f", g_ref_pitch, g_ref_roll);
}

static fsm_state_t classify_candidate_state(void)
{
    float accel_dyn = fabsf(g_feat.accel_mag - G_NOMINAL);

    if (!g_reference_valid) {
        return FSM_CALIBRANDO;
    }
    if (g_fall_latched || g_feat.fall_event) {
        return FSM_CAIDA;
    }
    if ((g_feat.delta_angle < TH_PARADO_ANGLE) &&
        (g_feat.gyro_mag < TH_GYRO_QUIETO) &&
        (accel_dyn < TH_ACCEL_DYN_QUIETO)) {
        return FSM_PARADO;
    }
    if ((g_feat.delta_angle >= TH_SENTADO_ANGLE_MIN) &&
        (g_feat.delta_angle < TH_SENTADO_ANGLE_MAX) &&
        (g_feat.gyro_mag < TH_GYRO_QUIETO) &&
        (accel_dyn < (TH_ACCEL_DYN_QUIETO + 0.3f))) {
        return FSM_SENTADO;
    }
    if (g_feat.strong_motion) {
        return FSM_CAMINANDO;
    }
    return g_state;
}

static void fsm_update(void)
{
    fsm_state_t next_candidate = classify_candidate_state();

    if (next_candidate == FSM_CAIDA) {
        g_fall_counter++;
        if (g_fall_counter >= CNT_CONFIRM_CAIDA) {
            g_fall_latched = true;
            g_state = FSM_CAIDA;
            g_candidate_state = FSM_CAIDA;
            g_stable_counter = 0;
            return;
        }
    } else {
        g_fall_counter = 0;
    }

    if (next_candidate != g_candidate_state) {
        g_candidate_state = next_candidate;
        g_stable_counter = 1;
    } else {
        g_stable_counter++;
    }

    switch (g_state) {
        case FSM_INIT:
            g_state = FSM_CALIBRANDO;
            break;
        case FSM_CALIBRANDO:
            if (g_reference_valid) g_state = FSM_PARADO;
            break;
        case FSM_PARADO:
            if (g_candidate_state == FSM_SENTADO && g_stable_counter >= CNT_CONFIRM_POSTURE) g_state = FSM_SENTADO;
            else if (g_candidate_state == FSM_CAMINANDO && g_stable_counter >= CNT_CONFIRM_CAMINANDO) g_state = FSM_CAMINANDO;
            break;
        case FSM_SENTADO:
            if (g_candidate_state == FSM_PARADO && g_stable_counter >= CNT_CONFIRM_POSTURE) g_state = FSM_PARADO;
            else if (g_candidate_state == FSM_CAMINANDO && g_stable_counter >= CNT_CONFIRM_CAMINANDO) g_state = FSM_CAMINANDO;
            break;
        case FSM_CAMINANDO:
            if (g_candidate_state == FSM_PARADO && g_stable_counter >= CNT_CONFIRM_POSTURE + 1) g_state = FSM_PARADO;
            else if (g_candidate_state == FSM_SENTADO && g_stable_counter >= CNT_CONFIRM_POSTURE + 1) g_state = FSM_SENTADO;
            break;
        case FSM_CAIDA:
            break;
        default:
            g_state = FSM_CALIBRANDO;
            break;
    }
}

static void ui_draw_buttons(void)
{
    lcd_fill_rect(BTN1_X1, BTN_Y1, BTN1_X2, BTN_Y2, COLOR_CYAN);
    lcd_fill_rect(BTN2_X1, BTN_Y1, BTN2_X2, BTN_Y2, COLOR_RED);
    lcd_fill_rect(BTN3_X1, BTN_Y1, BTN3_X2, BTN_Y2, COLOR_GRAY);

    lcd_draw_text(BTN1_X1 + 10, BTN_Y1 + 10, "CAL", COLOR_BLACK, COLOR_CYAN, 2);
    lcd_draw_text(BTN2_X1 + 10, BTN_Y1 + 10, "RST", COLOR_WHITE, COLOR_RED, 2);
    lcd_draw_text(BTN3_X1 + 6, BTN_Y1 + 10, "VIEW", COLOR_WHITE, COLOR_GRAY, 1);
}

static void ui_draw_estado(void)
{
    uint16_t bg = fsm_state_to_color(g_state);
    lcd_fill_screen(bg);
    lcd_draw_text(42, 18, "ACTIVIDAD", COLOR_WHITE, bg, 2);
    lcd_draw_text(70, 44, "ESTADO", COLOR_WHITE, bg, 2);
    lcd_draw_text(28, 76, fsm_state_to_text(g_state), COLOR_WHITE, bg, 3);

    char line1[20], line2[20], line3[24];
    snprintf(line1, sizeof(line1), "ANG:%.1f", g_feat.delta_angle);
    snprintf(line2, sizeof(line2), "GYR:%.1f", g_feat.gyro_mag);

    if (heart_link_is_fresh() && g_heart.finger_ok) {
        snprintf(line3, sizeof(line3), "FC:%.1f BPM", g_heart.bpm_x10 / 10.0f);
    } else {
        snprintf(line3, sizeof(line3), "FC:--.- BPM");
    }

    lcd_draw_text(44, 118, line1, COLOR_WHITE, bg, 2);
    lcd_draw_text(44, 136, line2, COLOR_WHITE, bg, 2);
    lcd_draw_text(28, 154, line3, COLOR_WHITE, bg, 2);

    ui_draw_buttons();
}

static void ui_draw_datos(void)
{
    uint16_t bg = COLOR_BG;
    lcd_fill_screen(bg);
    lcd_draw_text(72, 12, "DATOS", COLOR_WHITE, bg, 2);

    char l1[20], l2[20], l3[20], l4[20], l5[20], l6[20], l7[24];
    snprintf(l1, sizeof(l1), "AX %.1f", g_feat.ax);
    snprintf(l2, sizeof(l2), "AY %.1f", g_feat.ay);
    snprintf(l3, sizeof(l3), "AZ %.1f", g_feat.az);
    snprintf(l4, sizeof(l4), "G %.1f", g_feat.accel_mag);
    snprintf(l5, sizeof(l5), "GYR %.1f", g_feat.gyro_mag);
    snprintf(l6, sizeof(l6), "ANG %.1f", g_feat.delta_angle);
    if (heart_link_is_fresh() && g_heart.finger_ok) snprintf(l7, sizeof(l7), "FC %.1f", g_heart.bpm_x10 / 10.0f);
    else snprintf(l7, sizeof(l7), "FC --.-");

    lcd_draw_text(52, 40, l1, COLOR_WHITE, bg, 2);
    lcd_draw_text(52, 58, l2, COLOR_WHITE, bg, 2);
    lcd_draw_text(52, 76, l3, COLOR_WHITE, bg, 2);
    lcd_draw_text(58, 94, l4, COLOR_WHITE, bg, 2);
    lcd_draw_text(52, 112, l5, COLOR_WHITE, bg, 2);
    lcd_draw_text(58, 130, l6, COLOR_WHITE, bg, 2);
    lcd_draw_text(64, 148, l7, COLOR_WHITE, bg, 2);
    ui_draw_buttons();
}

static void ui_refresh(void)
{
    if (g_screen_mode == SCREEN_ESTADO) ui_draw_estado();
    else ui_draw_datos();
}

static void imu_task(void *arg)
{
    (void)arg;
    qmi8658_data_t data;
    memset(&data, 0, sizeof(data));

    while (1) {
        if (s_imu_ready) {
            bool ready = false;
            esp_err_t ret = qmi8658_is_data_ready(&s_imu, &ready);
            if (ret == ESP_OK && ready) {
                ret = qmi8658_read_sensor_data(&s_imu, &data);
                if (ret == ESP_OK) {
                    g_feat.ax = data.accelX;
                    g_feat.ay = data.accelY;
                    g_feat.az = data.accelZ;
                    g_feat.gx = data.gyroX;
                    g_feat.gy = data.gyroY;
                    g_feat.gz = data.gyroZ;
                    g_feat.temperature = data.temperature;
                    compute_features();
                    if (!g_reference_valid && g_feat.gyro_mag < TH_GYRO_QUIETO) calibrate_reference();
                    fsm_update();
                    ESP_LOGI(TAG,
                             "STATE=%s | ACC X=%.2f Y=%.2f Z=%.2f | GYR X=%.2f Y=%.2f Z=%.2f | ANG=%.1f",
                             fsm_state_to_text(g_state),
                             g_feat.ax, g_feat.ay, g_feat.az,
                             g_feat.gx, g_feat.gy, g_feat.gz,
                             g_feat.delta_angle);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void touch_task(void *arg)
{
    (void)arg;
    esp_lcd_touch_point_data_t point[1];
    uint8_t point_cnt = 0;

    while (1) {
        if (!s_touch_ready || !s_touch_sem) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (xSemaphoreTake(s_touch_sem, pdMS_TO_TICKS(50)) == pdTRUE) {
            esp_err_t ret = esp_lcd_touch_read_data(s_touch);
            if (ret != ESP_OK) {
                continue;
            }

            point_cnt = 0;
            ret = esp_lcd_touch_get_data(s_touch, point, &point_cnt, 1);
            if (ret != ESP_OK || point_cnt == 0) {
                continue;
            }

            TickType_t now = xTaskGetTickCount();
            if ((now - g_last_touch_tick) < pdMS_TO_TICKS(350)) {
                continue;
            }
            g_last_touch_tick = now;

            uint16_t x = point[0].x;
            uint16_t y = point[0].y;
            ESP_LOGI(TAG, "TOUCH x=%u y=%u", x, y);

            if (y >= BTN_Y1 && y <= BTN_Y2) {
                if (x >= BTN1_X1 && x <= BTN1_X2) {
                    calibrate_reference();
                } else if (x >= BTN2_X1 && x <= BTN2_X2) {
                    g_fall_latched = false;
                    g_fall_counter = 0;
                    g_state = (g_feat.delta_angle < TH_PARADO_ANGLE) ? FSM_PARADO : FSM_SENTADO;
                } else if (x >= BTN3_X1 && x <= BTN3_X2) {
                    g_screen_mode = (g_screen_mode == SCREEN_ESTADO) ? SCREEN_DATOS : SCREEN_ESTADO;
                }
                ui_refresh();
            }
        }
    }
}

static void screen_task(void *arg)
{
    (void)arg;
    fsm_state_t last_state = (fsm_state_t)-1;
    screen_mode_t last_mode = (screen_mode_t)-1;

    while (1) {
        if (g_state != last_state || g_screen_mode != last_mode) {
            ui_refresh();
            last_state = g_state;
            last_mode = g_screen_mode;
        } else {
            ui_refresh();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=======================================");
    ESP_LOGI(TAG, "Node 1 - ESP32-S3-Touch-LCD-1.28");
    ESP_LOGI(TAG, "FSM + TOUCH + LCD + IMU");
    ESP_LOGI(TAG, "=======================================");

    g_state = FSM_INIT;
    g_screen_mode = SCREEN_ESTADO;

    wifi_init_for_espnow();
    espnow_init_receiver();

    ESP_ERROR_CHECK(init_lcd());
    ESP_ERROR_CHECK(init_i2c());

    if (init_touch() != ESP_OK) {
        ESP_LOGW(TAG, "Continuing without touch");
    }
    if (init_imu() != ESP_OK) {
        ESP_LOGW(TAG, "Continuing without IMU");
    }

    g_state = FSM_CALIBRANDO;
    ui_refresh();

    xTaskCreate(imu_task, "imu_task", 4096, NULL, 5, NULL);
    xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, NULL);
    xTaskCreate(screen_task, "screen_task", 4096, NULL, 3, NULL);
}

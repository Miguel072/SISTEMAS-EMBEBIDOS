#include "lcd_ui.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "board_config.h"
#include "config.h"

static const char *TAG = "LCD_UI";
static esp_lcd_panel_handle_t s_panel = NULL;
static uint16_t *s_fb = NULL;

static void lcd_flush(void)
{
    if (s_panel && s_fb) {
        esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_H_RES, LCD_V_RES, s_fb);
    }
}

// 0=CARDIO, 1=SENSOR/DEDO, 2=IMU, 3=SISTEMA, 4=MENU
static volatile uint8_t s_screen_mode = 0;

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

#define C_BG      rgb565(4, 9, 18)
#define C_BG2     rgb565(7, 18, 32)
#define C_BLUE    rgb565(25, 80, 180)
#define C_CYAN    rgb565(0, 190, 220)
#define C_GREEN   rgb565(20, 190, 95)
#define C_RED     rgb565(230, 45, 45)
#define C_ORANGE  rgb565(255, 145, 25)
#define C_WHITE   rgb565(255,255,255)
#define C_YELLOW  rgb565(240,205,40)
#define C_DIM     rgb565(95, 115, 135)
#define C_BLACK   rgb565(0,0,0)

static bool glyph(char c, uint8_t out[7])
{
    memset(out, 0, 7);
    switch (c) {
        case 'A': { uint8_t g[7]={0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; memcpy(out,g,7); return true; }
        case 'B': { uint8_t g[7]={0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; memcpy(out,g,7); return true; }
        case 'C': { uint8_t g[7]={0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; memcpy(out,g,7); return true; }
        case 'D': { uint8_t g[7]={0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}; memcpy(out,g,7); return true; }
        case 'E': { uint8_t g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; memcpy(out,g,7); return true; }
        case 'F': { uint8_t g[7]={0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; memcpy(out,g,7); return true; }
        case 'G': { uint8_t g[7]={0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}; memcpy(out,g,7); return true; }
        case 'H': { uint8_t g[7]={0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; memcpy(out,g,7); return true; }
        case 'I': { uint8_t g[7]={0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}; memcpy(out,g,7); return true; }
        case 'K': { uint8_t g[7]={0x11,0x12,0x14,0x18,0x14,0x12,0x11}; memcpy(out,g,7); return true; }
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
        case 'X': { uint8_t g[7]={0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; memcpy(out,g,7); return true; }
        case 'Y': { uint8_t g[7]={0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; memcpy(out,g,7); return true; }
        case 'Z': { uint8_t g[7]={0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; memcpy(out,g,7); return true; }
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
        case '%': { uint8_t g[7]={0x19,0x1A,0x02,0x04,0x08,0x0B,0x13}; memcpy(out,g,7); return true; }
        case ' ': { uint8_t g[7]={0,0,0,0,0,0,0}; memcpy(out,g,7); return true; }
        default:  { uint8_t g[7]={0,0,0,0,0,0,0}; memcpy(out,g,7); return true; }
    }
}

static void fill_rect(int x1, int y1, int x2, int y2, uint16_t color)
{
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > LCD_H_RES) x2 = LCD_H_RES;
    if (y2 > LCD_V_RES) y2 = LCD_V_RES;
    if (x2 <= x1 || y2 <= y1) return;

    // Desde V17 se dibuja primero en un framebuffer y luego se refresca
    // toda la pantalla una sola vez. Esto evita parpadeos y pantallas
    // parcialmente dibujadas cuando el menú tiene muchos elementos.
    if (s_fb) {
        for (int y = y1; y < y2; y++) {
            uint16_t *row = s_fb + (y * LCD_H_RES) + x1;
            for (int x = x1; x < x2; x++) {
                *row++ = color;
            }
        }
        return;
    }

    if (!s_panel) return;
    int pixels = (x2 - x1) * (y2 - y1);
    uint16_t *buf = heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) return;
    for (int i = 0; i < pixels; i++) buf[i] = color;
    esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2, y2, buf);
    free(buf);
}


static void draw_ring(uint16_t color, int thickness)
{
    // Borde circular externo aprovechando la forma física del GC9A01.
    // Se dibuja con barras horizontales para evitar dependencias gráficas pesadas.
    const int cx = LCD_H_RES / 2;
    const int cy = LCD_V_RES / 2;
    const int r_outer = 118;
    const int r_inner = r_outer - thickness;

    for (int y = cy - r_outer; y <= cy + r_outer; y++) {
        int dy = y - cy;
        float outer_v = (float)(r_outer * r_outer - dy * dy);
        if (outer_v < 0) continue;
        int xo = (int)sqrtf(outer_v);

        int xi = -1;
        float inner_v = (float)(r_inner * r_inner - dy * dy);
        if (inner_v >= 0) {
            xi = (int)sqrtf(inner_v);
        }

        // Segmento izquierdo del anillo
        fill_rect(cx - xo, y, (xi >= 0) ? (cx - xi) : (cx + xo + 1), y + 1, color);
        // Segmento derecho del anillo
        if (xi >= 0) {
            fill_rect(cx + xi, y, cx + xo + 1, y + 1, color);
        }
    }
}


static void draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale);
static void draw_text(int x, int y, const char *text, uint16_t fg, uint16_t bg, int scale);


static void draw_soft_header(const char *title, uint16_t ring_color)
{
    // Cabecera corta, pensada para que no compita con la informacion principal.
    fill_rect(48, 14, 192, 32, C_BG2);
    draw_text(70, 19, title, C_WHITE, C_BG2, 1);
    fill_rect(34, 18, 42, 26, ring_color);
    fill_rect(198, 18, 206, 26, ring_color);
}

static void draw_status_dots(bool tx_ok, bool max_ok, uint16_t bg)
{
    // Dos indicadores pequeños tipo reloj: TX y MAX.
    fill_rect(49, 40, 61, 52, tx_ok ? C_GREEN : C_ORANGE);
    draw_text(65, 43, "TX", C_WHITE, bg, 1);

    fill_rect(145, 40, 157, 52, max_ok ? C_GREEN : C_RED);
    draw_text(161, 43, "MAX", C_WHITE, bg, 1);
}

static void draw_watch_shell(uint16_t ring_color, bool tx_ok, bool max_ok)
{
    // Borde externo: identidad visual tipo reloj/manilla.
    draw_ring(ring_color, 10);

    // Borde interno tenue para dar sensacion de caratula.
    draw_ring(C_BG2, 3);

    // Marcas cardinales pequeñas, como bisel de reloj.
    fill_rect(116, 8, 124, 18, ring_color);
    fill_rect(116, 222, 124, 232, ring_color);
    fill_rect(8, 116, 18, 124, ring_color);
    fill_rect(222, 116, 232, 124, ring_color);

    draw_status_dots(tx_ok, max_ok, C_BG);
}

static void draw_center_pill(int x1, int y1, int x2, int y2, const char *text, uint16_t fg, uint16_t bg, int scale)
{
    fill_rect(x1, y1, x2, y2, bg);
    int len = (int)strlen(text);
    int text_w = len * 6 * scale;
    int text_h = 7 * scale;
    int x = x1 + ((x2 - x1) - text_w) / 2;
    int y = y1 + ((y2 - y1) - text_h) / 2;
    if (x < x1) x = x1 + 2;
    draw_text(x, y, text, fg, bg, scale);
}

static uint16_t status_color(const max30100_result_t *vitals, bool tx_ok)
{
    if (!vitals || !vitals->device_ok) return C_RED;
    if (!vitals->finger_detected) return C_YELLOW;
    if (vitals->valid) {
        if (vitals->spo2 < 90.0f) return C_RED;
        if (vitals->bpm > 110.0f || vitals->bpm < 50.0f) return C_ORANGE;
    } else {
        return C_YELLOW;
    }
    if (!tx_ok) return C_ORANGE;
    return C_GREEN;
}

static const char *status_text(const max30100_result_t *vitals, bool tx_ok)
{
    if (!vitals || !vitals->device_ok) return "MAX NO";
    if (!vitals->finger_detected) return "COLOCA DEDO";
    if (!vitals->valid) return "MIDIENDO";
    if (vitals->spo2 < 90.0f) return "SPO2 BAJA";
    if (vitals->bpm > 110.0f) return "PULSO ALTO";
    if (vitals->bpm < 50.0f) return "PULSO BAJO";
    if (!tx_ok) return "TX FALLA";
    return "NORMAL";
}

static void draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    uint8_t g[7];
    glyph(c, g);
    for (int r = 0; r < 7; r++) {
        for (int col = 0; col < 5; col++) {
            bool on = (g[r] >> (4 - col)) & 1;
            fill_rect(x + col*scale, y + r*scale,
                      x + (col+1)*scale, y + (r+1)*scale,
                      on ? fg : bg);
        }
    }
}

static void draw_text(int x, int y, const char *text, uint16_t fg, uint16_t bg, int scale)
{
    while (*text) {
        char c = *text;
        if (c >= 'a' && c <= 'z') c -= 32;
        draw_char(x, y, c, fg, bg, scale);
        x += 6 * scale;
        text++;
    }
}

esp_err_t lcd_ui_init(void)
{
    gpio_config_t bl = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl));
    ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_BL, 1));

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    if (!s_fb) {
        s_fb = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (!s_fb) {
            ESP_LOGE(TAG, "No framebuffer memory for LCD UI");
            return ESP_ERR_NO_MEM;
        }
    }

    fill_rect(0, 0, LCD_H_RES, LCD_V_RES, C_BG);
    draw_ring(C_CYAN, 10);
    draw_ring(C_BG2, 3);
    draw_text(54, 42, "NODO 1", C_WHITE, C_BG, 3);
    draw_text(42, 94, "WEARABLE", C_CYAN, C_BG, 2);
    draw_text(46, 128, "BIOMETRICO", C_YELLOW, C_BG, 1);
    draw_text(50, 158, "INICIANDO", C_WHITE, C_BG, 2);
    lcd_flush();
    ESP_LOGI(TAG, "LCD ready");
    return ESP_OK;
}


void lcd_ui_handle_touch(uint16_t x, uint16_t y)
{
    // Navegacion pensada para pantalla circular:
    // - En cualquier vista: izquierda = anterior, derecha = siguiente, abajo-centro = menu.
    // - En menu: cada cuadrante abre una vista fija.
    uint8_t mode = s_screen_mode;

    if (mode == 4) {
        if (y < 120 && x < 120) {
            s_screen_mode = 0; // Cardio
        } else if (y < 120 && x >= 120) {
            s_screen_mode = 1; // Sensor / dedo
        } else if (y >= 120 && x < 120) {
            s_screen_mode = 2; // IMU
        } else {
            s_screen_mode = 3; // Sistema
        }
        return;
    }

    if (y > 178 && x > 70 && x < 170) {
        s_screen_mode = 4; // menu
    } else if (x < 80) {
        s_screen_mode = (mode == 0) ? 3 : (uint8_t)(mode - 1);
    } else if (x > 160) {
        s_screen_mode = (uint8_t)((mode + 1) % 4);
    } else {
        s_screen_mode = (uint8_t)((mode + 1) % 4);
    }
}

static void draw_nav_hint(void)
{
    // Botones tactiles discretos: no ocupan mucho espacio y ayudan al usuario.
    draw_text(42, 207, "ANT", C_DIM, C_BG, 1);
    draw_text(96, 207, "MENU", C_DIM, C_BG, 1);
    draw_text(170, 207, "SIG", C_DIM, C_BG, 1);
}

static void draw_menu_screen(uint16_t ring, bool tx_ok, bool max_ok)
{
    (void)tx_ok;
    (void)max_ok;

    // Menú V17: más limpio y centrado dentro del área circular.
    // Se evita dibujar texto cerca del borde inferior para que no se vea cortado.
    draw_soft_header("MENU", ring);

    // Separadores suaves.
    fill_rect(119, 58, 121, 178, C_BG2);
    fill_rect(42, 118, 198, 120, C_BG2);

    // Indicadores táctiles por cuadrante. Textos dentro del círculo útil.
    draw_text(45, 70, "CARDIO", C_WHITE, C_BG, 1);
    draw_text(138, 70, "SENSOR", C_WHITE, C_BG, 1);
    draw_text(53, 145, "IMU", C_WHITE, C_BG, 2);
    draw_text(130, 145, "SISTEMA", C_WHITE, C_BG, 1);

    draw_center_pill(48, 190, 192, 210, "TOCA OPCION", C_DIM, C_BG2, 1);
}


void lcd_ui_show_data(const qmi8658_sample_t *imu, const max30100_result_t *vitals, bool tx_ok, uint32_t seq)
{
    uint8_t screen_mode = s_screen_mode;
    fill_rect(0, 0, LCD_H_RES, LCD_V_RES, C_BG);

    char line[64];
    const bool max_ok = (vitals && vitals->device_ok);
    uint16_t ring = status_color(vitals, tx_ok);
    draw_watch_shell(ring, tx_ok, max_ok);

    if (screen_mode == 4) {
        draw_menu_screen(ring, tx_ok, max_ok);
    }
    else if (screen_mode == 0) {
        // ============================================================
        // Vista principal: estilo reloj/manilla biometrica.
        // Prioriza PULSO y SpO2, que son los datos visibles del usuario.
        // ============================================================
        draw_text(78, 28, "NODO 1", C_DIM, C_BG, 1);

        if (vitals && vitals->device_ok && vitals->finger_detected && vitals->valid) {
            snprintf(line, sizeof(line), "%03.0f", vitals->bpm);
            draw_text(62, 58, line, ring, C_BG, 6);
            draw_text(88, 114, "BPM", C_WHITE, C_BG, 2);

            snprintf(line, sizeof(line), "SPO2 %02.0f%%", vitals->spo2);
            draw_center_pill(42, 148, 198, 174, line, C_WHITE, C_BG2, 2);
            draw_center_pill(50, 184, 190, 202, status_text(vitals, tx_ok), ring, C_BG, 1);
        }
        else if (vitals && vitals->device_ok && vitals->finger_detected) {
            draw_text(44, 72, "MIDIENDO", C_YELLOW, C_BG, 3);
            draw_text(54, 128, "ESPERA", C_YELLOW, C_BG, 2);
            draw_center_pill(52, 166, 188, 190, "DEDO OK", C_WHITE, C_BG2, 2);
        }
        else if (vitals && vitals->device_ok) {
            draw_text(46, 62, "COLOCA", C_YELLOW, C_BG, 3);
            draw_text(66, 108, "DEDO", C_YELLOW, C_BG, 3);
            draw_center_pill(46, 162, 194, 188, "SENSOR OK", C_GREEN, C_BG2, 2);
        }
        else {
            draw_text(38, 62, "MAX3010X", C_RED, C_BG, 2);
            draw_text(58, 96, "NO OK", C_RED, C_BG, 3);
            draw_text(36, 150, "REVISA SDA15", C_YELLOW, C_BG, 1);
            draw_text(42, 166, "SCL16 3V3", C_YELLOW, C_BG, 1);
        }
    }
    else if (screen_mode == 1) {
        // ============================================================
        // Vista SENSOR: permite saber si el MAX y el dedo estan listos.
        // ============================================================
        draw_soft_header("SENSOR", ring);

        if (!vitals || !vitals->device_ok) {
            draw_text(38, 70, "MAX3010X", C_RED, C_BG, 2);
            draw_text(58, 104, "NO OK", C_RED, C_BG, 3);
            draw_text(36, 150, "REVISA 3V3 GND", C_YELLOW, C_BG, 1);
            draw_text(42, 166, "SDA15 SCL16", C_YELLOW, C_BG, 1);
        } else if (!vitals->finger_detected) {
            draw_text(48, 66, "COLOCA", C_YELLOW, C_BG, 3);
            draw_text(70, 112, "DEDO", C_YELLOW, C_BG, 3);
            draw_center_pill(48, 166, 192, 190, "MAX OK", C_GREEN, C_BG2, 2);
        } else if (!vitals->valid) {
            draw_text(44, 76, "MIDIENDO", C_YELLOW, C_BG, 2);
            draw_text(56, 116, "NO MUEVAS", C_WHITE, C_BG, 1);
            draw_center_pill(46, 158, 194, 182, "DEDO OK", C_GREEN, C_BG2, 2);
        } else {
            draw_text(44, 70, "SENSOR", C_GREEN, C_BG, 2);
            draw_text(72, 108, "OK", C_GREEN, C_BG, 4);
            draw_center_pill(42, 164, 198, 188, status_text(vitals, tx_ok), ring, C_BG2, 1);
        }
        draw_nav_hint();
    }
    else if (screen_mode == 2) {
        // ============================================================
        // Vista movimiento: compacta para verificar QMI8658.
        // ============================================================
        draw_soft_header("MOVIMIENTO", ring);
        draw_text(82, 48, "IMU", C_WHITE, C_BG, 3);

        if (imu && imu->valid) {
            draw_center_pill(40, 84, 114, 106, "ACC", C_YELLOW, C_BG2, 2);
            snprintf(line, sizeof(line), "X%5.1f", imu->acc_x);
            draw_text(35, 116, line, C_WHITE, C_BG, 1);
            snprintf(line, sizeof(line), "Y%5.1f", imu->acc_y);
            draw_text(35, 130, line, C_WHITE, C_BG, 1);
            snprintf(line, sizeof(line), "Z%5.1f", imu->acc_z);
            draw_text(35, 144, line, C_WHITE, C_BG, 1);

            draw_center_pill(126, 84, 200, 106, "GYR", C_YELLOW, C_BG2, 2);
            snprintf(line, sizeof(line), "X%5.1f", imu->gyr_x);
            draw_text(124, 116, line, C_WHITE, C_BG, 1);
            snprintf(line, sizeof(line), "Y%5.1f", imu->gyr_y);
            draw_text(124, 130, line, C_WHITE, C_BG, 1);
            snprintf(line, sizeof(line), "Z%5.1f", imu->gyr_z);
            draw_text(124, 144, line, C_WHITE, C_BG, 1);

            draw_center_pill(58, 182, 182, 204, "QMI8658 OK", C_GREEN, C_BG2, 1);
        } else {
            draw_text(48, 106, "IMU NO OK", C_RED, C_BG, 2);
        }
    }
    else {
        // ============================================================
        // Vista sistema: estado de comunicacion y sensor cardiaco.
        // ============================================================
        draw_soft_header("SISTEMA", ring);

        snprintf(line, sizeof(line), "CANAL %u", (unsigned int)ESPNOW_CHANNEL);
        draw_center_pill(48, 54, 192, 82, line, C_YELLOW, C_BG2, 2);

        snprintf(line, sizeof(line), "TX %s", tx_ok ? "OK" : "FAIL");
        draw_text(54, 100, line, tx_ok ? C_GREEN : C_ORANGE, C_BG, 3);

        snprintf(line, sizeof(line), "SEQ %lu", (unsigned long)seq);
        draw_center_pill(58, 146, 182, 168, line, C_WHITE, C_BG2, 1);

        if (max_ok) {
            draw_center_pill(54, 184, 186, 206, "MAX OK", C_GREEN, C_BG2, 2);
        } else {
            draw_center_pill(54, 184, 186, 206, "MAX NO", C_RED, C_BG2, 2);
        }
    }

    // En esta version la pantalla NO rota automaticamente.
    // El usuario cambia la vista con el touch.
    if (screen_mode != 4) {
        draw_nav_hint();
    }

    lcd_flush();
}

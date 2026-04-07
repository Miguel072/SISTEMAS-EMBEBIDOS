/**
 * @file xd58c.h
 * @brief Driver del sensor XD-58C — Sensor fotoeléctrico de pulso cardíaco.
 *
 * @details
 * El XD-58C (compatible con PulseSensor Amped) es un sensor analógico de
 * fotopletismografía (PPG) que emite luz verde (515 nm) y mide la variación
 * de reflectancia causada por el cambio de volumen sanguíneo en cada latido.
 *
 * Su salida es una **señal analógica de voltaje** (0–3.3 V a 3.3 V de
 * alimentación) que se lee directamente por el ADC del ESP32. No tiene
 * interfaz I2C/SPI ni mide SpO2 ni temperatura.
 *
 * @par Diferencias con MAX30102
 * | Característica | MAX30102       | XD-58C              |
 * |----------------|----------------|---------------------|
 * | Interfaz       | I2C digital    | Analógica (ADC)     |
 * | SpO2           | Sí (Rojo + IR) | No                  |
 * | Temperatura    | Sí             | No                  |
 * | Pines          | VCC,GND,SDA,SCL,INT | VCC,GND,SIG    |
 * | Resolución ADC | 18 bits intern | 12 bits ESP32 ADC   |
 * | Frecuencia     | 100 sps FIFO   | Configurable (500 Hz tim) |
 *
 * @par Conexión de hardware
 * | Pin XD-58C | GPIO ESP32    | Descripción                      |
 * |------------|---------------|----------------------------------|
 * | VCC (+)    | 3.3V          | Alimentación (NO usar 5V)        |
 * | GND (-)    | GND           | Tierra                           |
 * | SIG (S)    | GPIO34        | Señal analógica → ADC1_CH6       |
 *
 * @warning GPIO34 es entrada-only en ESP32 devkit. Ideal para ADC.
 *          No conectar a 5V: daña el ADC del ESP32.
 *
 * @par Algoritmo de detección de BPM
 * Basado en el algoritmo original de PulseSensor Amped (Joel Murphy /
 * Yury Gitman) adaptado para ESP32 + ESP-IDF v5.x con FreeRTOS:
 * - Timer hardware a 2 ms (500 Hz) dispara la lectura del ADC.
 * - Seguimiento adaptativo de pico (P) y valle (T) de la señal.
 * - Umbral dinámico = (P + T) / 2 con histéresis.
 * - Detección de latido en flanco ascendente sobre el umbral.
 * - Cálculo de IBI (Inter-Beat Interval) y promedio de los últimos 10 IBI.
 *
 * @author  Equipo ADL — Sistemas Embebidos y Tiempo Real
 * @version 2.0.0
 * @date    2025
 *
 * @warning NO apto para diagnóstico médico. Solo uso académico/demostrativo.
 */

#ifndef XD58C_H
#define XD58C_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 *  Parámetros de configuración (ajustables)
 * ============================================================ */

/** @defgroup XD58C_CONFIG Parámetros del driver XD-58C
 *  @{
 */

/** GPIO de entrada analógica — debe ser capaz de ADC1 (GPIO32–39 en ESP32) */
#define XD58C_ADC_GPIO          34

/** Canal ADC correspondiente a GPIO34 en ESP32 devkit */
#define XD58C_ADC_CHANNEL       ADC_CHANNEL_6    /* GPIO34 = ADC1_CH6 */

/** Período de muestreo en microsegundos (2000 µs = 500 Hz = 2 ms) */
#define XD58C_SAMPLE_PERIOD_US  2000U

/** Número de IBI almacenados para promediar BPM (igual al original Amped) */
#define XD58C_IBI_HISTORY       10U

/** BPM mínimo y máximo considerados fisiológicamente válidos */
#define XD58C_BPM_MIN           30
#define XD58C_BPM_MAX           220

/**
 * @brief Umbral de amplitud mínima de la señal para considerar dedo presente.
 * La señal en reposo (sin dedo) es aproximadamente ADC_MAX/2 (≈2048).
 * Con dedo, la variación pico-a-pico suele superar 200 LSB.
 */
#define XD58C_FINGER_AMP_MIN    100

/** Umbral inicial de detección (valor medio del rango ADC de 12 bits) */
#define XD58C_THRESH_INIT       2048

/** @} */

/* ============================================================
 *  Estructuras de datos
 * ============================================================ */

/**
 * @brief Resultado de signos vitales calculados por el driver XD-58C.
 */
typedef struct {
    float    bpm;          /**< Frecuencia cardíaca (latidos/min) */
    uint32_t ibi_ms;       /**< Último intervalo inter-latido (ms) */
    bool     finger_ok;    /**< true si hay dedo detectado con señal válida */
    uint8_t  bpm_quality;  /**< Calidad de la medición 0–100 */
    int      raw_signal;   /**< Último valor ADC crudo (0–4095) */
    bool     beat_flag;    /**< true durante el ciclo donde se detectó latido */
} xd58c_vitals_t;

/**
 * @brief Handle del driver XD-58C.
 *
 * Contiene el estado interno del algoritmo de detección de latidos.
 * No acceder directamente a los campos privados.
 */
typedef struct {
    /* ── ADC ─────────────────────────── */
    void               *adc_handle;  /**< Handle oneshot ADC (tipo opaco) */
    void               *cali_handle; /**< Handle de calibración ADC */
    bool                cali_ok;     /**< true si la calibración ADC es válida */

    /* ── Resultados publicados ───────── */
    volatile xd58c_vitals_t vitals;  /**< Resultado actualizado por la ISR del timer */
    SemaphoreHandle_t        beat_sem;/**< Señalado cada vez que se detecta un latido */

    /* ── Estado interno del algoritmo ── */
    volatile int      signal;        /**< Muestra ADC actual */
    volatile int      ibi;           /**< Intervalo inter-latido actual (ms) */
    volatile int      bpm;           /**< BPM actual */
    volatile int      P;             /**< Pico de la onda (peak) */
    volatile int      T;             /**< Valle de la onda (trough) */
    volatile int      thresh;        /**< Umbral dinámico de detección */
    volatile int      amp;           /**< Amplitud pico-a-valle */
    volatile uint32_t sample_counter;/**< Contador de muestras × 2 ms = tiempo en ms */
    volatile uint32_t last_beat_time;/**< Timestamp del último latido (ms) */
    volatile bool     pulse;         /**< true mientras la señal está sobre el umbral */
    volatile bool     first_beat;    /**< Bandera para el primer latido al inicio */
    volatile bool     second_beat;   /**< Bandera para el segundo latido */
    volatile int      rate[10];      /**< Historial de últimos 10 IBI (ms) */

    /* ── Timer ───────────────────────── */
    esp_timer_handle_t timer;        /**< Timer periódico de 2 ms */
    bool               initialized;  /**< true tras init exitosa */
} xd58c_handle_t;

/* ============================================================
 *  API pública
 * ============================================================ */

/**
 * @brief Inicializa el driver del sensor XD-58C.
 *
 * Configura el ADC1 con calibración, crea el semáforo de latido
 * e inicia el timer periódico de 2 ms que ejecuta el algoritmo
 * de detección de BPM en segundo plano.
 *
 * @param[out] handle  Puntero al handle a inicializar (debe ser estático o en heap).
 *
 * @return
 *   - ESP_OK en éxito.
 *   - ESP_ERR_NO_MEM si falla la creación del semáforo.
 *   - Otros códigos de ESP-IDF si falla el ADC o el timer.
 */
esp_err_t xd58c_init(xd58c_handle_t *handle);

/**
 * @brief Detiene el timer de muestreo (pausa sin liberar recursos).
 *
 * @param[in] handle Handle inicializado.
 * @return ESP_OK en éxito.
 */
esp_err_t xd58c_stop(xd58c_handle_t *handle);

/**
 * @brief Reanuda el timer de muestreo tras un stop.
 *
 * @param[in] handle Handle inicializado.
 * @return ESP_OK en éxito.
 */
esp_err_t xd58c_start(xd58c_handle_t *handle);

/**
 * @brief Obtiene una copia de los últimos vitales calculados.
 *
 * Esta función es segura para llamar desde cualquier tarea FreeRTOS.
 * Copia atómicamente el estado publicado por la ISR del timer.
 *
 * @param[in]  handle  Handle inicializado.
 * @param[out] out     Destino de la copia de vitales.
 */
void xd58c_get_vitals(const xd58c_handle_t *handle, xd58c_vitals_t *out);

/**
 * @brief Espera hasta que se detecte un nuevo latido (máximo timeout_ms).
 *
 * Bloquea la tarea actual hasta que la ISR del timer señale el semáforo
 * de latido, o se agote el timeout.
 *
 * @param[in] handle      Handle inicializado.
 * @param[in] timeout_ms  Timeout en ms. 0 = esperar indefinidamente.
 *
 * @return pdTRUE si se detectó un latido; pdFALSE si timeout.
 */
BaseType_t xd58c_wait_for_beat(xd58c_handle_t *handle, uint32_t timeout_ms);

/**
 * @brief Libera todos los recursos del driver.
 *
 * Detiene el timer, elimina el semáforo y libera el ADC.
 *
 * @param[in] handle Handle inicializado.
 * @return ESP_OK en éxito.
 */
esp_err_t xd58c_deinit(xd58c_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* XD58C_H */

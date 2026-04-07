/**
 * @file xd58c.c
 * @brief Implementación del driver XD-58C para ESP-IDF v5.x.
 *
 * @details
 * Implementa el algoritmo PulseSensor Amped (Joel Murphy / Yury Gitman)
 * portado de Arduino a ESP-IDF v5.x con las siguientes adaptaciones:
 *
 *  - El timer de Arduino (Timer2 a 2 ms) se reemplaza por **esp_timer**
 *    periódico con período de 2000 µs (500 Hz).
 *  - El ADC analógico de Arduino (10 bits, 0–1023) se reemplaza por
 *    **adc_oneshot** de ESP-IDF v5.x (12 bits, 0–4095) con calibración
 *    de curva de ajuste por eFuse.
 *  - Todos los umbrales del algoritmo se escalan × 4 para adaptarlos
 *    al rango de 12 bits (originalmente diseñados para 10 bits).
 *  - La variable `Signal` del algoritmo original se mapea de vuelta a
 *    0–1023 para mantener la compatibilidad matemática del algoritmo.
 *  - El resultado se publica a través de la estructura `vitals` volátil
 *    y un semáforo binario FreeRTOS.
 *
 * @author  Equipo ADL — Sistemas Embebidos y Tiempo Real
 * @version 2.0.0
 * @date    2025
 */

#include "xd58c.h"

#include <string.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "XD58C";

/* ── Escalado del rango ADC ──────────────────────────────────
 * El algoritmo Amped fue diseñado para 10 bits (0-1023).
 * El ADC del ESP32 es 12 bits (0-4095).
 * Mapeamos la lectura cruda a 0-1023 para mantener los umbrales originales.
 */
#define ADC_RAW_TO_SIGNAL(raw)   ((raw) >> 2)   /* /4 → 0-1023 */
#define SIGNAL_RANGE             1023
#define SIGNAL_MID               512

/* ── Semilla del historial IBI al arranque ───────────────────
 * Equivale a ~75 BPM (800 ms por latido) para estabilizar el promedio
 * rápidamente en las primeras mediciones.
 */
#define IBI_SEED_MS              800

/* ============================================================
 *  Callback del timer periódico (corazón del algoritmo)
 * ============================================================ */

/**
 * @brief Callback ejecutado cada 2 ms por el timer periódico.
 *
 * Implementa el algoritmo PulseSensor Amped completo:
 * 1. Lee el ADC y mapea a rango 0-1023.
 * 2. Sigue el pico (P) y el valle (T) de la onda de pulso.
 * 3. Detecta el latido en flanco ascendente sobre el umbral dinámico.
 * 4. Calcula IBI y BPM (promedio de últimas 10 medidas).
 * 5. Publica resultados en handle->vitals.
 * 6. Señala el semáforo de latido cuando corresponde.
 *
 * @param arg Puntero al handle xd58c_handle_t.
 */
static void IRAM_ATTR xd58c_timer_cb(void *arg)
{
    xd58c_handle_t *h = (xd58c_handle_t *)arg;

    /* ── Leer ADC y mapear a 0-1023 ───────────────────────── */
    int raw = 0;
    /* Nota: adc_oneshot_read es seguro desde ISR de timer con ESP-IDF v5.x */
    adc_oneshot_read((adc_oneshot_unit_handle_t)h->adc_handle,
                     XD58C_ADC_CHANNEL, &raw);
    h->signal = ADC_RAW_TO_SIGNAL(raw);
    h->vitals.raw_signal = raw;

    h->sample_counter += 2;  /* 2 ms por cada llamada */

    int N = (int)(h->sample_counter - h->last_beat_time);

    /* ── Seguir pico (P) y valle (T) ─────────────────────── */
    /* Solo durante la segunda mitad del período refractario (evita dichrótico) */
    if (h->signal < h->thresh && N > (h->ibi / 5) * 3) {
        if (h->signal < h->T) {
            h->T = h->signal;     /* Nuevo mínimo (valle) */
        }
    }
    if (h->signal > h->thresh && h->signal > h->P) {
        h->P = h->signal;         /* Nuevo máximo (pico) */
    }

    /* ── Detección de latido ─────────────────────────────── */
    /* Condición: señal sube sobre el umbral, no estamos en período refractario,
       y han pasado al menos 3/5 del último IBI desde el latido anterior */
    if (N > 250) {  /* Evita ruido de alta frecuencia (> 240 BPM) */

        if (h->signal > h->thresh && !h->pulse && N > (h->ibi / 5) * 3) {

            h->pulse = true;       /* Inicio de latido detectado */
            h->vitals.beat_flag = true;

            /* Calcular IBI */
            h->ibi = (int)(h->sample_counter - h->last_beat_time);
            h->last_beat_time = h->sample_counter;

            /* Primer latido: sembrar historial con un IBI razonable */
            if (h->first_beat) {
                h->first_beat = false;
                for (int i = 0; i < XD58C_IBI_HISTORY; i++) {
                    h->rate[i] = IBI_SEED_MS;
                }
                h->second_beat = true;
                goto update_vitals;
            }

            /* Segundo latido: re-sembrar con el IBI real medido */
            if (h->second_beat) {
                h->second_beat = false;
                for (int i = 0; i < XD58C_IBI_HISTORY; i++) {
                    h->rate[i] = h->ibi;
                }
            }

            /* Desplazar historial y añadir nuevo IBI */
            int run_total = 0;
            for (int i = 0; i < XD58C_IBI_HISTORY - 1; i++) {
                h->rate[i] = h->rate[i + 1];
                run_total += h->rate[i];
            }
            h->rate[XD58C_IBI_HISTORY - 1] = h->ibi;
            run_total += h->rate[XD58C_IBI_HISTORY - 1];

            /* BPM = 60000 ms / IBI_promedio */
            h->bpm = 60000 / (run_total / XD58C_IBI_HISTORY);

update_vitals:
            /* Publicar resultados */
            h->vitals.bpm      = (float)h->bpm;
            h->vitals.ibi_ms   = (uint32_t)h->ibi;
            h->vitals.finger_ok = (h->amp > XD58C_FINGER_AMP_MIN);
            h->vitals.bpm_quality = h->vitals.finger_ok ?
                                    (uint8_t)((float)h->amp / 5.0f > 100 ? 100 : (float)h->amp / 5.0f) : 0;

            /* El callback corre en contexto de tarea (ESP_TIMER_TASK),
               así que se usa la API normal de FreeRTOS. */
            xSemaphoreGive(h->beat_sem);
        }
    }

    /* ── Fin del latido: restaurar P y T ────────────────────── */
    if (h->signal < h->thresh && h->pulse) {
        h->pulse = false;
        h->vitals.beat_flag = false;
        h->amp   = h->P - h->T;              /* Amplitud pico-a-valle */
        h->thresh = h->amp / 2 + h->T;       /* Nuevo umbral dinámico */
        h->P     = h->thresh;                /* Reset pico */
        h->T     = h->thresh;                /* Reset valle */
    }

    /* ── Watchdog: sin latido por 2.5 s → reiniciar estado ── */
    if (N > 2500) {
        h->thresh        = SIGNAL_MID;
        h->P             = SIGNAL_MID;
        h->T             = SIGNAL_MID;
        h->last_beat_time= h->sample_counter;
        h->first_beat    = true;
        h->second_beat   = false;
        h->bpm           = 0;
        h->vitals.bpm    = 0.0f;
        h->vitals.ibi_ms = 0;
        h->vitals.finger_ok  = false;
        h->vitals.bpm_quality= 0;
        h->vitals.beat_flag  = false;
    }
}

/* ============================================================
 *  Implementación de la API pública
 * ============================================================ */

esp_err_t xd58c_init(xd58c_handle_t *handle)
{
    ESP_LOGI(TAG, "Inicializando driver XD-58C (analógico)...");

    /* ── Limpiar el handle ──────────────────────────────────── */
    memset(handle, 0, sizeof(xd58c_handle_t));

    /* ── Inicializar el estado del algoritmo Amped ─────────── */
    handle->thresh        = SIGNAL_MID;
    handle->P             = SIGNAL_MID;
    handle->T             = SIGNAL_MID;
    handle->ibi           = IBI_SEED_MS;
    handle->amp           = 100;
    handle->first_beat    = true;
    handle->second_beat   = false;
    for (int i = 0; i < XD58C_IBI_HISTORY; i++) {
        handle->rate[i] = IBI_SEED_MS;
    }

    /* ── Configurar ADC oneshot ────────────────────────────── */
    adc_oneshot_unit_init_cfg_t adc_cfg = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&adc_cfg,
                    (adc_oneshot_unit_handle_t *)&handle->adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error init ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configurar canal: 12 bits, atenuación 12 dB → rango 0–3.3 V */
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten    = ADC_ATTEN_DB_12,
    };
    ret = adc_oneshot_config_channel(
              (adc_oneshot_unit_handle_t)handle->adc_handle,
              XD58C_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error config canal ADC: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "ADC1 canal %d (GPIO%d) configurado — 12 bits, 0-3.3V",
             XD58C_ADC_CHANNEL, XD58C_ADC_GPIO);

    /* ── Calibración del ADC ───────────────────────────────── */
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ret = adc_cali_create_scheme_line_fitting(
              &cali_cfg, (adc_cali_handle_t *)&handle->cali_handle);
    if (ret == ESP_OK) {
        handle->cali_ok = true;
        ESP_LOGI(TAG, "Calibración ADC por eFuse activada ✓");
    } else {
        handle->cali_ok = false;
        ESP_LOGW(TAG, "Sin calibración ADC (eFuse no programado) — modo raw");
    }

    /* ── Semáforo binario de latido ────────────────────────── */
    handle->beat_sem = xSemaphoreCreateBinary();
    if (handle->beat_sem == NULL) {
        ESP_LOGE(TAG, "No se pudo crear semáforo de latido");
        return ESP_ERR_NO_MEM;
    }

    /* ── Timer periódico de 2 ms ───────────────────────────── */
    esp_timer_create_args_t timer_args = {
        .callback        = xd58c_timer_cb,
        .arg             = handle,
        .dispatch_method = ESP_TIMER_TASK,  /* Ejecutar en tarea de timer */
        .name            = "xd58c_sample",
        .skip_unhandled_events = true,
    };
    ret = esp_timer_create(&timer_args, &handle->timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error creando timer: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Iniciar timer periódico */
    ret = esp_timer_start_periodic(handle->timer, XD58C_SAMPLE_PERIOD_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error iniciando timer: %s", esp_err_to_name(ret));
        return ret;
    }

    handle->initialized = true;
    ESP_LOGI(TAG, "XD-58C listo — muestreo a %d Hz (cada %d µs) ✓",
             1000000 / XD58C_SAMPLE_PERIOD_US, XD58C_SAMPLE_PERIOD_US);
    return ESP_OK;
}

/* ──────────────────────────────────────────────────────────── */

esp_err_t xd58c_stop(xd58c_handle_t *handle)
{
    if (!handle->initialized) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "Timer de muestreo pausado");
    return esp_timer_stop(handle->timer);
}

/* ──────────────────────────────────────────────────────────── */

esp_err_t xd58c_start(xd58c_handle_t *handle)
{
    if (!handle->initialized) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "Timer de muestreo reanudado");
    return esp_timer_start_periodic(handle->timer, XD58C_SAMPLE_PERIOD_US);
}

/* ──────────────────────────────────────────────────────────── */

void xd58c_get_vitals(const xd58c_handle_t *handle, xd58c_vitals_t *out)
{
    /* Copia atómica — los campos individuales son tipos nativos de 32 bits.
       En ESP32 (Xtensa LX6) las lecturas de 32 bits son atómicas. */
    out->bpm         = handle->vitals.bpm;
    out->ibi_ms      = handle->vitals.ibi_ms;
    out->finger_ok   = handle->vitals.finger_ok;
    out->bpm_quality = handle->vitals.bpm_quality;
    out->raw_signal  = handle->vitals.raw_signal;
    out->beat_flag   = handle->vitals.beat_flag;
}

/* ──────────────────────────────────────────────────────────── */

BaseType_t xd58c_wait_for_beat(xd58c_handle_t *handle, uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == 0) ? portMAX_DELAY
                                         : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(handle->beat_sem, ticks);
}

/* ──────────────────────────────────────────────────────────── */

esp_err_t xd58c_deinit(xd58c_handle_t *handle)
{
    if (!handle->initialized) return ESP_OK;

    esp_timer_stop(handle->timer);
    esp_timer_delete(handle->timer);

    if (handle->beat_sem) {
        vSemaphoreDelete(handle->beat_sem);
        handle->beat_sem = NULL;
    }

    adc_oneshot_del_unit((adc_oneshot_unit_handle_t)handle->adc_handle);

    if (handle->cali_ok && handle->cali_handle) {
        adc_cali_delete_scheme_line_fitting(
            (adc_cali_handle_t)handle->cali_handle);
    }

    handle->initialized = false;
    ESP_LOGI(TAG, "Driver liberado ✓");
    return ESP_OK;
}

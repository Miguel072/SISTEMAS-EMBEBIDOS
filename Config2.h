/**
 * @file Config2.h
 * @brief Archivo central de configuración del sistema embebido.
 *
 * @details
 * Este archivo define todos los parámetros configurables del sistema:
 *
 * - Mapeo de pines del ESP32
 * - Umbrales de decisión (temperatura y luz)
 * - Periodos seguros de lectura de sensores
 * - Tiempos de transición de la FSM
 * - Patrones de parpadeo del LED RGB
 * - Periodo de depuración por Serial
 *
 * Este archivo NO contiene lógica.
 * Su propósito es desacoplar configuración de implementación.
 *
 * Cualquier cambio de hardware o reglas del sistema
 * debe realizarse aquí sin modificar la lógica principal.
 */

#pragma once
#include <Arduino.h>

/**
 * @defgroup PinMapping Mapeo de Pines (ESP32)
 * @brief Definición de conexiones físicas del hardware.
 * @{
 */

/** @brief Pin de datos del sensor DHT (temperatura y humedad). */
#define DHTPIN      4

/** @brief Pin ADC del sensor LDR (iluminación). */
#define LDR_PIN     34

/** @brief Pin del botón de inicio (activo en LOW). */
#define BTN_PIN     23

/** @brief Pin canal rojo del LED RGB. */
#define RGB_R_PIN   25

/** @brief Pin canal verde del LED RGB. */
#define RGB_G_PIN   26

/** @brief Pin canal azul del LED RGB. */
#define RGB_B_PIN   27

/** @} */


/**
 * @defgroup FlameSensor Configuración del Sensor de Flama
 * @brief Parámetros del sensor digital de flama.
 * @{
 */

/**
 * @brief Pin digital (DO) del módulo de flama.
 *
 * Se utiliza la salida digital del módulo.
 */
#define FLAME_DO_PIN  19

/**
 * @brief Define la lógica activa del sensor de flama.
 *
 * - 1 → Activo en LOW
 * - 0 → Activo en HIGH
 *
 * Permite compatibilidad con distintos módulos comerciales.
 */
#define FLAME_ACTIVE_LOW  0

/** @} */


/**
 * @defgroup Thresholds Umbrales del Sistema
 * @brief Reglas de decisión para la FSM.
 * @{
 */

/**
 * @brief Temperatura mínima para entrar en estado ALERTA (°C).
 */
#define TEMP_ALERT_C     26.0

/**
 * @brief Temperatura mínima para escalar a estado ALARMA (°C).
 */
#define TEMP_ALARM_C     30.0

/**
 * @brief Umbral de oscuridad para el LDR (valor ADC crudo).
 *
 * Se considera oscuridad si:
 * raw < LIGHT_RAW_TH
 */
#define LIGHT_RAW_TH     300

/** @} */


/**
 * @defgroup SensorTiming Periodos Seguros de Lectura
 * @brief Control de frecuencia para evitar lecturas inválidas.
 * @{
 */

/**
 * @brief Periodo mínimo entre lecturas del DHT (ms).
 *
 * El DHT11 requiere al menos 1 segundo entre lecturas.
 */
#define DHT_READ_PERIOD_MS   1000

/** @} */


/**
 * @defgroup FSMTimeouts Temporizaciones de la Máquina de Estados
 * @brief Tiempos de transición automáticos entre estados.
 * @{
 */

/** @brief Tiempo en MON_TEMP antes de evaluar transición a MON_LUZ (ms). */
#define T_TEMP_TO_LUZ_MS     2000

/** @brief Tiempo en MON_TEMP antes de transición a MON_HUMED (ms). */
#define T_TEMP_TO_HUM_MS     4000

/** @brief Tiempo en MON_HUMED antes de volver a MON_TEMP (ms). */
#define T_HUM_TO_TEMP_MS     3000

/** @brief Tiempo en MON_LUZ antes de volver a MON_TEMP (ms). */
#define T_LUZ_TO_TEMP_MS     5000

/** @brief Duración máxima del estado ALERTA (ms). */
#define T_ALERTA_TO_TEMP_MS  3000

/** @brief Duración total del estado ALARMA (ms). */
#define T_ALARMA_TOTAL_MS    5000

/** @} */


/**
 * @defgroup BlinkPatterns Patrones de Parpadeo LED
 * @brief Duraciones ON/OFF para cada estado visual.
 * @{
 */

/** @brief Tiempo encendido LED en estado INICIO (ms). */
#define INI_ON_MS      200

/** @brief Tiempo apagado LED en estado INICIO (ms). */
#define INI_OFF_MS     200

/** @brief Tiempo encendido LED en estado ALERTA (ms). */
#define ALERT_ON_MS    500

/** @brief Tiempo apagado LED en estado ALERTA (ms). */
#define ALERT_OFF_MS   900

/** @brief Tiempo encendido LED en estado ALARMA (ms). */
#define ALARM_ON_MS    100

/** @brief Tiempo apagado LED en estado ALARMA (ms). */
#define ALARM_OFF_MS   300

/** @} */


/**
 * @defgroup DebugConfig Configuración de Depuración
 * @brief Parámetros de impresión por puerto serie.
 * @{
 */

/**
 * @brief Periodo de impresión de variables por Serial (ms).
 */
#define DEBUG_PERIOD_MS 1000

/** @} */
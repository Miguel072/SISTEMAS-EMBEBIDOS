/**
 * @file Sensors2.h
 * @brief Interfaz pública del módulo de adquisición de sensores.
 *
 * @details
 * Este módulo proporciona acceso no bloqueante a:
 *
 * - Temperatura (DHT11)
 * - Humedad (DHT11)
 * - Nivel de luz (LDR analógico)
 * - Detección de flama (sensor digital DO)
 *
 * Características:
 * - Lectura cooperativa (sin delay)
 * - DHT limitado a 1 segundo mínimo entre lecturas
 * - Cache interno de valores
 * - Abstracción completa del hardware
 *
 * La FSM utiliza exclusivamente estas funciones
 * para obtener información del entorno.
 */

#pragma once
#include <Arduino.h>

/**
 * @defgroup Sensors Sensor Module
 * @brief Módulo de adquisición de datos físicos.
 * @{
 */


/**
 * @brief Inicializa el módulo de sensores.
 *
 * Configura:
 * - Sensor DHT
 * - Resolución ADC
 * - Pin digital del sensor de flama
 *
 * Debe llamarse una sola vez desde setup().
 */
void Sensors_Init();

/**
 * @brief Actualiza las lecturas de los sensores.
 *
 * Estrategia:
 * - LDR y flama → actualización rápida en cada ciclo.
 * - DHT → lectura limitada por periodo seguro.
 *
 * Debe llamarse continuamente dentro del loop().
 */
void Sensors_Update();


/**
 * @brief Obtiene la última temperatura válida.
 *
 * @return Temperatura en grados Celsius.
 */
float Sensors_GetTempC();

/**
 * @brief Obtiene la última humedad relativa válida.
 *
 * @return Humedad en porcentaje (%).
 */
float Sensors_GetHum();

/**
 * @brief Devuelve el valor crudo del ADC del LDR.
 *
 * @return Valor entero entre 0 y 4095.
 */
int Sensors_GetLightRaw();

/**
 * @brief Indica si el sistema considera que está oscuro.
 *
 * La evaluación se basa en:
 * LIGHT_RAW_TH definido en Config2.h
 *
 * @return true si el ambiente está oscuro.
 */
bool Sensors_IsDark();

/**
 * @brief Indica si el sensor detecta presencia de flama.
 *
 * La lógica activa (LOW/HIGH) se configura
 * mediante FLAME_ACTIVE_LOW en Config2.h.
 *
 * @return true si hay flama detectada.
 */
bool Sensors_FlameActive();

/** @} */
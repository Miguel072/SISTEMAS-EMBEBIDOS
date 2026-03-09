/**
 * @file Sensors2.cpp
 * @brief Implementación del módulo de adquisición de sensores.
 *
 * @details
 * Este módulo encapsula la lectura física de los sensores conectados al ESP32:
 *
 * - Sensor DHT11 (temperatura y humedad)
 * - Sensor LDR (lectura analógica de iluminación)
 * - Sensor digital de flama (DO)
 *
 * Características principales:
 * - Cache interno de valores
 * - Lectura no bloqueante
 * - Control de frecuencia para DHT11
 * - Evaluación de umbral de oscuridad
 * - Abstracción de lógica activa-alta / activa-baja del sensor de flama
 *
 * Este módulo NO contiene lógica de decisión.
 * Solo provee datos procesados a la FSM.
 */

#include "Sensors2.h"
#include "Config2.h"
#include <DHT.h>

/* =====================================================
   OBJETO DHT
   ===================================================== */

/**
 * @brief Instancia interna del sensor DHT11.
 *
 * Se mantiene privada al módulo para asegurar encapsulación.
 */
static DHT dht(DHTPIN, DHT11);

/* =====================================================
   VARIABLES CACHEADAS
   ===================================================== */

/** @brief Última temperatura válida leída (°C). */
static float g_tempC = NAN;

/** @brief Última humedad relativa válida (%). */
static float g_hum   = NAN;

/** @brief Último valor crudo del ADC del LDR. */
static int   g_lightRaw = 0;

/** @brief Resultado lógico de evaluación de oscuridad. */
static bool  g_isDark   = false;

/** @brief Estado lógico del sensor de flama. */
static bool  g_flameActive = false;

/** @brief Marca de tiempo de la última lectura del DHT (ms). */
static unsigned long g_lastDhtMs = 0;

/* =====================================================
   FUNCIONES INTERNAS
   ===================================================== */

/**
 * @brief Lee el estado digital del sensor de flama.
 *
 * Interpreta correctamente la lógica activa dependiendo
 * del valor configurado en FLAME_ACTIVE_LOW.
 *
 * @return true si la flama está activa.
 */
static inline bool readFlameDO()
{
  int raw = digitalRead(FLAME_DO_PIN);

#if FLAME_ACTIVE_LOW
  return (raw == LOW);
#else
  return (raw == HIGH);
#endif
}

/* =====================================================
   API PÚBLICA
   ===================================================== */

/**
 * @brief Inicializa el módulo de sensores.
 *
 * Configura:
 * - Sensor DHT
 * - Resolución ADC (12 bits)
 * - Pin digital del sensor de flama
 */
void Sensors_Init()
{
  dht.begin();

  /** @brief Configuración ADC del ESP32 a 12 bits (0–4095). */
  analogReadResolution(12);

  /**
   * @brief Pin DO del sensor de flama configurado con pull-up interno.
   *
   * Mejora estabilidad ante ruido eléctrico.
   */
  pinMode(FLAME_DO_PIN, INPUT_PULLUP);
}

/**
 * @brief Actualiza todas las lecturas de sensores.
 *
 * Estrategia:
 * - LDR y flama → lectura rápida en cada ciclo.
 * - DHT11 → lectura limitada por periodo mínimo.
 *
 * No bloquea la ejecución del sistema.
 */
void Sensors_Update()
{
  /* -------------------- LUZ (rápida) -------------------- */

  g_lightRaw = analogRead(LDR_PIN);
  g_isDark   = (g_lightRaw < LIGHT_RAW_TH);

  /* -------------------- FLAMA (rápida) -------------------- */

  g_flameActive = readFlameDO();

  /* -------------------- DHT (lenta) -------------------- */

  unsigned long now = millis();

  /**
   * @brief Control de frecuencia de lectura del DHT.
   *
   * Evita lecturas inválidas o saturación del sensor.
   */
  if (now - g_lastDhtMs < DHT_READ_PERIOD_MS)
    return;

  g_lastDhtMs = now;

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  /**
   * @brief Actualiza solo si la lectura es válida.
   *
   * El DHT puede devolver NAN en caso de error.
   */
  if (!isnan(t)) g_tempC = t;
  if (!isnan(h)) g_hum   = h;
}

/**
 * @brief Devuelve la temperatura actual en °C.
 * @return Temperatura cacheada.
 */
float Sensors_GetTempC()
{
  return g_tempC;
}

/**
 * @brief Devuelve la humedad relativa (%).
 * @return Humedad cacheada.
 */
float Sensors_GetHum()
{
  return g_hum;
}

/**
 * @brief Devuelve el valor crudo del ADC del LDR.
 * @return Valor 0–4095.
 */
int Sensors_GetLightRaw()
{
  return g_lightRaw;
}

/**
 * @brief Indica si el ambiente está oscuro.
 * @return true si lightRaw < LIGHT_RAW_TH.
 */
bool Sensors_IsDark()
{
  return g_isDark;
}

/**
 * @brief Indica si el sensor de flama detecta fuego.
 * @return true si la flama está activa.
 */
bool Sensors_FlameActive()
{
  return g_flameActive;
}
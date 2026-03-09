/**
 * @file MaquinaEstadosFSM_completofuncional.ino
 * @brief Punto de entrada principal del sistema embebido basado en FSM.
 *
 * @details
 * Este archivo contiene la inicialización y el ciclo principal del sistema.
 * Implementa una arquitectura cooperativa no bloqueante basada en:
 *
 * - Sensors2 → Adquisición de sensores
 * - TaskRT2  → Gestión temporal (blink, timeouts, debug)
 * - FmsApp2  → Máquina de Estados Finita (lógica principal)
 *
 * Arquitectura general del loop:
 * 1. Actualización de sensores
 * 2. Actualización de tareas temporizadas
 * 3. Evaluación de la FSM
 *
 * No se utiliza delay().
 * Todo el comportamiento temporal es gestionado mediante AsyncTaskLib.
 */

#include <Arduino.h>

#include "Config2.h"
#include "Sensors2.h"
#include "TaskRT2.h"
#include "FmsApp2.h"

/**
 * @defgroup MainApp Main Application
 * @brief Módulo principal del sistema.
 * @{
 */


/* =====================================================
   SETUP
   ===================================================== */

/**
 * @brief Inicializa el sistema.
 *
 * Configura:
 * - Comunicación Serial
 * - Pines GPIO (botón y LED RGB)
 * - Módulos internos (sensores, tareas, FSM)
 *
 * Se ejecuta una única vez al encender o reiniciar el ESP32.
 */
void setup()
{
  Serial.begin(115200);

  /* -------------------- CONFIGURACIÓN GPIO -------------------- */

  /** @brief Botón configurado con resistencia pull-up interna. */
  pinMode(BTN_PIN, INPUT_PULLUP);

  /** @brief Configuración de pines del LED RGB como salida. */
  pinMode(RGB_R_PIN, OUTPUT);
  pinMode(RGB_G_PIN, OUTPUT);
  pinMode(RGB_B_PIN, OUTPUT);

  /** @brief Estado inicial del LED RGB: apagado. */
  digitalWrite(RGB_R_PIN, LOW);
  digitalWrite(RGB_G_PIN, LOW);
  digitalWrite(RGB_B_PIN, LOW);

  /* -------------------- INICIALIZACIÓN DE MÓDULOS -------------------- */

  /**
   * @brief Inicializa módulo de sensores.
   */
  Sensors_Init();

  /**
   * @brief Inicializa módulo de tareas temporizadas.
   */
  Tasks_Init(); 

  /**
   * @brief Inicializa la máquina de estados.
   */
  Fsm_Init();

  /** @brief Mensaje de arranque del sistema. */
  Serial.println("=== ESP32 RT FSM Project Started ===");
}


/* =====================================================
   LOOP PRINCIPAL
   ===================================================== */

/**
 * @brief Ciclo principal cooperativo del sistema.
 *
 * Se ejecuta continuamente después de setup().
 *
 * Orden de ejecución:
 *
 * 1. Sensors_Update()
 *    - Actualiza temperatura, humedad y luz.
 *    - Implementa control interno de frecuencia para DHT.
 *
 * 2. Tasks_UpdateAll()
 *    - Gestiona parpadeos RGB.
 *    - Gestiona timeouts de transición.
 *    - Ejecuta tarea de depuración periódica.
 *
 * 3. Fsm_Update()
 *    - Evalúa transiciones.
 *    - Ejecuta acciones de entrada/salida.
 *
 * Importante:
 * No se utilizan funciones bloqueantes.
 * El sistema depende de ejecución rápida y continua del loop.
 */
void loop()
{
  // 1) Update all sensors (non-blocking, DHT throttled internally)
  Sensors_Update();

  // 2) Update all asynchronous tasks (blink + timeouts + debug)
  Tasks_UpdateAll();

  // 3) Update Finite State Machine
  Fsm_Update();
}

/** @} */
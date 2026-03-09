/**
 * @file TaskRT2.h
 * @brief Interfaz pública del módulo de tareas temporizadas cooperativas.
 *
 * @details
 * Este módulo implementa la capa temporal del sistema:
 *
 * - Parpadeo de LED RGB según estado (INICIO, ALERTA, ALARMA)
 * - Temporizadores de transición del diagrama de estados
 * - Tarea periódica de monitoreo por Serial
 *
 * Características:
 * - 100% no bloqueante
 * - Basado en AsyncTaskLib
 * - Generación de eventos mediante flags
 * - Separación total entre lógica temporal y lógica de estados (FSM)
 *
 * La FSM controla cuándo iniciar o detener tareas.
 * Este módulo solo ejecuta temporización y genera eventos.
 */

#pragma once
#include <Arduino.h>

/**
 * @defgroup TasksRT Real-Time Task Module
 * @brief Gestión cooperativa de temporizadores y parpadeos.
 * @{
 */


/* =====================================================
   INICIALIZACIÓN Y UPDATE GLOBAL
   ===================================================== */

/**
 * @brief Inicializa todas las tareas temporizadas.
 *
 * Configura callbacks internos:
 * - Blink encadenado
 * - Timeouts one-shot
 * - Tarea periódica de monitoreo
 *
 * Debe llamarse una vez desde setup().
 */
void Tasks_Init();

/**
 * @brief Actualiza todas las tareas activas.
 *
 * Debe llamarse continuamente dentro del loop().
 *
 * No bloquea la ejecución.
 */
void Tasks_UpdateAll();


/* =====================================================
   MONITOREO PERIÓDICO (SERIAL)
   ===================================================== */

/**
 * @brief Habilita impresión periódica de diagnóstico.
 *
 * La impresión incluye:
 * - Estado actual de la FSM
 * - Temperatura
 * - Humedad
 * - Luz
 * - Intentos
 *
 * Usado típicamente en estados MON_* y ALERTA.
 */
void Tasks_StartMonitoringDisplay();

/**
 * @brief Deshabilita impresión periódica.
 */
void Tasks_StopMonitoringDisplay();


/* =====================================================
   CONTROL DE PARPADEO RGB
   ===================================================== */

/**
 * @brief Inicia parpadeo verde (Estado INICIO).
 */
void Tasks_StartInicioBlink();

/**
 * @brief Detiene parpadeo verde.
 */
void Tasks_StopInicioBlink();

/**
 * @brief Inicia parpadeo azul (Estado ALERTA).
 */
void Tasks_StartAlertaBlink();

/**
 * @brief Detiene parpadeo azul.
 */
void Tasks_StopAlertaBlink();

/**
 * @brief Inicia parpadeo rojo (Estado ALARMA).
 */
void Tasks_StartAlarmaBlink();

/**
 * @brief Detiene parpadeo rojo.
 */
void Tasks_StopAlarmaBlink();


/* =====================================================
   CONTROL DE TIMEOUTS (TRANSICIONES FSM)
   ===================================================== */

/**
 * Cada Start:
 * - Reinicia el flag interno
 * - Arranca temporizador one-shot
 *
 * Cada Stop:
 * - Detiene el temporizador
 */

void Tasks_StartTempToLuz();
void Tasks_StopTempToLuz();

void Tasks_StartTempToHum();
void Tasks_StopTempToHum();

void Tasks_StartHumToTemp();
void Tasks_StopHumToTemp();

void Tasks_StartLuzToTemp();
void Tasks_StopLuzToTemp();

void Tasks_StartAlertaToTemp();
void Tasks_StopAlertaToTemp();

void Tasks_StartAlarmaToTemp();
void Tasks_StopAlarmaToTemp();


/* =====================================================
   EVENTOS DE TIMEOUT (POP FLAGS)
   ===================================================== */

/**
 * @brief Devuelve true si el timeout ocurrió.
 *
 * Además limpia el flag interno (consumo de evento).
 *
 * Este patrón evita:
 * - Repetición de eventos
 * - Condiciones persistentes no deseadas
 */

bool Tasks_PopTempToLuzDone();
bool Tasks_PopTempToHumDone();
bool Tasks_PopHumToTempDone();
bool Tasks_PopLuzToTempDone();
bool Tasks_PopAlertaToTempDone();
bool Tasks_PopAlarmaToTempDone();

/**
 * @brief Limpia todos los flags de timeout.
 *
 * Útil al cambiar de estado manualmente
 * o reiniciar el sistema lógico.
 */
void Tasks_ClearAllTimeoutFlags();

/** @} */
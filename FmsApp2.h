/**
 * @file FmsApp2.h
 * @brief Interfaz pública del módulo de Máquina de Estados Finita (FSM).
 *
 * @details
 * Este módulo expone la API principal para controlar la lógica
 * del sistema de monitoreo ambiental.
 *
 * La implementación interna se encuentra en FmsApp2.cpp y está
 * basada en una FSM con:
 * - 6 estados
 * - 11 transiciones
 * - Eventos temporales gestionados por TaskRT2
 * - Lecturas de sensores encapsuladas en Sensors2
 *
 * Este archivo solo declara la interfaz pública.
 */

#pragma once
#include <Arduino.h>

/**
 * @defgroup FsmApp FSM Application Module
 * @brief API pública del controlador lógico del sistema.
 * @{
 */

/**
 * @brief Inicializa la máquina de estados.
 *
 * Configura todas las transiciones y establece
 * el estado inicial (INICIO).
 *
 * Debe llamarse una sola vez en setup().
 */
void Fsm_Init();

/**
 * @brief Actualiza la máquina de estados.
 *
 * Evalúa transiciones y ejecuta cambios de estado si corresponde.
 * Debe llamarse en cada iteración del loop principal.
 */
void Fsm_Update();

/**
 * @brief Devuelve el nombre del estado actual.
 *
 * Utilizado principalmente para visualización
 * y depuración (TaskRT display).
 *
 * @return Cadena constante con el nombre del estado.
 */
const char* Fsm_GetStateName();

/**
 * @brief Devuelve el número de intentos acumulados
 * para escalar de ALERTA a ALARMA.
 *
 * @return Contador de intentos.
 */
int Fsm_GetAttempts();

/** @} */
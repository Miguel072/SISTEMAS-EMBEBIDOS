/**
 * @file FmsApp2.cpp
 * @brief Implementación de la Máquina de Estados Finita (FSM) del sistema.
 *
 * @details
 * Este módulo contiene la lógica principal del sistema de monitoreo ambiental.
 * La arquitectura está basada en una Máquina de Estados Finita (FSM)
 * implementada mediante la librería StateMachineLib.
 *
 * Características principales:
 * - 6 estados definidos explícitamente
 * - 11 transiciones controladas por sensores y temporizadores
 * - Acciones de entrada y salida por estado (OnEnter / OnLeave)
 * - Escalamiento progresivo de ALERTA a ALARMA
 * - Arquitectura no bloqueante (sin uso de delay)
 *
 * El sistema depende de:
 * - Sensors2 → Lectura encapsulada de sensores
 * - TaskRT2 → Gestión de temporización cooperativa
 * - Config2 → Parámetros y umbrales del sistema
 */

#include "FmsApp2.h"
#include "Config2.h"
#include "StateMachineLib.h"
#include "Sensors2.h"
#include "TaskRT2.h"

/* =====================================================
   FORWARD DECLARATIONS
   ===================================================== */

/** @brief Acciones de entrada y salida por estado. */
static void enterInicio();     static void leaveInicio();
static void enterMonTemp();    static void leaveMonTemp();
static void enterMonHumed();   static void leaveMonHumed();
static void enterMonLuz();     static void leaveMonLuz();
static void enterAlerta();     static void leaveAlerta();
static void enterAlarma();     static void leaveAlarma();

/* =====================================================
   DEFINICIÓN DE ESTADOS
   ===================================================== */

/**
 * @enum State
 * @brief Estados de la máquina.
 */
enum State
{
  INICIO = 0,      /**< Estado de inicialización */
  MON_TEMP = 1,    /**< Monitoreo de temperatura */
  MON_HUMED = 2,   /**< Monitoreo de humedad */
  MON_LUZ = 3,     /**< Monitoreo de iluminación */
  ALERTA_BLUE = 4, /**< Estado de advertencia */
  ALARMA_RED = 5   /**< Estado crítico */
};

/**
 * @brief Instancia de la FSM.
 * 6 estados, 11 transiciones.
 */
static StateMachine sm(6, 11);

/* =====================================================
   VARIABLES PÚBLICAS DE INFORMACIÓN
   ===================================================== */

/** @brief Estado actual del sistema (para visualización externa). */
static State g_currentState = INICIO;

/** @brief Contador de intentos de escalamiento a ALARMA. */
static int g_attempts = 0;

/**
 * @brief Devuelve el nombre del estado actual.
 * @return Cadena constante con el nombre del estado.
 */
const char* Fsm_GetStateName()
{
  switch (g_currentState)
  {
    case INICIO: return "INICIO";
    case MON_TEMP: return "MON_TEMP";
    case MON_HUMED: return "MON_HUMED";
    case MON_LUZ: return "MON_LUZ";
    case ALERTA_BLUE: return "ALERTA";
    case ALARMA_RED: return "ALARMA";
    default: return "UNKNOWN";
  }
}

/**
 * @brief Devuelve el número de intentos acumulados.
 * @return Cantidad de intentos de escalamiento.
 */
int Fsm_GetAttempts(){ return g_attempts; }

/* =====================================================
   CONTROL DE FLUJO
   ===================================================== */

/**
 * @brief Alternancia entre ciclos TEMP → HUM y TEMP → LUZ.
 */
static bool nextTempGoesToHum = true;

/**
 * @brief Evita múltiples incrementos de intento dentro del mismo estado ALERTA.
 */
static bool attemptCountedThisAlert = false;

/**
 * @brief Detecta flanco descendente del botón.
 * @return true si se detecta presión nueva.
 */
static bool Button_PressedEdge()
{
  static bool lastPressed = false;
  bool now = (digitalRead(BTN_PIN) == LOW);
  bool edge = (now && !lastPressed);
  lastPressed = now;
  return edge;
}

/* =====================================================
   CONDICIONES DE TRANSICIÓN
   ===================================================== */

/** @brief INICIO → MON_TEMP */
static bool trInicioToTemp(){ return Button_PressedEdge(); }

/** @brief MON_TEMP → ALERTA_BLUE */
static bool trTempToAlerta()
{
  float t = Sensors_GetTempC();
  return (!isnan(t) && t >= TEMP_ALERT_C);
}

/** @brief MON_TEMP → MON_LUZ */
static bool trTempToLuz()
{
  if (nextTempGoesToHum) return false;
  return Tasks_PopTempToLuzDone();
}

/** @brief MON_TEMP → MON_HUMED */
static bool trTempToHumed()
{
  if (!nextTempGoesToHum) return false;
  return Tasks_PopTempToHumDone();
}

/** @brief MON_LUZ → ALERTA_BLUE */
static bool trLuzToAlerta(){ return Sensors_IsDark(); }

/** @brief MON_LUZ → MON_TEMP */
static bool trLuzToTemp(){ return Tasks_PopLuzToTempDone(); }

/** @brief MON_HUMED → ALERTA_BLUE */
static bool trHumedToAlerta()
{
  float t = Sensors_GetTempC();
  return (!isnan(t) && t >= TEMP_ALERT_C);
}

/** @brief MON_HUMED → MON_TEMP */
static bool trHumedToTemp(){ return Tasks_PopHumToTempDone(); }

/** @brief ALERTA_BLUE → MON_TEMP */
static bool trAlertaToTemp(){ return Tasks_PopAlertaToTempDone(); }

/**
 * @brief ALERTA_BLUE → ALARMA_RED
 *
 * Escala a ALARMA si la temperatura ≥ TEMP_ALARM_C
 * durante 3 ciclos independientes.
 */
static bool trAlertaToAlarma()
{
  float t = Sensors_GetTempC();

  if (!attemptCountedThisAlert && !isnan(t) && t >= TEMP_ALARM_C)
  {
    g_attempts++;
    attemptCountedThisAlert = true;
  }
  return (g_attempts >= 3);
}

/** @brief ALARMA_RED → MON_TEMP */
static bool trAlarmaToTemp(){ return Tasks_PopAlarmaToTempDone(); }

/* =====================================================
   CONFIGURACIÓN DE LA FSM
   ===================================================== */

/**
 * @brief Configura todas las transiciones y acciones.
 */
static void setupStateMachine()
{
  sm.AddTransition(INICIO, MON_TEMP, trInicioToTemp);

  sm.AddTransition(MON_TEMP, ALERTA_BLUE, trTempToAlerta);
  sm.AddTransition(MON_TEMP, MON_LUZ, trTempToLuz);
  sm.AddTransition(MON_TEMP, MON_HUMED, trTempToHumed);

  sm.AddTransition(MON_LUZ, ALERTA_BLUE, trLuzToAlerta);
  sm.AddTransition(MON_LUZ, MON_TEMP, trLuzToTemp);

  sm.AddTransition(MON_HUMED, ALERTA_BLUE, trHumedToAlerta);
  sm.AddTransition(MON_HUMED, MON_TEMP, trHumedToTemp);

  sm.AddTransition(ALERTA_BLUE, ALARMA_RED, trAlertaToAlarma);
  sm.AddTransition(ALERTA_BLUE, MON_TEMP, trAlertaToTemp);

  sm.AddTransition(ALARMA_RED, MON_TEMP, trAlarmaToTemp);

  sm.SetOnEntering(INICIO, enterInicio);
  sm.SetOnLeaving(INICIO, leaveInicio);

  sm.SetOnEntering(MON_TEMP, enterMonTemp);
  sm.SetOnLeaving(MON_TEMP, leaveMonTemp);

  sm.SetOnEntering(MON_HUMED, enterMonHumed);
  sm.SetOnLeaving(MON_HUMED, leaveMonHumed);

  sm.SetOnEntering(MON_LUZ, enterMonLuz);
  sm.SetOnLeaving(MON_LUZ, leaveMonLuz);

  sm.SetOnEntering(ALERTA_BLUE, enterAlerta);
  sm.SetOnLeaving(ALERTA_BLUE, leaveAlerta);

  sm.SetOnEntering(ALARMA_RED, enterAlarma);
  sm.SetOnLeaving(ALARMA_RED, leaveAlarma);
}

/* =====================================================
   ACCIONES POR ESTADO
   ===================================================== */

/** @brief Acción al entrar en INICIO. */
static void enterInicio()
{
  g_currentState = INICIO;
  g_attempts = 0;
  nextTempGoesToHum = true;

  Tasks_StopMonitoringDisplay();
  Tasks_ClearAllTimeoutFlags();
  Tasks_StartInicioBlink();
}

/** @brief Acción al salir de INICIO. */
static void leaveInicio(){ Tasks_StopInicioBlink(); }

/** @brief Acción al entrar en MON_TEMP. */
static void enterMonTemp()
{
  g_currentState = MON_TEMP;
  Tasks_StartMonitoringDisplay();

  if (nextTempGoesToHum)
  {
    Tasks_StopTempToLuz();
    Tasks_StartTempToHum();
  }
  else
  {
    Tasks_StopTempToHum();
    Tasks_StartTempToLuz();
  }
}

/** @brief Acción al salir de MON_TEMP. */
static void leaveMonTemp()
{
  Tasks_StopTempToLuz();
  Tasks_StopTempToHum();
}

/** @brief Acción al entrar en MON_HUMED. */
static void enterMonHumed()
{
  g_currentState = MON_HUMED;
  Tasks_StartMonitoringDisplay();
  Tasks_StartHumToTemp();
  nextTempGoesToHum = false;
}

/** @brief Acción al salir de MON_HUMED. */
static void leaveMonHumed(){ Tasks_StopHumToTemp(); }

/** @brief Acción al entrar en MON_LUZ. */
static void enterMonLuz()
{
  g_currentState = MON_LUZ;
  Tasks_StartMonitoringDisplay();
  Tasks_StartLuzToTemp();
  nextTempGoesToHum = true;
}

/** @brief Acción al salir de MON_LUZ. */
static void leaveMonLuz(){ Tasks_StopLuzToTemp(); }

/** @brief Acción al entrar en ALERTA_BLUE. */
static void enterAlerta()
{
  g_currentState = ALERTA_BLUE;
  Tasks_StartMonitoringDisplay();
  attemptCountedThisAlert = false;

  Tasks_StartAlertaBlink();
  Tasks_StartAlertaToTemp();
}

/** @brief Acción al salir de ALERTA_BLUE. */
static void leaveAlerta()
{
  Tasks_StopAlertaBlink();
  Tasks_StopAlertaToTemp();
}

/** @brief Acción al entrar en ALARMA_RED. */
static void enterAlarma()
{
  g_currentState = ALARMA_RED;
  Tasks_StopMonitoringDisplay();

  Tasks_StartAlarmaBlink();
  Tasks_StartAlarmaToTemp();
}

/** @brief Acción al salir de ALARMA_RED. */
static void leaveAlarma()
{
  Tasks_StopAlarmaBlink();
  Tasks_StopAlarmaToTemp();
}

/* =====================================================
   API PÚBLICA
   ===================================================== */

/**
 * @brief Inicializa la FSM.
 */
void Fsm_Init()
{
  setupStateMachine();
  sm.SetState(INICIO, false, true);
}

/**
 * @brief Actualiza la máquina de estados.
 * Debe llamarse en cada ciclo del loop principal.
 */
void Fsm_Update()
{
  sm.Update();
}
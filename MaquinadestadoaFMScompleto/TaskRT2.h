#pragma once
#include <Arduino.h>

// =====================================================
//                    TASKS RT MODULE
// =====================================================
// - Blink tasks (Inicio/Alerta/Alarma)
// - Timeout tasks (Temp->Luz, Temp->Hum, etc.)
// - Debug periodic task
// =====================================================

void Tasks_Init();
void Tasks_UpdateAll();

void Tasks_StartDebug();

// -------------------- BLINK CONTROL --------------------
void Tasks_StartInicioBlink();
void Tasks_StopInicioBlink();

void Tasks_StartAlertaBlink();
void Tasks_StopAlertaBlink();

void Tasks_StartAlarmaBlink();
void Tasks_StopAlarmaBlink();

// -------------------- TIMEOUT START/STOP --------------------
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

// -------------------- TIMEOUT EVENTS (POP FLAGS) --------------------
bool Tasks_PopTempToLuzDone();
bool Tasks_PopTempToHumDone();
bool Tasks_PopHumToTempDone();
bool Tasks_PopLuzToTempDone();
bool Tasks_PopAlertaToTempDone();
bool Tasks_PopAlarmaToTempDone();

void Tasks_ClearAllTimeoutFlags();
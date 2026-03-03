#pragma once
#include <Arduino.h>

// =====================================================
//                 REAL-TIME TASK MODULE
//                 (AsyncTaskLib - Luis Llamas)
// =====================================================

void Tasks_Init();
void Tasks_UpdateAll();

// ------------------- BLINK CONTROL -------------------
void Tasks_StartInicioBlink();
void Tasks_StopInicioBlink();

void Tasks_StartAlertaBlink();
void Tasks_StopAlertaBlink();

void Tasks_StartAlarmaBlink();
void Tasks_StopAlarmaBlink();

// ------------------- TIMEOUT START/STOP -------------------
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

// ------------------- TIMEOUT EVENTS (POP FLAGS) -------------------
// "Pop" returns true ONCE when the timeout finishes, then clears the flag.
bool Tasks_PopTempToLuzDone();
bool Tasks_PopTempToHumDone();
bool Tasks_PopHumToTempDone();
bool Tasks_PopLuzToTempDone();
bool Tasks_PopAlertaToTempDone();
bool Tasks_PopAlarmaToTempDone();

// Optional helper
void Tasks_ClearAllTimeoutFlags();

// ------------------- DEBUG TASK -------------------
void Tasks_StartDebug();
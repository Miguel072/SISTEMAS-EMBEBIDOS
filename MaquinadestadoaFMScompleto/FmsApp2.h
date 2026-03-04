#pragma once
#include <Arduino.h>

// =====================================================
//                    FSM APP MODULE
// =====================================================
// Uses StateMachineLib (Luis Llamas)
// Uses tasks for timeouts (AsyncTaskLib) - no delay()
// =====================================================

void Fsm_Init();
void Fsm_Update();
#pragma once
#include <Arduino.h>

// =====================================================
//                    FSM APP MODULE
// =====================================================

void Fsm_Init();
void Fsm_Update();

// Used by TaskRT display
const char* Fsm_GetStateName();
int         Fsm_GetAttempts();
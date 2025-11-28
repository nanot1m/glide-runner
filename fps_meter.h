// Minimal FPS meter overlay with 30s history
#pragma once
#include <stdbool.h>

void FpsMeter_Init(void);
void FpsMeter_BeginFrame(void);
void FpsMeter_Draw(void);
void FpsMeter_SetEnabled(bool enabled);
bool FpsMeter_IsEnabled(void);

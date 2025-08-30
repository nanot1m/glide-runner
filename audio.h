// Simple audio wrapper for SFX and menu music
#pragma once
#include <stdbool.h>

void Audio_Init(void);
void Audio_Deinit(void);

// SFX helpers (play only if loaded)
void Audio_PlayHover(void);
void Audio_PlayMenuClick(void);
void Audio_PlayVictory(void);
void Audio_PlayDeath(void);
void Audio_PlayJump(void);

// Menu music control (fade handled inside)
void Audio_MenuMusicUpdate(bool inMenuScreens, float dt);

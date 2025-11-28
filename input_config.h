// Simple input configuration module
#pragma once
#include <stdbool.h>
#include "raylib.h"

typedef enum {
	ACT_ACTIVATE = 0,
	ACT_BACK,
	ACT_NAV_UP,
	ACT_NAV_DOWN,
	ACT_NAV_LEFT,
	ACT_NAV_RIGHT,
	ACT_LEFT,
	ACT_RIGHT,
	ACT_DOWN,
	ACT_JUMP,
	ACT__COUNT
} InputAction;

void InputConfig_Init(void); // loads config/input.cfg if present, else defaults
void InputConfig_UpdateTouch(void); // called each frame to feed virtual stick/touch buttons
bool InputDown(InputAction a); // any bound key is currently down
bool InputPressed(InputAction a); // any bound key was pressed this frame
const char *InputConfig_ActionLabel(InputAction a); // user-facing label
const char *InputConfig_PrimaryKeyName(InputAction a); // returns static string or NULL if none
const char *InputConfig_KeyName(int key); // returns name for supported keys or NULL
void InputConfig_SetSingleKey(InputAction a, int key); // replace bindings with a single key
void InputConfig_Save(void); // write current bindings to config/input.cfg

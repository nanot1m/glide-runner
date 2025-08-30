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
bool InputDown(InputAction a); // any bound key is currently down
bool InputPressed(InputAction a); // any bound key was pressed this frame

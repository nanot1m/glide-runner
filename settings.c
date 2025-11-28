#include "settings.h"
#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "fps_meter.h"
#include "input_config.h"
#include "ui.h"

typedef struct {
	const char *label;
	InputAction action; // invalid when action == ACT__COUNT
	bool isToggle;
} SettingItem;

static const SettingItem ITEMS[] = {
    {"FPS meter", ACT__COUNT, true},
    {"Jump", ACT_JUMP, false},
    {"Left", ACT_LEFT, false},
    {"Right", ACT_RIGHT, false},
    {"Down / Crouch", ACT_DOWN, false},
    {"Activate / Confirm", ACT_ACTIVATE, false},
    {"Back / Cancel", ACT_BACK, false},
};

static const UiListSpec SETTINGS_SPEC = {.startY = 90.0f, .stepY = 32.0f, .itemHeight = 28.0f, .fontSize = 22};
static int gSelected = 0;
static bool gWaitingForKey = false;
static InputAction gWaitingAction = ACT__COUNT;

static const char *LabelAt(int idx, void *ud) {
	(void)ud;
	const SettingItem *it = &ITEMS[idx];
	static char buf[128];
	if (it->isToggle) {
		snprintf(buf, sizeof(buf), "%s: %s", it->label, FpsMeter_IsEnabled() ? "On" : "Off");
	} else {
		const char *key = InputConfig_PrimaryKeyName(it->action);
		snprintf(buf, sizeof(buf), "%s: %s", it->label, key ? key : "Unbound");
	}
	return buf;
}

static void BeginRebind(InputAction a) {
	gWaitingForKey = true;
	gWaitingAction = a;
	InputGate_RequestBlockOnce();
}

static void HandleRebind(void) {
	int key = GetKeyPressed();
	if (key == 0) return;
	if (key == KEY_ESCAPE) {
		gWaitingForKey = false;
		gWaitingAction = ACT__COUNT;
		return;
	}
	const char *name = InputConfig_KeyName(key);
	if (!name) {
		Audio_PlayHover(); // indicate invalid choice
		return;
	}
	InputConfig_SetSingleKey(gWaitingAction, key);
	InputConfig_Save();
	gWaitingForKey = false;
	gWaitingAction = ACT__COUNT;
	Audio_PlayMenuClick();
}

void UpdateSettings(ScreenState *screen) {
	if (InputGate_BeginFrameBlocked()) return;

	if (gWaitingForKey) {
		HandleRebind();
		return;
	}

	if (InputPressed(ACT_BACK)) {
		InputGate_RequestBlockOnce();
		*screen = SCREEN_MENU;
		return;
	}

	bool activate = false;
	UiListHandle(&SETTINGS_SPEC, &gSelected, (int)(sizeof(ITEMS) / sizeof(ITEMS[0])), &activate);
	if (!activate) return;

	const SettingItem *it = &ITEMS[gSelected];
	if (it->isToggle) {
		bool next = !FpsMeter_IsEnabled();
		FpsMeter_SetEnabled(next);
		Audio_PlayMenuClick();
	} else {
		BeginRebind(it->action);
	}
}

void RenderSettings(void) {
	DrawText("SETTINGS", 20, 30, 32, DARKGRAY);
	UiListRenderCB(&SETTINGS_SPEC, gSelected, (int)(sizeof(ITEMS) / sizeof(ITEMS[0])), LabelAt, NULL, NULL, NULL, "Enter/Click: select | Esc/Back: return");
	if (gWaitingForKey && gWaitingAction != ACT__COUNT) {
		const char *label = InputConfig_ActionLabel(gWaitingAction);
		const char *msg = TextFormat("Press a key for %s (Esc to cancel)", label ? label : "action");
		int w = MeasureText(msg, 20);
		int x = GetScreenWidth() / 2 - w / 2;
		int y = GetScreenHeight() - 80;
		DrawRectangle(x - 10, y - 6, w + 20, 32, (Color){0, 0, 0, 180});
		DrawRectangleLines(x - 10, y - 6, w + 20, 32, (Color){120, 120, 120, 220});
		DrawText(msg, x, y, 20, RAYWHITE);
	}
}

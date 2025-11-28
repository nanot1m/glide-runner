#include "menu.h"
#include <stdio.h>
#include "audio.h"
#include "level.h"
#include "ui.h"

static const UiListSpec MENU_SPEC = {.startY = 70.0f, .stepY = 40.0f, .itemHeight = 28.0f, .fontSize = 24};

static const char *gMenuItems[MENU_OPTION_COUNT] = {
    "> Play level",
    "> Create new level",
    "> Edit existing level",
    "> Settings",
};

static const char *MenuLabelAtCB(int i, void *ud) {
	(void)ud;
	return gMenuItems[i];
}

void UpdateMenu(ScreenState *screen, int *selected) {
	int prev = *selected;
	bool activate = false;
	UiListHandle(&MENU_SPEC, selected, MENU_OPTION_COUNT, &activate);
	if (*selected != prev) { Audio_PlayHover(); }
	if (activate) {
		Audio_PlayMenuClick();
		if (*selected == MENU_EDIT_EXISTING) {
			*screen = SCREEN_SELECT_EDIT;
		} else if (*selected == MENU_CREATE_NEW) {
			gCreateNewRequested = true;
			snprintf(gLevelBinPath, sizeof(gLevelBinPath), "%s", LEVEL_FILE_BIN);
			*screen = SCREEN_LEVEL_EDITOR;
		} else if (*selected == MENU_PLAY_LEVEL) {
			*screen = SCREEN_SELECT_PLAY;
		} else if (*selected == MENU_SETTINGS) {
			*screen = SCREEN_SETTINGS;
		}
	}
}

void RenderMenu(int selected) {
	UiListRenderCB(&MENU_SPEC, selected, MENU_OPTION_COUNT, MenuLabelAtCB, NULL, "MAIN MENU", NULL, "Mouse: click items | WASD/Arrows: navigate | Enter/Space: select");
}

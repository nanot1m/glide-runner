#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "config.h"
#include "editor.h"
#include "fps_meter.h"
#include "game.h"
#include "input_config.h"
#include "level.h"
#include "menu.h"
#include "raylib.h"
#include "render.h"
#include "screens.h"
#include "settings.h"
#include "ui.h"

static const UiListSpec LIST_SPEC = {.startY = 70.0f, .stepY = 30.0f, .itemHeight = 24.0f, .fontSize = 24};

static LevelCatalog gCatalog;
static int gCatalogIndex = 0;

static const char *CatalogLabelAtCB(int i, void *ud) {
	LevelCatalog *c = (LevelCatalog *)ud;
	return c->items[i].baseName;
}

static void RenderLevelList(const char *title) {
	UiListRenderCB(&LIST_SPEC, gCatalogIndex, gCatalog.count, CatalogLabelAtCB, &gCatalog, title, "No levels found in ./levels", "UP/DOWN/W/S to select, ENTER/CLICK to confirm, ESC to back");
}

static inline void RestorePlayerPosFromTile(const LevelEditorState *ed, GameState *game) {
	Vector2 p = game->playerPos;
	FindTileWorldPos(ed, TILE_PLAYER, &p);
	game->playerPos = p;
}

static void ResetPlayerDefaults(GameState *game) {
	game->facingRight = true;
	game->playerPos = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};
	game->playerVel = (Vector2){0, 0};
	game->exitPos = (Vector2){WINDOW_WIDTH - SQUARE_SIZE * 2, WINDOW_HEIGHT - SQUARE_SIZE * 2};
	game->spriteScaleY = 1.0f;
	game->spriteScaleX = 1.0f;
	game->spriteRotation = 0.0f;
	game->hidden = false;
	game->groundSink = 0.0f;
}

static bool EnsureEditorLevel(GameState *game, bool *editorLoaded) {
	if (*editorLoaded) return true;
	EnsureLevelsDir();
	if (gCreateNewRequested) {
		CreateDefaultLevel(game, &editor);
		int nextIdx0 = FindNextLevelIndex();
		MakeLevelPathFromIndex(nextIdx0, gLevelBinPath, sizeof(gLevelBinPath));
		SaveLevelBinary(game, &editor);
		gCreateNewRequested = false;
		*editorLoaded = true;
		return true;
	}
	FILE *bf = fopen(gLevelBinPath, "rb");
	bool haveExisting = (bf != NULL);
	if (bf) fclose(bf);
	bool loaded = false;
	if (haveExisting) loaded = LoadLevelBinary(game, &editor);
	if (!loaded) { CreateDefaultLevel(game, &editor); }
	*editorLoaded = true;
	return true;
}

static bool EnsureGameLevel(GameState *game, bool *gameLevelLoaded) {
	if (*gameLevelLoaded) return true;
	bool loaded = LoadLevelBinary(game, &editor);
	if (!loaded) { CreateDefaultLevel(game, &editor); }
	*gameLevelLoaded = true;
	game->runTime = 0.0f;
	game->score = 0;
	Game_ResetVisuals(game);
	Game_ClearOutcome();
	return true;
}

static bool ScreenUsesFixedStep(ScreenState s) {
	return (s == SCREEN_TEST_PLAY || s == SCREEN_GAME_LEVEL);
}

static void UpdateScreen(ScreenState *screen, GameState *game, float dt, bool *editorLoaded, bool *gameLevelLoaded, int *menuSelected) {
	bool blockInput = InputGate_BeginFrameBlocked();
	switch (*screen) {
	case SCREEN_MENU:
		if (blockInput) break;
		UpdateMenu(screen, menuSelected);
		break;

	case SCREEN_SELECT_EDIT: {
		if (blockInput) break;
		ScanLevels(&gCatalog);
		if (InputPressed(ACT_BACK)) {
			InputGate_RequestBlockOnce();
			*screen = SCREEN_MENU;
			break;
		}
		bool activate = false;
		int prevIndex = gCatalogIndex;
		UiListHandle(&LIST_SPEC, &gCatalogIndex, gCatalog.count, &activate);
		if (gCatalogIndex != prevIndex) { Audio_PlayHover(); }
		if (activate && gCatalog.count > 0) {
			Audio_PlayMenuClick();
			snprintf(gLevelBinPath, sizeof(gLevelBinPath), "%s", gCatalog.items[gCatalogIndex].binPath);
			*editorLoaded = false;
			*screen = SCREEN_LEVEL_EDITOR;
		}
		break;
	}

	case SCREEN_SELECT_PLAY: {
		if (blockInput) break;
		ScanLevels(&gCatalog);
		if (InputPressed(ACT_BACK)) {
			InputGate_RequestBlockOnce();
			*screen = SCREEN_MENU;
			break;
		}
		bool activate = false;
		int prevIndex = gCatalogIndex;
		UiListHandle(&LIST_SPEC, &gCatalogIndex, gCatalog.count, &activate);
		if (gCatalogIndex != prevIndex) { Audio_PlayHover(); }
		if (activate && gCatalog.count > 0) {
			Audio_PlayMenuClick();
			snprintf(gLevelBinPath, sizeof(gLevelBinPath), "%s", gCatalog.items[gCatalogIndex].binPath);
			*gameLevelLoaded = false;
			*screen = SCREEN_GAME_LEVEL;
		}
		break;
	}

	case SCREEN_SETTINGS:
		if (blockInput) break;
		if (InputPressed(ACT_BACK)) {
			InputGate_RequestBlockOnce();
			*screen = SCREEN_MENU;
			break;
		}
		UpdateSettings(screen);
		break;

	case SCREEN_LEVEL_EDITOR:
		if (!EnsureEditorLevel(game, editorLoaded)) break;
		if (!blockInput) UpdateLevelEditor(screen, game);
		break;

	case SCREEN_TEST_PLAY: {
		if (!EnsureGameLevel(game, gameLevelLoaded)) break;
		if (blockInput) break;
		if (IsKeyPressed(KEY_ESCAPE)) {
			InputGate_RequestBlockOnce();
			RestorePlayerPosFromTile(&editor, game);
			*screen = SCREEN_LEVEL_EDITOR;
			Game_ClearOutcome();
			break;
		}
		UpdateGame(game, &editor, dt);
		if (Game_Death()) {
			RestorePlayerPosFromTile(&editor, game);
			*screen = SCREEN_LEVEL_EDITOR;
			Game_ClearOutcome();
			break;
		}
		break;
	}

	case SCREEN_GAME_LEVEL:
		if (!EnsureGameLevel(game, gameLevelLoaded)) break;
		if (blockInput) break;
		if (InputPressed(ACT_BACK)) {
			InputGate_RequestBlockOnce();
			*screen = SCREEN_MENU;
			break;
		}
		UpdateGame(game, &editor, dt);
		if (Game_Death()) {
			*screen = SCREEN_DEATH;
			break;
		}
		if (Game_Victory()) { *screen = SCREEN_VICTORY; }
		break;

	case SCREEN_DEATH:
		if (!blockInput) {
			if (InputPressed(ACT_ACTIVATE)) {
				InputGate_RequestBlockOnce();
				Game_ResetVisuals(game);
				Game_ClearOutcome();
				*gameLevelLoaded = false;
				*screen = SCREEN_GAME_LEVEL;
				break;
			} else if (InputPressed(ACT_BACK)) {
				InputGate_RequestBlockOnce();
				*screen = SCREEN_MENU;
				break;
			}
		}
		break;

	case SCREEN_VICTORY:
		if (!blockInput) {
			if (InputPressed(ACT_ACTIVATE)) {
				InputGate_RequestBlockOnce();
				Game_ClearOutcome();
				*gameLevelLoaded = false;
				*screen = SCREEN_GAME_LEVEL;
				break;
			} else if (InputPressed(ACT_BACK)) {
				InputGate_RequestBlockOnce();
				*screen = SCREEN_MENU;
				break;
			}
		}
		break;
	}
}

static void RenderScreen(ScreenState screen, GameState *game, float frameDt, int menuSelected) {
	switch (screen) {
	case SCREEN_MENU:
		RenderMenu(menuSelected);
		break;
	case SCREEN_SELECT_EDIT:
		RenderLevelList("Select a level to edit");
		break;
	case SCREEN_SELECT_PLAY:
		RenderLevelList("Select a level to play");
		break;
	case SCREEN_SETTINGS:
		RenderSettings();
		break;
	case SCREEN_LEVEL_EDITOR:
		RenderLevelEditor(game);
		break;
	case SCREEN_TEST_PLAY:
		RenderGame(game, &editor, frameDt);
		break;
	case SCREEN_GAME_LEVEL:
		RenderGame(game, &editor, frameDt);
		break;
	case SCREEN_DEATH:
		Render_DrawDust(frameDt);
		RenderDeath();
		break;
	case SCREEN_VICTORY:
		RenderVictory(game);
		break;
	}
}

int main(void) {
	// Request proper scaling on high-DPI displays and enable vsync
	SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);
	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Glide Runner");
	if (!Render_Init()) {
		fprintf(stderr, "Failed to load core textures. Falling back to primitive rendering.\n");
	}
	SetAudioStreamBufferSizeDefault(1024);
	InitAudioDevice();
	SetMasterVolume(0.8f);
	Audio_Init();
	InputConfig_Init();
	SetExitKey(0);
	SetTargetFPS((int)BASE_FPS);
	FpsMeter_Init();

	GameState game = {0};
	ResetPlayerDefaults(&game);

	ScreenState screen = SCREEN_MENU;
	ScreenState lastScreen = SCREEN_MENU;
	int menuSelected = 0;
	editor.cursor = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};

	bool editorLoaded = false;
	bool gameLevelLoaded = false;
	float accumulator = 0.0f;

	while (!WindowShouldClose()) {
		FpsMeter_BeginFrame();
		InputConfig_UpdateTouch();
		float frameDt = GetFrameTime();
		if (frameDt > 0.25f) frameDt = 0.25f;

		if (ScreenUsesFixedStep(screen)) {
			accumulator += frameDt;
			if (accumulator > 0.25f) accumulator = 0.25f;
			while (accumulator >= BASE_DT) {
				UpdateScreen(&screen, &game, BASE_DT, &editorLoaded, &gameLevelLoaded, &menuSelected);
				accumulator -= BASE_DT;
			}
		} else {
			accumulator = 0.0f;
			UpdateScreen(&screen, &game, frameDt, &editorLoaded, &gameLevelLoaded, &menuSelected);
		}

		BeginDrawing();
		ClearBackground(RAYWHITE);
		RenderScreen(screen, &game, frameDt, menuSelected);
		FpsMeter_Draw();
		EndDrawing();

		if (screen == SCREEN_MENU && lastScreen != SCREEN_MENU) {
			editorLoaded = false;
			gameLevelLoaded = false;
			Game_ClearOutcome();
			menuSelected = 0;
			Game_ResetVisuals(&game);
			ResetPlayerDefaults(&game);
		}

		bool inMenuScreens = (screen == SCREEN_MENU || screen == SCREEN_SELECT_EDIT || screen == SCREEN_SELECT_PLAY || screen == SCREEN_LEVEL_EDITOR || screen == SCREEN_SETTINGS);
		Audio_MenuMusicUpdate(inMenuScreens, frameDt);
		lastScreen = screen;
	}

	Audio_Deinit();
	CloseAudioDevice();
	Render_Deinit();
	CloseWindow();
	return 0;
}

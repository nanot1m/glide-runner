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
#include "settings.h"
#include "raylib.h"
#include "render.h"
#include "screens.h"
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

int main(void) {
	// Request proper scaling on high-DPI displays and enable vsync
	SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);
	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Glide Runner");
	Render_Init();
	SetAudioStreamBufferSizeDefault(1024);
	InitAudioDevice();
	SetMasterVolume(0.8f);
	Audio_Init();
	InputConfig_Init();
	SetExitKey(0);
	FpsMeter_Init();

	GameState game = {0};
	game.facingRight = true;
	game.playerPos = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};
	game.playerVel = (Vector2){0, 0};
	game.exitPos = (Vector2){WINDOW_WIDTH - SQUARE_SIZE * 2, WINDOW_HEIGHT - SQUARE_SIZE * 2};
	game.spriteScaleY = 1.0f;

	ScreenState screen = SCREEN_MENU;
	int menuSelected = 0;
	editor.cursor = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};

	bool editorLoaded = false;
	bool gameLevelLoaded = false;

	while (!WindowShouldClose()) {
		FpsMeter_BeginFrame();
		InputConfig_UpdateTouch();
		switch (screen) {
		case SCREEN_MENU:
			UpdateMenu(&screen, &menuSelected);
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderMenu(menuSelected);
			FpsMeter_Draw();
			EndDrawing();
			break;

		case SCREEN_SELECT_EDIT: {
			ScanLevels(&gCatalog);
			if (InputPressed(ACT_BACK)) {
				InputGate_RequestBlockOnce();
				screen = SCREEN_MENU;
				break;
			}
			bool activate = false;
			int prevIndex = gCatalogIndex;
			UiListHandle(&LIST_SPEC, &gCatalogIndex, gCatalog.count, &activate);
			if (gCatalogIndex != prevIndex) { Audio_PlayHover(); }
			if (activate && gCatalog.count > 0) {
				Audio_PlayMenuClick();
				snprintf(gLevelBinPath, sizeof(gLevelBinPath), "%s", gCatalog.items[gCatalogIndex].binPath);
				editorLoaded = false;
				screen = SCREEN_LEVEL_EDITOR;
			}
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderLevelList("Select a level to edit");
			FpsMeter_Draw();
			EndDrawing();
			break;
		}

		case SCREEN_SELECT_PLAY: {
			ScanLevels(&gCatalog);
			if (InputPressed(ACT_BACK)) {
				InputGate_RequestBlockOnce();
				screen = SCREEN_MENU;
				break;
			}
			bool activate = false;
			int prevIndex = gCatalogIndex;
			UiListHandle(&LIST_SPEC, &gCatalogIndex, gCatalog.count, &activate);
			if (gCatalogIndex != prevIndex) { Audio_PlayHover(); }
			if (activate && gCatalog.count > 0) {
				Audio_PlayMenuClick();
				snprintf(gLevelBinPath, sizeof(gLevelBinPath), "%s", gCatalog.items[gCatalogIndex].binPath);
				gameLevelLoaded = false;
				screen = SCREEN_GAME_LEVEL;
			}
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderLevelList("Select a level to play");
			FpsMeter_Draw();
			EndDrawing();
			break;
		}

		case SCREEN_SETTINGS: {
			if (InputPressed(ACT_BACK)) {
				InputGate_RequestBlockOnce();
				screen = SCREEN_MENU;
				break;
			}
			UpdateSettings(&screen);
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderSettings();
			FpsMeter_Draw();
			EndDrawing();
			break;
		}

		case SCREEN_LEVEL_EDITOR: {
			if (!editorLoaded) {
				EnsureLevelsDir();
				if (gCreateNewRequested) {
					CreateDefaultLevel(&game, &editor);
					int nextIdx0 = FindNextLevelIndex();
					MakeLevelPathFromIndex(nextIdx0, gLevelBinPath, sizeof(gLevelBinPath));
					SaveLevelBinary(&game, &editor);
					gCreateNewRequested = false;
					editorLoaded = true;
				} else {
					FILE *bf = fopen(gLevelBinPath, "rb");
					bool haveExisting = (bf != NULL);
					if (bf) fclose(bf);
					bool loaded = false;
					if (haveExisting) loaded = LoadLevelBinary(&game, &editor);
					if (!loaded) { CreateDefaultLevel(&game, &editor); }
					editorLoaded = true;
				}
			}
			UpdateLevelEditor(&screen, &game);
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderLevelEditor(&game);
			FpsMeter_Draw();
			EndDrawing();
			break;
		}

		case SCREEN_TEST_PLAY: {
			if (!gameLevelLoaded) {
				bool loaded = LoadLevelBinary(&game, &editor);
				if (!loaded) { CreateDefaultLevel(&game, &editor); }
				gameLevelLoaded = true;
				game.runTime = 0.0f;
				game.score = 0;
			}
			if (InputGate_BeginFrameBlocked()) {
				BeginDrawing();
				ClearBackground(RAYWHITE);
				RenderGame(&game);
				FpsMeter_Draw();
				EndDrawing();
				break;
			}
			if (IsKeyPressed(KEY_ESCAPE)) {
				InputGate_RequestBlockOnce();
				RestorePlayerPosFromTile(&editor, &game);
				screen = SCREEN_LEVEL_EDITOR;
				Game_ClearOutcome();
				break;
			}
			UpdateGame(&game);
			if (Game_Death()) {
				RestorePlayerPosFromTile(&editor, &game);
				screen = SCREEN_LEVEL_EDITOR;
				Game_ClearOutcome();
				break;
			}
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderGame(&game);
			FpsMeter_Draw();
			EndDrawing();
			break;
		}

		case SCREEN_GAME_LEVEL: {
			if (!gameLevelLoaded) {
				bool loaded = LoadLevelBinary(&game, &editor);
				if (!loaded) { CreateDefaultLevel(&game, &editor); }
				gameLevelLoaded = true;
				game.runTime = 0.0f;
				game.score = 0;
			}
			if (InputGate_BeginFrameBlocked()) {
				BeginDrawing();
				ClearBackground(RAYWHITE);
				RenderGame(&game);
				FpsMeter_Draw();
				EndDrawing();
				break;
			}
			if (InputPressed(ACT_BACK)) {
				InputGate_RequestBlockOnce();
				screen = SCREEN_MENU;
				break;
			}
			UpdateGame(&game);
			if (Game_Death()) {
				screen = SCREEN_DEATH;
				break;
			}
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderGame(&game);
			FpsMeter_Draw();
			EndDrawing();
			if (Game_Victory()) { screen = SCREEN_VICTORY; }
			break;
		}

		case SCREEN_DEATH: {
			// Handle input before drawing on web to avoid frame-edge loss
			if (!InputGate_BeginFrameBlocked()) {
				if (InputPressed(ACT_ACTIVATE)) {
					InputGate_RequestBlockOnce();
					Game_ClearOutcome();
					gameLevelLoaded = false;
					screen = SCREEN_GAME_LEVEL;
					break;
				} else if (InputPressed(ACT_BACK)) {
					InputGate_RequestBlockOnce();
					screen = SCREEN_MENU;
					break;
				}
			}
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderDeath();
			FpsMeter_Draw();
			EndDrawing();
			break;
		}

		case SCREEN_VICTORY: {
			// Handle input before drawing on web to avoid frame-edge loss
			if (!InputGate_BeginFrameBlocked()) {
				if (InputPressed(ACT_ACTIVATE)) {
					InputGate_RequestBlockOnce();
					Game_ClearOutcome();
					gameLevelLoaded = false;
					screen = SCREEN_GAME_LEVEL;
					break;
				} else if (InputPressed(ACT_BACK)) {
					InputGate_RequestBlockOnce();
					screen = SCREEN_MENU;
					break;
				}
			}
			BeginDrawing();
			ClearBackground(RAYWHITE);
			RenderVictory(&game);
			FpsMeter_Draw();
			EndDrawing();
			break;
		}
		}

		if (screen == SCREEN_MENU) {
			editorLoaded = false;
			gameLevelLoaded = false;
			Game_ClearOutcome();
		}

		bool inMenuScreens = (screen == SCREEN_MENU || screen == SCREEN_SELECT_EDIT || screen == SCREEN_SELECT_PLAY || screen == SCREEN_LEVEL_EDITOR);
		Audio_MenuMusicUpdate(inMenuScreens, GetFrameTime());
	}

	Audio_Deinit();
	CloseAudioDevice();
	Render_Deinit();
	CloseWindow();
	return 0;
}

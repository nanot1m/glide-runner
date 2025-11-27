#include "editor.h"
#include "input_config.h"
#include "raylib.h"
#include "render.h"
#include "ui.h"

static double arrowLastTime = 0;
static double arrowInterval = 0.2; // 200ms

void UpdateLevelEditor(ScreenState *screen, GameState *game) {
	if (InputGate_BeginFrameBlocked()) return;
	if (IsKeyPressed(KEY_TAB)) { editor.tool = (editor.tool + 1) % TOOL_COUNT; }
	if (IsKeyPressed(KEY_ONE)) editor.tool = TOOL_PLAYER;
	if (IsKeyPressed(KEY_TWO)) editor.tool = TOOL_ADD_BLOCK;
	if (IsKeyPressed(KEY_THREE)) editor.tool = TOOL_REMOVE_BLOCK;
	if (IsKeyPressed(KEY_FOUR)) editor.tool = TOOL_EXIT;
	if (IsKeyPressed(KEY_FIVE)) editor.tool = TOOL_LASER_TRAP;

	double now = GetTime();
	bool moved = false;
	if (now - arrowLastTime >= arrowInterval) {
		if (IsKeyDown(KEY_RIGHT)) {
			editor.cursor.x += SQUARE_SIZE;
			moved = true;
		}
		if (IsKeyDown(KEY_LEFT)) {
			editor.cursor.x -= SQUARE_SIZE;
			moved = true;
		}
		if (IsKeyDown(KEY_UP)) {
			editor.cursor.y -= SQUARE_SIZE;
			moved = true;
		}
		if (IsKeyDown(KEY_DOWN)) {
			editor.cursor.y += SQUARE_SIZE;
			moved = true;
		}
		if (moved) arrowLastTime = now;
	}

	Vector2 mouse = GetMousePosition();
	if (mouse.x >= 0 && mouse.x < WINDOW_WIDTH && mouse.y >= 0 && mouse.y < WINDOW_HEIGHT) { editor.cursor = SnapToGrid(mouse); }

	if (editor.cursor.x < 0) editor.cursor.x = 0;
	if (editor.cursor.x > WINDOW_WIDTH - SQUARE_SIZE) editor.cursor.x = WINDOW_WIDTH - SQUARE_SIZE;
	if (editor.cursor.y < 0) editor.cursor.y = 0;
	if (editor.cursor.y > WINDOW_HEIGHT - SQUARE_SIZE) editor.cursor.y = WINDOW_HEIGHT - SQUARE_SIZE;

	switch (editor.tool) {
	case TOOL_PLAYER:
		if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
			SetUniqueTile(&editor, cx, cy, TILE_PLAYER);
			game->playerPos = (Vector2){CellToWorld(cx), CellToWorld(cy)};
		}
		break;
	case TOOL_ADD_BLOCK:
		if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
			if (GetTile(&editor, cx, cy) != TILE_PLAYER && GetTile(&editor, cx, cy) != TILE_EXIT)
				SetTile(&editor, cx, cy, TILE_BLOCK);
		}
		break;
	case TOOL_REMOVE_BLOCK:
		if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
			TileType t = GetTile(&editor, cx, cy);
			if (t == TILE_BLOCK || t == TILE_LASER) SetTile(&editor, cx, cy, TILE_EMPTY);
		}
		break;
	case TOOL_EXIT:
		if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
			SetUniqueTile(&editor, cx, cy, TILE_EXIT);
			game->exitPos = (Vector2){CellToWorld(cx), CellToWorld(cy)};
		}
		break;
	case TOOL_LASER_TRAP:
		if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
			int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
			if (GetTile(&editor, cx, cy) == TILE_EMPTY) SetTile(&editor, cx, cy, TILE_LASER);
		}
		break;
	default:
		break;
	}

	if (InputPressed(ACT_BACK)) {
		SaveLevelBinary(game, &editor);
		InputGate_RequestBlockOnce();
		*screen = SCREEN_MENU;
	}

	if (InputPressed(ACT_ACTIVATE)) {
		SaveLevelBinary(game, &editor);
		*screen = SCREEN_TEST_PLAY;
	}
}

void RenderLevelEditor(const GameState *game) {
	for (int x = 0; x <= WINDOW_WIDTH; x += SQUARE_SIZE) DrawLine(x, 0, x, WINDOW_HEIGHT, LIGHTGRAY);
	for (int y = 0; y <= WINDOW_HEIGHT; y += SQUARE_SIZE) DrawLine(0, y, WINDOW_WIDTH, y, LIGHTGRAY);
	RenderTiles(&editor);
	// Draw player using actual AABB/sprite so it scales with SQUARE_SIZE changes
	RenderPlayer(game);
	DrawRectangleRec((Rectangle){game->exitPos.x, game->exitPos.y, (float)SQUARE_SIZE, (float)SQUARE_SIZE}, GREEN);
	DrawText("LEVEL EDITOR", 20, 20, 32, DARKGRAY);
	const char *toolNames[TOOL_COUNT] = {"Player Location", "Add Block", "Remove Block", "Level Exit", "Laser Trap"};
	DrawText(TextFormat("Tool: %s (Tab to switch)", toolNames[editor.tool]), 20, 60, 18, BLUE);
	DrawText("Arrows/Mouse: Move cursor | Space/Left Click: Use tool | 1-5: Tools (5=Laser) | ESC: Menu", 20, 85, 18, DARKGRAY);
	DrawRectangleLines((int)editor.cursor.x, (int)editor.cursor.y, SQUARE_SIZE, SQUARE_SIZE, RED);
}

#include "render.h"
#include <math.h>
#include "raylib.h"

Rectangle PlayerAABB(const GameState *g) {
	float h = g->crouching ? PLAYER_H_CROUCH : PLAYER_H;
	return (Rectangle){g->playerPos.x, g->playerPos.y, PLAYER_W, h};
}

Rectangle ExitAABB(const GameState *g) {
	return (Rectangle){g->exitPos.x, g->exitPos.y, (float)SQUARE_SIZE, (float)SQUARE_SIZE};
}

Rectangle TileRect(int cx, int cy) {
	return (Rectangle){CellToWorld(cx), CellToWorld(cy), (float)SQUARE_SIZE, (float)SQUARE_SIZE};
}

Rectangle LaserStripeRect(Vector2 laserPos) {
	return (Rectangle){laserPos.x, laserPos.y + LASER_STRIPE_OFFSET, (float)SQUARE_SIZE, LASER_STRIPE_THICKNESS};
}

Rectangle LaserCollisionRect(Vector2 laserPos) {
	// Reduce horizontal bounds by 1px while keeping the left edge fixed
	return (Rectangle){laserPos.x, laserPos.y + LASER_STRIPE_OFFSET, (float)SQUARE_SIZE - 1.0f, LASER_STRIPE_THICKNESS};
}

void RenderTiles(const LevelEditorState *ed) {
	for (int y = 0; y < GRID_ROWS; ++y) {
		for (int x = 0; x < GRID_COLS; ++x) {
			TileType t = ed->tiles[y][x];
			if (IsSolidTile(t)) {
				Rectangle r = TileRect(x, y);
				DrawRectangleRec(r, GRAY);
			} else if (IsHazardTile(t)) {
				Rectangle lr = LaserStripeRect((Vector2){CellToWorld(x), CellToWorld(y)});
				DrawRectangleRec(lr, RED);
			}
		}
	}
}

void DrawStats(const GameState *g) {
	DrawText(TextFormat("FPS: %d", GetFPS()), 10, 40, 20, RED);
	DrawText(TextFormat("Vel: (%.0f, %.0f)", g->playerVel.x, g->playerVel.y), 10, 70, 20, DARKGRAY);
	DrawText(g->onGround ? "Grounded" : "Air", 10, 100, 20, DARKGRAY);
	DrawText(TextFormat("Coy: %.2f  Buf: %.2f", g->coyoteTimer, g->jumpBufferTimer), 10, 130, 20, DARKGRAY);
}

// --- Player sprite rendering ---

static Texture2D gIdleTex = {0};
static Texture2D gRunTex = {0};

void Render_Init(void) {
	if (gIdleTex.id == 0) {
		gIdleTex = LoadTexture("assets/GlideManIdle.png");
		SetTextureFilter(gIdleTex, TEXTURE_FILTER_POINT);
	}
	if (gRunTex.id == 0) {
		gRunTex = LoadTexture("assets/GlideManRun.png");
		SetTextureFilter(gRunTex, TEXTURE_FILTER_POINT);
	}
}

void Render_Deinit(void) {
	if (gIdleTex.id != 0) {
		UnloadTexture(gIdleTex);
		gIdleTex.id = 0;
	}
	if (gRunTex.id != 0) {
		UnloadTexture(gRunTex);
		gRunTex.id = 0;
	}
}

void RenderPlayer(const GameState *g) {
	// Choose animation based on simple state: running vs idle
	const float speed = (float)fabsf(g->playerVel.x);
	const bool running = g->onGround && speed > 1.0f;
	const Texture2D tex = running ? gRunTex : gIdleTex;
	// Destination rectangle: width = SQUARE_SIZE; height reflects crouch (half height)
	Rectangle aabb = PlayerAABB(g);
	float dstW = (float)SQUARE_SIZE;
	// Scale height based on physics AABB vs standing height so crouch is half
	float heightScale = (float)(aabb.height / (float)PLAYER_H); // 1.0 standing, 0.5 crouched
	float dstH = (float)SQUARE_SIZE * heightScale;
	float dstX = aabb.x + (aabb.width - dstW) * 0.5f; // center horizontally over physics
	float dstY = (aabb.y + aabb.height) - dstH; // keep feet anchored
	Rectangle dst = (Rectangle){dstX, dstY, dstW, dstH};

	if (tex.id == 0) {
		// Fallback: draw a square at the sprite destination size
		DrawRectangleRec(dst, BLUE);
		return;
	}

	// Each sheet: 5 columns, 1 row, each 16x16
	const int cols = 5;
	const int fw = 16, fh = 16;
	const float fps = 10.0f; // animation speed
	int frame = (int)(g->runTime * fps) % cols;

	// Source rect, handle horizontal flip using negative width
	bool faceRight = g->facingRight;
	Rectangle src;
	if (faceRight) {
		src = (Rectangle){(float)(frame * fw), 0.0f, (float)fw, (float)fh};
	} else {
		src = (Rectangle){(float)((frame + 1) * fw), 0.0f, (float)-fw, (float)fh};
	}

	// Draw with no rotation, origin at top-left
	DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

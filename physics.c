#include "physics.h"
#include "config.h"
#include "level.h"
#include <math.h>

// Forward declaration from game.c/level.c if needed, but level.h has the helpers.
// We need access to the current level. level.h declares `gLevel` usually?
// Actually level.h declares `editor` but game.c had a static `gLevel`.
// We should probably expose the current level in level.h or pass it in.
// For now, let's assume we can use the helpers in level.h which use `gLevel` if we make it accessible,
// OR we rely on `BlockAtCell` which was static in game.c.
// Better approach: Move `BlockAtCell` to level.h/c or make it a helper here that calls level functions.

// `level.h` has `IsSolidTile` and `gLevel` is static in `game.c`.
// We need to access the tile data.
// Let's look at `level.h` again. It has `LevelEditorState editor`.
// But `game.c` used `const struct LevelEditorState *gLevel`.
// We should probably add a `Physics_SetLevel` or similar, or just access `editor` if that's the global state.
// However, `game.c` supports playing a level separate from the editor state potentially?
// Let's check `level.h` content again to be sure.

// Re-reading level.h from previous turn...
// It has `extern LevelEditorState editor;`
// It has `IsSolidTile`.
// It DOES NOT expose the current level pointer used by the game loop if it differs from `editor`.
// In `game.c`, `UpdateGame` takes `const struct LevelEditorState *level`.
// So we should pass the level to these functions or set a global in physics.c.
// Let's add `Physics_SetLevel` to `physics.h` and a static global in `physics.c`.

static const LevelEditorState *gPhysicsLevel = NULL;

void Physics_SetLevel(const LevelEditorState *level) {
	gPhysicsLevel = level;
}

bool Physics_BlockAtCell(int cx, int cy) {
	if (!gPhysicsLevel) return false;
	if (!InBoundsCell(cx, cy)) return true; // Out of bounds is solid
	return IsSolidTile(gPhysicsLevel->tiles[cy][cx]);
}

static bool BlockAtCell(int cx, int cy) {
	return Physics_BlockAtCell(cx, cy);
}

bool AABBOverlapsSolid(float x, float y, float w, float h) {
	if (!gPhysicsLevel) return false;
	// Check against per-tile solid collision rectangles
	Rectangle pr = (Rectangle){x, y, w, h};
	int left = WorldToCellX(x);
	int right = WorldToCellX(x + w - 0.001f);
	int top = WorldToCellY(y);
	int bottom = WorldToCellY(y + h - 0.001f);
	for (int cy = top; cy <= bottom; ++cy) {
		for (int cx = left; cx <= right; ++cx) {
			if (!InBoundsCell(cx, cy)) return true; // out of bounds treated as solid
			TileType t = gPhysicsLevel->tiles[cy][cx];
			if (!IsSolidTile(t)) continue;
			Rectangle tr = TileSolidCollisionRect(cx, cy, t);
			if (tr.width > 0.0f && tr.height > 0.0f && CheckCollisionRecs(pr, tr)) return true;
		}
	}
	return false;
}

void MoveEntity(Vector2 *pos, Vector2 *vel, float w, float h, float dt, bool *hitLeft, bool *hitRight, bool *hitTop, bool *hitBottom) {
	if (hitLeft) *hitLeft = false;
	if (hitRight) *hitRight = false;
	if (hitTop) *hitTop = false;
	if (hitBottom) *hitBottom = false;

	float halfW = w * 0.5f;
	float halfH = h * 0.5f;

	// X Axis
	float dx = vel->x * dt;
	if (dx != 0.0f) {
		float nextX = pos->x + dx;
		float leadingX = (dx > 0) ? (nextX + halfW) : (nextX - halfW);
		int startCellY = WorldToCellY(pos->y - halfH + 0.01f);
		int endCellY = WorldToCellY(pos->y + halfH - 0.01f);
		int cellX = WorldToCellX(leadingX);

		bool collision = false;
		for (int cy = startCellY; cy <= endCellY; ++cy) {
			if (BlockAtCell(cellX, cy)) {
				collision = true;
				break;
			}
		}

		if (collision) {
			if (dx > 0) {
				if (hitRight) *hitRight = true;
				pos->x = CellToWorld(cellX) - halfW - 0.001f;
				vel->x = 0;
			} else {
				if (hitLeft) *hitLeft = true;
				pos->x = CellToWorld(cellX + 1) + halfW + 0.001f;
				vel->x = 0;
			}
		} else {
			pos->x = nextX;
		}
	}

	// Y Axis
	float dy = vel->y * dt;
	if (dy != 0.0f) {
		float nextY = pos->y + dy;
		float leadingY = (dy > 0) ? (nextY + halfH) : (nextY - halfH);
		int startCellX = WorldToCellX(pos->x - halfW + 0.01f);
		int endCellX = WorldToCellX(pos->x + halfW - 0.01f);
		int cellY = WorldToCellY(leadingY);

		bool collision = false;
		for (int cx = startCellX; cx <= endCellX; ++cx) {
			if (BlockAtCell(cx, cellY)) {
				collision = true;
				break;
			}
		}

		if (collision) {
			if (dy > 0) {
				if (hitBottom) *hitBottom = true;
				pos->y = CellToWorld(cellY) - halfH - 0.001f;
				vel->y = 0;
			} else {
				if (hitTop) *hitTop = true;
				pos->y = CellToWorld(cellY + 1) + halfH + 0.001f;
				vel->y = 0;
			}
		} else {
			pos->y = nextY;
		}
	}
}

void PushEntityOutOfSolids(Vector2 *pos, Vector2 *vel, float w, float h) {
	float left = pos->x - w * 0.5f;
	float top = pos->y - h * 0.5f;
	if (!AABBOverlapsSolid(left, top, w, h)) return;

	float bestDx = 0.0f, bestDy = 0.0f;
	bool haveDx = false, haveDy = false;

	// Try pushing horizontally
	float dirX = (vel->x >= 0.0f) ? -1.0f : 1.0f;
	int maxHorizontal = (int)ceilf(w) + 2;
	for (int i = 0; i < maxHorizontal; ++i) {
		float dx = dirX * (float)(i + 1);
		float candidateLeft = left + dx;
		if (!AABBOverlapsSolid(candidateLeft, top, w, h)) {
			bestDx = dx;
			haveDx = true;
			break;
		}
	}

	// Try pushing vertically
	float dirY = (vel->y >= 0.0f) ? -1.0f : 1.0f;
	int maxVertical = (int)ceilf(h) + 2;
	for (int i = 0; i < maxVertical; ++i) {
		float dy = dirY * (float)(i + 1);
		float candidateTop = top + dy;
		if (!AABBOverlapsSolid(left, candidateTop, w, h)) {
			bestDy = dy;
			haveDy = true;
			break;
		}
	}

	if (haveDy && (!haveDx || fabsf(bestDy) <= fabsf(bestDx))) {
		pos->y += bestDy;
		vel->y = 0.0f;
	} else if (haveDx) {
		pos->x += bestDx;
		vel->x = 0.0f;
	}
}

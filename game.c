#include "game.h"
#include <math.h>
#include <string.h>
#include "audio.h"
#include "input_config.h"
#include "level.h"
#include "render.h"
#include "ui.h"

static bool victory = false;
static bool death = false;
static float deathAnimTimer = 0.0f;

static const struct LevelEditorState *gLevel = NULL;
static const float kDeathAnimDuration = 0.7f;
enum { kMaxSpawners = 64, kMaxEnemies = 128 };
static const float kSpawnInterval = (float)ROGUE_SPAWN_INTERVAL_MS / 1000.0f;
static const float kEnemyW = ROGUE_ENEMY_W;
static const float kEnemyH = ROGUE_ENEMY_H;

typedef struct EnemySpawner {
	Vector2 pos;
	float timer;
} EnemySpawner;

typedef struct Enemy {
	Vector2 pos;
	Vector2 vel;
	bool active;
} Enemy;

static EnemySpawner gSpawners[kMaxSpawners];
static int gSpawnerCount = 0;
static Enemy gEnemies[kMaxEnemies];

typedef struct {
	float w;
	float h;
} WarriorClipDims;

static const WarriorClipDims kDimsIdle = {20.0f, 33.0f};
static const WarriorClipDims kDimsRun = {30.0f, 31.0f};
static const WarriorClipDims kDimsDeath = {44.0f, 43.0f};
static const WarriorClipDims kDimsHurt = {23.0f, 32.0f};
static const WarriorClipDims kDimsJump = {20.0f, 33.0f};
static const WarriorClipDims kDimsUpToFall = {21.0f, 32.0f};
static const WarriorClipDims kDimsFall = {21.0f, 34.0f};
static const WarriorClipDims kDimsWall = {16.0f, 36.0f};
static const WarriorClipDims kDimsCrouch = {21.0f, 24.0f};
static const WarriorClipDims kDimsDash = {35.0f, 31.0f};
static const WarriorClipDims kDimsSlide = {36.0f, 24.0f};
static const WarriorClipDims kDimsLadder = {20.0f, 40.0f};
// Wall / ledge helpers
static const float kWallStickBuffer = 0.10f;
static const float kLedgeHandSampleFrac = 0.35f; // fraction down from head to sample for hand position
static const float kLedgeSnapPadding = 2.0f; // offset to keep the player clear of the ledge when hanging

static bool BlockAtCell(int cx, int cy); // forward

static bool AABBOverlapsSolid(float x, float y, float w, float h) {
	// Check against per-tile solid collision rectangles
	Rectangle pr = (Rectangle){x, y, w, h};
	int left = WorldToCellX(x);
	int right = WorldToCellX(x + w - 0.001f);
	int top = WorldToCellY(y);
	int bottom = WorldToCellY(y + h - 0.001f);
	for (int cy = top; cy <= bottom; ++cy) {
		for (int cx = left; cx <= right; ++cx) {
			if (!InBoundsCell(cx, cy)) return true; // out of bounds treated as solid
			TileType t = gLevel->tiles[cy][cx];
			if (!IsSolidTile(t)) continue;
			Rectangle tr = TileSolidCollisionRect(cx, cy, t);
			if (tr.width > 0.0f && tr.height > 0.0f && CheckCollisionRecs(pr, tr)) return true;
		}
	}
	return false;
}

static Rectangle EnemyAABB(const Enemy *e) {
	return (Rectangle){e->pos.x, e->pos.y, kEnemyW, kEnemyH};
}

// Returns new position after moving along one axis while clamping to solids. Sets hit flags on collision.
static float MoveAxisAgainstTiles(float pos, float other, float halfAlong, float halfOther, float vel, float dt, bool vertical, bool *hitNeg, bool *hitPos) {
	const float eps = 0.001f;
	float dist = vel * dt;
	if (dist == 0.0f) return pos;
	float rectLeft = vertical ? (other - halfOther) : (pos - halfAlong);
	float rectTop = vertical ? (pos - halfAlong) : (other - halfOther);
	float rectW = vertical ? (halfOther * 2.0f) : (halfAlong * 2.0f);
	float rectH = vertical ? (halfAlong * 2.0f) : (halfOther * 2.0f);
	bool overlapping = AABBOverlapsSolid(rectLeft + eps, rectTop + eps, rectW - eps * 2.0f, rectH - eps * 2.0f);
	float startEdge = pos + ((dist > 0.0f) ? halfAlong : -halfAlong);
	float endEdge = startEdge + dist;
	int otherMin = vertical ? WorldToCellX((other - halfOther) + eps) : WorldToCellY((other - halfOther) + eps);
	int otherMax = vertical ? WorldToCellX((other + halfOther) - eps) : WorldToCellY((other + halfOther) - eps);
	float allowed = dist;

	if (dist > 0.0f) {
		int first = vertical ? WorldToCellY(startEdge + eps) : WorldToCellX(startEdge + eps);
		int last = vertical ? WorldToCellY(endEdge + eps) : WorldToCellX(endEdge + eps);
		for (int axisCell = first; axisCell <= last; ++axisCell) {
			for (int o = otherMin; o <= otherMax; ++o) {
				int cx = vertical ? o : axisCell;
				int cy = vertical ? axisCell : o;
				if (!BlockAtCell(cx, cy)) continue;
				float boundary = CellToWorld(axisCell); // top or left boundary
				float candidate = boundary - startEdge - eps;
				if (candidate < allowed) {
					if (candidate < 0.0f && !overlapping) candidate = 0.0f; // avoid pulling back unless resolving existing overlap
					allowed = candidate;
					if (hitPos) *hitPos = true;
				}
			}
		}
	} else { // dist < 0
		int first = vertical ? WorldToCellY(startEdge - eps) : WorldToCellX(startEdge - eps);
		int last = vertical ? WorldToCellY(endEdge - eps) : WorldToCellX(endEdge - eps);
		for (int axisCell = first; axisCell >= last; --axisCell) {
			for (int o = otherMin; o <= otherMax; ++o) {
				int cx = vertical ? o : axisCell;
				int cy = vertical ? axisCell : o;
				if (!BlockAtCell(cx, cy)) continue;
				float boundary = CellToWorld(axisCell + 1); // bottom or right boundary
				float candidate = boundary - startEdge + eps; // negative
				if (candidate > allowed) {
					if (candidate > 0.0f && !overlapping) candidate = 0.0f; // avoid pulling back unless resolving existing overlap
					allowed = candidate;
					if (hitNeg) *hitNeg = true;
				}
			}
		}
	}

	pos += allowed;
	if (allowed != dist) {
		// Snap flush to the blocker to avoid re-penetration on next frame.
		if (allowed > dist && hitNeg) *hitNeg = true;
		if (allowed < dist && hitPos) *hitPos = true;
	}
	return pos;
}

// Move an AABB against tile solids, clamping to the first obstacle per axis.
// Returns contact flags for the axis-aligned faces.
static void MovePlayerWithCollisions(GameState *game, float w, float h, float dt,
                                     bool *hitLeft, bool *hitRight, bool *hitTop, bool *hitBottom) {
	float halfW = w * 0.5f;
	float halfH = h * 0.5f;
	float posX = game->playerPos.x;
	float posY = game->playerPos.y;

	if (hitLeft) *hitLeft = false;
	if (hitRight) *hitRight = false;
	if (hitTop) *hitTop = false;
	if (hitBottom) *hitBottom = false;

	posX = MoveAxisAgainstTiles(posX, posY, halfW, halfH, game->playerVel.x, dt, false, hitLeft, hitRight);
	if ((hitLeft && *hitLeft) || (hitRight && *hitRight)) game->playerVel.x = 0.0f;
	posY = MoveAxisAgainstTiles(posY, posX, halfH, halfW, game->playerVel.y, dt, true, hitTop, hitBottom);
	if ((hitTop && *hitTop) || (hitBottom && *hitBottom)) game->playerVel.y = 0.0f;

	game->playerPos.x = posX;
	game->playerPos.y = posY;
}

static void Rogue_Clear(void) {
	gSpawnerCount = 0;
	memset(gSpawners, 0, sizeof(gSpawners));
	memset(gEnemies, 0, sizeof(gEnemies));
}

static void Rogue_BuildFromLevel(const struct LevelEditorState *level) {
	Rogue_Clear();
	if (!level) return;
	for (int y = 0; y < GRID_ROWS; ++y) {
		for (int x = 0; x < GRID_COLS; ++x) {
			if (!IsSpawnerTile(level->tiles[y][x])) continue;
			if (gSpawnerCount >= kMaxSpawners) return;
			EnemySpawner *s = &gSpawners[gSpawnerCount++];
			s->pos = (Vector2){CellToWorld(x), CellToWorld(y)};
			s->timer = 0.0f;
		}
	}
}

static void Rogue_SpawnEnemy(Vector2 spawnPos) {
	if (AABBOverlapsSolid(spawnPos.x, spawnPos.y, kEnemyW, kEnemyH)) return;
	for (int i = 0; i < kMaxEnemies; ++i) {
		Enemy *e = &gEnemies[i];
		if (e->active) continue;
		e->active = true;
		e->pos = spawnPos;
		e->vel = (Vector2){0, 0};
		return;
	}
}

static void Rogue_ResolveEnemyEnemyCollisions(void) {
	for (int i = 0; i < kMaxEnemies; ++i) {
		Enemy *a = &gEnemies[i];
		if (!a->active) continue;
		for (int j = i + 1; j < kMaxEnemies; ++j) {
			Enemy *b = &gEnemies[j];
			if (!b->active) continue;
			Rectangle ra = EnemyAABB(a);
			Rectangle rb = EnemyAABB(b);
			if (!CheckCollisionRecs(ra, rb)) continue;

			float penX = fminf(ra.x + ra.width - rb.x, rb.x + rb.width - ra.x);
			float penY = fminf(ra.y + ra.height - rb.y, rb.y + rb.height - ra.y);
			Vector2 pushA = {0, 0};
			Vector2 pushB = {0, 0};
			if (penX < penY) {
				float dir = (ra.x < rb.x) ? -1.0f : 1.0f;
				float amt = penX * 0.5f;
				pushA.x = dir * amt;
				pushB.x = -dir * amt;
				a->vel.x = 0.0f;
				b->vel.x = 0.0f;
			} else {
				float dir = (ra.y < rb.y) ? -1.0f : 1.0f;
				float amt = penY * 0.5f;
				pushA.y = dir * amt;
				pushB.y = -dir * amt;
				a->vel.y = 0.0f;
				b->vel.y = 0.0f;
			}

			Enemy movedA = *a;
			Enemy movedB = *b;
			movedA.pos.x += pushA.x;
			movedA.pos.y += pushA.y;
			movedB.pos.x += pushB.x;
			movedB.pos.y += pushB.y;
			bool blockedA = AABBOverlapsSolid(movedA.pos.x, movedA.pos.y, kEnemyW, kEnemyH);
			bool blockedB = AABBOverlapsSolid(movedB.pos.x, movedB.pos.y, kEnemyW, kEnemyH);
			if (!blockedA) a->pos = movedA.pos;
			if (!blockedB) b->pos = movedB.pos;
		}
	}
}

static void ResolveAxis(float *pos, float *vel, float other, float w, float h, bool vertical, float dt, bool xIsCenter, bool yIsCenter) {
	float remaining = *vel * dt;
	if (remaining == 0.0f) return;
	float sign = (remaining > 0) ? 1.0f : -1.0f;
	while (remaining != 0.0f) {
		float step = remaining;
		float maxStep = (float)SQUARE_SIZE - 1.0f;
		if (step > maxStep) step = maxStep;
		if (step < -maxStep) step = -maxStep;
		float newPos = *pos + step;
		float x = vertical ? other : newPos;
		if (xIsCenter) x -= w * 0.5f;
		float y = vertical ? newPos : other;
		if (yIsCenter) y -= h * 0.5f;
		if (AABBOverlapsSolid(x, y, w, h)) {
			int pixels = (int)fabsf(step);
			for (int i = 0; i < pixels; ++i) {
				newPos = *pos + sign;
				x = vertical ? other : newPos;
				if (xIsCenter) x -= w * 0.5f;
				y = vertical ? newPos : other;
				if (yIsCenter) y -= h * 0.5f;
				if (AABBOverlapsSolid(x, y, w, h)) {
					*vel = 0.0f;
					return;
				}
				*pos = newPos;
			}
		} else {
			*pos = newPos;
		}
		remaining -= step;
	}
}

// If a collision still leaves the player overlapping a solid block, nudge them back out
static void PushPlayerOutOfSolids(GameState *game, float *centerX, float *y, float w, float h, bool preferVertical) {
	float left = *centerX - w * 0.5f;
	float top = *y - h * 0.5f;
	if (!AABBOverlapsSolid(left, top, w, h)) return;

	float bestDx = 0.0f, bestDy = 0.0f;
	bool haveDx = false, haveDy = false;

	float dirX = (game->playerVel.x >= 0.0f) ? -1.0f : 1.0f;
	if (game->playerVel.x == 0.0f) dirX = game->facingRight ? -1.0f : 1.0f;
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

	float dirY = (game->playerVel.y >= 0.0f) ? -1.0f : 1.0f;
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

	if (preferVertical && haveDy) {
		*y += bestDy;
		game->playerVel.y = 0.0f;
		return;
	}
	if (haveDx && (!haveDy || fabsf(bestDx) <= fabsf(bestDy))) {
		*centerX += bestDx;
		game->playerVel.x = 0.0f;
	} else if (haveDy) {
		*y += bestDy;
		game->playerVel.y = 0.0f;
	}
}

static bool PlayerTouchesHazard(const GameState *g) {
	Rectangle pb = PlayerAABB(g);
	for (int y = 0; y < GRID_ROWS; ++y)
		for (int x = 0; x < GRID_COLS; ++x) {
			TileType t = gLevel->tiles[y][x];
			if (!IsHazardTile(t)) continue;
			Vector2 lp = (Vector2){CellToWorld(x), CellToWorld(y)};
			Rectangle lr = LaserCollisionRect(lp);
			if (CheckCollisionRecs(pb, lr)) return true;
		}
	return false;
}

// Coarse solid check used for ground probing across tiles below the player
static bool BlockAtCell(int cx, int cy) {
	if (!InBoundsCell(cx, cy)) return true;
	return IsSolidTile(gLevel->tiles[cy][cx]);
}

static bool TouchingWall(const GameState *g, bool leftSide, float aabbW, float aabbH) {
	float left = g->playerPos.x - aabbW * 0.5f;
	float x = leftSide ? (left - 1.0f) : (left + aabbW);
	float top = g->playerPos.y - aabbH * 0.5f;
	return AABBOverlapsSolid(x, top, 1.0f, aabbH);
}

static bool CanGrabLedge(const GameState *g, bool leftSide, float aabbW, float aabbH, float *outSnapY) {
	// Only allow ledge grabs while falling or near-neutral vertical speed
	if (g->playerVel.y < -40.0f) return false;

	float top = g->playerPos.y - aabbH * 0.5f;
	float handY = top + aabbH * kLedgeHandSampleFrac;
	float checkX = leftSide ? (g->playerPos.x - aabbW * 0.5f - 1.0f) : (g->playerPos.x + aabbW * 0.5f + 1.0f);
	int wallCellX = WorldToCellX(checkX);
	int handCellY = WorldToCellY(handY);
	int aboveCellY = handCellY - 1;
	float blockTop = CellToWorld(handCellY);

	// Need a solid tile for the hands, empty space above it, and be close to the top edge.
	if (!BlockAtCell(wallCellX, handCellY)) return false;
	if (BlockAtCell(wallCellX, aboveCellY)) return false;
	if (top > blockTop + 6.0f) return false; // too far below the ledge top to snap
	float maxHandY = blockTop + (float)SQUARE_SIZE * 0.7f;
	if (handY > maxHandY) return false;

	if (outSnapY) {
		*outSnapY = blockTop + kLedgeSnapPadding + aabbH * 0.5f;
	}
	return true;
}

static WarriorClipDims WarriorDimsForState(const GameState *g) {
	float speed = fabsf(g->playerVel.x);
	float maxSpeedXNow = g->crouching ? MAX_SPEED_X_CROUCH : MAX_SPEED_X;
	bool running = g->onGround && speed > (0.4f * maxSpeedXNow);
	bool falling = !g->onGround && g->playerVel.y > 80.0f;
	bool rising = !g->onGround && g->playerVel.y < -60.0f;
	bool transitioning = !g->onGround && !rising && !falling;
	bool dying = Game_IsDying();
	bool hurt = g->hurtTimer > 0.0f;

	if (dying) return kDimsDeath;
	if (hurt) return kDimsHurt;
	if (g->animLadder) return kDimsLadder;
	if (g->wallSliding) return kDimsWall;
	if (g->animSlide) return kDimsSlide;
	if (g->animDash) return kDimsDash;
	if (g->crouchAnimDir != 0) return kDimsCrouch;
	if (!g->onGround) {
		if (rising)
			return kDimsJump;
		else if (transitioning)
			return kDimsUpToFall;
		else
			return kDimsFall;
	}
	if (g->crouching) return kDimsCrouch;
	if (running) return kDimsRun;
	return kDimsIdle;
}

static WarriorClipDims WarriorDimsForCrouchState(const GameState *g, bool crouching) {
	GameState tmp = *g;
	tmp.crouching = crouching;
	tmp.crouchAnimDir = 0;
	return WarriorDimsForState(&tmp);
}

static void Rogue_UpdateSpawners(float dt) {
	if (kSpawnInterval <= 0.0f) return;
	for (int i = 0; i < gSpawnerCount; ++i) {
		EnemySpawner *s = &gSpawners[i];
		s->timer -= dt;
		while (s->timer <= 0.0f) {
			Vector2 spawnPos = (Vector2){
			    s->pos.x + ((float)SQUARE_SIZE - kEnemyW) * 0.5f,
			    s->pos.y + ((float)SQUARE_SIZE - kEnemyH)};
			Rogue_SpawnEnemy(spawnPos);
			s->timer += kSpawnInterval;
		}
	}
}

static void Rogue_UpdateEnemies(GameState *game, float dt) {
	for (int i = 0; i < kMaxEnemies; ++i) {
		Enemy *e = &gEnemies[i];
		if (!e->active) continue;
		float playerMidX = game->playerPos.x;
		float enemyMidX = e->pos.x + kEnemyW * 0.5f;
		float dir = (playerMidX >= enemyMidX) ? 1.0f : -1.0f;
		e->vel.x = dir * ROGUE_ENEMY_SPEED;
		e->vel.y += GRAVITY * dt;
		if (e->vel.y > ROGUE_ENEMY_MAX_FALL) e->vel.y = ROGUE_ENEMY_MAX_FALL;

		float newX = e->pos.x;
		float newY = e->pos.y;
		ResolveAxis(&newX, &e->vel.x, newY, kEnemyW, kEnemyH, false, dt, false, false);
		ResolveAxis(&newY, &e->vel.y, newX, kEnemyW, kEnemyH, true, dt, false, false);

		if (newX < 0.0f) {
			newX = 0.0f;
			e->vel.x = 0.0f;
		}
		if (newX > WINDOW_WIDTH - kEnemyW) {
			newX = WINDOW_WIDTH - kEnemyW;
			e->vel.x = 0.0f;
		}
		if (newY > WINDOW_HEIGHT - kEnemyH) {
			newY = WINDOW_HEIGHT - kEnemyH;
			e->vel.y = 0.0f;
		}
		e->pos.x = newX;
		e->pos.y = newY;

		if (e->pos.y > WINDOW_HEIGHT + kEnemyH * 2.0f) {
			e->active = false;
		}
	}
	Rogue_ResolveEnemyEnemyCollisions();
}

static void TriggerDeath(GameState *game) {
	if (death) return;
	death = true;
	deathAnimTimer = kDeathAnimDuration;
	game->spriteRotation = 0.0f;
	game->hidden = false;
	game->hurtTimer = 0.0f;
	game->crouchAnimTime = 0.0f;
	game->crouchAnimDir = 0;
	Audio_PlayDeath();
	Render_SpawnDeathExplosion(game);
}

static void Rogue_HandleEnemyPlayerCollisions(GameState *game) {
	if (death) return;
	Rectangle pb = PlayerAABB(game);
	float playerBottom = pb.y + pb.height;
	for (int i = 0; i < kMaxEnemies; ++i) {
		Enemy *e = &gEnemies[i];
		if (!e->active) continue;
		Rectangle eb = EnemyAABB(e);
		if (!CheckCollisionRecs(pb, eb)) continue;
		float enemyTop = eb.y;
		bool stomping = (game->playerVel.y > 0.0f) && (playerBottom <= enemyTop + ROGUE_STOMP_GRACE);
		if (stomping) {
			e->active = false;
			game->playerVel.y = ROGUE_STOMP_BOUNCE_SPEED;
			game->onGround = false;
			game->coyoteTimer = 0.0f;
			game->jumpBufferTimer = 0.0f;
			Render_SpawnLandDust(game);
		} else {
			TriggerDeath(game);
			return;
		}
	}
}

static void Rogue_RenderEnemies(void) {
	for (int i = 0; i < kMaxEnemies; ++i) {
		const Enemy *e = &gEnemies[i];
		if (!e->active) continue;
		Rectangle r = EnemyAABB(e);
		Color body = (Color){40, 40, 70, 255};
		Color outline = (Color){15, 15, 25, 255};
		DrawRectangleRounded(r, 0.3f, 6, body);
		DrawRectangleLinesEx(r, 2.0f, outline);
		Vector2 eyeL = (Vector2){r.x + r.width * 0.38f, r.y + r.height * 0.4f};
		Vector2 eyeR = (Vector2){r.x + r.width * 0.62f, eyeL.y};
		DrawCircleV(eyeL, 3.0f, WHITE);
		DrawCircleV(eyeR, 3.0f, WHITE);
		DrawCircleV(eyeL, 1.5f, outline);
		DrawCircleV(eyeR, 1.5f, outline);
#if DEBUG_DRAW_BOUNDS
		DrawRectangleLinesEx(r, 1.0f, YELLOW);
#endif
	}
}

static void Rogue_Update(GameState *game, float dt) {
	Rogue_UpdateSpawners(dt);
	Rogue_UpdateEnemies(game, dt);
	Rogue_HandleEnemyPlayerCollisions(game);
}

void UpdateGame(GameState *game, const struct LevelEditorState *level, float dt) {
	gLevel = level;
	if (death) {
		if (deathAnimTimer > 0.0f) {
			deathAnimTimer -= dt;
			float t = 1.0f - (deathAnimTimer / kDeathAnimDuration);
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;
			game->spriteRotation = 90.0f * t;
			// Drift with momentum and gravity
			game->playerVel.y += GRAVITY * GRAVITY_FALL_MULT * dt;
			if (deathAnimTimer < 0.2f) game->hidden = true;
			if (deathAnimTimer < 0.0f) deathAnimTimer = 0.0f;
		}
		float aabbW = 0.0f, aabbH = 0.0f;
		Game_CurrentAABBDims(game, &aabbW, &aabbH);
		float newX = game->playerPos.x;
		float newY = game->playerPos.y;
		ResolveAxis(&newX, &game->playerVel.x, newY, aabbW, aabbH, false, dt, true, true);
		ResolveAxis(&newY, &game->playerVel.y, newX, aabbW, aabbH, true, dt, true, true);
		PushPlayerOutOfSolids(game, &newX, &newY, aabbW, aabbH, false);
		float halfW = aabbW * 0.5f;
		float halfH = aabbH * 0.5f;
		float aabbLeft = newX - halfW;
		float belowY = newY + halfH + 1.0f;
		int leftCell = WorldToCellX(aabbLeft + 1.0f);
		int rightCell = WorldToCellX(aabbLeft + aabbW - 2.0f);
		int belowCellY = WorldToCellY(belowY);
		game->onGround = false;
		for (int cx = leftCell; cx <= rightCell; ++cx)
			if (BlockAtCell(cx, belowCellY)) {
				game->onGround = true;
				break;
			}
		if (newX < halfW) {
			newX = halfW;
			game->playerVel.x = 0.0f;
		}
		if (newX > WINDOW_WIDTH - halfW) {
			newX = WINDOW_WIDTH - halfW;
			game->playerVel.x = 0.0f;
		}
		if (newY < halfH) {
			newY = halfH;
			game->playerVel.y = 0.0f;
		}
		if (newY > WINDOW_HEIGHT - halfH) {
			newY = WINDOW_HEIGHT - halfH;
			game->playerVel.y = 0.0f;
			game->onGround = true;
		}
		game->playerPos.x = newX;
		game->playerPos.y = newY;
		return;
	}
	if (victory) return;
	if (InputGate_BeginFrameBlocked()) return;

	if (game->hurtTimer > 0.0f) {
		game->hurtTimer -= dt;
		if (game->hurtTimer < 0.0f) game->hurtTimer = 0.0f;
	}
	game->animDash = false;
	game->animSlide = false;
	game->animLadder = false;
	if (game->crouchAnimDir != 0) {
		game->crouchAnimTime += dt;
		float dur = 3.0f / 10.0f; // 3 frames at 10 fps
		if (game->crouchAnimTime >= dur) {
			game->crouchAnimTime = dur;
			game->crouchAnimDir = 0;
		}
	}

	game->runTime += dt;
	bool didGroundJumpThisFrame = false;
	if (game->coyoteTimer > 0.0f) game->coyoteTimer -= dt;
	if (game->jumpBufferTimer > 0.0f) game->jumpBufferTimer -= dt;
	if (game->groundStickTimer > 0.0f) game->groundStickTimer -= dt;
	if (game->wallCoyoteTimer > 0.0f) game->wallCoyoteTimer -= dt;

	bool wantJumpPress = InputPressed(ACT_JUMP);
	bool jumpDown = InputDown(ACT_JUMP);
	bool left = InputDown(ACT_LEFT);
	bool right = InputDown(ACT_RIGHT);
	bool down = InputDown(ACT_DOWN);

	if (wantJumpPress) game->jumpBufferTimer = JUMP_BUFFER_TIME;

	// Handle crouch transitions with feet-anchoring and headroom check when standing up
	WarriorClipDims prevDims = WarriorDimsForState(game);
	float prevBottom = game->playerPos.y + prevDims.h * 0.5f; // groundSink is render-only, keep physics bottom fixed
	bool prevCrouch = game->crouching;
	bool wantCrouch = down;
	if (prevCrouch && !wantCrouch) {
		WarriorClipDims standDims = WarriorDimsForCrouchState(game, false);
		float standY = prevBottom - standDims.h;
		float standX = game->playerPos.x - standDims.w * 0.5f;
		if (AABBOverlapsSolid(standX, standY, standDims.w, standDims.h)) {
			wantCrouch = true; // blocked overhead, remain crouched
		}
	}
	game->crouching = wantCrouch;
	if (prevCrouch != game->crouching) {
		game->crouchAnimTime = 0.0f;
		game->crouchAnimDir = game->crouching ? 1 : -1;
	}
	WarriorClipDims newDims = WarriorDimsForState(game);
	if (fabsf(newDims.h - prevDims.h) > 0.001f) {
		game->playerPos.y = prevBottom - newDims.h * 0.5f;
	}

	float maxSpeedX = game->crouching ? MAX_SPEED_X_CROUCH : MAX_SPEED_X;
	float aabbW = 0.0f, aabbH = 0.0f;
	Game_CurrentAABBDims(game, &aabbW, &aabbH);

	bool touchingLeft = TouchingWall(game, true, aabbW, aabbH);
	bool touchingRight = TouchingWall(game, false, aabbW, aabbH);

	float accel = game->onGround ? MOVE_ACCEL : AIR_ACCEL;
	bool pushingLeft = left && !right;
	bool pushingRight = right && !left;
	bool accelApplied = false;
	if (!game->crouching && !game->edgeHang) {
		if (pushingLeft) {
			game->playerVel.x -= accel * dt;
			accelApplied = true;
		} else if (pushingRight) {
			game->playerVel.x += accel * dt;
			accelApplied = true;
		}
	}
	if (!accelApplied && game->onGround) {
		float frames = dt / BASE_DT;
		float fr = game->crouching ? CROUCH_FRICTION : GROUND_FRICTION;
		if (frames > 0.0f) game->playerVel.x *= powf(fr, frames);
	} else if (!accelApplied && !game->edgeHang) {
		float frames = dt / BASE_DT;
		if (frames > 0.0f) game->playerVel.x *= powf(AIR_FRICTION, frames);
	} else if (game->edgeHang) {
		game->playerVel.x = 0.0f;
	}

	if (game->playerVel.x > maxSpeedX) game->playerVel.x = maxSpeedX;
	if (game->playerVel.x < -maxSpeedX) game->playerVel.x = -maxSpeedX;

	// Apply gravity with asymmetric multiplier (stronger when falling for better game feel)
	if (!game->edgeHang) {
		float gravityMult = (game->playerVel.y > 0.0f) ? GRAVITY_FALL_MULT : 1.0f;
		game->playerVel.y += GRAVITY * gravityMult * dt;
		if (game->playerVel.y > MAX_SPEED_Y) game->playerVel.y = MAX_SPEED_Y;
	} else {
		game->playerVel.y = 0.0f;
	}

	// Wall interaction checks (use current position before movement)
	// Do not allow wall interactions while grounded
	if (game->onGround) {
		touchingLeft = false;
		touchingRight = false;
		game->wallCoyoteTimer = 0.0f;
	}

	// Refresh wall coyote window while airborne and touching a wall
	if (!game->onGround && (touchingLeft || touchingRight)) {
		game->wallCoyoteTimer = WALL_COYOTE_TIME;
		game->wallCoyoteDir = touchingLeft ? -1 : +1;
	}

	bool canJumpNow = game->onGround || game->coyoteTimer > 0.0f || game->edgeHang;
	if (game->jumpBufferTimer > 0.0f && canJumpNow) {
		game->playerVel.y = JUMP_SPEED;
		game->playerVel.x = game->edgeHang ? 0.0f : game->playerVel.x;
		game->onGround = false;
		game->coyoteTimer = 0.0f;
		game->jumpBufferTimer = 0.0f;
		didGroundJumpThisFrame = true;
		if (game->edgeHang) {
			game->edgeHang = false;
			game->edgeHangDir = 0;
			game->wallCoyoteTimer = 0.0f;
		}
	}

	// Wall jump when airborne and either touching a wall or within wall coyote window
	bool canWallJump = !game->onGround && !game->edgeHang &&
	                   (touchingLeft || touchingRight || game->wallCoyoteTimer > 0.0f);
	if (game->jumpBufferTimer > 0.0f && canWallJump) {
		int dir = touchingLeft ? -1 : (touchingRight ? +1 : game->wallCoyoteDir);
		game->playerVel.y = JUMP_SPEED;
		game->playerVel.x = (dir == -1) ? WALL_JUMP_PUSH_X : -WALL_JUMP_PUSH_X;
		game->jumpBufferTimer = 0.0f;
		game->coyoteTimer = 0.0f;
		game->wallCoyoteTimer = 0.0f;
		Audio_PlayJump();
		Render_SpawnWallJumpDust(game, dir);
	}

	// Variable jump: if jump released while moving upward, damp upward velocity
	if (game->jumpPrevDown && !jumpDown && game->playerVel.y < 0.0f) {
		game->playerVel.y *= JUMP_CUT_MULT;
	}

	float prevVelY = game->playerVel.y;
	bool hitLeft = false, hitRight = false, hitTop = false, hitBottom = false;
	bool wasGround = game->onGround;
	MovePlayerWithCollisions(game, aabbW, aabbH, dt, &hitLeft, &hitRight, &hitTop, &hitBottom);
	float halfW = aabbW * 0.5f;
	float halfH = aabbH * 0.5f;
	float belowY = game->playerPos.y + halfH + 1.0f;
	int leftCell = WorldToCellX(game->playerPos.x - halfW + 1.0f);
	int rightCell = WorldToCellX(game->playerPos.x + halfW - 1.0f);
	int belowCellY = WorldToCellY(belowY);
	game->onGround = hitBottom;
	if (!game->onGround) {
		for (int cx = leftCell; cx <= rightCell; ++cx) {
			if (BlockAtCell(cx, belowCellY)) {
				game->onGround = true;
				break;
			}
		}
	}
	bool landedThisFrame = (!wasGround) && game->onGround;
	float contactW = 0.0f, contactH = 0.0f;
	Game_CurrentAABBDims(game, &contactW, &contactH);
	bool overlapLeft = TouchingWall(game, true, contactW, contactH);
	bool overlapRight = TouchingWall(game, false, contactW, contactH);
	game->wallContactLeft = hitLeft || overlapLeft;
	game->wallContactRight = hitRight || overlapRight;
	if (game->groundStickTimer > 0.0f) game->onGround = true;
	if (landedThisFrame) {
		game->groundStickTimer = GROUND_STICK_TIME;
	}
	if (landedThisFrame && prevVelY > ANIM_HURT_LAND_SPEED) {
		game->hurtTimer = ANIM_HURT_DURATION;
	}

	// Prefer ledge hang when close to an edge; otherwise slide.
	if (game->onGround) {
		game->edgeHang = false;
		game->edgeHangDir = 0;
	}
	if (!game->onGround) {
		if (game->edgeHang) {
			bool stillTouching = (game->edgeHangDir < 0) ? game->wallContactLeft : game->wallContactRight;
			bool pressingAway = (game->edgeHangDir < 0 && right) || (game->edgeHangDir > 0 && left);
			if (!stillTouching || down || pressingAway) {
				game->edgeHang = false;
				game->edgeHangDir = 0;
			} else {
				game->playerVel.x = 0.0f;
				game->playerVel.y = 0.0f;
			}
		}
		if (!game->edgeHang && game->wallContactLeft) {
			float snapY = 0.0f;
			if (CanGrabLedge(game, true, contactW, contactH, &snapY)) {
				game->edgeHang = true;
				game->edgeHangDir = -1;
				game->playerPos.y = snapY;
				game->playerVel.x = 0.0f;
				game->playerVel.y = 0.0f;
			}
		} else if (!game->edgeHang && game->wallContactRight) {
			float snapY = 0.0f;
			if (CanGrabLedge(game, false, contactW, contactH, &snapY)) {
				game->edgeHang = true;
				game->edgeHangDir = +1;
				game->playerPos.y = snapY;
				game->playerVel.x = 0.0f;
				game->playerVel.y = 0.0f;
			}
		}
	}

	if (!game->onGround && !game->edgeHang && (game->wallContactLeft || game->wallContactRight)) {
		game->wallStickTimer = kWallStickBuffer;
		game->wallCoyoteTimer = WALL_COYOTE_TIME;
		game->wallCoyoteDir = game->wallContactLeft ? -1 : +1;
	} else if (game->wallStickTimer > 0.0f) {
		game->wallStickTimer -= dt;
		if (game->wallStickTimer < 0.0f) game->wallStickTimer = 0.0f;
	}

	bool slideActive = (!game->onGround) && !game->edgeHang &&
	                   ((game->wallContactLeft || game->wallContactRight || game->wallStickTimer > 0.0f));
	if (slideActive) {
		if (game->playerVel.y > WALL_SLIDE_MAX_FALL) {
			float decel = WALL_SLIDE_ACCEL * dt;
			game->playerVel.y -= decel;
			if (game->playerVel.y < WALL_SLIDE_MAX_FALL) game->playerVel.y = WALL_SLIDE_MAX_FALL;
		}
		if (game->wallContactLeft && game->playerVel.x < 0.0f) game->playerVel.x = 0.0f;
		if (game->wallContactRight && game->playerVel.x > 0.0f) game->playerVel.x = 0.0f;
	}
	game->wallSliding = slideActive;
	game->jumpPrevDown = jumpDown;
	if (didGroundJumpThisFrame) {
		Audio_PlayJump();
		Render_SpawnJumpDust(game);
	}
	// Update facing based on horizontal velocity, keep last when near zero
	if (game->playerVel.x > 1.0f)
		game->facingRight = true;
	else if (game->playerVel.x < -1.0f)
		game->facingRight = false;
	float speedX = fabsf(game->playerVel.x);
	game->animSlide = game->crouching && game->onGround && speedX > ANIM_SLIDE_SPEED;
	game->animDash = (!game->crouching) && game->onGround && speedX > (MAX_SPEED_X * ANIM_DASH_SPEED_FRAC);
	bool ladderHold = game->wallSliding && fabsf(game->playerVel.y) < ANIM_LADDER_SLIDE_SPEED;
	game->animLadder = ladderHold;
	if (game->onGround) game->coyoteTimer = COYOTE_TIME;
	if (landedThisFrame) {
		Render_SpawnLandDust(game);
	}
	float targetSink = game->onGround ? 1.0f : 0.0f;
	game->groundSink += (targetSink - game->groundSink) * (12.0f * dt);

	Rogue_Update(game, dt);
	if (death) return;

	if (CheckCollisionRecs(PlayerAABB(game), ExitAABB(game))) {
		victory = true;
		game->score = (int)(game->runTime * 1000.0f);
		Audio_PlayVictory();
	}
	if (PlayerTouchesHazard(game)) {
		TriggerDeath(game);
	}
}

void Game_OnLevelLoaded(GameState *game, const struct LevelEditorState *level) {
	Rogue_BuildFromLevel(level);
}

void RenderGame(const GameState *game, const struct LevelEditorState *level, float dt) {
	gLevel = level;
	RenderTilesGameplay(level, game);
	Rogue_RenderEnemies();
	Render_DrawDust(dt);
	RenderPlayer(game);
	DrawRectangleRec(ExitAABB(game), GREEN);
#if DEBUG_DRAW_BOUNDS
	DrawStats(game);
#endif
}

bool Game_Victory(void) { return victory; }
bool Game_Death(void) { return death && deathAnimTimer <= 0.0f; }
bool Game_IsDying(void) { return death; }
float Game_DeathProgress(void) {
	if (!death || kDeathAnimDuration <= 0.0f) return 0.0f;
	float t = 1.0f - (deathAnimTimer / kDeathAnimDuration);
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	return t;
}
void Game_ClearOutcome(void) {
	victory = false;
	death = false;
	deathAnimTimer = 0.0f;
	Rogue_Clear();
	// clear transient animation flags
	for (int i = 0; i < kMaxEnemies; ++i) {
		gEnemies[i].active = false;
	}
	// reset animation timers/flags
}

// Reset transient render flags on respawn
void Game_ResetVisuals(GameState *game) {
	if (!game) return;
	game->spriteRotation = 0.0f;
	game->hidden = false;
	game->groundSink = 0.0f;
	game->hurtTimer = 0.0f;
	game->animDash = false;
	game->animSlide = false;
	game->animLadder = false;
	game->crouchAnimTime = 0.0f;
	game->crouchAnimDir = 0;
	game->wallContactLeft = false;
	game->wallContactRight = false;
	game->wallStickTimer = 0.0f;
	game->edgeHang = false;
	game->edgeHangDir = 0;
}
void Game_CurrentAABBDims(const GameState *g, float *outW, float *outH) {
	if (!g) {
		if (outW) *outW = 0.0f;
		if (outH) *outH = 0.0f;
		return;
	}
	WarriorClipDims d = WarriorDimsForState(g);
	float scale = 1.0f;
	float w = d.w * scale;
	float h = d.h * scale;
	if (outW) *outW = w;
	if (outH) *outH = h;
}

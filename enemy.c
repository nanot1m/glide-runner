#include "enemy.h"
#include "config.h"
#include "physics.h"
#include "level.h"
#include <string.h>
#include <math.h>

static EnemySpawner gSpawners[MAX_SPAWNERS];
static int gSpawnerCount = 0;
static Enemy gEnemies[MAX_ENEMIES];

static const float kSpawnInterval = (float)ROGUE_SPAWN_INTERVAL_MS / 1000.0f;
static const float kEnemyW = ROGUE_ENEMY_W;
static const float kEnemyH = ROGUE_ENEMY_H;

void Enemy_Init(void) {
	Enemy_Clear();
}

void Enemy_Clear(void) {
	gSpawnerCount = 0;
	memset(gSpawners, 0, sizeof(gSpawners));
	memset(gEnemies, 0, sizeof(gEnemies));
}

void Enemy_BuildFromLevel(const struct LevelEditorState *level) {
	Enemy_Clear();
	if (!level) return;
	for (int y = 0; y < GRID_ROWS; ++y) {
		for (int x = 0; x < GRID_COLS; ++x) {
			if (!IsSpawnerTile(level->tiles[y][x])) continue;
			if (gSpawnerCount >= MAX_SPAWNERS) return;
			EnemySpawner *s = &gSpawners[gSpawnerCount++];
			s->pos = (Vector2){CellToWorld(x), CellToWorld(y)};
			s->timer = 0.0f;
		}
	}
}

static void SpawnEnemy(Vector2 spawnPos) {
	if (AABBOverlapsSolid(spawnPos.x, spawnPos.y, kEnemyW, kEnemyH)) return;
	for (int i = 0; i < MAX_ENEMIES; ++i) {
		Enemy *e = &gEnemies[i];
		if (e->active) continue;
		e->active = true;
		e->pos = spawnPos;
		e->vel = (Vector2){0, 0};
		return;
	}
}

static Rectangle EnemyAABB(const Enemy *e) {
	return (Rectangle){e->pos.x, e->pos.y, kEnemyW, kEnemyH};
}

static void ResolveEnemyEnemyCollisions(void) {
	for (int i = 0; i < MAX_ENEMIES; ++i) {
		Enemy *a = &gEnemies[i];
		if (!a->active) continue;
		for (int j = i + 1; j < MAX_ENEMIES; ++j) {
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

// Need to access GameState for player pos/vel/health.
// We can include game.h (already done) but we need to declare the functions we use if they are not in a header.
// `PlayerAABB` is in game.c static. We should move it to player.h or make it public in game.h.
// For now, let's assume we'll move `PlayerAABB` to `player.h` or `game.h`.
// Also `TriggerDeath` and `Render_SpawnLandDust` etc.
// This is getting tricky. `TriggerDeath` is in `game.c`.
// Maybe we should keep `HandleEnemyPlayerCollisions` in `game.c` for now, and just expose `gEnemies`?
// Or better, pass a callback or return events?
// Let's try to implement `HandleCollision` here but we need `TakeDamage` and `TriggerDeath`.
// `TakeDamage` was static in `game.c`.
// Let's declare them as extern for now or move them to `player.h` later.

extern Rectangle PlayerAABB(const GameState *game); // Will be in player.h
extern void TakeDamage(GameState *game, Vector2 sourcePos); // Will be in player.h
extern void Render_SpawnLandDust(GameState *game); // In render.h? No, static in render.c? No, `Render_SpawnLandDust` is in `render.h` usually?
// Check render.h
// `render.h` has `Render_SpawnLandDust`.

static void HandleEnemyPlayerCollisions(GameState *game) {
	if (Game_IsDying()) return;
	Rectangle pb = PlayerAABB(game);
	float playerBottom = pb.y + pb.height;
	for (int i = 0; i < MAX_ENEMIES; ++i) {
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
			TakeDamage(game, e->pos);
			return;
		}
	}
}

void Enemy_Update(GameState *game, float dt) {
	// Update Spawners
	if (kSpawnInterval > 0.0f) {
		for (int i = 0; i < gSpawnerCount; ++i) {
			EnemySpawner *s = &gSpawners[i];
			s->timer -= dt;
			while (s->timer <= 0.0f) {
				Vector2 spawnPos = (Vector2){
				    s->pos.x + ((float)SQUARE_SIZE - kEnemyW) * 0.5f,
				    s->pos.y + ((float)SQUARE_SIZE - kEnemyH)};
				SpawnEnemy(spawnPos);
				s->timer += kSpawnInterval;
			}
		}
	}

	// Update Enemies
	for (int i = 0; i < MAX_ENEMIES; ++i) {
		Enemy *e = &gEnemies[i];
		if (!e->active) continue;
		float playerMidX = game->playerPos.x;
		float enemyMidX = e->pos.x + kEnemyW * 0.5f;
		float dir = (playerMidX >= enemyMidX) ? 1.0f : -1.0f;
		e->vel.x = dir * ROGUE_ENEMY_SPEED;
		e->vel.y += GRAVITY * dt;
		if (e->vel.y > ROGUE_ENEMY_MAX_FALL) e->vel.y = ROGUE_ENEMY_MAX_FALL;

		MoveEntity(&e->pos, &e->vel, kEnemyW, kEnemyH, dt, NULL, NULL, NULL, NULL);

		if (e->pos.x < 0.0f) {
			e->pos.x = 0.0f;
			e->vel.x = 0.0f;
		}
		if (e->pos.x > WINDOW_WIDTH - kEnemyW) {
			e->pos.x = WINDOW_WIDTH - kEnemyW;
			e->vel.x = 0.0f;
		}
		if (e->pos.y > WINDOW_HEIGHT - kEnemyH) {
			e->pos.y = WINDOW_HEIGHT - kEnemyH;
			e->vel.y = 0.0f;
		}

		if (e->pos.y > WINDOW_HEIGHT + kEnemyH * 2.0f) {
			e->active = false;
		}
	}
	ResolveEnemyEnemyCollisions();
	HandleEnemyPlayerCollisions(game);
}

void Enemy_Render(void) {
	for (int i = 0; i < MAX_ENEMIES; ++i) {
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

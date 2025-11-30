#pragma once
#include "raylib.h"
#include "game.h"

// Constants
#define MAX_SPAWNERS 64
#define MAX_ENEMIES 128

typedef struct EnemySpawner {
	Vector2 pos;
	float timer;
} EnemySpawner;

typedef struct Enemy {
	Vector2 pos;
	Vector2 vel;
	bool active;
} Enemy;

// Global enemy state (exposed for rendering/collision if needed, or keep hidden and expose functions)
// For now, let's expose functions to manage them.
// But game.c needs to iterate them for collision? Or we move collision logic here?
// Plan said: "Move Rogue_UpdateEnemies, Rogue_RenderEnemies, Rogue_SpawnEnemy".
// Collision with player is in `Rogue_HandleEnemyPlayerCollisions`. We should move that too or expose enemies.
// Let's expose the arrays for now to minimize friction, or better, keep them static in enemy.c and provide accessors.
// Given the time, let's keep them static in enemy.c and provide `Enemy_GetActive` or similar if needed,
// OR just move the collision logic to `enemy.c` as `Enemy_HandlePlayerCollision(GameState* g)`.

void Enemy_Init(void);
void Enemy_Clear(void);
void Enemy_BuildFromLevel(const struct LevelEditorState *level);
void Enemy_Update(struct GameState *game, float dt);
void Enemy_Render(void);

#include "game.h"
#include <math.h>
#include <string.h>
#include "audio.h"
#include "input_config.h"
#include "level.h"
#include "render.h"
#include "ui.h"
#include "physics.h"
#include "player.h"
#include "enemy.h"

static bool victory = false;
static bool death = false;
static float deathAnimTimer = 0.0f;

static const struct LevelEditorState *gLevel = NULL;
static const float kDeathAnimDuration = 0.7f;

void Game_TriggerDeath(GameState *game) {
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

void UpdateGame(GameState *game, const struct LevelEditorState *level, float dt) {
	gLevel = level;
	Physics_SetLevel(level); // Update physics level reference

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
		Vector2 pPos = game->playerPos;
		MoveEntity(&pPos, &game->playerVel, aabbW, aabbH, dt, NULL, NULL, NULL, NULL);
		PushEntityOutOfSolids(&pPos, &game->playerVel, aabbW, aabbH);
		
		float halfW = aabbW * 0.5f;
		float halfH = aabbH * 0.5f;
		
		if (pPos.x < halfW) {
			pPos.x = halfW;
			game->playerVel.x = 0.0f;
		}
		if (pPos.x > WINDOW_WIDTH - halfW) {
			pPos.x = WINDOW_WIDTH - halfW;
			game->playerVel.x = 0.0f;
		}
		if (pPos.y < halfH) {
			pPos.y = halfH;
			game->playerVel.y = 0.0f;
		}
		if (pPos.y > WINDOW_HEIGHT - halfH) {
			pPos.y = WINDOW_HEIGHT - halfH;
			game->playerVel.y = 0.0f;
			game->onGround = true;
		}
		game->playerPos.x = pPos.x;
		game->playerPos.y = pPos.y;
		return;
	}
	if (victory) return;
	if (InputGate_BeginFrameBlocked()) return;

	UpdatePlayer(game, dt);
	Enemy_Update(game, dt);

	if (death) return;

	if (CheckCollisionRecs(PlayerAABB(game), ExitAABB(game))) {
		victory = true;
		game->score = (int)(game->runTime * 1000.0f);
		Audio_PlayVictory();
	}
	
	// Hazard check
	// We need to access IsHazardTile.
	// PlayerTouchesHazard was static.
	// Let's implement it here or move to player.c?
	// It depends on `gLevel`.
	// Let's implement it here using `gLevel`.
	Rectangle pb = PlayerAABB(game);
	for (int y = 0; y < GRID_ROWS; ++y)
		for (int x = 0; x < GRID_COLS; ++x) {
			TileType t = gLevel->tiles[y][x];
			if (!IsHazardTile(t)) continue;
			Vector2 lp = (Vector2){CellToWorld(x), CellToWorld(y)};
			Rectangle lr = LaserCollisionRect(lp);
			if (CheckCollisionRecs(pb, lr)) {
				Game_TriggerDeath(game);
				break;
			}
		}
}

void Game_OnLevelLoaded(GameState *game, const struct LevelEditorState *level) {
	Enemy_BuildFromLevel(level);
}

void RenderGame(const GameState *game, const struct LevelEditorState *level, float dt) {
	gLevel = level;
	RenderTilesGameplay(level, game);
	Enemy_Render();
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
	Enemy_Clear();
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
	game->health = ROGUE_PLAYER_HEALTH;
	game->maxHealth = ROGUE_PLAYER_HEALTH;
	game->invincibilityTimer = 0.0f;
}

// Game state and game loop API
#pragma once
#include <stdbool.h>
#include "config.h"
#include "raylib.h"

typedef struct GameState {
	int score; // milliseconds elapsed for the run (lower is better)
	float runTime; // seconds elapsed in current run
	Vector2 playerPos; // center of player AABB
	Vector2 playerVel; // px/s
	bool onGround; // is standing on a block or floor
	float coyoteTimer; // seconds left to allow jump after leaving ground
	float jumpBufferTimer; // seconds left to consume buffered jump
	Vector2 exitPos;
	bool crouching; // crouch state
	float groundStickTimer; // seconds to remain grounded after contact
	bool facingRight; // rendering: last facing direction
	bool jumpPrevDown; // for variable jump: last-frame jump held
	float wallCoyoteTimer; // seconds to allow wall jump after leaving wall
	int wallCoyoteDir; // -1 if last touching left wall, +1 if right
	bool wallSliding; // currently wall sliding
	bool wallContactLeft; // touching solid on left (grounded check)
	bool wallContactRight; // touching solid on right (grounded check)
	float wallStickTimer; // debounce timer for wall contact visuals
	bool edgeHang; // hanging on a ledge instead of sliding
	int edgeHangDir; // -1 if hanging on left edge, +1 if right, 0 otherwise
	float spriteRotation; // degrees
	bool hidden; // skip rendering when true
	float groundSink; // small render offset for grounded weight
	float hurtTimer; // timer for hurt animation
	bool animDash; // high-speed ground dash visual
	bool animSlide; // crouch slide visual
	bool animLadder; // ladder/wall cling visual
	float crouchAnimTime; // progress through crouch enter/exit anim
	int crouchAnimDir; // 1 = crouch down, -1 = stand up, 0 = hold/none
	int health;
	int maxHealth;
	float invincibilityTimer;
} GameState;

struct LevelEditorState;

void UpdateGame(GameState *game, const struct LevelEditorState *level, float dt);
void RenderGame(const GameState *game, const struct LevelEditorState *level, float dt);
void Game_OnLevelLoaded(GameState *game, const struct LevelEditorState *level);

// Outcome flags
bool Game_Victory(void);
bool Game_Death(void);
bool Game_IsDying(void);
float Game_DeathProgress(void); // 0..1 over death animation duration
void Game_CurrentAABBDims(const GameState *g, float *outW, float *outH);
void Game_ClearOutcome(void);
void Game_ResetVisuals(GameState *game);

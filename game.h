// Game state and game loop API
#pragma once
#include <stdbool.h>
#include "config.h"
#include "raylib.h"

typedef struct GameState {
	int score; // milliseconds elapsed for the run (lower is better)
	float runTime; // seconds elapsed in current run
	Vector2 playerPos; // top-left corner of player AABB
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
} GameState;

void UpdateGame(GameState *game);
void RenderGame(const GameState *game);

// Outcome flags
bool Game_Victory(void);
bool Game_Death(void);
void Game_ClearOutcome(void);

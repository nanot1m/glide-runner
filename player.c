#include "player.h"
#include "config.h"
#include "physics.h"
#include "level.h"
#include "audio.h"
#include "render.h"
#include "input_config.h"
#include <math.h>

// Dimensions
typedef struct {
	float w;
	float h;
} WarriorClipDims;

static const WarriorClipDims kDimsIdle = {20.0f, 33.0f};
static const WarriorClipDims kDimsRun = {20.0f, 31.0f};
static const WarriorClipDims kDimsDeath = {20.0f, 43.0f};
static const WarriorClipDims kDimsHurt = {20.0f, 32.0f};
static const WarriorClipDims kDimsJump = {20.0f, 33.0f};
static const WarriorClipDims kDimsUpToFall = {20.0f, 32.0f};
static const WarriorClipDims kDimsFall = {20.0f, 34.0f};
static const WarriorClipDims kDimsWall = {20.0f, 36.0f};
static const WarriorClipDims kDimsCrouch = {20.0f, 24.0f};
static const WarriorClipDims kDimsDash = {20.0f, 31.0f};
static const WarriorClipDims kDimsSlide = {20.0f, 24.0f};
static const WarriorClipDims kDimsLadder = {20.0f, 40.0f};

// Wall / ledge helpers
static const float kWallStickBuffer = 0.10f;
static const float kLedgeHandSampleFrac = 0.35f; // fraction down from head to sample for hand position
static const float kLedgeSnapPadding = 2.0f; // offset to keep the player clear of the ledge when hanging

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

Rectangle PlayerAABB(const GameState *game) {
	float w, h;
	Game_CurrentAABBDims(game, &w, &h);
	return (Rectangle){game->playerPos.x - w * 0.5f, game->playerPos.y - h * 0.5f, w, h};
}

void TriggerDeath(GameState *game) {
	// We need to access the static `death` variable in game.c?
	// Or we should move `death` state to `GameState`?
	// `game.h` has `Game_Death()` etc accessors but the state is static in `game.c`.
	// This is a problem. `death` should probably be in `GameState`.
	// But `GameState` is passed around.
	// Let's assume for now we can't easily move `death` to `GameState` without breaking other things (like `Game_IsDying` accessors).
	// Actually, `game.h` defines `GameState`.
	// Let's add `bool death` to `GameState`?
	// The current `game.c` has `static bool death = false;`.
	// If we move it to `GameState`, we need to update all accessors.
	// Given the scope, let's expose `Game_SetDeath` in `game.h` and implement it in `game.c`.
	// Wait, `TriggerDeath` was in `game.c`.
	// If we move `TriggerDeath` to `player.c`, it needs to modify the global death state.
	// Let's declare `Game_TriggerDeath` in `game.h` and keep the implementation in `game.c`?
	// But `TakeDamage` calls `TriggerDeath`.
	// `TakeDamage` is logic we want in `player.c` (or `game.c`).
	// If `TakeDamage` is in `player.c`, it needs to call `Game_TriggerDeath`.
	// Let's rename `TriggerDeath` to `Game_TriggerDeath` and keep it in `game.c` (exposed in `game.h`).
	// And `TakeDamage` can be in `player.c`.
}

// Wait, I can't easily implement `TriggerDeath` here if it modifies static state in `game.c`.
// I will implement `TakeDamage` here but call `Game_TriggerDeath` which I will add to `game.h`.

// Also `BlockAtCell` is needed for `CanGrabLedge`.
// We made `BlockAtCell` static in `physics.c`.
// We should expose `Physics_BlockAtCell` or similar?
// Or just use `AABBOverlapsSolid`?
// `CanGrabLedge` checks specific cells.
// Let's add `Physics_IsBlockAt(int cx, int cy)` to `physics.h`.

// Re-implementing CanGrabLedge requires BlockAtCell.
// I'll add `Physics_BlockAtCell` to `physics.h` in the next step or just assume it exists and add it.
// Actually, I can just include `level.h` and use `gPhysicsLevel` if I exposed it? No.
// I'll use `IsSolidTile` and `editor` from `level.h`?
// `level.h` has `extern LevelEditorState editor;`.
// But the game might be running a level that is NOT `editor`.
// `physics.c` has `gPhysicsLevel`.
// I should expose `Physics_BlockAtCell` in `physics.h`.

// For now, I will comment out `TriggerDeath` implementation here and `TakeDamage` and `UpdatePlayer`
// until I fix the dependencies in `game.h` / `physics.h`.
// Actually, I should write `player.c` assuming `Game_TriggerDeath` and `Physics_BlockAtCell` exist,
// and then update `game.h` and `physics.h` and `game.c` accordingly.

extern void Game_TriggerDeath(GameState *game);
extern bool Physics_BlockAtCell(int cx, int cy);

static bool TouchingWall(const GameState *g, bool leftSide, float aabbW, float aabbH) {
	float left = g->playerPos.x - aabbW * 0.5f;
	float x = leftSide ? (left - 1.0f) : (left + aabbW);
	float top = g->playerPos.y - aabbH * 0.5f;
	return AABBOverlapsSolid(x, top, 1.0f, aabbH);
}

static bool CanGrabLedge(const GameState *g, bool leftSide, float aabbW, float aabbH, float *outSnapY) {
	if (g->playerVel.y < -40.0f) return false;

	float top = g->playerPos.y - aabbH * 0.5f;
	float handY = top + aabbH * kLedgeHandSampleFrac;
	float checkX = leftSide ? (g->playerPos.x - aabbW * 0.5f - 1.0f) : (g->playerPos.x + aabbW * 0.5f + 1.0f);
	int wallCellX = WorldToCellX(checkX);
	int handCellY = WorldToCellY(handY);
	int aboveCellY = handCellY - 1;
	float blockTop = CellToWorld(handCellY);

	if (!Physics_BlockAtCell(wallCellX, handCellY)) return false;
	if (Physics_BlockAtCell(wallCellX, aboveCellY)) return false;
	if (top > blockTop + 6.0f) return false;
	float maxHandY = blockTop + (float)SQUARE_SIZE * 0.7f;
	if (handY > maxHandY) return false;

	if (outSnapY) {
		*outSnapY = blockTop + kLedgeSnapPadding + aabbH * 0.5f;
	}
	return true;
}

void TakeDamage(GameState *game, Vector2 sourcePos) {
	if (game->invincibilityTimer > 0.0f) return;
	if (game->health <= 0) return;

	game->health--;
	game->invincibilityTimer = ROGUE_INVINCIBILITY_TIME;
	game->hurtTimer = ANIM_HURT_DURATION;

	// Knockback
	float dirX = (game->playerPos.x < sourcePos.x) ? -1.0f : 1.0f;
	game->playerVel.x = dirX * ROGUE_KNOCKBACK_FORCE_X;
	game->playerVel.y = ROGUE_KNOCKBACK_FORCE_Y;
	game->onGround = false;

	if (game->health <= 0) {
		Game_TriggerDeath(game);
	}
}

void UpdatePlayer(GameState *game, float dt) {
	if (game->hurtTimer > 0.0f) {
		game->hurtTimer -= dt;
		if (game->hurtTimer < 0.0f) game->hurtTimer = 0.0f;
	}
	if (game->invincibilityTimer > 0.0f) {
		game->invincibilityTimer -= dt;
		if (game->invincibilityTimer < 0.0f) game->invincibilityTimer = 0.0f;
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

	// Handle crouch transitions
	WarriorClipDims prevDims = WarriorDimsForState(game);
	float prevBottom = game->playerPos.y + prevDims.h * 0.5f;
	bool prevCrouch = game->crouching;
	bool wantCrouch = down;
	if (prevCrouch && !wantCrouch) {
		WarriorClipDims standDims = WarriorDimsForCrouchState(game, false);
		float standY = prevBottom - standDims.h;
		float standX = game->playerPos.x - standDims.w * 0.5f;
		if (AABBOverlapsSolid(standX, standY, standDims.w, standDims.h)) {
			wantCrouch = true; // blocked overhead
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

	// Apply gravity
	if (!game->edgeHang) {
		float gravityMult = (game->playerVel.y > 0.0f) ? GRAVITY_FALL_MULT : 1.0f;
		game->playerVel.y += GRAVITY * gravityMult * dt;
		if (game->playerVel.y > MAX_SPEED_Y) game->playerVel.y = MAX_SPEED_Y;
	} else {
		game->playerVel.y = 0.0f;
	}

	if (game->onGround) {
		touchingLeft = false;
		touchingRight = false;
		game->wallCoyoteTimer = 0.0f;
	}

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

	if (game->jumpPrevDown && !jumpDown && game->playerVel.y < 0.0f) {
		game->playerVel.y *= JUMP_CUT_MULT;
	}

	float prevVelY = game->playerVel.y;
	bool hitLeft = false, hitRight = false, hitTop = false, hitBottom = false;
	bool wasGround = game->onGround;
	
	MoveEntity(&game->playerPos, &game->playerVel, aabbW, aabbH, dt, &hitLeft, &hitRight, &hitTop, &hitBottom);

	float halfW = aabbW * 0.5f;
	float halfH = aabbH * 0.5f;
	float belowY = game->playerPos.y + halfH + 1.0f;
	int leftCell = WorldToCellX(game->playerPos.x - halfW + 1.0f);
	int rightCell = WorldToCellX(game->playerPos.x + halfW - 1.0f);
	int belowCellY = WorldToCellY(belowY);
	game->onGround = hitBottom;
	if (!game->onGround) {
		for (int cx = leftCell; cx <= rightCell; ++cx) {
			if (Physics_BlockAtCell(cx, belowCellY)) {
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
}

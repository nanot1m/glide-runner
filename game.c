#include "game.h"
#include <math.h>
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

static void ResolveAxis(float *pos, float *vel, float other, float w, float h, bool vertical, float dt) {
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
		float y = vertical ? newPos : other;
		if (AABBOverlapsSolid(x, y, w, h)) {
			int pixels = (int)fabsf(step);
			for (int i = 0; i < pixels; ++i) {
				newPos = *pos + sign;
				x = vertical ? other : newPos;
				y = vertical ? newPos : other;
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

static bool TouchingWall(const GameState *g, bool leftSide, float aabbH) {
	float x = leftSide ? (g->playerPos.x - 1.0f) : (g->playerPos.x + PLAYER_W);
	float y = g->playerPos.y;
	return AABBOverlapsSolid(x, y, 1.0f, aabbH);
}

void UpdateGame(GameState *game, const struct LevelEditorState *level, float dt) {
	gLevel = level;
	if (death) {
		if (deathAnimTimer > 0.0f) {
			deathAnimTimer -= dt;
			float t = 1.0f - (deathAnimTimer / kDeathAnimDuration);
			if (t < 0.0f) t = 0.0f;
			if (t > 1.0f) t = 1.0f;
			float scale = 1.0f + t; // grows to 2x
			game->spriteScaleX = scale;
			game->spriteScaleY = scale;
			game->spriteRotation = 90.0f * t;
			// Drift with momentum and gravity, without collision
			game->playerVel.y += GRAVITY * GRAVITY_FALL_MULT * dt;
			game->playerPos.x += game->playerVel.x * dt;
			game->playerPos.y += game->playerVel.y * dt;
			if (deathAnimTimer < 0.2f) game->hidden = true;
			if (deathAnimTimer < 0.0f) deathAnimTimer = 0.0f;
		}
		return;
	}
	if (victory) return;
	if (InputGate_BeginFrameBlocked()) return;
	if (game->spriteScaleY <= 0.0f) game->spriteScaleY = 1.0f;

	game->runTime += dt;
	bool didGroundJumpThisFrame = false;
	bool didWallJumpThisFrame = false;
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
	bool desiredCrouch = down;
	if (desiredCrouch != game->crouching) {
		if (desiredCrouch) {
			// Stand -> crouch: move top down so feet stay anchored
			float delta = (PLAYER_H - PLAYER_H_CROUCH);
			float newY = game->playerPos.y + delta;
			// Clamp within window
			float maxY = WINDOW_HEIGHT - PLAYER_H_CROUCH;
			if (newY > maxY) newY = maxY;
			game->playerPos.y = newY;
			game->crouching = true;
		} else {
			// Crouch -> stand: only if headroom is clear; keep feet anchored
			float feet = game->playerPos.y + PLAYER_H_CROUCH;
			float standY = feet - PLAYER_H;
			if (!AABBOverlapsSolid(game->playerPos.x, standY, PLAYER_W, PLAYER_H)) {
				game->playerPos.y = standY;
				game->crouching = false;
			} else {
				// Block stand up if obstructed
				game->crouching = true;
			}
		}
	}

	float maxSpeedX = game->crouching ? MAX_SPEED_X_CROUCH : MAX_SPEED_X;
	float aabbH = game->crouching ? PLAYER_H_CROUCH : PLAYER_H;

	float accel = game->onGround ? MOVE_ACCEL : AIR_ACCEL;
	bool accelApplied = false;
	if (!game->crouching) {
		if (left && !right) {
			game->playerVel.x -= accel * dt;
			accelApplied = true;
		} else if (right && !left) {
			game->playerVel.x += accel * dt;
			accelApplied = true;
		}
	}
	if (!accelApplied && game->onGround) {
		float frames = dt / BASE_DT;
		float fr = game->crouching ? CROUCH_FRICTION : GROUND_FRICTION;
		if (frames > 0.0f) game->playerVel.x *= powf(fr, frames);
	} else if (!accelApplied) {
		float frames = dt / BASE_DT;
		if (frames > 0.0f) game->playerVel.x *= powf(AIR_FRICTION, frames);
	}

	if (game->playerVel.x > maxSpeedX) game->playerVel.x = maxSpeedX;
	if (game->playerVel.x < -maxSpeedX) game->playerVel.x = -maxSpeedX;

	// Apply gravity with asymmetric multiplier (stronger when falling for better game feel)
	float gravityMult = (game->playerVel.y > 0.0f) ? GRAVITY_FALL_MULT : 1.0f;
	game->playerVel.y += GRAVITY * gravityMult * dt;
	if (game->playerVel.y > MAX_SPEED_Y) game->playerVel.y = MAX_SPEED_Y;

	// Wall interaction checks (use current position before movement)
	bool touchingLeft = TouchingWall(game, true, aabbH);
	bool touchingRight = TouchingWall(game, false, aabbH);
	// Do not allow wall interactions while grounded
	if (game->onGround) {
		touchingLeft = false;
		touchingRight = false;
		game->wallCoyoteTimer = 0.0f;
	}

	// Wall slide: gradually decelerate to max slide speed when pressing into a wall while airborne
	bool isWallSliding = !game->onGround && ((touchingLeft && left && !right) || (touchingRight && right && !left));
	if (isWallSliding) {
		if (game->playerVel.y > WALL_SLIDE_MAX_FALL) {
			// Gradually decelerate to wall slide speed
			float decel = WALL_SLIDE_ACCEL * dt;
			game->playerVel.y -= decel;
			if (game->playerVel.y < WALL_SLIDE_MAX_FALL) {
				game->playerVel.y = WALL_SLIDE_MAX_FALL;
			}
		}
		game->wallSliding = true;
	} else {
		game->wallSliding = false;
	}

	// Refresh wall coyote window while airborne and touching a wall
	if (!game->onGround && (touchingLeft || touchingRight)) {
		game->wallCoyoteTimer = WALL_COYOTE_TIME;
		game->wallCoyoteDir = touchingLeft ? -1 : +1;
	}

	bool canJumpNow = game->onGround || game->coyoteTimer > 0.0f;
	if (game->jumpBufferTimer > 0.0f && canJumpNow) {
		game->playerVel.y = JUMP_SPEED;
		game->onGround = false;
		game->coyoteTimer = 0.0f;
		game->jumpBufferTimer = 0.0f;
		didGroundJumpThisFrame = true;
	}

	// Wall jump when airborne and either touching a wall or within wall coyote window
	bool canWallJump = !game->onGround && (touchingLeft || touchingRight || game->wallCoyoteTimer > 0.0f);
	if (game->jumpBufferTimer > 0.0f && canWallJump) {
		int dir = touchingLeft ? -1 : (touchingRight ? +1 : game->wallCoyoteDir);
		game->playerVel.y = JUMP_SPEED;
		game->playerVel.x = (dir == -1) ? WALL_JUMP_PUSH_X : -WALL_JUMP_PUSH_X;
		game->jumpBufferTimer = 0.0f;
		game->coyoteTimer = 0.0f;
		game->wallCoyoteTimer = 0.0f;
		didWallJumpThisFrame = true;
		Audio_PlayJump();
		Render_SpawnWallJumpDust(game, dir);
	}

	// Variable jump: if jump released while moving upward, damp upward velocity
	if (game->jumpPrevDown && !jumpDown && game->playerVel.y < 0.0f) {
		game->playerVel.y *= JUMP_CUT_MULT;
	}

	float newX = game->playerPos.x;
	float newY = game->playerPos.y;
	ResolveAxis(&newX, &game->playerVel.x, newY, PLAYER_W, aabbH, false, dt);
	ResolveAxis(&newY, &game->playerVel.y, newX, PLAYER_W, aabbH, true, dt);
	bool wasGround = game->onGround;
	game->onGround = false;

	bool landedThisFrame = false;
	float belowY = newY + aabbH + 1.0f;
	int leftCell = WorldToCellX(newX + 1.0f);
	int rightCell = WorldToCellX(newX + PLAYER_W - 2.0f);
	int belowCellY = WorldToCellY(belowY);
	for (int cx = leftCell; cx <= rightCell; ++cx)
		if (BlockAtCell(cx, belowCellY)) {
			game->onGround = true;
			break;
		}
	if (!wasGround && game->onGround) {
		game->groundStickTimer = GROUND_STICK_TIME;
		landedThisFrame = true;
	}
	if (game->groundStickTimer > 0.0f) game->onGround = true;

	if (newX < 0) {
		newX = 0;
		game->playerVel.x = 0;
	}
	if (newX > WINDOW_WIDTH - PLAYER_W) {
		newX = WINDOW_WIDTH - PLAYER_W;
		game->playerVel.x = 0;
	}
	if (newY < 0) {
		newY = 0;
		game->playerVel.y = 0;
	}
	if (newY > WINDOW_HEIGHT - aabbH) {
		newY = WINDOW_HEIGHT - aabbH;
		game->playerVel.y = 0;
		game->onGround = true;
	}

	game->playerPos.x = newX;
	game->playerPos.y = newY;
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
	if (game->onGround) game->coyoteTimer = COYOTE_TIME;
	if (didGroundJumpThisFrame || didWallJumpThisFrame) game->spriteScaleY = PLAYER_JUMP_STRETCH;
	if (landedThisFrame) {
		game->spriteScaleY = PLAYER_LAND_SQUASH;
		Render_SpawnLandDust(game);
	}
	// Recover squash/stretch back toward neutral
	game->spriteScaleY += (1.0f - game->spriteScaleY) * (PLAYER_SQUASH_RECOVER * dt);
	float targetSink = game->onGround ? 1.0f : 0.0f;
	game->groundSink += (targetSink - game->groundSink) * (12.0f * dt);

	if (CheckCollisionRecs(PlayerAABB(game), ExitAABB(game))) {
		victory = true;
		game->score = (int)(game->runTime * 1000.0f);
		Audio_PlayVictory();
	}
	if (PlayerTouchesHazard(game)) {
		death = true;
		deathAnimTimer = kDeathAnimDuration;
		game->spriteScaleX = 1.0f;
		game->spriteScaleY = 1.0f;
		game->spriteRotation = 0.0f;
		game->hidden = false;
		Audio_PlayDeath();
		Render_SpawnDeathExplosion(game);
	}
}

void RenderGame(const GameState *game, const struct LevelEditorState *level, float dt) {
	gLevel = level;
	RenderTilesGameplay(level, game);
	Render_DrawDust(dt);
	RenderPlayer(game);
	DrawRectangleRec(ExitAABB(game), GREEN);
	// TODO: show stats in debug builds
	// DrawStats(game);
}

bool Game_Victory(void) { return victory; }
bool Game_Death(void) { return death && deathAnimTimer <= 0.0f; }
void Game_ClearOutcome(void) {
	victory = false;
	death = false;
	deathAnimTimer = 0.0f;
}

// Reset transient render flags on respawn
void Game_ResetVisuals(GameState *game) {
	if (!game) return;
	game->spriteScaleX = 1.0f;
	game->spriteScaleY = 1.0f;
	game->spriteRotation = 0.0f;
	game->hidden = false;
	game->groundSink = 0.0f;
}

#include "game.h"
#include <math.h>
#include "audio.h"
#include "input_config.h"
#include "level.h"
#include "render.h"
#include "ui.h"

static bool victory = false;
static bool death = false;

static bool BlockAtCell(int cx, int cy) {
	if (!InBoundsCell(cx, cy)) return true;
	return IsSolidTile(editor.tiles[cy][cx]);
}

static bool AABBOverlapsSolid(float x, float y, float w, float h) {
	int left = WorldToCellX(x);
	int right = WorldToCellX(x + w - 0.001f);
	int top = WorldToCellY(y);
	int bottom = WorldToCellY(y + h - 0.001f);
	for (int cy = top; cy <= bottom; ++cy)
		for (int cx = left; cx <= right; ++cx)
			if (BlockAtCell(cx, cy)) return true;
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
			TileType t = editor.tiles[y][x];
			if (!IsHazardTile(t)) continue;
			Rectangle lr = LaserStripeRect((Vector2){CellToWorld(x), CellToWorld(y)});
			if (CheckCollisionRecs(pb, lr)) return true;
		}
	return false;
}

static bool TouchingWall(const GameState *g, bool leftSide, float aabbH) {
	float x = leftSide ? (g->playerPos.x - 1.0f) : (g->playerPos.x + PLAYER_W);
	float y = g->playerPos.y;
	return AABBOverlapsSolid(x, y, 1.0f, aabbH);
}

void UpdateGame(GameState *game) {
	if (victory || death) return;
	if (InputGate_BeginFrameBlocked()) return;

	float dt = GetFrameTime();
	if (dt > 0.033f) dt = 0.033f;
	game->runTime += dt;
	bool didGroundJumpThisFrame = false;
	if (game->coyoteTimer > 0.0f) game->coyoteTimer -= dt;
	if (game->jumpBufferTimer > 0.0f) game->jumpBufferTimer -= dt;
	if (game->groundStickTimer > 0.0f) game->groundStickTimer -= dt;

	bool wantJumpPress = InputPressed(ACT_JUMP);
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
	game->playerVel.y += GRAVITY * dt;
	if (game->playerVel.y > MAX_SPEED_Y) game->playerVel.y = MAX_SPEED_Y;

	// Wall interaction checks (use current position before movement)
	bool touchingLeft = TouchingWall(game, true, aabbH);
	bool touchingRight = TouchingWall(game, false, aabbH);

	// Wall slide: clamp fall speed when pressing into a wall while airborne
	if (!game->onGround && ((touchingLeft && left && !right) || (touchingRight && right && !left))) {
		if (game->playerVel.y > WALL_SLIDE_MAX_FALL) game->playerVel.y = WALL_SLIDE_MAX_FALL;
	}

	bool canJumpNow = game->onGround || game->coyoteTimer > 0.0f;
	if (game->jumpBufferTimer > 0.0f && canJumpNow) {
		game->playerVel.y = JUMP_SPEED;
		game->onGround = false;
		game->coyoteTimer = 0.0f;
		game->jumpBufferTimer = 0.0f;
		didGroundJumpThisFrame = true;
	}

	// Wall jump when airborne and touching a wall
	if (game->jumpBufferTimer > 0.0f && !game->onGround && (touchingLeft || touchingRight)) {
		game->playerVel.y = JUMP_SPEED;
		game->playerVel.x = touchingLeft ? WALL_JUMP_PUSH_X : -WALL_JUMP_PUSH_X;
		game->jumpBufferTimer = 0.0f;
		game->coyoteTimer = 0.0f;
		Audio_PlayJump();
	}

	float newX = game->playerPos.x;
	float newY = game->playerPos.y;
	ResolveAxis(&newX, &game->playerVel.x, newY, PLAYER_W, aabbH, false, dt);
	ResolveAxis(&newY, &game->playerVel.y, newX, PLAYER_W, aabbH, true, dt);
	bool wasGround = game->onGround;
	game->onGround = false;

	float belowY = newY + aabbH + 1.0f;
	int leftCell = WorldToCellX(newX + 1.0f);
	int rightCell = WorldToCellX(newX + PLAYER_W - 2.0f);
	int belowCellY = WorldToCellY(belowY);
	for (int cx = leftCell; cx <= rightCell; ++cx)
		if (BlockAtCell(cx, belowCellY)) {
			game->onGround = true;
			break;
		}
	if (!wasGround && game->onGround) { game->groundStickTimer = GROUND_STICK_TIME; }
	if (game->groundStickTimer > 0.0f) game->onGround = true;
	if (didGroundJumpThisFrame) { Audio_PlayJump(); }

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
	if (game->onGround) game->coyoteTimer = COYOTE_TIME;

	if (CheckCollisionRecs(PlayerAABB(game), ExitAABB(game))) {
		victory = true;
		game->score = (int)(game->runTime * 1000.0f);
		Audio_PlayVictory();
	}
	if (PlayerTouchesHazard(game)) {
		death = true;
		Audio_PlayDeath();
	}
}

void RenderGame(const GameState *game) {
	RenderTiles(&editor);
	DrawRectangleRec(PlayerAABB(game), BLUE);
	DrawRectangleRec(ExitAABB(game), GREEN);
	// TODO: show stats in debug builds
	// DrawStats(game);
}

bool Game_Victory(void) { return victory; }
bool Game_Death(void) { return death; }
void Game_ClearOutcome(void) {
	victory = false;
	death = false;
}

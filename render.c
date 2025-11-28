#include "render.h"
#include <math.h>
#include <string.h>
#include "raylib.h"

static Texture2D gBlockTileset = {0};
static const int BLOCK_TILE_SIZE = 32;
static int gBlockTileCols = 0;

Rectangle PlayerAABB(const GameState *g) {
	float h = g->crouching ? PLAYER_H_CROUCH : PLAYER_H;
	return (Rectangle){g->playerPos.x, g->playerPos.y, PLAYER_W, h};
}

Rectangle ExitAABB(const GameState *g) {
	return (Rectangle){g->exitPos.x, g->exitPos.y, (float)SQUARE_SIZE, (float)SQUARE_SIZE};
}

Rectangle TileRect(int cx, int cy) {
	return (Rectangle){CellToWorld(cx), CellToWorld(cy), (float)SQUARE_SIZE, (float)SQUARE_SIZE};
}

Rectangle LaserStripeRect(Vector2 laserPos) {
	return (Rectangle){laserPos.x, laserPos.y + LASER_STRIPE_OFFSET, (float)SQUARE_SIZE, LASER_STRIPE_THICKNESS};
}

Rectangle LaserCollisionRect(Vector2 laserPos) {
	// Reduce horizontal bounds by 1px while keeping the left edge fixed
	return (Rectangle){laserPos.x, laserPos.y + LASER_STRIPE_OFFSET, (float)SQUARE_SIZE - 1.0f, LASER_STRIPE_THICKNESS};
}

static inline bool IsBlockAt(const LevelEditorState *ed, int cx, int cy) {
	if (!InBoundsCell(cx, cy)) return false;
	return IsSolidTile(ed->tiles[cy][cx]);
}

static Rectangle BlockTileSrc(int tx, int ty) {
	return (Rectangle){(float)(tx * BLOCK_TILE_SIZE), (float)(ty * BLOCK_TILE_SIZE), (float)BLOCK_TILE_SIZE, (float)BLOCK_TILE_SIZE};
}

static Rectangle ChooseBlockSrc(const LevelEditorState *ed, int cx, int cy) {
	bool up = IsBlockAt(ed, cx, cy - 1);
	// bool isUpEdge = up && !IsBlockAt(ed, cx, cy - 2);
	bool down = IsBlockAt(ed, cx, cy + 1);
	// bool isDownEdge = down && !IsBlockAt(ed, cx, cy + 2);
	bool left = IsBlockAt(ed, cx - 1, cy);
	// bool isLeftEdge = left && !IsBlockAt(ed, cx - 2, cy);
	bool right = IsBlockAt(ed, cx + 1, cy);
	// bool isRightEdge = right && !IsBlockAt(ed, cx + 2, cy);
	bool upLeft = IsBlockAt(ed, cx - 1, cy - 1);
	bool upRight = IsBlockAt(ed, cx + 1, cy - 1);
	bool downLeft = IsBlockAt(ed, cx - 1, cy + 1);
	bool downRight = IsBlockAt(ed, cx + 1, cy + 1);

	if (!up && !down) {
		if (!left && !right) return BlockTileSrc(3, 3); // isolated middle
		if (!left && right) return BlockTileSrc(0, 3); // left edge
		if (!right && left) return BlockTileSrc(2, 3); // right edge
		return BlockTileSrc(1, 3); // middle row
	}

	// Use simple 3x3 framing tiles: row 0 top with grass, row1 middle, row2 bottom
	if (!up) {
		if (left && right && !downLeft && !downRight) return BlockTileSrc(9, 3); // inner bottom
		if (left && right && !downLeft) return BlockTileSrc(7, 0); // inner bottom-left
		if (left && right && !downRight) return BlockTileSrc(6, 0); // inner bottom-right
		if (!left && !right) return BlockTileSrc(3, 0); // isolated top
		if (!left && !downRight) return BlockTileSrc(4, 0); // inner bottom-right
		if (!left) return BlockTileSrc(0, 0); // top-left corner
		if (!right && !downLeft) return BlockTileSrc(5, 0); // inner bottom-right
		if (!right) return BlockTileSrc(2, 0); // top-right corner
		return BlockTileSrc(1, 0); // top edge
	}

	if (!down) {
		if (left && right && !upLeft && !upRight) return BlockTileSrc(8, 3); // inner top
		if (left && right && !upLeft) return BlockTileSrc(7, 1); // inner top-left
		if (left && right && !upRight) return BlockTileSrc(6, 1); // inner top-right
		if (!left && !right) return BlockTileSrc(3, 2); // isolated bottom
		if (!left && !upRight) return BlockTileSrc(4, 1); // inner top-right
		if (!left) return BlockTileSrc(0, 2); // bottom-left
		if (!right && !upLeft) return BlockTileSrc(5, 1); // inner top-left
		if (!right) return BlockTileSrc(2, 2); // bottom-right
		return BlockTileSrc(1, 2); // bottom edge
	}

	if (left && right) {
		if (!downLeft && !downRight && !upLeft && !upRight) return BlockTileSrc(8, 1);
		if (upLeft && upRight && !downLeft && !downRight) return BlockTileSrc(9, 2);
		if (!upLeft && !downLeft && upRight && downRight) return BlockTileSrc(9, 0);
		if (upLeft && downLeft && !upRight && !downRight) return BlockTileSrc(8, 0);
		if (!upLeft && !upRight && !downRight && downLeft) return BlockTileSrc(9, 1); // inner bottom-left
		if (!upLeft && !upRight && downRight && !downLeft) return BlockTileSrc(10, 1); // inner bottom-left
		if (upLeft && downRight && !upRight && !downLeft) return BlockTileSrc(10, 2); // inner top-right
		if (!upLeft && !downRight && upRight && downLeft) return BlockTileSrc(10, 3); // inner top-left
		if (!downRight && !downLeft && !upRight && upLeft) return BlockTileSrc(11, 2); 
		if (!downRight && !downLeft && upRight && !upLeft) return BlockTileSrc(11, 3); 
		if (!upLeft && !upRight) return BlockTileSrc(8, 2); // inner top
		if (!upLeft) return BlockTileSrc(5, 3); // inner top-left
		if (!upRight) return BlockTileSrc(4, 3); // inner top-right
		if (!downRight) return BlockTileSrc(4, 2); // inner bottom
	}

	// middle row
	if (!left && right) {
		if (!upRight && !downRight) return BlockTileSrc(4, 0);
		if (!downRight) return BlockTileSrc(6, 2);
		if (!upRight) return BlockTileSrc(6, 3);
		return BlockTileSrc(0, 1);
	}
	if (!right && left) {
		if (!upLeft && !downLeft) return BlockTileSrc(5, 0);
		if (!downLeft) return BlockTileSrc(7, 2);
		if (!upLeft) return BlockTileSrc(7, 3);
		return BlockTileSrc(2, 1);
	}
	if (!right && !left) return BlockTileSrc(3, 1);

	return BlockTileSrc(1, 1); // fully surrounded
}

static void DrawBlock(Rectangle dest, Rectangle srcOverride) {
	if (gBlockTileset.id == 0) {
		DrawRectangleRec(dest, GRAY);
		return;
	}
	Rectangle src = srcOverride;
	DrawTexturePro(gBlockTileset, src, dest, (Vector2){0, 0}, 0.0f, WHITE);
}

void RenderTiles(const LevelEditorState *ed) {
	for (int y = 0; y < GRID_ROWS; ++y) {
		for (int x = 0; x < GRID_COLS; ++x) {
			TileType t = ed->tiles[y][x];
			if (IsSolidTile(t)) {
				Rectangle r = TileRect(x, y);
				Rectangle src = ChooseBlockSrc(ed, x, y);
				DrawBlock(r, src);
			} else if (IsHazardTile(t)) {
				Rectangle lr = LaserStripeRect((Vector2){CellToWorld(x), CellToWorld(y)});
				DrawRectangleRec(lr, RED);
			}
		}
	}
}

void RenderTilesGameplay(const LevelEditorState *ed, const GameState *g) {
	int leftCell = WorldToCellX(g->playerPos.x + 1.0f);
	int rightCell = WorldToCellX(g->playerPos.x + PLAYER_W - 2.0f);
	int footCellY = WorldToCellY(g->playerPos.y + (g->crouching ? PLAYER_H_CROUCH : PLAYER_H) + 0.5f);
	for (int y = 0; y < GRID_ROWS; ++y) {
		for (int x = 0; x < GRID_COLS; ++x) {
			TileType t = ed->tiles[y][x];
			if (IsSolidTile(t)) {
				Rectangle r = TileRect(x, y);
				if (g->onGround && y == footCellY && x >= leftCell && x <= rightCell) {
					r.y += 1.0f;
				}
				Rectangle src = ChooseBlockSrc(ed, x, y);
				DrawBlock(r, src);
			} else if (IsHazardTile(t)) {
				Rectangle lr = LaserStripeRect((Vector2){CellToWorld(x), CellToWorld(y)});
				DrawRectangleRec(lr, RED);
			}
		}
	}
}

void DrawStats(const GameState *g) {
	DrawText(TextFormat("FPS: %d", GetFPS()), 10, 40, 20, RED);
	DrawText(TextFormat("Vel: (%.0f, %.0f)", g->playerVel.x, g->playerVel.y), 10, 70, 20, DARKGRAY);
	DrawText(g->onGround ? "Grounded" : "Air", 10, 100, 20, DARKGRAY);
	DrawText(TextFormat("Coy: %.2f  Buf: %.2f", g->coyoteTimer, g->jumpBufferTimer), 10, 130, 20, DARKGRAY);
}

// --- Player sprite rendering ---

static Texture2D gIdleTex = {0};
static Texture2D gRunTex = {0};
// Pre-flipped left-facing sheets to avoid per-draw flipping artifacts
static Texture2D gIdleTexL = {0};
static Texture2D gRunTexL = {0};

static float gRunDustTimer = 0.0f;

// --- Dust particles ---
typedef struct DustParticle {
	Vector2 pos;
	Vector2 vel;
	float radius;
	float lifetime;
	float age;
	Color color;
	bool active;
} DustParticle;

static DustParticle gDust[DUST_MAX];
static int gDustCursor = 0;

static float RandRange(float min, float max) {
	float t = (float)GetRandomValue(0, 1000000) / 1000000.0f;
	return min + t * (max - min);
}

static void Dust_Reset(void) {
	memset(gDust, 0, sizeof(gDust));
	gDustCursor = 0;
}

static void Dust_SpawnOne(Vector2 pos, Vector2 vel, float radius, float life) {
	int idx = gDustCursor;
	gDustCursor = (gDustCursor + 1) % DUST_MAX;
	gDust[idx].pos = pos;
	gDust[idx].vel = vel;
	gDust[idx].radius = radius;
	gDust[idx].lifetime = life;
	gDust[idx].age = 0.0f;
	gDust[idx].color = (Color){200, 200, 200, 255};
	gDust[idx].active = true;
}

static void Dust_Burst(Vector2 origin, float dirSign, int count, float baseSpeed) {
	for (int i = 0; i < count; ++i) {
		float vx = (baseSpeed * RandRange(0.55f, 0.95f)) * dirSign + RandRange(-40.0f, 40.0f);
		float vy = -fabsf(baseSpeed) * RandRange(0.35f, 0.55f);
		Vector2 vel = (Vector2){vx, vy};
		float radius = RandRange(3.0f, 7.0f);
		float life = RandRange(0.35f, 0.60f);
		Dust_SpawnOne(origin, vel, radius, life);
	}
}

void Render_SpawnJumpDust(const GameState *g) {
	Rectangle aabb = PlayerAABB(g);
	float footY = aabb.y + aabb.height * 1.5f;
	Vector2 center = (Vector2){aabb.x + aabb.width * 0.5f, footY};
	// Burst straight out from under the player to keep dust under the feet
	Dust_Burst(center, -0.4f, 12, 200.0f);
	Dust_Burst(center, 0.4f, 12, 200.0f);
	// Slightly upward puff to fill between feet
	Dust_Burst(center, 0.0f, 12, 180.0f);
}

void Render_SpawnLandDust(const GameState *g) {
	Rectangle aabb = PlayerAABB(g);
	Vector2 left = (Vector2){aabb.x + aabb.width * 0.2f, aabb.y + aabb.height * 1.5f};
	Vector2 right = (Vector2){aabb.x + aabb.width * 0.8f, left.y};
	float speed = 240.0f + fabsf(g->playerVel.x) * 0.2f;
	Dust_Burst(left, -1.0f, 8, speed);
	Dust_Burst(right, 1.0f, 8, speed);
}

void Render_SpawnWallJumpDust(const GameState *g, int wallDir) {
	Rectangle aabb = PlayerAABB(g);
	float x = (wallDir < 0) ? aabb.x - 2.0f : (aabb.x + aabb.width + 2.0f);
	float midY = aabb.y + aabb.height * 0.6f;
	Vector2 origin = (Vector2){x, midY};
	float dir = (wallDir < 0) ? -1.0f : 1.0f;
	Dust_Burst(origin, dir, 10, 240.0f);
}

void Render_SpawnDeathExplosion(const GameState *g) {
	Rectangle aabb = PlayerAABB(g);
	Vector2 center = (Vector2){aabb.x + aabb.width * 0.5f, aabb.y + aabb.height * 0.5f};
	for (int i = 0; i < 64; ++i) {
		float angle = RandRange(0.0f, 6.28318f);
		float speed = RandRange(180.0f, 360.0f);
		Vector2 vel = (Vector2){cosf(angle) * speed, sinf(angle) * speed};
		float r = RandRange(3.5f, 6.5f);
		float life = RandRange(0.35f, 0.6f);
		Dust_SpawnOne(center, vel, r, life);
		gDust[(gDustCursor + DUST_MAX - 1) % DUST_MAX].color = (Color){220, 40, 40, 255};
	}
}

static void Dust_Update(float dt) {
	for (int i = 0; i < DUST_MAX; ++i) {
		DustParticle *p = &gDust[i];
		if (!p->active) continue;
		p->age += dt;
		if (p->age >= p->lifetime) {
			p->active = false;
			continue;
		}
		p->vel.x *= DUST_DRAG;
		p->vel.y += DUST_GRAVITY * dt;
		p->pos.x += p->vel.x * dt;
		p->pos.y += p->vel.y * dt;
	}
}

static void Dust_Draw(void) {
	for (int i = 0; i < DUST_MAX; ++i) {
		const DustParticle *p = &gDust[i];
		if (!p->active) continue;
		float t = p->age / p->lifetime;
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;
		float radius = p->radius * (1.0f - 0.35f * t);
		unsigned char a = (unsigned char)(255.0f * (1.0f - t));
		Color c = p->color;
		c.a = a;
		DrawCircleV(p->pos, radius, c);
	}
}

void Render_DrawDust(float dt) {
	Dust_Update(dt);
	Dust_Draw();
}

bool Render_Init(void) {
	if (gIdleTex.id == 0) {
		gIdleTex = LoadTexture("assets/GlideManIdle.png");
		SetTextureFilter(gIdleTex, TEXTURE_FILTER_POINT);
	}
	if (gRunTex.id == 0) {
		gRunTex = LoadTexture("assets/GlideManRun.png");
		SetTextureFilter(gRunTex, TEXTURE_FILTER_POINT);
	}
	// Create left-facing variants by flipping images once at init
	if (gIdleTexL.id == 0) {
		Image img = LoadImage("assets/GlideManIdle.png");
		if (img.data != NULL) {
			ImageFlipHorizontal(&img);
			gIdleTexL = LoadTextureFromImage(img);
			UnloadImage(img);
			if (gIdleTexL.id != 0) SetTextureFilter(gIdleTexL, TEXTURE_FILTER_POINT);
		}
	}
	if (gRunTexL.id == 0) {
		Image img = LoadImage("assets/GlideManRun.png");
		if (img.data != NULL) {
			ImageFlipHorizontal(&img);
			gRunTexL = LoadTextureFromImage(img);
			UnloadImage(img);
			if (gRunTexL.id != 0) SetTextureFilter(gRunTexL, TEXTURE_FILTER_POINT);
		}
	}
	if (gBlockTileset.id == 0) {
		gBlockTileset = LoadTexture("assets/tilesetgrass.png");
		if (gBlockTileset.id != 0) {
			SetTextureFilter(gBlockTileset, TEXTURE_FILTER_POINT);
			gBlockTileCols = gBlockTileset.width / BLOCK_TILE_SIZE;
		}
	}
	Dust_Reset();
	// Return success if at least one of the core sprites loaded; fallback drawing still works
	bool spritesReady = (gIdleTex.id != 0) && (gRunTex.id != 0) && (gIdleTexL.id != 0) && (gRunTexL.id != 0);
	return spritesReady;
}

void Render_Deinit(void) {
	if (gIdleTex.id != 0) {
		UnloadTexture(gIdleTex);
		gIdleTex.id = 0;
	}
	if (gRunTex.id != 0) {
		UnloadTexture(gRunTex);
		gRunTex.id = 0;
	}
	if (gIdleTexL.id != 0) {
		UnloadTexture(gIdleTexL);
		gIdleTexL.id = 0;
	}
	if (gRunTexL.id != 0) {
		UnloadTexture(gRunTexL);
		gRunTexL.id = 0;
	}
	if (gBlockTileset.id != 0) {
		UnloadTexture(gBlockTileset);
		gBlockTileset.id = 0;
		gBlockTileCols = 0;
	}
	gRunDustTimer = 0.0f;
	Dust_Reset();
}

void RenderPlayer(const GameState *g) {
	// Choose animation based on simple state: running vs idle
	if (g->hidden) return;

	const float speed = (float)fabsf(g->playerVel.x);
	const float maxSpeedXNow = g->crouching ? MAX_SPEED_X_CROUCH : MAX_SPEED_X;
	const bool movingFast = speed > (0.5f * maxSpeedXNow);
	const bool running = g->onGround && movingFast;
	bool faceRight = g->facingRight;
	const Texture2D tex = running
	                          ? (faceRight ? gRunTex : gRunTexL)
	                          : (faceRight ? gIdleTex : gIdleTexL);
	// Destination rectangle: width = SQUARE_SIZE; height reflects crouch (half height)
	Rectangle aabb = PlayerAABB(g);
	float dstW = (float)SQUARE_SIZE * (g->spriteScaleX <= 0.0f ? 1.0f : g->spriteScaleX);
	// Scale height based on physics AABB vs standing height so crouch is half
	float heightScale = (float)(aabb.height / (float)PLAYER_H); // 1.0 standing, 0.5 crouched
	float squashY = g->spriteScaleY;
	if (squashY < 0.2f) squashY = 0.2f; // avoid collapsing
	float dstH = (float)SQUARE_SIZE * heightScale * squashY;
	float dstX = aabb.x + (aabb.width - dstW) * 0.5f; // center horizontally over physics
	float dstY = (aabb.y + aabb.height) - dstH; // keep feet anchored
	dstY += g->groundSink;
	Rectangle dst = (Rectangle){dstX, dstY, dstW, dstH};

	if (tex.id == 0) {
		// Fallback: draw a square at the sprite destination size
		DrawRectangleRec(dst, BLUE);
		return;
	}

	// Each sheet: 5 columns, 1 row, each 16x16
	const int cols = 5;
	const int fw = 16, fh = 16;
	const float fps = 10.0f; // animation speed
	int frame = (int)(g->runTime * fps) % cols;

	// Source rect, handle horizontal flip using negative width
	// For left-facing, offset x slightly from the exact frame boundary to avoid border sampling
	// Source rect: always positive width; left/right is baked into chosen texture
	Rectangle src = (Rectangle){(float)(frame * fw), 0.0f, (float)fw, (float)fh};

	float dt = GetFrameTime();
	if (dt > 0.033f) dt = 0.033f; // clamp for stability
	if (!running) gRunDustTimer = 0.0f;

	// Subtle running dust at any grounded speed
	gRunDustTimer += dt;
	if (running && gRunDustTimer >= 0.05f) {
		Rectangle baseAabb = PlayerAABB(g);
		float dir = g->playerVel.x >= 0.0f ? 1.0f : -1.0f;
		float x = (dir > 0.0f) ? (baseAabb.x + baseAabb.width + 2.0f) : (baseAabb.x - 2.0f);
		float y = baseAabb.y + baseAabb.height - 3.0f;
		float speedFrac = speed / maxSpeedXNow;
		if (speedFrac < 0.25f) speedFrac = 0.25f;
		if (speedFrac > 1.0f) speedFrac = 1.0f;
		for (int i = 0; i < 4; ++i) {
			Vector2 pos = (Vector2){x + RandRange(-2.0f, 2.0f), y + RandRange(-2.0f, 2.0f)};
			float mag = RandRange(50.0f, 110.0f) * speedFrac;
			Vector2 vel = (Vector2){-dir * mag, RandRange(-20.0f, 10.0f) * speedFrac};
			float r = RandRange(3.0f, 5.5f);
			Dust_SpawnOne(pos, vel, r, RandRange(0.24f, 0.36f));
		}
		gRunDustTimer = 0.0f;
	}

	DrawTexturePro(tex, src, dst, (Vector2){0, 0}, g->spriteRotation, WHITE);
}

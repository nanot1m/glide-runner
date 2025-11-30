#include "render.h"
#include <math.h>
#include <string.h>
#include "autotiler.h"
#include "raylib.h"

static Texture2D gBlockTileset = {0};
static const int BLOCK_TILE_SIZE = 32;
static int gBlockTileCols = 0;
static Texture2D gWarriorSheet = {0};
#ifndef WARRIOR_FRAME_W
#define WARRIOR_FRAME_W 69
#endif
#ifndef WARRIOR_FRAME_H
#define WARRIOR_FRAME_H 44
#endif
static const int WARRIOR_SHEET_COLS = 6;
static const int WARRIOR_SHEET_ROWS = 17;
static const int WARRIOR_TOTAL_FRAMES = WARRIOR_SHEET_COLS * WARRIOR_SHEET_ROWS;



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

// Callback function for autotiler to check if a block exists
static bool CheckBlockForAutotiler(const void *context, int cx, int cy) {
	const LevelEditorState *ed = (const LevelEditorState *)context;
	if (!InBoundsCell(cx, cy)) return false;
	return IsSolidTile(ed->tiles[cy][cx]);
}

static Rectangle ChooseBlockSrc(const LevelEditorState *ed, int cx, int cy) {
	// Pass context directly to autotiler
	return Autotiler_GetBlockTile(ed, cx, cy);
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
			} else if (IsSpawnerTile(t)) {
				Rectangle r = TileRect(x, y);
				Color c = (Color){120, 40, 200, 255};
				DrawRectangleRounded(r, 0.35f, 6, c);
				DrawRectangleLinesEx(r, 2.0f, (Color){90, 20, 160, 255});
			}
		}
	}
}

void RenderTilesGameplay(const LevelEditorState *ed, const GameState *g) {
	Rectangle aabb = PlayerAABB(g);
	int leftCell = WorldToCellX(aabb.x + 1.0f);
	int rightCell = WorldToCellX(aabb.x + aabb.width - 2.0f);
	int footCellY = WorldToCellY(aabb.y + aabb.height + 0.5f);
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
			} else if (IsSpawnerTile(t)) {
				Rectangle r = TileRect(x, y);
				Color c = (Color){120, 40, 200, 255};
				DrawRectangleRounded(r, 0.35f, 6, c);
				DrawRectangleLinesEx(r, 2.0f, (Color){90, 20, 160, 255});
			}
		}
	}
}

void DrawStats(const GameState *g) {
	float y = 40.0f;
	const float step = 18.0f;
	DrawText(TextFormat("FPS: %d", GetFPS()), 10, (int)y, 18, RED);
	y += step;
	DrawText(TextFormat("Pos: (%.0f, %.0f)", g->playerPos.x, g->playerPos.y), 10, (int)y, 18, DARKGRAY);
	y += step;
	DrawText(TextFormat("Vel: (%.0f, %.0f)", g->playerVel.x, g->playerVel.y), 10, (int)y, 18, DARKGRAY);
	y += step;
	DrawText(TextFormat("Ground: %s  WallSlide: %s", g->onGround ? "yes" : "no", g->wallSliding ? "yes" : "no"), 10, (int)y, 18, DARKGRAY);
	y += step;
	DrawText(TextFormat("Wall L/R: %d / %d  Stick: %.2f", g->wallContactLeft, g->wallContactRight, g->wallStickTimer), 10, (int)y, 18, DARKGRAY);
	y += step;
	DrawText(TextFormat("Crouch: %s  Facing: %s", g->crouching ? "yes" : "no", g->facingRight ? "R" : "L"), 10, (int)y, 18, DARKGRAY);
	y += step;
	DrawText(TextFormat("Coyote: %.2f  WallCoy: %.2f", g->coyoteTimer, g->wallCoyoteTimer), 10, (int)y, 18, DARKGRAY);
	y += step;
	DrawText(TextFormat("JumpBuf: %.2f  Dash:%d Slide:%d Ladder:%d", g->jumpBufferTimer, g->animDash, g->animSlide, g->animLadder), 10, (int)y, 18, DARKGRAY);
}

// --- Player sprite rendering ---

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

typedef struct {
	int startFrame;
	int frameCount;
	float fps;
	bool loop;
} WarriorAnim;

enum {
	WA_IDLE,
	WA_RUN,
	WA_ATTACK,
	WA_DEATH,
	WA_HURT,
	WA_JUMP,
	WA_UP_TO_FALL,
	WA_FALL,
	WA_EDGE_GRAB,
	WA_EDGE_IDLE,
	WA_WALL,
	WA_CROUCH_DOWN,
	WA_CROUCH_UP,
	WA_DASH,
	WA_DASH_ATTACK,
	WA_SLIDE,
	WA_SLIDE_EXIT,
	WA_LADDER,
};

static WarriorAnim gWarriorAnims[] = {
    [WA_IDLE] = {.startFrame = 0, .frameCount = 6, .fps = 8.0f, .loop = true},
    [WA_RUN] = {.startFrame = 6, .frameCount = 8, .fps = 14.0f, .loop = true},
    [WA_ATTACK] = {.startFrame = 14, .frameCount = 12, .fps = 14.0f, .loop = false},
    [WA_DEATH] = {.startFrame = 26, .frameCount = 11, .fps = 10.0f, .loop = false},
    [WA_HURT] = {.startFrame = 37, .frameCount = 4, .fps = 12.0f, .loop = false},
    [WA_JUMP] = {.startFrame = 41, .frameCount = 3, .fps = 12.0f, .loop = false},
    [WA_UP_TO_FALL] = {.startFrame = 44, .frameCount = 2, .fps = 10.0f, .loop = false},
    [WA_FALL] = {.startFrame = 46, .frameCount = 3, .fps = 10.0f, .loop = true},
    [WA_EDGE_GRAB] = {.startFrame = 49, .frameCount = 5, .fps = 10.0f, .loop = false},
    [WA_EDGE_IDLE] = {.startFrame = 54, .frameCount = 6, .fps = 8.0f, .loop = true},
    [WA_WALL] = {.startFrame = 60, .frameCount = 3, .fps = 8.0f, .loop = true},
    [WA_CROUCH_DOWN] = {.startFrame = 63, .frameCount = 3, .fps = 10.0f, .loop = false},
    [WA_CROUCH_UP] = {.startFrame = 66, .frameCount = 3, .fps = 10.0f, .loop = false},
    [WA_DASH] = {.startFrame = 69, .frameCount = 7, .fps = 14.0f, .loop = true},
    [WA_DASH_ATTACK] = {.startFrame = 76, .frameCount = 10, .fps = 14.0f, .loop = true},
    [WA_SLIDE] = {.startFrame = 86, .frameCount = 3, .fps = 12.0f, .loop = false}, // enter slide
    [WA_SLIDE_EXIT] = {.startFrame = 89, .frameCount = 2, .fps = 12.0f, .loop = false}, // exit slide
    [WA_LADDER] = {.startFrame = 91, .frameCount = 8, .fps = 10.0f, .loop = true},
};

static Rectangle WarriorFrameRect(int frameIndex) {
	int col = frameIndex % WARRIOR_SHEET_COLS;
	int row = frameIndex / WARRIOR_SHEET_COLS;
	return (Rectangle){(float)(col * WARRIOR_FRAME_W), (float)(row * WARRIOR_FRAME_H), (float)WARRIOR_FRAME_W, (float)WARRIOR_FRAME_H};
}

static void RenderPlayerWarrior(const GameState *g) {
	if (g->hidden || gWarriorSheet.id == 0) return;
	static int sLastAnim = -1;
	static float sAnimTime = 0.0f;
	static float sLastRunTime = -1.0f;
	static bool sPrevSlide = false;
	static float sSlideExitTimer = 0.0f;

	float dt = GetFrameTime();
	if (dt > 0.033f) dt = 0.033f;
	if (sLastRunTime < 0.0f || g->runTime < sLastRunTime) {
		sLastAnim = -1;
		sAnimTime = 0.0f;
		sPrevSlide = false;
		sSlideExitTimer = 0.0f;
	}
	sLastRunTime = g->runTime;

	float speed = fabsf(g->playerVel.x);
	float maxSpeedXNow = g->crouching ? MAX_SPEED_X_CROUCH : MAX_SPEED_X;
	bool wallBlocked = g->onGround && (g->wallContactLeft || g->wallContactRight);
	bool running = g->onGround && !wallBlocked && speed > (0.1f * maxSpeedXNow);
	bool rising = (!g->onGround) && g->playerVel.y < -40.0f;
	bool falling = (!g->onGround) && g->playerVel.y > 60.0f;
	bool atPeak = (!g->onGround) && !rising && !falling;
	bool edgeHang = g->edgeHang;
	bool wallStick = (!g->onGround) && !edgeHang && (g->wallSliding || g->wallContactLeft || g->wallContactRight || g->wallStickTimer > 0.0f);
	bool dying = Game_IsDying();
	bool hurt = g->hurtTimer > 0.0f;
	float overrideT = -1.0f;
	float slideExitDuration = (float)gWarriorAnims[WA_SLIDE_EXIT].frameCount / gWarriorAnims[WA_SLIDE_EXIT].fps;
	float edgeGrabDuration = (float)gWarriorAnims[WA_EDGE_GRAB].frameCount / gWarriorAnims[WA_EDGE_GRAB].fps;

	bool sliding = g->animSlide;
	if (sliding) {
		sSlideExitTimer = 0.0f; // cancel exit if re-entering slide
	} else if (sPrevSlide && slideExitDuration > 0.0f) {
		sSlideExitTimer = slideExitDuration;
	}
	sPrevSlide = sliding;
	if (sSlideExitTimer > 0.0f) {
		sSlideExitTimer -= dt;
		if (sSlideExitTimer < 0.0f) sSlideExitTimer = 0.0f;
	}
	bool playingSlideExit = sSlideExitTimer > 0.0f;
	bool enteringEdgeHang = edgeHang && sLastAnim != WA_EDGE_GRAB && sLastAnim != WA_EDGE_IDLE;
	bool continuingEdgeGrab = (sLastAnim == WA_EDGE_GRAB) && (sAnimTime < edgeGrabDuration);

	int anim = WA_IDLE;
	if (dying) {
		anim = WA_DEATH;
	} else if (hurt) {
		anim = WA_HURT;
	} else if (edgeHang) {
		anim = (enteringEdgeHang || continuingEdgeGrab) ? WA_EDGE_GRAB : WA_EDGE_IDLE;
	} else if (g->animLadder) {
		anim = WA_LADDER;
	} else if (wallStick) {
		anim = WA_WALL;
		overrideT = 0.0f; // lock to first frame to avoid jittery looping
	} else if (sliding) {
		anim = WA_SLIDE;
	} else if (playingSlideExit) {
		anim = WA_SLIDE_EXIT;
		overrideT = slideExitDuration - sSlideExitTimer;
	} else if (g->animDash) {
		anim = WA_DASH;
	} else if (!g->onGround) {
		if (rising) {
			anim = WA_JUMP;
		} else if (atPeak) {
			anim = WA_UP_TO_FALL;
		} else {
			anim = WA_FALL;
		}
	} else if (g->crouchAnimDir == 1) {
		anim = WA_CROUCH_DOWN;
		overrideT = g->crouchAnimTime;
	} else if (g->crouchAnimDir == -1) {
		anim = WA_CROUCH_UP;
		overrideT = g->crouchAnimTime;
	} else if (g->crouching) {
		anim = WA_CROUCH_DOWN;
		// Hold final crouch frame
		WarriorAnim hold = gWarriorAnims[WA_CROUCH_DOWN];
		overrideT = (hold.frameCount - 1) / hold.fps;
	} else if (running) {
		anim = WA_RUN;
	}

	if (anim != sLastAnim) {
		sAnimTime = 0.0f;
		sLastAnim = anim;
	} else {
		sAnimTime += dt;
	}

	WarriorAnim def = gWarriorAnims[anim];
	float t = (overrideT >= 0.0f) ? overrideT : sAnimTime;
	int frame = (int)floorf(t * def.fps);
	if (!def.loop) {
		if (frame >= def.frameCount) frame = def.frameCount - 1;
	} else {
		frame = frame % def.frameCount;
	}
	int spriteFrame = def.startFrame + frame;
	if (spriteFrame >= WARRIOR_TOTAL_FRAMES) spriteFrame = WARRIOR_TOTAL_FRAMES - 1;
	Rectangle src = WarriorFrameRect(spriteFrame);

	float scaleX = WARRIOR_SCALE;
	float scaleY = WARRIOR_SCALE;
	float pivotSrcX = (float)WARRIOR_FRAME_W * 0.38f;
	float pivotSrcY = (float)WARRIOR_FRAME_H * 0.5f;
	float pivotWorldX = pivotSrcX * scaleX;
	float pivotWorldY = pivotSrcY * scaleY;

	Vector2 origin = (Vector2){pivotWorldX, pivotWorldY};

	float dstW = (float)WARRIOR_FRAME_W * scaleX;
	float dstH = (float)WARRIOR_FRAME_H * scaleY;

	if (!g->facingRight) {
		src.width = -src.width;
		src.x -= pivotSrcX * 0.5f;
	}
	Rectangle aabb = PlayerAABB(g);

	float dstX = g->playerPos.x - dstW * 0.5f + pivotWorldX * 1.2f;
	// float dstY = g->playerPos.y - dstH * 0.5f + pivotWorldY; // + g->groundSink;
	float dstY = aabb.y + aabb.height - dstH / 2; // align to feet

	Rectangle dst = (Rectangle){dstX, dstY, dstW, dstH};

	DrawTexturePro(gWarriorSheet, src, dst, origin, g->spriteRotation, WHITE);
#if DEBUG_DRAW_BOUNDS
	DrawRectangleLinesEx(aabb, 1.0f, RED);
	DrawCircleV(g->playerPos, 2.0f, YELLOW);
#endif
}

bool Render_Init(void) {
	if (gBlockTileset.id == 0) {
		gBlockTileset = LoadTexture("assets/tilesetgrass.png");
		if (gBlockTileset.id != 0) {
			SetTextureFilter(gBlockTileset, TEXTURE_FILTER_POINT);
			gBlockTileCols = gBlockTileset.width / BLOCK_TILE_SIZE;
		}
	}
	if (gWarriorSheet.id == 0) {
		gWarriorSheet = LoadTexture("assets/warrior_sheet.png");
		if (gWarriorSheet.id != 0) SetTextureFilter(gWarriorSheet, TEXTURE_FILTER_POINT);
	}
	// Initialize autotiler with tilemap layout
	TilemapLayout layout = {
	    // Row with no vertical neighbors
	    .rowNoVertical_isolated = {3, 3},
	    .rowNoVertical_leftEdge = {0, 3},
	    .rowNoVertical_rightEdge = {2, 3},
	    .rowNoVertical_middle = {1, 3},
	    // Top band
	    .topBand_isolated = {3, 0},
	    .topBand_innerBottom = {9, 3},
	    .topBand_innerBottomLeft = {7, 0},
	    .topBand_innerBottomRight = {6, 0},
	    .topBand_edge = {1, 0},
	    .topBand_innerBottomRightNoDownRight = {4, 0},
	    .topBand_topLeftCorner = {0, 0},
	    .topBand_innerBottomLeftNoDownLeft = {5, 0},
	    .topBand_topRightCorner = {2, 0},
	    // Bottom band
	    .bottomBand_isolated = {3, 2},
	    .bottomBand_innerTop = {8, 3},
	    .bottomBand_innerTopLeft = {7, 1},
	    .bottomBand_innerTopRight = {6, 1},
	    .bottomBand_edge = {1, 2},
	    .bottomBand_innerTopRightNoUpRight = {4, 1},
	    .bottomBand_bottomLeft = {0, 2},
	    .bottomBand_innerTopLeftNoUpLeft = {5, 1},
	    .bottomBand_bottomRight = {2, 2},
	    // Interior with sides
	    .interior_allDiagonalsOpen = {8, 1},
	    .interior_upDiagonals = {9, 2},
	    .interior_rightDiagonals = {9, 0},
	    .interior_leftDiagonals = {8, 0},
	    .interior_downLeft = {9, 1},
	    .interior_downRight = {10, 1},
	    .interior_upLeftDownRight = {10, 2},
	    .interior_upRightDownLeft = {10, 3},
	    .interior_upLeft = {11, 2},
	    .interior_upRight = {11, 3},
	    .interior_upDiagonalsOpen = {8, 2},
	    .interior_upLeftOpen = {5, 3},
	    .interior_upRightOpen = {4, 3},
	    .interior_downRightOpen = {4, 2},
	    .interior_full = {1, 1},
	    // Open left
	    .openLeft_allOpen = {4, 0},
	    .openLeft_downRightOpen = {6, 2},
	    .openLeft_upRightOpen = {6, 3},
	    .openLeft_leftEdge = {0, 1},
	    // Open right
	    .openRight_allOpen = {5, 0},
	    .openRight_downLeftOpen = {7, 2},
	    .openRight_upLeftOpen = {7, 3},
	    .openRight_rightEdge = {2, 1},
	    // Isolated tiles
	    .isolated_vertical = {3, 1},
	    .isolated_full = {1, 1}};
	AutotilerConfig autotilerConfig = {
	    .tileSize = BLOCK_TILE_SIZE,
	    .checkBlock = CheckBlockForAutotiler,
	    .layout = layout};
	bool autotilerReady = Autotiler_Init(&autotilerConfig);
	Dust_Reset();
	// Return success if at least one of the core sprites loaded; fallback drawing still works
	bool spritesReady = (gWarriorSheet.id != 0);
	return spritesReady && autotilerReady;
}

void Render_Deinit(void) {
	if (gBlockTileset.id != 0) {
		UnloadTexture(gBlockTileset);
		gBlockTileset.id = 0;
		gBlockTileCols = 0;
	}
	if (gWarriorSheet.id != 0) {
		UnloadTexture(gWarriorSheet);
		gWarriorSheet.id = 0;
	}
	gRunDustTimer = 0.0f;
	Dust_Reset();
}

void RenderPlayer(const GameState *g) {
	// Choose animation based on simple state: running vs idle
	if (g->hidden) return;

	RenderPlayerWarrior(g);
}

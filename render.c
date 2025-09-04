#include "render.h"
#include <math.h>
#include <string.h>
#include "raylib.h"

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

void RenderTiles(const LevelEditorState *ed) {
	for (int y = 0; y < GRID_ROWS; ++y) {
		for (int x = 0; x < GRID_COLS; ++x) {
			TileType t = ed->tiles[y][x];
			if (IsSolidTile(t)) {
				Rectangle r = TileRect(x, y);
				DrawRectangleRec(r, GRAY);
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

// --- Ghost trail state ---
typedef struct GhostSample {
    Rectangle dst;      // destination rect used for rendering at capture time
    bool facingRight;   // sprite flip state
    int frame;          // animation frame index at capture
    float age;          // seconds since captured
} GhostSample;

// Tunables
#define GHOST_MAX 8
#define GHOST_SAMPLE_DT 0.05f   // seconds between samples while running
#define GHOST_LIFETIME 0.40f    // fade out duration

static GhostSample gGhosts[GHOST_MAX];
static int gGhostCount = 0;
static float gGhostSampleTimer = 0.0f;

void Render_Init(void) {
	if (gIdleTex.id == 0) {
		gIdleTex = LoadTexture("assets/GlideManIdle.png");
		SetTextureFilter(gIdleTex, TEXTURE_FILTER_POINT);
	}
	if (gRunTex.id == 0) {
		gRunTex = LoadTexture("assets/GlideManRun.png");
		SetTextureFilter(gRunTex, TEXTURE_FILTER_POINT);
	}
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
    // Clear ghost trail
    gGhostCount = 0;
    gGhostSampleTimer = 0.0f;
}

void RenderPlayer(const GameState *g) {
    // Choose animation based on simple state: running vs idle
    const float speed = (float)fabsf(g->playerVel.x);
    const float maxSpeedXNow = g->crouching ? MAX_SPEED_X_CROUCH : MAX_SPEED_X;
    const bool movingFast = speed > (0.5f * maxSpeedXNow);
    const bool running = g->onGround && movingFast;
    const Texture2D tex = running ? gRunTex : gIdleTex;
	// Destination rectangle: width = SQUARE_SIZE; height reflects crouch (half height)
	Rectangle aabb = PlayerAABB(g);
	float dstW = (float)SQUARE_SIZE;
	// Scale height based on physics AABB vs standing height so crouch is half
	float heightScale = (float)(aabb.height / (float)PLAYER_H); // 1.0 standing, 0.5 crouched
	float dstH = (float)SQUARE_SIZE * heightScale;
	float dstX = aabb.x + (aabb.width - dstW) * 0.5f; // center horizontally over physics
	float dstY = (aabb.y + aabb.height) - dstH; // keep feet anchored
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
    bool faceRight = g->facingRight;
    Rectangle src;
    if (faceRight) {
        src = (Rectangle){(float)(frame * fw), 0.0f, (float)fw, (float)fh};
    } else {
        src = (Rectangle){(float)((frame + 1) * fw), 0.0f, (float)-fw, (float)fh};
    }

    // --- Update ghost trail ---
    float dt = GetFrameTime();
    if (dt > 0.033f) dt = 0.033f; // clamp for stability
    // Age existing ghosts and drop expired ones
    for (int i = 0; i < gGhostCount; ++i) {
        gGhosts[i].age += dt;
    }
    while (gGhostCount > 0 && gGhosts[gGhostCount - 1].age > GHOST_LIFETIME) {
        gGhostCount--;
    }
    // Take samples while running, spaced in time
    gGhostSampleTimer += dt;
    if (movingFast && gRunTex.id != 0 && gGhostSampleTimer >= GHOST_SAMPLE_DT) {
        // Insert at front, shift others back
        if (gGhostCount < GHOST_MAX) {
            // make room for new head
            if (gGhostCount > 0) memmove(&gGhosts[1], &gGhosts[0], sizeof(GhostSample) * gGhostCount);
            gGhostCount++;
        } else {
            // shift and overwrite the last (drop oldest)
            memmove(&gGhosts[1], &gGhosts[0], sizeof(GhostSample) * (GHOST_MAX - 1));
        }
        gGhosts[0].dst = dst;
        gGhosts[0].facingRight = faceRight;
        gGhosts[0].frame = frame;
        gGhosts[0].age = 0.0f;
        gGhostSampleTimer = 0.0f;
    }

    // Draw ghosts first so player appears on top
    for (int i = 0; i < gGhostCount; ++i) {
        const GhostSample *gh = &gGhosts[i];
        float t = gh->age / GHOST_LIFETIME; // 0..1
        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
        // Alpha fades out; newer ghosts are brighter
        float alpha = (1.0f - t) * 0.5f; // up to 50% opacity
        unsigned char a = (unsigned char)(alpha * 255.0f);
        Rectangle gsrc;
        if (gh->facingRight) {
            gsrc = (Rectangle){(float)(gh->frame * fw), 0.0f, (float)fw, (float)fh};
        } else {
            gsrc = (Rectangle){(float)((gh->frame + 1) * fw), 0.0f, (float)-fw, (float)fh};
        }
        DrawTexturePro(gRunTex, gsrc, gh->dst, (Vector2){0, 0}, 0.0f, (Color){255, 255, 255, a});
    }

    // Draw current sprite on top with no rotation, origin at top-left
    DrawTexturePro(tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
}

#include "raylib.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h> // _mkdir
#else
#include <sys/stat.h> // mkdir
#include <sys/types.h>
#include <unistd.h>
#endif
#include <errno.h>
#include <math.h>

// Global victory flag
static bool victory = false;
static bool death = false;

// Editor constants
#define MAX_SQUARES 1024
#define SQUARE_SIZE 20
#define LEVEL_FILE "levels/level1.txt"

// Window constants
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// --- Physics tuning (feel free to tweak) ---
#define GRAVITY 1800.0f        // px/s^2
#define MOVE_ACCEL 7000.0f     // px/s^2
#define AIR_ACCEL 4200.0f      // px/s^2
#define GROUND_FRICTION 0.88f  // applied each frame on ground
#define AIR_FRICTION 0.985f    // applied each frame in air (weak)
#define MAX_SPEED_X 420.0f     // px/s
#define MAX_SPEED_Y 1400.0f    // px/s
#define JUMP_SPEED -620.0f     // px/s (negative is up)
#define COYOTE_TIME 0.12f      // s — jump grace after leaving ground
#define JUMP_BUFFER_TIME 0.12f // s — remember jump before landing

// Ground contact tolerance: keeps player grounded briefly to avoid flicker
#define GROUND_STICK_TIME 0.030f // 30 ms

// Player hitbox (smaller than a tile so the player fits 1-tile gaps)
#define PLAYER_W ((float)SQUARE_SIZE * 0.8f)
#define PLAYER_H ((float)SQUARE_SIZE * 0.9f)

// Crouch tuning
#define PLAYER_H_CROUCH (PLAYER_H * 0.5f)
#define MAX_SPEED_X_CROUCH 420.0f // px/s while crouching
#define CROUCH_FRICTION 0.97f     // weaker ground damping while crouching

// Wall interaction tuning
#define WALL_JUMP_PUSH_X 420.0f    // horizontal impulse on wall jump (px/s)
#define WALL_SLIDE_MAX_FALL 260.0f // cap downward speed while sliding on wall (px/s)

// Snap a pixel position to the editor grid
static inline Vector2 SnapToGrid(Vector2 p)
{
   int gx = ((int)p.x / SQUARE_SIZE) * SQUARE_SIZE;
   int gy = ((int)p.y / SQUARE_SIZE) * SQUARE_SIZE;
   if (gx < 0)
      gx = 0;
   if (gx > WINDOW_WIDTH - SQUARE_SIZE)
      gx = WINDOW_WIDTH - SQUARE_SIZE;
   if (gy < 0)
      gy = 0;
   if (gy > WINDOW_HEIGHT - SQUARE_SIZE)
      gy = WINDOW_HEIGHT - SQUARE_SIZE;
   return (Vector2){(float)gx, (float)gy};
}

// Screen states
typedef enum
{
   SCREEN_MENU,
   SCREEN_LEVEL_EDITOR,
   SCREEN_GAME_LEVEL,
   SCREEN_VICTORY,
   SCREEN_DEATH
} ScreenState;

// Menu options
#define MENU_OPTION_COUNT 2
typedef enum
{
   MENU_LEVEL_EDITOR,
   MENU_GAME_LEVEL
} MenuOption;

typedef struct
{
   Vector2 pos;
} EditorSquare;

typedef enum
{
   TOOL_PLAYER,
   TOOL_ADD_BLOCK,
   TOOL_REMOVE_BLOCK,
   TOOL_EXIT,
   TOOL_LASER_TRAP,
   TOOL_COUNT
} EditorTool;

typedef struct
{
   Vector2 cursor;
   EditorSquare squares[MAX_SQUARES];
   int squareCount;
   EditorSquare lasers[MAX_SQUARES];
   int laserCount;
   EditorTool tool;
} LevelEditorState;

// Editor state (global for simplicity)
static LevelEditorState editor = {0};

// Editor input timing
static double arrowLastTime = 0;
static double arrowInterval = 0.2; // 200ms

static void EnsureLevelsDir(void)
{
#ifdef _WIN32
   if (_mkdir("levels") == -1 && errno != EEXIST)
   {
      // ignore errors other than "already exists"
   }
#else
   if (mkdir("levels", 0755) == -1 && errno != EEXIST)
   {
      // ignore errors other than "already exists"
   }
#endif
}

// Define game state structure
typedef struct GameState
{
   int score;
   Vector2 playerPos;     // top-left corner of player AABB
   Vector2 playerVel;     // px/s
   bool onGround;         // is standing on a block or floor
   float coyoteTimer;     // seconds left to allow jump after leaving ground
   float jumpBufferTimer; // seconds left to consume buffered jump
   Vector2 exitPos;
   bool crouching;         // crouch state
   float groundStickTimer; // seconds to remain grounded after contact
   // Add more game variables as needed
} GameState;

// ----------------------
// Rendering & AABB helpers
// ----------------------
static inline float PlayerCurrentHeight(const GameState *g)
{
    return g->crouching ? PLAYER_H_CROUCH : PLAYER_H;
}

static inline Rectangle PlayerAABB(const GameState *g)
{
    float h = PlayerCurrentHeight(g);
    return (Rectangle){ g->playerPos.x, g->playerPos.y, PLAYER_W, h };
}

static inline Rectangle ExitAABB(const GameState *g)
{
    return (Rectangle){ g->exitPos.x, g->exitPos.y, (float)SQUARE_SIZE, (float)SQUARE_SIZE };
}

static inline Rectangle LaserStripeRect(Vector2 laserPos)
{
    return (Rectangle){ laserPos.x, laserPos.y + 1.0f, (float)SQUARE_SIZE, 3.0f };
}

// Draw all solid tiles (blocks) and laser stripes used by both editor and game
static void RenderTiles(const LevelEditorState *ed)
{
    for (int i = 0; i < ed->squareCount; ++i)
    {
        DrawRectangleV(ed->squares[i].pos, (Vector2){SQUARE_SIZE, SQUARE_SIZE}, GRAY);
    }
    for (int i = 0; i < ed->laserCount; ++i)
    {
        Rectangle lr = LaserStripeRect(ed->lasers[i].pos);
        DrawRectangleRec(lr, RED);
    }
}

static void DrawStats(const GameState *g)
{
    DrawText(TextFormat("FPS: %d", GetFPS()), 10, 40, 20, RED);
    DrawText(TextFormat("Vel: (%.0f, %.0f)", g->playerVel.x, g->playerVel.y), 10, 70, 20, DARKGRAY);
    DrawText(g->onGround ? "Grounded" : "Air", 10, 100, 20, DARKGRAY);
    DrawText(TextFormat("Coy: %.2f  Buf: %.2f", g->coyoteTimer, g->jumpBufferTimer), 10, 130, 20, DARKGRAY);
}

static void RenderMessageScreen(const char *title, const char *subtitle, Color accent)
{
    int cx = WINDOW_WIDTH / 2;
    int cy = WINDOW_HEIGHT / 2;
    int titleW = MeasureText(title, 40);
    DrawText(title, cx - titleW / 2, cy - 60, 40, accent);
    int subW = MeasureText(subtitle, 24);
    DrawText(subtitle, cx - subW / 2, cy - 10, 24, DARKGRAY);
    const char *hint = "Press Enter/Space/Esc to return to menu";
    int hintW = MeasureText(hint, 20);
    DrawText(hint, cx - hintW / 2, cy + 40, 20, BLUE);
}

// ---- Grid collision helpers ----
static inline int WorldToCellX(float x) { return (int)floorf(x / (float)SQUARE_SIZE); }
static inline int WorldToCellY(float y) { return (int)floorf(y / (float)SQUARE_SIZE); }
static inline float CellToWorld(int c) { return (float)(c * SQUARE_SIZE); }

// Helper: check if block exists at position
int FindBlockIndex(const LevelEditorState *ed, Vector2 pos)
{
   for (int i = 0; i < ed->squareCount; i++)
   {
      if (ed->squares[i].pos.x == pos.x && ed->squares[i].pos.y == pos.y)
      {
         return i;
      }
   }
   return -1;
}

static bool BlockAtCell(int cx, int cy)
{
   if (cx < 0 || cy < 0)
      return true; // treat outside as solid
   if (CellToWorld(cx) > WINDOW_WIDTH - SQUARE_SIZE)
      return true;
   if (CellToWorld(cy) > WINDOW_HEIGHT - SQUARE_SIZE)
      return true;
   Vector2 tilePos = {CellToWorld(cx), CellToWorld(cy)};
   return FindBlockIndex(&editor, tilePos) != -1;
}

// Check if any grid cell overlapped by an AABB is solid
static bool AABBOverlapsSolid(float x, float y, float w, float h)
{
   int left = WorldToCellX(x);
   int right = WorldToCellX(x + w - 0.001f);
   int top = WorldToCellY(y);
   int bottom = WorldToCellY(y + h - 0.001f);
   for (int cy = top; cy <= bottom; ++cy)
   {
      for (int cx = left; cx <= right; ++cx)
      {
         if (BlockAtCell(cx, cy))
            return true;
      }
   }
   return false;
}

// Resolve movement along one axis (swept), returns corrected position and zeroes velocity on impact
static void ResolveAxis(float *pos, float *vel, float other, float w, float h, bool vertical)
{
   // Move step by step to the boundary of the next cell to avoid tunneling
   float remaining = *vel * GetFrameTime();
   if (remaining == 0.0f)
      return;
   float sign = (remaining > 0) ? 1.0f : -1.0f;
   while (remaining != 0.0f)
   {
      float step = remaining;
      // limit step to at most one cell to avoid skipping
      float maxStep = (float)SQUARE_SIZE - 1.0f;
      if (step > maxStep)
         step = maxStep;
      if (step < -maxStep)
         step = -maxStep;

      float newPos = *pos + step;
      float x = vertical ? other : newPos;
      float y = vertical ? newPos : other;
      if (AABBOverlapsSolid(x, y, w, h))
      {
         // move to contact by moving pixel-by-pixel opposite direction until free
         int pixels = (int)fabsf(step);
         for (int i = 0; i < pixels; ++i)
         {
            newPos = *pos + sign; // move 1 pixel
            x = vertical ? other : newPos;
            y = vertical ? newPos : other;
            if (AABBOverlapsSolid(x, y, w, h))
            {
               // collision at this pixel; stop just before contact
               *vel = 0.0f;
               return;
            }
            *pos = newPos;
         }
      }
      else
      {
         *pos = newPos;
      }
      remaining -= step;
   }
}

// Save level to file
void SaveLevel(const GameState *game, const LevelEditorState *editor)
{
   EnsureLevelsDir();
   FILE *f = fopen(LEVEL_FILE, "w");
   if (!f)
      return;
   // Save player position
   fprintf(f, "PLAYER %d %d\n", (int)game->playerPos.x, (int)game->playerPos.y);
   // Save exit position
   fprintf(f, "EXIT %d %d\n", (int)game->exitPos.x, (int)game->exitPos.y);
   // Save blocks
   for (int i = 0; i < editor->squareCount; i++)
   {
      fprintf(f, "BLOCK %d %d\n", (int)editor->squares[i].pos.x, (int)editor->squares[i].pos.y);
   }
   // Save lasers
   for (int i = 0; i < editor->laserCount; i++)
   {
      fprintf(f, "LASER %d %d\n", (int)editor->lasers[i].pos.x, (int)editor->lasers[i].pos.y);
   }
   fclose(f);
}

// Load level from file
bool LoadLevel(GameState *game, LevelEditorState *editor)
{
   FILE *f = fopen(LEVEL_FILE, "r");
   if (!f)
      return false;
   char type[16];
   int x, y;
   editor->squareCount = 0;
   editor->laserCount = 0;
   while (fscanf(f, "%15s %d %d", type, &x, &y) == 3)
   {
      if (strcmp(type, "PLAYER") == 0)
      {
         game->playerPos = (Vector2){x, y};
      }
      else if (strcmp(type, "EXIT") == 0)
      {
         game->exitPos = (Vector2){x, y};
      }
      else if (strcmp(type, "BLOCK") == 0 && editor->squareCount < MAX_SQUARES)
      {
         editor->squares[editor->squareCount].pos = (Vector2){x, y};
         editor->squareCount++;
      }
      else if (strcmp(type, "LASER") == 0 && editor->laserCount < MAX_SQUARES)
      {
         editor->lasers[editor->laserCount].pos = (Vector2){x, y};
         editor->laserCount++;
      }
   }
   fclose(f);
   return true;
}

// Update menu logic
void UpdateMenu(ScreenState *screen, int *selected)
{
   // Navigate menu
   if (IsKeyPressed(KEY_DOWN))
   {
      (*selected)++;
      if (*selected >= MENU_OPTION_COUNT)
         *selected = 0;
   }
   if (IsKeyPressed(KEY_UP))
   {
      (*selected)--;
      if (*selected < 0)
         *selected = MENU_OPTION_COUNT - 1;
   }
   // Select option
   if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
   {
      if (*selected == MENU_LEVEL_EDITOR)
      {
         *screen = SCREEN_LEVEL_EDITOR;
      }
      else if (*selected == MENU_GAME_LEVEL)
      {
         *screen = SCREEN_GAME_LEVEL;
      }
   }
}

// Render menu
void RenderMenu(int selected)
{
   DrawText("MAIN MENU", 20, 20, 40, DARKGRAY);
   Color color1 = (selected == MENU_LEVEL_EDITOR) ? RED : BLUE;
   Color color2 = (selected == MENU_GAME_LEVEL) ? RED : BLUE;
   DrawText("> Open Level Editor", 20, 70, 24, color1);
   DrawText("> Start Game Level", 20, 110, 24, color2);
   DrawText("Use UP/DOWN arrows to navigate, Enter/Space to select", 20, 160, 18, DARKGRAY);
}

// Render victory screen
void RenderVictory(void)
{
    RenderMessageScreen("VICTORY!", "You reached the exit.", GREEN);
}

void RenderDeath(void)
{
    RenderMessageScreen("YOU DIED!", "You touched a laser.", RED);
}

// Helper: add block if not exists
void AddBlock(LevelEditorState *ed, Vector2 pos)
{
   if (ed->squareCount < MAX_SQUARES && FindBlockIndex(ed, pos) == -1)
   {
      ed->squares[ed->squareCount].pos = pos;
      ed->squareCount++;
   }
}

int FindLaserIndex(LevelEditorState *ed, Vector2 pos)
{
   for (int i = 0; i < ed->laserCount; ++i)
   {
      if (ed->lasers[i].pos.x == pos.x && ed->lasers[i].pos.y == pos.y)
      {
         return i;
      }
   }
   return -1;
}

void AddLaser(LevelEditorState *ed, Vector2 pos)
{
   if (ed->laserCount < MAX_SQUARES && FindLaserIndex(ed, pos) == -1)
   {
      ed->lasers[ed->laserCount].pos = pos;
      ed->laserCount++;
   }
}

void RemoveLaser(LevelEditorState *ed, Vector2 pos)
{
   int idx = FindLaserIndex(ed, pos);
   if (idx != -1)
   {
      for (int i = idx; i < ed->laserCount - 1; i++)
      {
         ed->lasers[i] = ed->lasers[i + 1];
      }
      ed->laserCount--;
   }
}

// Helper: remove block if exists
void RemoveBlock(LevelEditorState *ed, Vector2 pos)
{
   int idx = FindBlockIndex(ed, pos);
   if (idx != -1)
   {
      for (int i = idx; i < ed->squareCount - 1; i++)
      {
         ed->squares[i] = ed->squares[i + 1];
      }
      ed->squareCount--;
   }
}

// Fill perimeter with blocks
void FillPerimeter(LevelEditorState *ed)
{
   for (int x = 0; x < WINDOW_WIDTH; x += SQUARE_SIZE)
   {
      AddBlock(ed, (Vector2){x, 0});
      AddBlock(ed, (Vector2){x, WINDOW_HEIGHT - SQUARE_SIZE});
   }
   for (int y = SQUARE_SIZE; y < WINDOW_HEIGHT - SQUARE_SIZE; y += SQUARE_SIZE)
   {
      AddBlock(ed, (Vector2){0, y});
      AddBlock(ed, (Vector2){WINDOW_WIDTH - SQUARE_SIZE, y});
   }
}

// Create default level
void CreateDefaultLevel(GameState *game, LevelEditorState *editor)
{
   game->playerPos = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};
   game->exitPos = (Vector2){WINDOW_WIDTH - SQUARE_SIZE * 2, WINDOW_HEIGHT - SQUARE_SIZE * 2};
   game->groundStickTimer = 0.0f;
   editor->squareCount = 0;
   editor->laserCount = 0;
   FillPerimeter(editor);
}

// Update level editor logic
void UpdateLevelEditor(ScreenState *screen, GameState *game)
{
   // Switch tool with Tab
   if (IsKeyPressed(KEY_TAB))
   {
      editor.tool = (editor.tool + 1) % TOOL_COUNT;
   }
   // Hotkeys 1..4 to select tools directly
   if (IsKeyPressed(KEY_ONE))
      editor.tool = TOOL_PLAYER;
   if (IsKeyPressed(KEY_TWO))
      editor.tool = TOOL_ADD_BLOCK;
   if (IsKeyPressed(KEY_THREE))
      editor.tool = TOOL_REMOVE_BLOCK;
   if (IsKeyPressed(KEY_FOUR))
      editor.tool = TOOL_EXIT;
   if (IsKeyPressed(KEY_FIVE))
      editor.tool = TOOL_LASER_TRAP;

   // Arrow key repeat logic
   double now = GetTime();
   bool moved = false;
   if (now - arrowLastTime >= arrowInterval)
   {
      if (IsKeyDown(KEY_RIGHT))
      {
         editor.cursor.x += SQUARE_SIZE;
         moved = true;
      }
      if (IsKeyDown(KEY_LEFT))
      {
         editor.cursor.x -= SQUARE_SIZE;
         moved = true;
      }
      if (IsKeyDown(KEY_UP))
      {
         editor.cursor.y -= SQUARE_SIZE;
         moved = true;
      }
      if (IsKeyDown(KEY_DOWN))
      {
         editor.cursor.y += SQUARE_SIZE;
         moved = true;
      }
      if (moved)
         arrowLastTime = now;
   }

   // Mouse control: snap cursor to grid under mouse when inside window
   Vector2 mouse = GetMousePosition();
   if (mouse.x >= 0 && mouse.x < WINDOW_WIDTH && mouse.y >= 0 && mouse.y < WINDOW_HEIGHT)
   {
      editor.cursor = SnapToGrid(mouse);
   }

   // Clamp cursor to window
   if (editor.cursor.x < 0)
      editor.cursor.x = 0;
   if (editor.cursor.x > WINDOW_WIDTH - SQUARE_SIZE)
      editor.cursor.x = WINDOW_WIDTH - SQUARE_SIZE;
   if (editor.cursor.y < 0)
      editor.cursor.y = 0;
   if (editor.cursor.y > WINDOW_HEIGHT - SQUARE_SIZE)
      editor.cursor.y = WINDOW_HEIGHT - SQUARE_SIZE;

   // Tool actions
   switch (editor.tool)
   {
   case TOOL_PLAYER:
      // Set player position with space or left mouse button
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         game->playerPos = editor.cursor;
      }
      break;
   case TOOL_ADD_BLOCK:
      // Only create blocks while space or left mouse button is held down
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         if (FindBlockIndex(&editor, editor.cursor) == -1)
         {
            AddBlock(&editor, editor.cursor);
         }
      }
      break;
   case TOOL_REMOVE_BLOCK:
      // Only remove blocks while space or left mouse button is held down
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         if (FindBlockIndex(&editor, editor.cursor) != -1)
         {
            RemoveBlock(&editor, editor.cursor);
         }
         if (FindLaserIndex(&editor, editor.cursor) != -1)
         {
            RemoveLaser(&editor, editor.cursor);
         }
      }
      break;
   case TOOL_EXIT:
      // Set exit position with space or left mouse button
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         game->exitPos = editor.cursor;
      }
      break;
   case TOOL_LASER_TRAP:
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         if (FindLaserIndex(&editor, editor.cursor) == -1)
         {
            AddLaser(&editor, editor.cursor);
         }
      }
      break;
   default:
      break;
   }

   // Press Escape to save and return to menu
   if (IsKeyPressed(KEY_ESCAPE))
   {
      SaveLevel(game, &editor);
      *screen = SCREEN_MENU;
   }
}

// Render level editor
void RenderLevelEditor(const GameState *game)
{
   // Draw grid
   for (int x = 0; x <= WINDOW_WIDTH; x += SQUARE_SIZE)
   {
      DrawLine(x, 0, x, WINDOW_HEIGHT, LIGHTGRAY);
   }
   for (int y = 0; y <= WINDOW_HEIGHT; y += SQUARE_SIZE)
   {
      DrawLine(0, y, WINDOW_WIDTH, y, LIGHTGRAY);
   }
   RenderTiles(&editor);
   DrawRectangleRec((Rectangle){game->playerPos.x, game->playerPos.y, (float)SQUARE_SIZE, (float)SQUARE_SIZE}, BLUE);
   DrawRectangleRec((Rectangle){game->exitPos.x,   game->exitPos.y,   (float)SQUARE_SIZE, (float)SQUARE_SIZE}, GREEN);

   DrawText("LEVEL EDITOR", 20, 20, 32, DARKGRAY);
   const char *toolNames[TOOL_COUNT] = {"Player Location", "Add Block", "Remove Block", "Level Exit", "Laser Trap"};
   DrawText(TextFormat("Tool: %s (Tab to switch)", toolNames[editor.tool]), 20, 60, 18, BLUE);
   DrawText("Arrows/Mouse: Move cursor | Space/Left Click: Use tool | 1-5: Tools (5=Laser) | ESC: Menu", 20, 85, 18, DARKGRAY);

   // Draw cursor
   DrawRectangleLines((int)editor.cursor.x, (int)editor.cursor.y, SQUARE_SIZE, SQUARE_SIZE, RED);
}

// Update game logic
void UpdateGame(GameState *game)
{
   if (victory || death)
      return;

   float dt = GetFrameTime();
   if (dt > 0.033f)
      dt = 0.033f; // clamp big frames for stability

   // Decrement timers
   if (game->coyoteTimer > 0.0f)
      game->coyoteTimer -= dt;
   if (game->jumpBufferTimer > 0.0f)
      game->jumpBufferTimer -= dt;
   if (game->groundStickTimer > 0.0f)
      game->groundStickTimer -= dt;

   // Read input
   bool wantJumpPress = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
   bool left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
   bool right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
   bool down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);

   // --- CROUCH state & AABB height management ---
   // Keep player feet anchored (playerPos stores top-left)
   float currentH = game->crouching ? PLAYER_H_CROUCH : PLAYER_H;
   if (down)
   {
      if (!game->crouching)
      {
         float dy = PLAYER_H - PLAYER_H_CROUCH; // lower top so feet stay
         float ny = game->playerPos.y + dy;
         if (!AABBOverlapsSolid(game->playerPos.x, ny, PLAYER_W, PLAYER_H_CROUCH))
         {
            game->playerPos.y = ny;
            game->crouching = true;
            currentH = PLAYER_H_CROUCH;
         }
      }
   }
   else
   {
      if (game->crouching)
      {
         float dy = PLAYER_H - PLAYER_H_CROUCH; // raise top back if room
         float ny = game->playerPos.y - dy;
         if (!AABBOverlapsSolid(game->playerPos.x, ny, PLAYER_W, PLAYER_H))
         {
            game->playerPos.y = ny;
            game->crouching = false;
            currentH = PLAYER_H;
         }
      }
   }
   float aabbH = game->crouching ? PLAYER_H_CROUCH : PLAYER_H;

   // Detect wall contact (1px probes on the sides of the current AABB)
   bool touchingLeft = AABBOverlapsSolid(game->playerPos.x - 1.0f, game->playerPos.y, 1.0f, aabbH);
   bool touchingRight = AABBOverlapsSolid(game->playerPos.x + PLAYER_W, game->playerPos.y, 1.0f, aabbH);

   // Buffer jump on press
   if (wantJumpPress)
      game->jumpBufferTimer = JUMP_BUFFER_TIME;

   // Horizontal acceleration (disabled while crouching)
   float accelX = 0.0f;
   if (!game->crouching)
   {
      if (left && !right)
         accelX = (game->onGround ? -MOVE_ACCEL : -AIR_ACCEL);
      if (right && !left)
         accelX = (game->onGround ? MOVE_ACCEL : AIR_ACCEL);
   }
   game->playerVel.x += accelX * dt;

   // Use buffered jump if allowed by coyote/onGround
   if (!game->crouching)
   {
      if ((game->onGround || game->coyoteTimer > 0.0f) && game->jumpBufferTimer > 0.0f)
      {
         game->playerVel.y = JUMP_SPEED;
         game->onGround = false;
         game->coyoteTimer = 0.0f;
         game->jumpBufferTimer = 0.0f;
      }
   }

   // Wall jump: if airborne, touching a wall, and jump was just pressed
   if (!game->crouching && !game->onGround && (touchingLeft || touchingRight) && wantJumpPress)
   {
      game->playerVel.y = JUMP_SPEED;
      if (touchingLeft)
         game->playerVel.x = WALL_JUMP_PUSH_X; // push to the right
      if (touchingRight)
         game->playerVel.x = -WALL_JUMP_PUSH_X; // push to the left
   }

   // Gravity
   game->playerVel.y += GRAVITY * dt;

   // Wall slide: limit fall speed while touching a wall and in air
   if (!game->onGround && (touchingLeft || touchingRight) && game->playerVel.y > WALL_SLIDE_MAX_FALL)
   {
      game->playerVel.y = WALL_SLIDE_MAX_FALL;
   }

   // Friction
   if (game->onGround)
   {
      game->playerVel.x *= (game->crouching ? CROUCH_FRICTION : GROUND_FRICTION);
   }
   else
   {
      game->playerVel.x *= AIR_FRICTION;
   }

   // Clamp speeds
   float maxX = game->crouching ? MAX_SPEED_X_CROUCH : MAX_SPEED_X;
   if (game->playerVel.x > maxX)
      game->playerVel.x = maxX;
   if (game->playerVel.x < -maxX)
      game->playerVel.x = -maxX;
   if (game->playerVel.y > MAX_SPEED_Y)
      game->playerVel.y = MAX_SPEED_Y;
   if (game->playerVel.y < -MAX_SPEED_Y)
      game->playerVel.y = -MAX_SPEED_Y;

   // Move & collide: separate axis resolution
   float newX = game->playerPos.x;
   float newY = game->playerPos.y;
   ResolveAxis(&newX, &game->playerVel.x, newY, PLAYER_W, aabbH, false);

   // Y axis
   bool wasGround = game->onGround;
   game->onGround = false;
   ResolveAxis(&newY, &game->playerVel.y, newX, PLAYER_W, aabbH, true);

   // Robust ground check with tolerance: thicker (2px) probe + short stick window
   bool groundProbe = AABBOverlapsSolid(newX, newY + 2.0f, PLAYER_W, aabbH);
   if (groundProbe && game->playerVel.y >= 0.0f)
   {
      game->onGround = true;
      game->groundStickTimer = GROUND_STICK_TIME; // refresh stick window
      game->playerVel.y = 0.0f;                   // ensure stable rest
   }
   else
   {
      // If we were very recently grounded and not moving up, keep as grounded
      if (game->groundStickTimer > 0.0f && game->playerVel.y <= 0.0f)
      {
         game->onGround = true;
      }
   }

   // Bounds as solid walls/floor/ceiling
   if (newX < 0)
   {
      newX = 0;
      game->playerVel.x = 0;
   }
   if (newX > WINDOW_WIDTH - PLAYER_W)
   {
      newX = WINDOW_WIDTH - PLAYER_W;
      game->playerVel.x = 0;
   }
   if (newY < 0)
   {
      newY = 0;
      game->playerVel.y = 0;
   }
   if (newY > WINDOW_HEIGHT - aabbH)
   {
      newY = WINDOW_HEIGHT - aabbH;
      game->playerVel.y = 0;
      game->onGround = true;
   }

   game->playerPos.x = newX;
   game->playerPos.y = newY;

   // Refresh coyote timer when we are on ground; otherwise it keeps ticking down
   if (game->onGround)
      game->coyoteTimer = COYOTE_TIME;

   // Victory: if player's AABB overlaps the exit tile
   if (CheckCollisionRecs(PlayerAABB(game), ExitAABB(game)))
   {
       victory = true;
   }

   // Death: if player's AABB overlaps any laser trap stripe
   {
       Rectangle pb = PlayerAABB(game);
       for (int i = 0; i < editor.laserCount; ++i)
       {
           if (CheckCollisionRecs(pb, LaserStripeRect(editor.lasers[i].pos)))
           {
               death = true;
               break;
           }
       }
   }
}

// Render game
void RenderGame(const GameState *game)
{
    // World tiles
    RenderTiles(&editor);

    // Player & exit
    DrawRectangleRec(PlayerAABB(game), BLUE);
    DrawRectangleRec(ExitAABB(game), GREEN);

    // HUD
    DrawStats(game);
}

int main(void)
{
   // Initialize window (width, height, title)
   InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Empty Window");
   SetExitKey(0); // Disable default Esc-to-close behavior

   // Set the target FPS (optional)
   SetTargetFPS(120);

   // Initialize game state
   GameState game = {0};
   // Offset by one block from left bottom for player and cursor
   game.playerPos = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};
   game.playerVel = (Vector2){0, 0};
   game.onGround = false;
   game.coyoteTimer = 0.0f;
   game.jumpBufferTimer = 0.0f;
   // Offset by one block from right bottom for exit
   game.exitPos = (Vector2){WINDOW_WIDTH - SQUARE_SIZE * 2, WINDOW_HEIGHT - SQUARE_SIZE * 2};
   game.crouching = false;

   // Initialize screen state
   ScreenState screen = SCREEN_MENU;
   int menuSelected = 0;

   // Main game loop
   // Initialize editor cursor to left bottom offset by one block
   editor.cursor = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};

   bool editorLoaded = false;
   bool gameLevelLoaded = false;
   while (!WindowShouldClose()) // Detect window close button or ESC key
   {
      // Update and render based on screen state
      switch (screen)
      {
      case SCREEN_MENU:
         UpdateMenu(&screen, &menuSelected);
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderMenu(menuSelected);
         EndDrawing();
         break;
      case SCREEN_LEVEL_EDITOR:
      {
         if (!editorLoaded)
         {
            bool loaded = LoadLevel(&game, &editor);
            if (!loaded)
            {
               CreateDefaultLevel(&game, &editor);
            }
            editorLoaded = true;
         }
         UpdateLevelEditor(&screen, &game);
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderLevelEditor(&game);
         EndDrawing();
         break;
      }
      case SCREEN_GAME_LEVEL:
      {
         if (!gameLevelLoaded)
         {
            bool loaded = LoadLevel(&game, &editor);
            if (!loaded)
            {
               CreateDefaultLevel(&game, &editor);
            }
            gameLevelLoaded = true;
         }
         // ESC always returns to main menu while playing
         if (IsKeyPressed(KEY_ESCAPE))
         {
            screen = SCREEN_MENU;
            break;
         }
         UpdateGame(&game);
         if (death)
         {
            screen = SCREEN_DEATH;
            break;
         }
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderGame(&game);
         EndDrawing();

         // If victory, transition to victory screen
         if (victory)
         {
            screen = SCREEN_VICTORY;
         }
         break;
      }
      case SCREEN_DEATH:
      {
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderDeath();
         EndDrawing();
         if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE))
         {
            screen = SCREEN_MENU;
         }
         break;
      }
      case SCREEN_VICTORY:
      {
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderVictory();
         EndDrawing();
         if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE))
         {
            screen = SCREEN_MENU;
         }
         break;
      }
      }
      // Reset flags when returning to menu
      if (screen == SCREEN_MENU)
      {
         editorLoaded = false;
         gameLevelLoaded = false;
         victory = false;
         death = false;
      }
   }

   // De-initialize window
   CloseWindow();

   return 0;
}
#include "raylib.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#ifdef _WIN32
#include <direct.h> // _mkdir
#else
#include <sys/stat.h> // mkdir
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#endif
#include <errno.h>
#include <math.h>

// ---- Audio (optional if you add .wav files to ./assets) ----
static Sound sfxJump, sfxVictory, sfxDeath, sfxMenu;
static bool sfxJumpLoaded = false, sfxVictoryLoaded = false, sfxDeathLoaded = false, sfxMenuLoaded = false;

static Sound sfxHover;
static bool sfxHoverLoaded = false;

static Music musicMenu;

static bool musicMenuLoaded = false;
static bool musicMenuPlaying = false;
// Menu music fade params
#define MENU_MUSIC_VOL 0.6f
#define MENU_MUSIC_FADE 1.5f // volume units per second
static float musicMenuVol = 0.0f;
static float musicMenuTargetVol = MENU_MUSIC_VOL;

static bool LoadSoundIfExists(const char *path, Sound *out)
{
   if (FileExists(path))
   {
      *out = LoadSound(path);
      return true;
   }
   return false;
}

static bool LoadMusicIfExists(const char *path, Music *out)
{
   if (FileExists(path))
   {
      *out = LoadMusicStream(path);
      return true;
   }
   return false;
}

static void LoadAllSfx(void)
{
   // Expect files in ./assets (you can rename these if needed)
   sfxJumpLoaded = LoadSoundIfExists("assets/jump.wav", &sfxJump);
   sfxVictoryLoaded = LoadSoundIfExists("assets/victory.wav", &sfxVictory);
   sfxDeathLoaded = LoadSoundIfExists("assets/death.wav", &sfxDeath);
   sfxMenuLoaded = LoadSoundIfExists("assets/menu.wav", &sfxMenu);
   sfxHoverLoaded = LoadSoundIfExists("assets/hover.wav", &sfxHover);
}

static void UnloadAllSfx(void)
{
   if (sfxJumpLoaded)
      UnloadSound(sfxJump);
   if (sfxVictoryLoaded)
      UnloadSound(sfxVictory);
   if (sfxDeathLoaded)
      UnloadSound(sfxDeath);
   if (sfxMenuLoaded)
      UnloadSound(sfxMenu);
   if (sfxHoverLoaded)
      UnloadSound(sfxHover);
}

static void LoadMenuMusic(void)
{
   // Try a default path; you can change the filename/format
   musicMenuLoaded = LoadMusicIfExists("assets/menu.mp3", &musicMenu);
   if (musicMenuLoaded)
   {
      musicMenu.looping = true;        // loop in menu
      SetMusicVolume(musicMenu, 0.0f); // start silent, we'll fade in
      musicMenuVol = 0.0f;
      musicMenuTargetVol = MENU_MUSIC_VOL;
   }
}

static void UnloadMenuMusic(void)
{
   if (musicMenuLoaded)
   {
      if (musicMenuPlaying)
         StopMusicStream(musicMenu);
      UnloadMusicStream(musicMenu);
      musicMenuLoaded = false;
      musicMenuPlaying = false;
   }
}

static void PrimeSfxWarmup(void)
{
   // Play-and-stop each loaded sound at zero volume to warm up the mixer/device
   if (sfxJumpLoaded)
   {
      SetSoundVolume(sfxJump, 0.0f);
      PlaySound(sfxJump);
      StopSound(sfxJump);
      SetSoundVolume(sfxJump, 1.0f);
   }
   if (sfxVictoryLoaded)
   {
      SetSoundVolume(sfxVictory, 0.0f);
      PlaySound(sfxVictory);
      StopSound(sfxVictory);
      SetSoundVolume(sfxVictory, 1.0f);
   }
   if (sfxDeathLoaded)
   {
      SetSoundVolume(sfxDeath, 0.0f);
      PlaySound(sfxDeath);
      StopSound(sfxDeath);
      SetSoundVolume(sfxDeath, 1.0f);
   }
   if (sfxMenuLoaded)
   {
      SetSoundVolume(sfxMenu, 0.0f);
      PlaySound(sfxMenu);
      StopSound(sfxMenu);
      SetSoundVolume(sfxMenu, 1.0f);
   }
}

// Global victory flag
static bool victory = false;
static bool death = false;

// Editor constants
#define MAX_SQUARES 1024
#define SQUARE_SIZE 20
#define LEVEL_FILE_BIN "levels/level1.lvl"

// Mutable current level paths (updated by menus / hashing)
static char gLevelBinPath[260] = LEVEL_FILE_BIN;
static bool gCreateNewRequested = false;
// One-shot input block when entering menu to avoid carry-over presses/clicks
// Unified input gate for debouncing across the whole app
typedef enum
{
   IG_FREE = 0,
   IG_BLOCK_ONCE = 1,
   IG_LATCHED = 2
} InputGateState;
static InputGateState gInputGate = IG_FREE;

// Window constants
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Grid derived from window size and tile size
#define GRID_COLS (WINDOW_WIDTH / SQUARE_SIZE)
#define GRID_ROWS (WINDOW_HEIGHT / SQUARE_SIZE)

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
   SCREEN_SELECT_EDIT,
   SCREEN_SELECT_PLAY,
   SCREEN_LEVEL_EDITOR,
   SCREEN_TEST_PLAY,
   SCREEN_GAME_LEVEL,
   SCREEN_VICTORY,
   SCREEN_DEATH
} ScreenState;

// Menu options
#define MENU_OPTION_COUNT 3
typedef enum
{
   MENU_EDIT_EXISTING,
   MENU_CREATE_NEW,
   MENU_PLAY_LEVEL
} MenuOption;

typedef enum
{
   TOOL_PLAYER,
   TOOL_ADD_BLOCK,
   TOOL_REMOVE_BLOCK,
   TOOL_EXIT,
   TOOL_LASER_TRAP,
   TOOL_COUNT
} EditorTool;

typedef enum
{
   TILE_EMPTY = 0,
   TILE_BLOCK = 1,
   TILE_LASER = 2,
   TILE_PLAYER = 3,
   TILE_EXIT = 4
} TileType;

typedef struct GameState
{
   int score;            // milliseconds elapsed for the run (lower is better)
   float runTime;        // seconds elapsed in current run (not serialized)
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

typedef struct
{
   Vector2 cursor;
   TileType tiles[GRID_ROWS][GRID_COLS];
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
   { /* ignore other errors */
   }
#else
   if (mkdir("levels", 0755) == -1 && errno != EEXIST)
   { /* ignore other errors */
   }
#endif
}
// ---- Level catalog ----
typedef struct
{
   char baseName[128];
   char binPath[260];
   char textPath[260];
} LevelEntry;
typedef struct
{
   LevelEntry items[256];
   int count;
} LevelCatalog;

// ---- Numbered level filenames: level{index+1}.lvl ----
static int ParseLevelNumber(const char *baseName)
{
   // Expect baseName like "level7" -> returns 0-based index 6, or -1 if not matching
   if (!baseName)
      return -1;
   if (strncmp(baseName, "level", 5) != 0)
      return -1;
   const char *p = baseName + 5;
   if (!*p)
      return -1;
   int n = 0;
   while (*p)
   {
      if (*p < '0' || *p > '9')
         return -1;
      n = n * 10 + (*p - '0');
      p++;
   }
   if (n <= 0)
      return -1; // filenames are 1-based
   return n - 1; // convert to 0-based index
}

static void MakeLevelPathFromIndex(int index0, char *out, size_t outSz)
{
   // index0 is 0-based; filename is 1-based
   snprintf(out, outSz, "levels/level%d.lvl", index0 + 1);
}

static void CatalogSortByNumber(LevelCatalog *cat)
{
   // simple insertion sort by parsed level number ascending; unknowns go last
   for (int i = 1; i < cat->count; ++i)
   {
      LevelEntry key = cat->items[i];
      int kn = ParseLevelNumber(key.baseName);
      int j = i - 1;
      while (j >= 0)
      {
         int jn = ParseLevelNumber(cat->items[j].baseName);
         if (jn <= kn && !(jn == -1 && kn != -1))
            break;
         cat->items[j + 1] = cat->items[j];
         --j;
      }
      cat->items[j + 1] = key;
   }
}

static void ScanLevels(LevelCatalog *cat)
{
   cat->count = 0;
#ifndef _WIN32
   DIR *d = opendir("levels");
   if (d)
   {
      struct dirent *ent;
      while ((ent = readdir(d)) != NULL)
      {
         if (ent->d_type == DT_DIR)
            continue;
         const char *name = ent->d_name;
         size_t len = strlen(name);
         if (len > 4 && strcmp(name + len - 4, ".lvl") == 0)
         {
            LevelEntry *e = &cat->items[cat->count++];
            snprintf(e->binPath, sizeof(e->binPath), "levels/%s", name);
            snprintf(e->baseName, sizeof(e->baseName), "%.*s", (int)(len - 4), name);
            e->textPath[0] = '\0';
            if (cat->count >= 256)
               break;
         }
      }
      closedir(d);
   }
   // sort by numeric order (level1, level2, ...)
   CatalogSortByNumber(cat);
#else
   (void)cat;
#endif
}

static int FindNextLevelIndex(void)
{
   LevelCatalog tmp;
   ScanLevels(&tmp);
   int maxSeen = -1;
   for (int i = 0; i < tmp.count; ++i)
   {
      int n = ParseLevelNumber(tmp.items[i].baseName);
      if (n > maxSeen)
         maxSeen = n;
   }
   return maxSeen + 1; // next available 0-based index
}

// Binary level I/O
bool SaveLevelBinary(const GameState *game, const LevelEditorState *ed);
bool LoadLevelBinary(GameState *game, LevelEditorState *ed);

// ---- Grid collision helpers ----
static inline int WorldToCellX(float x) { return (int)floorf(x / (float)SQUARE_SIZE); }
static inline int WorldToCellY(float y) { return (int)floorf(y / (float)SQUARE_SIZE); }
static inline float CellToWorld(int c) { return (float)(c * SQUARE_SIZE); }

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
   return (Rectangle){g->playerPos.x, g->playerPos.y, PLAYER_W, h};
}

static inline Rectangle ExitAABB(const GameState *g)
{
   return (Rectangle){g->exitPos.x, g->exitPos.y, (float)SQUARE_SIZE, (float)SQUARE_SIZE};
}

// Tile rectangle helper (world-space rect for a tile cell)
static inline Rectangle TileRect(int cx, int cy)
{
   return (Rectangle){CellToWorld(cx), CellToWorld(cy), (float)SQUARE_SIZE, (float)SQUARE_SIZE};
}

// Laser visual/physics parameters
#define LASER_STRIPE_THICKNESS 3.0f
#define LASER_STRIPE_OFFSET 1.0f

static inline Rectangle LaserStripeRect(Vector2 laserPos)
{
   return (Rectangle){laserPos.x, laserPos.y + LASER_STRIPE_OFFSET, (float)SQUARE_SIZE, LASER_STRIPE_THICKNESS};
}

// ---- Tile classification helpers ----
static inline bool IsSolidTile(TileType t)
{
   return (t == TILE_BLOCK);
}

static inline bool IsHazardTile(TileType t)
{
   return (t == TILE_LASER);
}

// Draw all solid tiles (blocks) and laser stripes used by both editor and game
static void RenderTiles(const LevelEditorState *ed)
{
   for (int y = 0; y < GRID_ROWS; ++y)
   {
      for (int x = 0; x < GRID_COLS; ++x)
      {
         TileType t = ed->tiles[y][x];
         if (IsSolidTile(t))
         {
            Rectangle r = TileRect(x, y);
            DrawRectangleRec(r, GRAY);
         }
         else if (IsHazardTile(t))
         {
            Rectangle lr = LaserStripeRect((Vector2){CellToWorld(x), CellToWorld(y)});
            DrawRectangleRec(lr, RED);
         }
      }
   }
}

static void DrawStats(const GameState *g)
{
   DrawText(TextFormat("FPS: %d", GetFPS()), 10, 40, 20, RED);
   DrawText(TextFormat("Vel: (%.0f, %.0f)", g->playerVel.x, g->playerVel.y), 10, 70, 20, DARKGRAY);
   DrawText(g->onGround ? "Grounded" : "Air", 10, 100, 20, DARKGRAY);
   DrawText(TextFormat("Coy: %.2f  Buf: %.2f", g->coyoteTimer, g->jumpBufferTimer), 10, 130, 20, DARKGRAY);
}

// Render a centered stack of messages. Provide one or more triplets of:
// (const char* text, Color color, int fontSize), terminated with a NULL text.
// Example:
//   RenderMessageScreen("Title", RED, 40,
//                       "Subtitle", DARKGRAY, 24,
//                       NULL);
static void RenderMessageScreen(const char *firstText, Color firstColor, int firstFontSize, ...)
{
   if (!firstText)
      return;

   typedef struct { const char *text; Color color; int size; } Line;
   Line lines[16];
   int count = 0;

   lines[count++] = (Line){ firstText, firstColor, firstFontSize };

   va_list ap;
   va_start(ap, firstFontSize);
   while (count < (int)(sizeof(lines)/sizeof(lines[0])))
   {
      const char *t = va_arg(ap, const char*);
      if (t == NULL)
         break;
      Color c = va_arg(ap, Color);
      int sz = va_arg(ap, int);
      lines[count++] = (Line){ t, c, sz };
   }
   va_end(ap);

   // Compute total height to vertically center
   const int spacing = 10;
   int totalH = 0;
   for (int i = 0; i < count; ++i)
      totalH += lines[i].size;
   if (count > 1) totalH += spacing * (count - 1);

   int cx = WINDOW_WIDTH / 2;
   int y = WINDOW_HEIGHT / 2 - totalH / 2;
   for (int i = 0; i < count; ++i)
   {
      int w = MeasureText(lines[i].text, lines[i].size);
      DrawText(lines[i].text, cx - w / 2, y, lines[i].size, lines[i].color);
      y += lines[i].size + spacing;
   }
}

// ---- Tile helpers for grid-based level ----
static inline bool InBoundsCell(int cx, int cy)
{
   return cx >= 0 && cy >= 0 && cx < GRID_COLS && cy < GRID_ROWS;
}
static inline TileType GetTile(const LevelEditorState *ed, int cx, int cy)
{
   if (!InBoundsCell(cx, cy))
      return TILE_BLOCK; // out of bounds is solid
   return ed->tiles[cy][cx];
}
static inline void SetTile(LevelEditorState *ed, int cx, int cy, TileType v)
{
   if (!InBoundsCell(cx, cy))
      return;
   ed->tiles[cy][cx] = v;
}
static void SetUniqueTile(LevelEditorState *ed, int cx, int cy, TileType v)
{
   for (int y = 0; y < GRID_ROWS; ++y)
      for (int x = 0; x < GRID_COLS; ++x)
         if (ed->tiles[y][x] == v)
            ed->tiles[y][x] = TILE_EMPTY;
   SetTile(ed, cx, cy, v);
}
static bool FindTileWorldPos(const LevelEditorState *ed, TileType v, Vector2 *out)
{
   for (int y = 0; y < GRID_ROWS; ++y)
      for (int x = 0; x < GRID_COLS; ++x)
         if (ed->tiles[y][x] == v)
         {
            *out = (Vector2){CellToWorld(x), CellToWorld(y)};
            return true;
         }
   return false;
}

// ---- Grid collision helpers ----
static bool BlockAtCell(int cx, int cy)
{
   // Treat out of bounds as solid
   if (!InBoundsCell(cx, cy))
      return true;
   return IsSolidTile(editor.tiles[cy][cx]);
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

// Check if player's AABB overlaps any hazard tile (e.g., laser stripes)
static bool PlayerTouchesHazard(const GameState *g)
{
   Rectangle pb = PlayerAABB(g);
   for (int y = 0; y < GRID_ROWS; ++y)
   {
      for (int x = 0; x < GRID_COLS; ++x)
      {
         TileType t = editor.tiles[y][x];
         if (!IsHazardTile(t))
            continue;
         Rectangle lr = LaserStripeRect((Vector2){CellToWorld(x), CellToWorld(y)});
         if (CheckCollisionRecs(pb, lr))
            return true;
      }
   }
   return false;
}

bool SaveLevelBinary(const GameState *game, const LevelEditorState *ed)
{
   EnsureLevelsDir();
   FILE *f = fopen(gLevelBinPath, "wb");
   if (!f)
      return false;

   // Header
   const char magic[4] = {'L', 'V', 'L', '1'};
   const uint8_t version = 1;
   const uint16_t cols = (uint16_t)GRID_COLS;
   const uint16_t rows = (uint16_t)GRID_ROWS;

   if (fwrite(magic, 1, 4, f) != 4)
   {
      fclose(f);
      return false;
   }
   if (fwrite(&version, 1, 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fwrite(&cols, sizeof(cols), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fwrite(&rows, sizeof(rows), 1, f) != 1)
   {
      fclose(f);
      return false;
   }

   // Positions (pixels). Prefer tiles if present to keep in sync with editor grid
   Vector2 p = game->playerPos, e = game->exitPos;
   FindTileWorldPos(ed, TILE_PLAYER, &p);
   FindTileWorldPos(ed, TILE_EXIT, &e);
   int32_t px = (int32_t)p.x, py = (int32_t)p.y;
   int32_t ex = (int32_t)e.x, ey = (int32_t)e.y;

   if (fwrite(&px, sizeof(px), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fwrite(&py, sizeof(py), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fwrite(&ex, sizeof(ex), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fwrite(&ey, sizeof(ey), 1, f) != 1)
   {
      fclose(f);
      return false;
   }

   // Tiles (row-major)
   for (int y = 0; y < GRID_ROWS; ++y)
      for (int x = 0; x < GRID_COLS; ++x)
      {
         uint8_t t = (uint8_t)ed->tiles[y][x];
         if (fwrite(&t, 1, 1, f) != 1)
         {
            fclose(f);
            return false;
         }
      }

   fclose(f);
   return true;
}

bool LoadLevelBinary(GameState *game, LevelEditorState *ed)
{
   FILE *f = fopen(gLevelBinPath, "rb");
   if (!f)
      return false;

   char magic[4];
   uint8_t version = 0;
   uint16_t cols = 0, rows = 0;

   if (fread(magic, 1, 4, f) != 4)
   {
      fclose(f);
      return false;
   }
   if (memcmp(magic, "LVL1", 4) != 0)
   {
      fclose(f);
      return false;
   }
   if (fread(&version, 1, 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (version != 1)
   {
      fclose(f);
      return false;
   }
   if (fread(&cols, sizeof(cols), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fread(&rows, sizeof(rows), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (cols != GRID_COLS || rows != GRID_ROWS)
   {
      fclose(f);
      return false;
   }

   int32_t px, py, ex, ey;
   if (fread(&px, sizeof(px), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fread(&py, sizeof(py), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fread(&ex, sizeof(ex), 1, f) != 1)
   {
      fclose(f);
      return false;
   }
   if (fread(&ey, sizeof(ey), 1, f) != 1)
   {
      fclose(f);
      return false;
   }

   // Clear grid then read tiles
   for (int y = 0; y < GRID_ROWS; ++y)
      for (int x = 0; x < GRID_COLS; ++x)
         ed->tiles[y][x] = TILE_EMPTY;

   for (int y = 0; y < GRID_ROWS; ++y)
      for (int x = 0; x < GRID_COLS; ++x)
      {
         uint8_t t;
         if (fread(&t, 1, 1, f) != 1)
         {
            fclose(f);
            return false;
         }
         ed->tiles[y][x] = (TileType)t;
      }

   fclose(f);

   // Apply positions
   game->playerPos = (Vector2){(float)px, (float)py};
   game->exitPos = (Vector2){(float)ex, (float)ey};
   return true;
}

static inline void RestorePlayerPosFromTile(const LevelEditorState *ed, GameState *game)
{
   Vector2 p = game->playerPos;
   FindTileWorldPos(ed, TILE_PLAYER, &p);
   game->playerPos = p;
}

// --- Generalized UI list (mouse + WASD/Arrows) ---
typedef struct
{
   float startY;     // top Y of first item
   float stepY;      // vertical spacing between items
   float itemHeight; // height of hover rect
   int fontSize;     // text size
} UiListSpec;

// Hover suppression state: when the user navigates via keyboard, temporarily
// disable hover selection/visuals until the mouse moves again.
static bool gUiSuppressHover = false;
static Vector2 gUiLastMouse = { -9999.0f, -9999.0f };

static Rectangle UiListItemRect(const UiListSpec *spec, int index)
{
   float x = 20.0f;
   float y = spec->startY + index * spec->stepY;
   float w = (float)(WINDOW_WIDTH - 40);
   float h = spec->itemHeight;
   return (Rectangle){x, y, w, h};
}

static int UiListIndexAtMouse(Vector2 m, const UiListSpec *spec, int itemCount)
{
   for (int i = 0; i < itemCount; ++i)
      if (CheckCollisionPointRec(m, UiListItemRect(spec, i)))
         return i;
   return -1;
}

// Activation helper: keyboard activation is always allowed; mouse activation
// should only happen if the cursor is over a list item (handled in UiListHandle).
static inline bool UiListIsKeyActivatePressed(void)
{
   return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

// Any input relevant anywhere (menu, editor, gameplay)
static bool AnyInputDown(void)
{
   bool anyKeyHeld =
       IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_ESCAPE) ||
       IsKeyDown(KEY_UP) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) ||
       IsKeyDown(KEY_W) || IsKeyDown(KEY_A) || IsKeyDown(KEY_S) || IsKeyDown(KEY_D) ||
       IsKeyDown(KEY_TAB) ||
       IsKeyDown(KEY_ONE) || IsKeyDown(KEY_TWO) || IsKeyDown(KEY_THREE) || IsKeyDown(KEY_FOUR) || IsKeyDown(KEY_FIVE);
   bool anyMouseHeld = IsMouseButtonDown(MOUSE_LEFT_BUTTON) ||
                       IsMouseButtonDown(MOUSE_RIGHT_BUTTON) ||
                       IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
   return anyKeyHeld || anyMouseHeld;
}

static inline void InputGate_RequestBlockOnce(void)
{
   gInputGate = IG_BLOCK_ONCE;
}

// Returns true if input should be ignored this frame due to gate state.
static inline bool InputGate_BeginFrameBlocked(void)
{
   if (gInputGate == IG_BLOCK_ONCE || gInputGate == IG_LATCHED)
   {
      if (AnyInputDown())
         return true;       // keep blocking while anything is held
      gInputGate = IG_FREE; // clear when everything is released
      return false;
   }
   return false;
}

static inline void InputGate_LatchIfEdgeOccurred(bool edgePressed)
{
   if (edgePressed ||
       IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ||
       IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) ||
       IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON))
   {
      gInputGate = IG_LATCHED;
   }
}

// Updates selected index by keyboard/mouse and reports activation
static void UiListHandle(const UiListSpec *spec, int *selected, int itemCount, bool *outActivated)
{
   if (InputGate_BeginFrameBlocked())
   {
      if (outActivated)
         *outActivated = false;
      return;
   }

   // arrows + WASD cyclic navigation (edge-only)
   bool pressDown = IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S);
   bool pressUp = IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W);
   bool pressLeft = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
   bool pressRight = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);

   if (pressDown)
   {
      (*selected)++;
      if (*selected >= itemCount)
         *selected = (itemCount > 0 ? 0 : 0);
   }
   if (pressUp)
   {
      (*selected)--;
      if (*selected < 0)
         *selected = (itemCount > 0 ? itemCount - 1 : 0);
   }

   // If any keyboard navigation occurred this frame, suppress hover until
   // the mouse moves to intentionally re-engage hover behavior.
   if (pressDown || pressUp || pressLeft || pressRight)
      gUiSuppressHover = true;

   // mouse hover selects (does not latch; hover isn't a press)
   Vector2 m = GetMousePosition();
   // Re-enable hover if the mouse moved.
   if (m.x != gUiLastMouse.x || m.y != gUiLastMouse.y)
      gUiSuppressHover = false;
   gUiLastMouse = m;

   int hover = gUiSuppressHover ? -1 : UiListIndexAtMouse(m, spec, itemCount);
   if (hover != -1)
      *selected = hover;

   // activation (edge-only)
   bool keyActivate = UiListIsKeyActivatePressed();
   bool mouseActivate = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && (hover != -1);
   bool pressedActivate = keyActivate || mouseActivate;
   bool activated = (itemCount > 0) && pressedActivate;
   if (outActivated)
      *outActivated = activated;

   // Latch if any actionable press occurred this frame (nav or activation or mouse button press)
   InputGate_LatchIfEdgeOccurred(pressDown || pressUp || pressLeft || pressRight || pressedActivate);
}

typedef const char *(*LabelAtFn)(int index, void *ud);

static void UiListRenderCB(const UiListSpec *spec, int selected, int itemCount, LabelAtFn labelAt, void *ud,
                           const char *title, const char *emptyMsg, const char *hint)
{
   if (title)
      DrawText(title, 20, 20, 32, DARKGRAY);
   if (itemCount == 0)
   {
      if (emptyMsg)
         DrawText(emptyMsg, 20, 70, 24, RED);
      DrawText("Press ESC to go back", 20, 110, 18, DARKGRAY);
      return;
   }
   Vector2 m = GetMousePosition();
   // Re-enable hover if the mouse moved.
   if (m.x != gUiLastMouse.x || m.y != gUiLastMouse.y)
      gUiSuppressHover = false;
   gUiLastMouse = m;
   int hover = gUiSuppressHover ? -1 : UiListIndexAtMouse(m, spec, itemCount);
   for (int i = 0; i < itemCount; ++i)
   {
      Rectangle r = UiListItemRect(spec, i);
      if (hover == i)
         DrawRectangleRec(r, (Color){230, 230, 230, 255});
      Color c = (selected == i) ? RED : BLUE;
      const char *label = labelAt ? labelAt(i, ud) : "";
      DrawText(label, (int)r.x, (int)r.y, spec->fontSize, c);
   }
   if (hint)
      DrawText(hint, 20, (int)(spec->startY + itemCount * spec->stepY + 10), 18, DARKGRAY);
}

static const UiListSpec MENU_SPEC = {.startY = 70.0f, .stepY = 40.0f, .itemHeight = 28.0f, .fontSize = 24};
static const UiListSpec LIST_SPEC = {.startY = 70.0f, .stepY = 30.0f, .itemHeight = 24.0f, .fontSize = 24};

// Global menu labels and callbacks (no nested functions)
static const char *gMenuItems[MENU_OPTION_COUNT] = {
    "> Edit existing level",
    "> Create new level",
    "> Play level"};

static const char *MenuLabelAtCB(int i, void *ud)
{
   (void)ud;
   return gMenuItems[i];
}

static const char *CatalogLabelAtCB(int i, void *ud)
{
   LevelCatalog *c = (LevelCatalog *)ud;
   return c->items[i].baseName;
}

// Update menu logic
void UpdateMenu(ScreenState *screen, int *selected)
{
   int prev = *selected;
   bool activate = false;
   UiListHandle(&MENU_SPEC, selected, MENU_OPTION_COUNT, &activate);
   if (*selected != prev)
   {
      if (sfxHoverLoaded)
         PlaySound(sfxHover);
   }
   if (activate)
   {
      if (sfxMenuLoaded)
         PlaySound(sfxMenu);
      if (*selected == MENU_EDIT_EXISTING)
      {
         *screen = SCREEN_SELECT_EDIT;
      }
      else if (*selected == MENU_CREATE_NEW)
      {
         gCreateNewRequested = true;
         snprintf(gLevelBinPath, sizeof(gLevelBinPath), "%s", LEVEL_FILE_BIN);
         *screen = SCREEN_LEVEL_EDITOR;
      }
      else if (*selected == MENU_PLAY_LEVEL)
      {
         *screen = SCREEN_SELECT_PLAY;
      }
   }
}

// Render menu
void RenderMenu(int selected)
{
   UiListRenderCB(&MENU_SPEC, selected, MENU_OPTION_COUNT, MenuLabelAtCB, NULL,
                  "MAIN MENU",
                  NULL,
                  "Mouse: click items | WASD/Arrows: navigate | Enter/Space: select");
}
// ---- Level list UI ----
static LevelCatalog gCatalog;
static int gCatalogIndex = 0;

static void RenderLevelList(const char *title)
{
   UiListRenderCB(&LIST_SPEC, gCatalogIndex, gCatalog.count, CatalogLabelAtCB, &gCatalog,
                  title,
                  "No levels found in ./levels",
                  "UP/DOWN/W/S to select, ENTER/CLICK to confirm, ESC to back");
}

// Render victory screen
void RenderVictory(const GameState *game)
{
   float seconds = (float)game->score / 1000.0f;
   const char *scoreTxt = TextFormat("Score: %.2f s", seconds);
   RenderMessageScreen(
       "VICTORY!", GREEN, 40,
       "You reached the exit.", DARKGRAY, 24,
       scoreTxt, BLUE, 28,
       "Enter: restart | Space/Esc: menu", BLUE, 20,
       NULL);
}

void RenderDeath(void)
{
   RenderMessageScreen(
       "YOU DIED!", RED, 40,
       "You touched a laser.", DARKGRAY, 24,
       "Enter: restart | Space/Esc: menu", BLUE, 20,
       NULL);
}

void FillPerimeter(LevelEditorState *ed)
{
   for (int x = 0; x < GRID_COLS; ++x)
   {
      SetTile(ed, x, 0, TILE_BLOCK);
      SetTile(ed, x, GRID_ROWS - 1, TILE_BLOCK);
   }
   for (int y = 1; y < GRID_ROWS - 1; ++y)
   {
      SetTile(ed, 0, y, TILE_BLOCK);
      SetTile(ed, GRID_COLS - 1, y, TILE_BLOCK);
   }
}

void CreateDefaultLevel(GameState *game, LevelEditorState *ed)
{
   // clear grid
   for (int y = 0; y < GRID_ROWS; ++y)
      for (int x = 0; x < GRID_COLS; ++x)
         ed->tiles[y][x] = TILE_EMPTY;
   FillPerimeter(ed);
   Vector2 p = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};
   Vector2 e = (Vector2){WINDOW_WIDTH - SQUARE_SIZE * 2, WINDOW_HEIGHT - SQUARE_SIZE * 2};
   SetUniqueTile(ed, WorldToCellX(p.x), WorldToCellY(p.y), TILE_PLAYER);
   SetUniqueTile(ed, WorldToCellX(e.x), WorldToCellY(e.y), TILE_EXIT);
   game->playerPos = p;
   game->exitPos = e;
   game->groundStickTimer = 0.0f;
}

void UpdateLevelEditor(ScreenState *screen, GameState *game)
{
   if (InputGate_BeginFrameBlocked())
      return;
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
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
         SetUniqueTile(&editor, cx, cy, TILE_PLAYER);
         game->playerPos = (Vector2){CellToWorld(cx), CellToWorld(cy)};
      }
      break;
   case TOOL_ADD_BLOCK:
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
         if (GetTile(&editor, cx, cy) != TILE_PLAYER && GetTile(&editor, cx, cy) != TILE_EXIT)
            SetTile(&editor, cx, cy, TILE_BLOCK);
      }
      break;
   case TOOL_REMOVE_BLOCK:
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
         TileType t = GetTile(&editor, cx, cy);
         if (t == TILE_BLOCK || t == TILE_LASER)
            SetTile(&editor, cx, cy, TILE_EMPTY);
      }
      break;
   case TOOL_EXIT:
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
         SetUniqueTile(&editor, cx, cy, TILE_EXIT);
         game->exitPos = (Vector2){CellToWorld(cx), CellToWorld(cy)};
      }
      break;
   case TOOL_LASER_TRAP:
      if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
      {
         int cx = WorldToCellX(editor.cursor.x), cy = WorldToCellY(editor.cursor.y);
         if (GetTile(&editor, cx, cy) == TILE_EMPTY)
            SetTile(&editor, cx, cy, TILE_LASER);
      }
      break;
   default:
      break;
   }

   // Press Escape to save and return to menu
   if (IsKeyPressed(KEY_ESCAPE))
   {
      SaveLevelBinary(game, &editor);
      InputGate_RequestBlockOnce();
      *screen = SCREEN_MENU;
   }

   // Quick test: save and run the level when Enter is pressed
   if (IsKeyPressed(KEY_ENTER))
   {
      SaveLevelBinary(game, &editor);
      victory = false;
      death = false;
      *screen = SCREEN_TEST_PLAY;
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
   DrawRectangleRec((Rectangle){game->exitPos.x, game->exitPos.y, (float)SQUARE_SIZE, (float)SQUARE_SIZE}, GREEN);

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
   if (InputGate_BeginFrameBlocked())
      return;

   float dt = GetFrameTime();
   if (dt > 0.033f)
      dt = 0.033f; // clamp big frames for stability
   // Accumulate run timer (seconds)
   game->runTime += dt;
   bool didGroundJumpThisFrame = false;

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

   // Play jump SFX immediately on keypress when a jump is actually possible
   if (wantJumpPress && sfxJumpLoaded)
   {
      bool canGroundOrCoyote = !game->crouching && (game->onGround || game->coyoteTimer > 0.0f);
      bool canWall = !game->crouching && !game->onGround && (touchingLeft || touchingRight);
      if (canGroundOrCoyote || canWall)
         PlaySound(sfxJump);
   }

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
         didGroundJumpThisFrame = true; // mark that we initiated a jump from ground/coyote
         game->onGround = false;
         game->coyoteTimer = 0.0f;
         game->jumpBufferTimer = 0.0f;
      }
   }

   // Wall jump: only if airborne and NOT the same frame as a ground jump
   if (!game->crouching && !game->onGround && !didGroundJumpThisFrame && (touchingLeft || touchingRight) && wantJumpPress)
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
      // Finalize score as milliseconds elapsed
      game->score = (int)(game->runTime * 1000.0f);
      if (sfxVictoryLoaded)
         PlaySound(sfxVictory);
   }

   // Death: if player's AABB overlaps any hazard
   if (PlayerTouchesHazard(game))
   {
      death = true;
      if (sfxDeathLoaded)
         PlaySound(sfxDeath);
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
   // Audio buffering: use a stable, larger buffer as requested
   SetAudioStreamBufferSizeDefault(1024);
   InitAudioDevice();
   SetMasterVolume(0.8f); // tweak to taste
   LoadAllSfx();
   PrimeSfxWarmup();
   LoadMenuMusic();
   SetExitKey(0); // Disable default Esc-to-close behavior
   SetTargetFPS(120);

   // Initialize game state
   GameState game = (GameState){0};
   game.playerPos = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};
   game.playerVel = (Vector2){0, 0};
   game.onGround = false;
   game.coyoteTimer = 0.0f;
   game.jumpBufferTimer = 0.0f;
   game.exitPos = (Vector2){WINDOW_WIDTH - SQUARE_SIZE * 2, WINDOW_HEIGHT - SQUARE_SIZE * 2};
   game.crouching = false;

   // Initialize screen state
   ScreenState screen = SCREEN_MENU;
   int menuSelected = 0;

   // Initialize editor cursor
   editor.cursor = (Vector2){SQUARE_SIZE, WINDOW_HEIGHT - SQUARE_SIZE * 2};

   bool editorLoaded = false;
   bool gameLevelLoaded = false;
   while (!WindowShouldClose())
   {
      switch (screen)
      {
      case SCREEN_MENU:
         UpdateMenu(&screen, &menuSelected);
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderMenu(menuSelected);
         EndDrawing();
         break;

      case SCREEN_SELECT_EDIT:
      {
         ScanLevels(&gCatalog);
         if (IsKeyPressed(KEY_ESCAPE))
         {
            InputGate_RequestBlockOnce();
            screen = SCREEN_MENU;
            break;
         }
         bool activate = false;
         UiListHandle(&LIST_SPEC, &gCatalogIndex, gCatalog.count, &activate);
         if (activate && gCatalog.count > 0)
         {
            snprintf(gLevelBinPath, sizeof(gLevelBinPath), "%s", gCatalog.items[gCatalogIndex].binPath);
            editorLoaded = false;
            screen = SCREEN_LEVEL_EDITOR;
         }
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderLevelList("Select a level to edit");
         EndDrawing();
         break;
      }

      case SCREEN_SELECT_PLAY:
      {
         ScanLevels(&gCatalog);
         if (IsKeyPressed(KEY_ESCAPE))
         {
            InputGate_RequestBlockOnce();
            screen = SCREEN_MENU;
            break;
         }
         bool activate = false;
         UiListHandle(&LIST_SPEC, &gCatalogIndex, gCatalog.count, &activate);
         if (activate && gCatalog.count > 0)
         {
            snprintf(gLevelBinPath, sizeof(gLevelBinPath), "%s", gCatalog.items[gCatalogIndex].binPath);
            gameLevelLoaded = false;
            screen = SCREEN_GAME_LEVEL;
         }
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderLevelList("Select a level to play");
         EndDrawing();
         break;
      }

      case SCREEN_LEVEL_EDITOR:
      {
         if (!editorLoaded)
         {
            EnsureLevelsDir();

            if (gCreateNewRequested)
            {
               // Create a brand-new level file with the next index
               CreateDefaultLevel(&game, &editor);
               int nextIdx0 = FindNextLevelIndex();
               MakeLevelPathFromIndex(nextIdx0, gLevelBinPath, sizeof(gLevelBinPath));
               SaveLevelBinary(&game, &editor);
               gCreateNewRequested = false;
               editorLoaded = true;
            }
            else
            {
               // Load existing (from selection or default sentinel)
               FILE *bf = fopen(gLevelBinPath, "rb");
               bool haveExisting = (bf != NULL);
               if (bf)
                  fclose(bf);

               bool loaded = false;
               if (haveExisting)
                  loaded = LoadLevelBinary(&game, &editor);
               if (!loaded)
               {
                  CreateDefaultLevel(&game, &editor);
               }
               editorLoaded = true;
            }
         }
         UpdateLevelEditor(&screen, &game);
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderLevelEditor(&game);
         EndDrawing();
         break;
      }

      case SCREEN_TEST_PLAY:
      {
         // Load current level from disk if needed (use the same loader as gameplay)
         if (!gameLevelLoaded)
         {
            bool loaded = LoadLevelBinary(&game, &editor);
            if (!loaded)
            {
               CreateDefaultLevel(&game, &editor);
            }
            gameLevelLoaded = true;
            // Reset timer/score for a new run
            game.runTime = 0.0f;
            game.score = 0;
         }
         // Frame-level input block: still render, skip update/inputs
         if (InputGate_BeginFrameBlocked())
         {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            RenderGame(&game);
            EndDrawing();
            break;
         }
         // During test play, ESC returns to the editor instead of the main menu
         if (IsKeyPressed(KEY_ESCAPE))
         {
            InputGate_RequestBlockOnce(); // prevent ESC from triggering Editor's ESC handler
            RestorePlayerPosFromTile(&editor, &game);
            screen = SCREEN_LEVEL_EDITOR;
            // Reset transient flags and allow editing to continue
            victory = false;
            death = false;
            break;
         }
         UpdateGame(&game);
         if (death)
         {
            // In test play: return directly to editor on death
            RestorePlayerPosFromTile(&editor, &game);
            screen = SCREEN_LEVEL_EDITOR;
            victory = false;
            death = false;
            break;
         }
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderGame(&game);
         EndDrawing();
         if (victory)
         {
            // In test play: return directly to editor on victory
            RestorePlayerPosFromTile(&editor, &game);
            screen = SCREEN_LEVEL_EDITOR;
            victory = false;
            death = false;
         }
         break;
      }

      case SCREEN_GAME_LEVEL:
      {
         if (!gameLevelLoaded)
         {
            bool loaded = LoadLevelBinary(&game, &editor);
            if (!loaded)
            {
               CreateDefaultLevel(&game, &editor);
            }
            gameLevelLoaded = true;
            // Reset timer/score for a new run
            game.runTime = 0.0f;
            game.score = 0;
         }
         // Frame-level input block: still render, skip update/inputs
         if (InputGate_BeginFrameBlocked())
         {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            RenderGame(&game);
            EndDrawing();
            break;
         }
         if (IsKeyPressed(KEY_ESCAPE))
         {
            InputGate_RequestBlockOnce();
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
         // Enter: restart current level; Space/Escape: back to menu
         if (IsKeyPressed(KEY_ENTER))
         {
            InputGate_RequestBlockOnce();
            victory = false;
            death = false;
            gameLevelLoaded = false; // force reload of the level data
            screen = SCREEN_GAME_LEVEL;
         }
         else if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE))
         {
            InputGate_RequestBlockOnce();
            screen = SCREEN_MENU;
         }
         break;
      }

      case SCREEN_VICTORY:
      {
         BeginDrawing();
         ClearBackground(RAYWHITE);
         RenderVictory(&game);
         EndDrawing();
         // Enter: restart current level; Space/Escape: back to menu
         if (IsKeyPressed(KEY_ENTER))
         {
            InputGate_RequestBlockOnce();
            victory = false;
            death = false;
            gameLevelLoaded = false; // force reload of the level data
            screen = SCREEN_GAME_LEVEL;
         }
         else if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE))
         {
            InputGate_RequestBlockOnce();
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

      // --- Menu music control with fade: play on all non-game screens, fade out only during gameplay ---
      if (musicMenuLoaded)
      {
         bool inMenuScreens = (screen == SCREEN_MENU || screen == SCREEN_SELECT_EDIT || screen == SCREEN_SELECT_PLAY || screen == SCREEN_LEVEL_EDITOR);
         float dt = GetFrameTime();
         // Ensure playback state
         if (inMenuScreens)
         {
            if (!musicMenuPlaying)
            {
               PlayMusicStream(musicMenu);
               musicMenuPlaying = true;
            }
            musicMenuTargetVol = MENU_MUSIC_VOL;
         }
         else
         {
            // In gameplay: fade to 0, then stop
            musicMenuTargetVol = 0.0f;
         }

         // Step volume toward target
         if (musicMenuVol < musicMenuTargetVol)
         {
            musicMenuVol += MENU_MUSIC_FADE * dt;
            if (musicMenuVol > musicMenuTargetVol)
               musicMenuVol = musicMenuTargetVol;
         }
         else if (musicMenuVol > musicMenuTargetVol)
         {
            musicMenuVol -= MENU_MUSIC_FADE * dt;
            if (musicMenuVol < musicMenuTargetVol)
               musicMenuVol = musicMenuTargetVol;
         }

         // Apply volume and update stream
         SetMusicVolume(musicMenu, musicMenuVol);
         if (musicMenuPlaying)
            UpdateMusicStream(musicMenu);

         // If fully faded out during gameplay, stop the stream
         if (!inMenuScreens && musicMenuPlaying && musicMenuVol <= 0.001f)
         {
            StopMusicStream(musicMenu);
            musicMenuPlaying = false;
         }
      }
   }

   UnloadAllSfx();
   UnloadMenuMusic();
   CloseAudioDevice();
   CloseWindow();
   return 0;
}

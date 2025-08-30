// Level grid, editor state, level IO, and catalog
#pragma once
#include "raylib.h"
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "config.h"

// Paths
#define LEVEL_FILE_BIN "levels/level1.lvl"

// Tiles/tools
typedef enum {
  TILE_EMPTY = 0,
  TILE_BLOCK = 1,
  TILE_LASER = 2,
  TILE_PLAYER = 3,
  TILE_EXIT = 4
} TileType;

typedef enum {
  TOOL_PLAYER,
  TOOL_ADD_BLOCK,
  TOOL_REMOVE_BLOCK,
  TOOL_EXIT,
  TOOL_LASER_TRAP,
  TOOL_COUNT
} EditorTool;

typedef struct {
  Vector2 cursor;
  TileType tiles[GRID_ROWS][GRID_COLS];
  EditorTool tool;
} LevelEditorState;

extern LevelEditorState editor;
extern char gLevelBinPath[260];
extern bool gCreateNewRequested;

// Tile helpers
static inline bool InBoundsCell(int cx, int cy) {
  return cx >= 0 && cy >= 0 && cx < GRID_COLS && cy < GRID_ROWS;
}
static inline int WorldToCellX(float x) { return (int)floorf(x / (float)SQUARE_SIZE); }
static inline int WorldToCellY(float y) { return (int)floorf(y / (float)SQUARE_SIZE); }
static inline float CellToWorld(int c) { return (float)(c * SQUARE_SIZE); }
static inline bool IsSolidTile(TileType t) { return t == TILE_BLOCK; }
static inline bool IsHazardTile(TileType t) { return t == TILE_LASER; }

void SetTile(LevelEditorState *ed, int cx, int cy, TileType v);
TileType GetTile(const LevelEditorState *ed, int cx, int cy);
void SetUniqueTile(LevelEditorState *ed, int cx, int cy, TileType v);
bool FindTileWorldPos(const LevelEditorState *ed, TileType v, Vector2 *out);

// Level IO
struct GameState; // forward decl to avoid include cycle
bool SaveLevelBinary(const struct GameState *game, const LevelEditorState *ed);
bool LoadLevelBinary(struct GameState *game, LevelEditorState *ed);
void EnsureLevelsDir(void);
void FillPerimeter(LevelEditorState *ed);
void CreateDefaultLevel(struct GameState *game, LevelEditorState *ed);

// Level catalog
typedef struct {
  char baseName[128];
  char binPath[260];
  char textPath[260];
} LevelEntry;

typedef struct {
  LevelEntry items[256];
  int count;
} LevelCatalog;

void ScanLevels(LevelCatalog *cat);
int FindNextLevelIndex(void); // 0-based next index
void MakeLevelPathFromIndex(int index0, char *out, size_t outSz);

// Utils
Vector2 SnapToGrid(Vector2 p);


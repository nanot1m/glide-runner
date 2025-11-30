// Level grid, editor state, level IO, and catalog
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include "config.h"
#include "raylib.h"

// Paths
#ifdef PLATFORM_WEB
#define LEVELS_DIR_READ "levels" // preloaded, read-only
#define LEVELS_DIR_WRITE "user-levels" // IDBFS-backed, writable
#else
#define LEVELS_DIR_READ "levels"
#define LEVELS_DIR_WRITE "levels"
#endif

#define LEVEL_FILE_BIN LEVELS_DIR_WRITE "/level1.lvl"

// Tiles/tools
typedef enum {
	TILE_EMPTY = 0,
	TILE_BLOCK = 1,
	TILE_LASER = 2,
	TILE_PLAYER = 3,
	TILE_EXIT = 4,
	TILE_SPAWNER = 5
} TileType;

typedef enum {
	TOOL_PLAYER,
	TOOL_ADD_BLOCK,
	TOOL_REMOVE_BLOCK,
	TOOL_EXIT,
	TOOL_LASER_TRAP,
	TOOL_SPAWNER,
	TOOL_COUNT
} EditorTool;

typedef struct LevelEditorState {
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
static inline bool IsSpawnerTile(TileType t) { return t == TILE_SPAWNER; }

// Solid collision box for a tile at cell coordinates, in world space.
// Visuals remain SQUARE_SIZE; this function defines physics bounds per tile type.
static inline Rectangle TileSolidCollisionRect(int cx, int cy, TileType t) {
	float x = CellToWorld(cx);
	float y = CellToWorld(cy);
	switch (t) {
	case TILE_BLOCK:
		return (Rectangle){x, y, (float)SQUARE_SIZE, (float)SQUARE_SIZE};
	default:
		return (Rectangle){0, 0, 0, 0}; // non-solid
	}
}

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

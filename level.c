#include "level.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <direct.h>
#endif

LevelEditorState editor = {0};
char gLevelBinPath[260] = LEVEL_FILE_BIN;
bool gCreateNewRequested = false;

Vector2 SnapToGrid(Vector2 p) {
	int gx = ((int)p.x / SQUARE_SIZE) * SQUARE_SIZE;
	int gy = ((int)p.y / SQUARE_SIZE) * SQUARE_SIZE;
	if (gx < 0) gx = 0;
	if (gx > WINDOW_WIDTH - SQUARE_SIZE) gx = WINDOW_WIDTH - SQUARE_SIZE;
	if (gy < 0) gy = 0;
	if (gy > WINDOW_HEIGHT - SQUARE_SIZE) gy = WINDOW_HEIGHT - SQUARE_SIZE;
	return (Vector2){(float)gx, (float)gy};
}

TileType GetTile(const LevelEditorState *ed, int cx, int cy) {
	if (!InBoundsCell(cx, cy)) return TILE_BLOCK; // out of bounds is solid
	return ed->tiles[cy][cx];
}
void SetTile(LevelEditorState *ed, int cx, int cy, TileType v) {
	if (!InBoundsCell(cx, cy)) return;
	ed->tiles[cy][cx] = v;
}
void SetUniqueTile(LevelEditorState *ed, int cx, int cy, TileType v) {
	for (int y = 0; y < GRID_ROWS; ++y)
		for (int x = 0; x < GRID_COLS; ++x)
			if (ed->tiles[y][x] == v)
				ed->tiles[y][x] = TILE_EMPTY;
	SetTile(ed, cx, cy, v);
}
bool FindTileWorldPos(const LevelEditorState *ed, TileType v, Vector2 *out) {
	for (int y = 0; y < GRID_ROWS; ++y)
		for (int x = 0; x < GRID_COLS; ++x)
			if (ed->tiles[y][x] == v) {
				*out = (Vector2){CellToWorld(x), CellToWorld(y)};
				return true;
			}
	return false;
}

void EnsureLevelsDir(void) {
#ifdef _WIN32
	if (_mkdir("levels") == -1 && errno != EEXIST) { /* ignore */
	}
#else
	if (mkdir("levels", 0755) == -1 && errno != EEXIST) { /* ignore */
	}
#endif
}

void FillPerimeter(LevelEditorState *ed) {
	for (int x = 0; x < GRID_COLS; ++x) {
		SetTile(ed, x, 0, TILE_BLOCK);
		SetTile(ed, x, GRID_ROWS - 1, TILE_BLOCK);
	}
	for (int y = 1; y < GRID_ROWS - 1; ++y) {
		SetTile(ed, 0, y, TILE_BLOCK);
		SetTile(ed, GRID_COLS - 1, y, TILE_BLOCK);
	}
}

void CreateDefaultLevel(GameState *game, LevelEditorState *ed) {
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

bool SaveLevelBinary(const GameState *game, const LevelEditorState *ed) {
	EnsureLevelsDir();
	FILE *f = fopen(gLevelBinPath, "wb");
	if (!f) return false;
	const char magic[4] = {'L', 'V', 'L', '1'};
	uint8_t version = 1;
	uint16_t cols = (uint16_t)GRID_COLS, rows = (uint16_t)GRID_ROWS;
	if (fwrite(magic, 1, 4, f) != 4) {
		fclose(f);
		return false;
	}
	if (fwrite(&version, 1, 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fwrite(&cols, sizeof(cols), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fwrite(&rows, sizeof(rows), 1, f) != 1) {
		fclose(f);
		return false;
	}
	Vector2 p = game->playerPos, e = game->exitPos;
	FindTileWorldPos(ed, TILE_PLAYER, &p);
	FindTileWorldPos(ed, TILE_EXIT, &e);
	int32_t px = (int32_t)p.x, py = (int32_t)p.y, ex = (int32_t)e.x, ey = (int32_t)e.y;
	if (fwrite(&px, sizeof(px), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fwrite(&py, sizeof(py), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fwrite(&ex, sizeof(ex), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fwrite(&ey, sizeof(ey), 1, f) != 1) {
		fclose(f);
		return false;
	}
	for (int y = 0; y < GRID_ROWS; ++y)
		for (int x = 0; x < GRID_COLS; ++x) {
			uint8_t t = (uint8_t)ed->tiles[y][x];
			if (fwrite(&t, 1, 1, f) != 1) {
				fclose(f);
				return false;
			}
		}
	fclose(f);
	return true;
}

bool LoadLevelBinary(GameState *game, LevelEditorState *ed) {
	FILE *f = fopen(gLevelBinPath, "rb");
	if (!f) return false;
	char magic[4];
	uint8_t version = 0;
	uint16_t cols = 0, rows = 0;
	if (fread(magic, 1, 4, f) != 4) {
		fclose(f);
		return false;
	}
	if (memcmp(magic, "LVL1", 4) != 0) {
		fclose(f);
		return false;
	}
	if (fread(&version, 1, 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (version != 1) {
		fclose(f);
		return false;
	}
	if (fread(&cols, sizeof(cols), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fread(&rows, sizeof(rows), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (cols != GRID_COLS || rows != GRID_ROWS) {
		fclose(f);
		return false;
	}
	int32_t px, py, ex, ey;
	if (fread(&px, sizeof(px), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fread(&py, sizeof(py), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fread(&ex, sizeof(ex), 1, f) != 1) {
		fclose(f);
		return false;
	}
	if (fread(&ey, sizeof(ey), 1, f) != 1) {
		fclose(f);
		return false;
	}
	for (int y = 0; y < GRID_ROWS; ++y)
		for (int x = 0; x < GRID_COLS; ++x) ed->tiles[y][x] = TILE_EMPTY;
	for (int y = 0; y < GRID_ROWS; ++y)
		for (int x = 0; x < GRID_COLS; ++x) {
			uint8_t t;
			if (fread(&t, 1, 1, f) != 1) {
				fclose(f);
				return false;
			}
			ed->tiles[y][x] = (TileType)t;
		}
	fclose(f);
	game->playerPos = (Vector2){(float)px, (float)py};
	game->exitPos = (Vector2){(float)ex, (float)ey};
	return true;
}

// ---- Level catalog ----
static int ParseLevelNumber(const char *baseName) {
	if (!baseName) return -1;
	if (strncmp(baseName, "level", 5) != 0) return -1;
	const char *p = baseName + 5;
	if (!*p) return -1;
	int n = 0;
	while (*p) {
		if (*p < '0' || *p > '9') return -1;
		n = n * 10 + (*p - '0');
		++p;
	}
	if (n <= 0) return -1;
	return n - 1;
}

static void CatalogSortByNumber(LevelCatalog *cat) {
	for (int i = 1; i < cat->count; ++i) {
		LevelEntry key = cat->items[i];
		int kn = ParseLevelNumber(key.baseName);
		int j = i - 1;
		while (j >= 0) {
			int jn = ParseLevelNumber(cat->items[j].baseName);
			if (jn <= kn && !(jn == -1 && kn != -1)) break;
			cat->items[j + 1] = cat->items[j];
			--j;
		}
		cat->items[j + 1] = key;
	}
}

void ScanLevels(LevelCatalog *cat) {
	cat->count = 0;
#ifndef _WIN32
	DIR *d = opendir("levels");
	if (d) {
		struct dirent *ent;
		while ((ent = readdir(d)) != NULL) {
			if (ent->d_type == DT_DIR) continue;
			const char *name = ent->d_name;
			size_t len = strlen(name);
			if (len > 4 && strcmp(name + len - 4, ".lvl") == 0) {
				LevelEntry *e = &cat->items[cat->count++];
				snprintf(e->binPath, sizeof(e->binPath), "levels/%s", name);
				snprintf(e->baseName, sizeof(e->baseName), "%.*s", (int)(len - 4), name);
				e->textPath[0] = '\0';
				if (cat->count >= 256) break;
			}
		}
		closedir(d);
	}
	CatalogSortByNumber(cat);
#else
	(void)cat;
#endif
}

int FindNextLevelIndex(void) {
	LevelCatalog tmp;
	ScanLevels(&tmp);
	int maxSeen = -1;
	for (int i = 0; i < tmp.count; ++i) {
		int n = ParseLevelNumber(tmp.items[i].baseName);
		if (n > maxSeen) maxSeen = n;
	}
	return maxSeen + 1;
}

void MakeLevelPathFromIndex(int index0, char *out, size_t outSz) {
	snprintf(out, outSz, "levels/level%d.lvl", index0 + 1);
}

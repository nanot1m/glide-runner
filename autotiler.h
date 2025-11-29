// Autotiler - Automatically selects appropriate tile sprites based on neighboring tiles
#pragma once
#include <stdbool.h>
#include "raylib.h"

// Callback function type to check if a block exists at given cell coordinates
// Returns true if there's a solid block at (cx, cy), false otherwise
typedef bool (*AutotilerCheckBlockFunc)(const void *context, int cx, int cy);

// Tile position in tileset (column, row)
typedef struct {
	int col;
	int row;
} TilePos;

// Tilemap layout configuration - defines positions of tiles in the tileset
typedef struct {
	// Row with no vertical neighbors
	TilePos rowNoVertical_isolated;
	TilePos rowNoVertical_leftEdge;
	TilePos rowNoVertical_rightEdge;
	TilePos rowNoVertical_middle;

	// Top band (no upward neighbor)
	TilePos topBand_isolated;
	TilePos topBand_innerBottom;
	TilePos topBand_innerBottomLeft;
	TilePos topBand_innerBottomRight;
	TilePos topBand_edge;
	TilePos topBand_innerBottomRightNoDownRight;
	TilePos topBand_topLeftCorner;
	TilePos topBand_innerBottomLeftNoDownLeft;
	TilePos topBand_topRightCorner;

	// Bottom band (no downward neighbor)
	TilePos bottomBand_isolated;
	TilePos bottomBand_innerTop;
	TilePos bottomBand_innerTopLeft;
	TilePos bottomBand_innerTopRight;
	TilePos bottomBand_edge;
	TilePos bottomBand_innerTopRightNoUpRight;
	TilePos bottomBand_bottomLeft;
	TilePos bottomBand_innerTopLeftNoUpLeft;
	TilePos bottomBand_bottomRight;

	// Interior with sides (has up and down neighbors)
	TilePos interior_allDiagonalsOpen;
	TilePos interior_upDiagonals;
	TilePos interior_rightDiagonals;
	TilePos interior_leftDiagonals;
	TilePos interior_downLeft;
	TilePos interior_downRight;
	TilePos interior_upLeftDownRight;
	TilePos interior_upRightDownLeft;
	TilePos interior_upLeft;
	TilePos interior_upRight;
	TilePos interior_upDiagonalsOpen;
	TilePos interior_upLeftOpen;
	TilePos interior_upRightOpen;
	TilePos interior_downRightOpen;
	TilePos interior_full;

	// Open left (no left neighbor)
	TilePos openLeft_allOpen;
	TilePos openLeft_downRightOpen;
	TilePos openLeft_upRightOpen;
	TilePos openLeft_leftEdge;

	// Open right (no right neighbor)
	TilePos openRight_allOpen;
	TilePos openRight_downLeftOpen;
	TilePos openRight_upLeftOpen;
	TilePos openRight_rightEdge;

	// Isolated tiles
	TilePos isolated_vertical;
	TilePos isolated_full;
} TilemapLayout;

// Autotiler configuration
typedef struct {
	int tileSize; // Size of each tile in the tileset (e.g., 32 pixels)
	AutotilerCheckBlockFunc checkBlock; // Function to check if block exists at position
	TilemapLayout layout; // Tileset layout configuration
} AutotilerConfig;

// Initialize autotiler with configuration
// Returns true if initialization was successful, false otherwise
bool Autotiler_Init(const AutotilerConfig *config);

// Get the source rectangle for a block at the given cell coordinates
// Based on the neighboring blocks, returns the appropriate tile from the tileset
// context: Context to pass to the checkBlock function
// cx, cy: cell coordinates of the block to get the tile for
// Returns: Rectangle describing the source region in the tileset texture
Rectangle Autotiler_GetBlockTile(const void *context, int cx, int cy);

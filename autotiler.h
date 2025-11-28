// Autotiler - Automatically selects appropriate tile sprites based on neighboring tiles
#pragma once
#include <stdbool.h>
#include "raylib.h"

// Callback function type to check if a block exists at given cell coordinates
// Returns true if there's a solid block at (cx, cy), false otherwise
typedef bool (*AutotilerCheckBlockFunc)(const void *context, int cx, int cy);

// Autotiler configuration
typedef struct {
	int tileSize; // Size of each tile in the tileset (e.g., 32 pixels)
	AutotilerCheckBlockFunc checkBlock; // Function to check if block exists at position
} AutotilerConfig;

// Initialize autotiler with configuration
void Autotiler_Init(const AutotilerConfig *config);

// Get the source rectangle for a block at the given cell coordinates
// Based on the neighboring blocks, returns the appropriate tile from the tileset
// context: Context to pass to the checkBlock function
// cx, cy: cell coordinates of the block to get the tile for
// Returns: Rectangle describing the source region in the tileset texture
Rectangle Autotiler_GetBlockTile(const void *context, int cx, int cy);

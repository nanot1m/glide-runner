#pragma once
#include "raylib.h"
#include "level.h"

// Check if an AABB overlaps any solid tiles
bool AABBOverlapsSolid(float x, float y, float w, float h);

// Move an entity (pos/vel) against the tilemap, handling collisions
// w, h: entity dimensions
// dt: delta time
// hit flags: optional output flags for collision on each side
void MoveEntity(Vector2 *pos, Vector2 *vel, float w, float h, float dt, bool *hitLeft, bool *hitRight, bool *hitTop, bool *hitBottom);

// Nudge an entity out of solids if it's overlapping
void PushEntityOutOfSolids(Vector2 *pos, Vector2 *vel, float w, float h);

// Set the current level for physics checks
void Physics_SetLevel(const struct LevelEditorState *level);

// Check if a cell is solid
bool Physics_BlockAtCell(int cx, int cy);

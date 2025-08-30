// Rendering helpers shared by game/editor
#pragma once
#include "raylib.h"
#include "game.h"
#include "level.h"

// Laser visual params
#define LASER_STRIPE_THICKNESS 3.0f
#define LASER_STRIPE_OFFSET 1.0f

Rectangle PlayerAABB(const GameState *g);
Rectangle ExitAABB(const GameState *g);
Rectangle TileRect(int cx, int cy);
Rectangle LaserStripeRect(Vector2 laserPos);

void RenderTiles(const LevelEditorState *ed);
void DrawStats(const GameState *g);


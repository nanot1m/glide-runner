#pragma once
#include "game.h"

// Player AABB for collision
Rectangle PlayerAABB(const GameState *game);

// Handle player input and physics
void UpdatePlayer(GameState *game, float dt);

// Render player sprite and effects
void RenderPlayer(const GameState *game);

// Apply damage to player
void TakeDamage(GameState *game, Vector2 sourcePos);

// Trigger death sequence
void TriggerDeath(GameState *game);

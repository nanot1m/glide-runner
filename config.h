// Shared configuration constants
#pragma once

// Grid / window sizing
// Define grid dimensions in tiles, then derive window size from tile size
// Keeping values equivalent to the previous 800x600 window with 20px tiles
// so behavior remains unchanged unless you tweak cols/rows.
// Grid / tiles
// Size of one square in pixels
#define SQUARE_SIZE 32
// Number of columns and rows in the grid
#define GRID_COLS 40
#define GRID_ROWS 30
// Derived window size in pixels
#define WINDOW_WIDTH (GRID_COLS * SQUARE_SIZE)
#define WINDOW_HEIGHT (GRID_ROWS * SQUARE_SIZE)

// Feature toggles
#define ENABLE_FPS_METER 1

// Timing
#define BASE_FPS 120.0f
#define BASE_DT (1.0f / BASE_FPS)

// (Other constants below)

// Physics tuning
#define GRAVITY 1800.0f
#define GRAVITY_FALL_MULT 1.5f // Higher gravity when falling for snappier feel
#define MOVE_ACCEL 1200.0f // Reduced to slow ground acceleration
#define AIR_ACCEL 750.0f // Reduced for gentler air control
#define GROUND_FRICTION 0.88f
#define AIR_FRICTION 0.985f
#define MAX_SPEED_X 320.0f
#define MAX_SPEED_Y 1400.0f
#define JUMP_SPEED -760.0f // ~1.5x higher jump (via sqrt scaling)
#define JUMP_CUT_MULT 0.5f // on jump release, damp upward velocity by this factor
#define COYOTE_TIME 0.12f
#define JUMP_BUFFER_TIME 0.12f

#define GROUND_STICK_TIME 0.030f

// Player hitbox / movement
#define MAX_SPEED_X_CROUCH 320.0f
#define CROUCH_FRICTION 0.97f

// Dust particle tuning
#define DUST_MAX 96
#define DUST_GRAVITY 240.0f
#define DUST_DRAG 0.97f

// Wall interaction
#define WALL_JUMP_PUSH_X 320.0f // Slightly increased for better wall jump distance
#define WALL_SLIDE_MAX_FALL 400.0f // Target max fall speed while wall sliding
#define WALL_SLIDE_ACCEL 2000.0f // Faster decel to clamp toward the target
// Allow a brief grace period to wall-jump after leaving a wall
#define WALL_COYOTE_TIME 0.12f

// Roguelike mode tuning
#define ROGUE_SPAWN_INTERVAL_MS 1500 // interval per spawner
#define ROGUE_ENEMY_SPEED 220.0f
#define ROGUE_ENEMY_MAX_FALL 900.0f
#define ROGUE_ENEMY_W ((float)SQUARE_SIZE * 0.75f)
#define ROGUE_ENEMY_H ((float)SQUARE_SIZE * 0.75f)
#define ROGUE_STOMP_BOUNCE_SPEED -520.0f
#define ROGUE_STOMP_GRACE 10.0f

// Warrior anim tuning
#define ANIM_DASH_SPEED_FRAC 1.2f // fraction of MAX_SPEED_X to consider dash (keep run anim at normal cap)
#define ANIM_SLIDE_SPEED 80.0f // crouch + above this speed => slide anim
#define ANIM_HURT_LAND_SPEED 950.0f // landing impact to trigger hurt anim
#define ANIM_HURT_DURATION 0.35f
#define ANIM_LADDER_SLIDE_SPEED 40.0f // wall slide speed threshold to use ladder idle
#define WARRIOR_SCALE (SQUARE_SIZE * 1.4f / (float)WARRIOR_FRAME_H) // visual scale multiplier for warrior sprite
#define WARRIOR_FRAME_W 69
#define WARRIOR_FRAME_H 44

// Debug
#define DEBUG_DRAW_BOUNDS 0

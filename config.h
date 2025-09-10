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

// Timing
#define BASE_FPS 120.0f
#define BASE_DT (1.0f / BASE_FPS)

// (Other constants below)

// Physics tuning
#define GRAVITY 1800.0f
#define MOVE_ACCEL 1500.0f
#define AIR_ACCEL 900.0f
#define GROUND_FRICTION 0.88f
#define AIR_FRICTION 0.985f
#define MAX_SPEED_X 640.0f
#define MAX_SPEED_Y 1400.0f
#define JUMP_SPEED -760.0f // ~1.5x higher jump (via sqrt scaling)
#define JUMP_CUT_MULT 0.5f // on jump release, damp upward velocity by this factor
#define COYOTE_TIME 0.12f
#define JUMP_BUFFER_TIME 0.12f

#define GROUND_STICK_TIME 0.030f

// Player hitbox
#define PLAYER_W ((float)SQUARE_SIZE * 0.8f)
#define PLAYER_H ((float)SQUARE_SIZE * 0.9f)
#define PLAYER_H_CROUCH (PLAYER_H * 0.5f)
#define MAX_SPEED_X_CROUCH 640.0f
#define CROUCH_FRICTION 0.97f

// Wall interaction
#define WALL_JUMP_PUSH_X 300.0f
#define WALL_SLIDE_MAX_FALL 260.0f
// Allow a brief grace period to wall-jump after leaving a wall
#define WALL_COYOTE_TIME 0.12f

// Shared configuration constants
#pragma once

// Window
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Grid / tiles
#define SQUARE_SIZE 20
#define GRID_COLS (WINDOW_WIDTH / SQUARE_SIZE)
#define GRID_ROWS (WINDOW_HEIGHT / SQUARE_SIZE)

// Physics tuning
#define GRAVITY 1800.0f
#define MOVE_ACCEL 7000.0f
#define AIR_ACCEL 4200.0f
#define GROUND_FRICTION 0.88f
#define AIR_FRICTION 0.985f
#define MAX_SPEED_X 420.0f
#define MAX_SPEED_Y 1400.0f
#define JUMP_SPEED -620.0f
#define COYOTE_TIME 0.12f
#define JUMP_BUFFER_TIME 0.12f

#define GROUND_STICK_TIME 0.030f

// Player hitbox
#define PLAYER_W ((float)SQUARE_SIZE * 0.8f)
#define PLAYER_H ((float)SQUARE_SIZE * 0.9f)
#define PLAYER_H_CROUCH (PLAYER_H * 0.5f)
#define MAX_SPEED_X_CROUCH 420.0f
#define CROUCH_FRICTION 0.97f

// Wall interaction
#define WALL_JUMP_PUSH_X 420.0f
#define WALL_SLIDE_MAX_FALL 260.0f


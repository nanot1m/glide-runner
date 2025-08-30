#include "render.h"
#include "raylib.h"

Rectangle PlayerAABB(const GameState *g) {
  float h = g->crouching ? PLAYER_H_CROUCH : PLAYER_H;
  return (Rectangle){ g->playerPos.x, g->playerPos.y, PLAYER_W, h };
}

Rectangle ExitAABB(const GameState *g) {
  return (Rectangle){ g->exitPos.x, g->exitPos.y, (float)SQUARE_SIZE, (float)SQUARE_SIZE };
}

Rectangle TileRect(int cx, int cy) {
  return (Rectangle){ CellToWorld(cx), CellToWorld(cy), (float)SQUARE_SIZE, (float)SQUARE_SIZE };
}

Rectangle LaserStripeRect(Vector2 laserPos) {
  return (Rectangle){ laserPos.x, laserPos.y + LASER_STRIPE_OFFSET, (float)SQUARE_SIZE, LASER_STRIPE_THICKNESS };
}

void RenderTiles(const LevelEditorState *ed) {
  for (int y = 0; y < GRID_ROWS; ++y) {
    for (int x = 0; x < GRID_COLS; ++x) {
      TileType t = ed->tiles[y][x];
      if (IsSolidTile(t)) {
        Rectangle r = TileRect(x, y);
        DrawRectangleRec(r, GRAY);
      } else if (IsHazardTile(t)) {
        Rectangle lr = LaserStripeRect((Vector2){ CellToWorld(x), CellToWorld(y) });
        DrawRectangleRec(lr, RED);
      }
    }
  }
}

void DrawStats(const GameState *g) {
  DrawText(TextFormat("FPS: %d", GetFPS()), 10, 40, 20, RED);
  DrawText(TextFormat("Vel: (%.0f, %.0f)", g->playerVel.x, g->playerVel.y), 10, 70, 20, DARKGRAY);
  DrawText(g->onGround ? "Grounded" : "Air", 10, 100, 20, DARKGRAY);
  DrawText(TextFormat("Coy: %.2f  Buf: %.2f", g->coyoteTimer, g->jumpBufferTimer), 10, 130, 20, DARKGRAY);
}


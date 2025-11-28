#include "autotiler.h"
#include <stddef.h>
#include <stdbool.h>
#include "raylib.h"

static AutotilerConfig gConfig = {0};

void Autotiler_Init(const AutotilerConfig *config) {
	if (config != NULL) {
		gConfig = *config;
	}
}

static inline bool IsBlockAt(void *context, int cx, int cy) {
	if (gConfig.checkBlock == NULL) return false;
	return gConfig.checkBlock(context, cx, cy);
}

static Rectangle BlockTileSrc(int tx, int ty) {
	return (Rectangle){(float)(tx * gConfig.tileSize), (float)(ty * gConfig.tileSize), (float)gConfig.tileSize, (float)gConfig.tileSize};
}

static Rectangle ChooseRowNoVertical(bool left, bool right) {
	if (!left && !right) return BlockTileSrc(3, 3); // isolated middle
	if (!left && right) return BlockTileSrc(0, 3); // left edge
	if (!right && left) return BlockTileSrc(2, 3); // right edge
	return BlockTileSrc(1, 3); // middle row
}

static Rectangle ChooseTopBand(bool left, bool right, bool downLeft, bool downRight) {
	if (!left && !right) return BlockTileSrc(3, 0); // isolated top
	if (left && right) {
		if (!downLeft && !downRight) return BlockTileSrc(9, 3); // inner bottom
		if (!downLeft && downRight) return BlockTileSrc(7, 0); // inner bottom-left
		if (downLeft && !downRight) return BlockTileSrc(6, 0); // inner bottom-right
		return BlockTileSrc(1, 0); // top edge
	}
	if (!left) {
		if (!downRight) return BlockTileSrc(4, 0); // inner bottom-right
		return BlockTileSrc(0, 0); // top-left corner
	}
	// !right
	if (!downLeft) return BlockTileSrc(5, 0); // inner bottom-left
	return BlockTileSrc(2, 0); // top-right corner
}

static Rectangle ChooseBottomBand(bool left, bool right, bool upLeft, bool upRight) {
	if (!left && !right) return BlockTileSrc(3, 2); // isolated bottom
	if (left && right) {
		if (!upLeft && !upRight) return BlockTileSrc(8, 3); // inner top
		if (!upLeft && upRight) return BlockTileSrc(7, 1); // inner top-left
		if (upLeft && !upRight) return BlockTileSrc(6, 1); // inner top-right
		return BlockTileSrc(1, 2); // bottom edge
	}
	if (!left) {
		if (!upRight) return BlockTileSrc(4, 1); // inner top-right
		return BlockTileSrc(0, 2); // bottom-left
	}
	// !right
	if (!upLeft) return BlockTileSrc(5, 1); // inner top-left
	return BlockTileSrc(2, 2); // bottom-right
}

static Rectangle ChooseInteriorWithSides(bool upLeft, bool upRight, bool downLeft, bool downRight) {
	if (!downLeft && !downRight && !upLeft && !upRight) return BlockTileSrc(8, 1);
	if (upLeft && upRight && !downLeft && !downRight) return BlockTileSrc(9, 2);
	if (!upLeft && !downLeft && upRight && downRight) return BlockTileSrc(9, 0);
	if (upLeft && downLeft && !upRight && !downRight) return BlockTileSrc(8, 0);
	if (!upLeft && !upRight && !downRight && downLeft) return BlockTileSrc(9, 1);
	if (!upLeft && !upRight && downRight && !downLeft) return BlockTileSrc(10, 1);
	if (upLeft && downRight && !upRight && !downLeft) return BlockTileSrc(10, 2);
	if (!upLeft && !downRight && upRight && downLeft) return BlockTileSrc(10, 3);
	if (!downRight && !downLeft && !upRight && upLeft) return BlockTileSrc(11, 2);
	if (!downRight && !downLeft && upRight && !upLeft) return BlockTileSrc(11, 3);
	if (!upLeft && !upRight) return BlockTileSrc(8, 2);
	if (!upLeft) return BlockTileSrc(5, 3);
	if (!upRight) return BlockTileSrc(4, 3);
	if (!downRight) return BlockTileSrc(4, 2);
	return BlockTileSrc(1, 1);
}

static Rectangle ChooseOpenLeft(bool upRight, bool downRight) {
	if (!upRight && !downRight) return BlockTileSrc(4, 0);
	if (!downRight) return BlockTileSrc(6, 2);
	if (!upRight) return BlockTileSrc(6, 3);
	return BlockTileSrc(0, 1);
}

static Rectangle ChooseOpenRight(bool upLeft, bool downLeft) {
	if (!upLeft && !downLeft) return BlockTileSrc(5, 0);
	if (!downLeft) return BlockTileSrc(7, 2);
	if (!upLeft) return BlockTileSrc(7, 3);
	return BlockTileSrc(2, 1);
}

Rectangle Autotiler_GetBlockTile(void *context, int cx, int cy) {
	bool up = IsBlockAt(context, cx, cy - 1);
	bool down = IsBlockAt(context, cx, cy + 1);
	bool left = IsBlockAt(context, cx - 1, cy);
	bool right = IsBlockAt(context, cx + 1, cy);
	bool upLeft = IsBlockAt(context, cx - 1, cy - 1);
	bool upRight = IsBlockAt(context, cx + 1, cy - 1);
	bool downLeft = IsBlockAt(context, cx - 1, cy + 1);
	bool downRight = IsBlockAt(context, cx + 1, cy + 1);

	if (!up && !down) {
		return ChooseRowNoVertical(left, right);
	}
	if (!up) {
		return ChooseTopBand(left, right, downLeft, downRight);
	}
	if (!down) {
		return ChooseBottomBand(left, right, upLeft, upRight);
	}
	if (left && right) {
		return ChooseInteriorWithSides(upLeft, upRight, downLeft, downRight);
	}
	if (!left && right) {
		return ChooseOpenLeft(upRight, downRight);
	}
	if (!right && left) {
		return ChooseOpenRight(upLeft, downLeft);
	}
	if (!right && !left) return BlockTileSrc(3, 1);

	return BlockTileSrc(1, 1);
}

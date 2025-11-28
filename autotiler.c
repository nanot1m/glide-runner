#include "autotiler.h"
#include <stdbool.h>
#include <stddef.h>
#include "raylib.h"

static AutotilerConfig gConfig = {0};

bool Autotiler_Init(const AutotilerConfig *config) {
	if (config != NULL && config->checkBlock != NULL && config->tileSize > 0) {
		gConfig = *config;
		return true;
	}
	return false;
}

static inline bool IsBlockAt(const void *context, int cx, int cy) {
	if (gConfig.checkBlock == NULL) return false;
	return gConfig.checkBlock(context, cx, cy);
}

static Rectangle BlockTileSrc(TilePos pos) {
	return (Rectangle){(float)(pos.col * gConfig.tileSize), (float)(pos.row * gConfig.tileSize), (float)gConfig.tileSize, (float)gConfig.tileSize};
}

static Rectangle ChooseRowNoVertical(bool left, bool right) {
	if (!left && !right) return BlockTileSrc(gConfig.layout.rowNoVertical_isolated);
	if (!left && right) return BlockTileSrc(gConfig.layout.rowNoVertical_leftEdge);
	if (!right && left) return BlockTileSrc(gConfig.layout.rowNoVertical_rightEdge);
	return BlockTileSrc(gConfig.layout.rowNoVertical_middle);
}

static Rectangle ChooseTopBand(bool left, bool right, bool downLeft, bool downRight) {
	if (!left && !right) return BlockTileSrc(gConfig.layout.topBand_isolated);
	if (left && right) {
		if (!downLeft && !downRight) return BlockTileSrc(gConfig.layout.topBand_innerBottom);
		if (!downLeft && downRight) return BlockTileSrc(gConfig.layout.topBand_innerBottomLeft);
		if (downLeft && !downRight) return BlockTileSrc(gConfig.layout.topBand_innerBottomRight);
		return BlockTileSrc(gConfig.layout.topBand_edge);
	}
	if (!left) {
		if (!downRight) return BlockTileSrc(gConfig.layout.topBand_innerBottomRightNoDownRight);
		return BlockTileSrc(gConfig.layout.topBand_topLeftCorner);
	}
	// !right
	if (!downLeft) return BlockTileSrc(gConfig.layout.topBand_innerBottomLeftNoDownLeft);
	return BlockTileSrc(gConfig.layout.topBand_topRightCorner);
}

static Rectangle ChooseBottomBand(bool left, bool right, bool upLeft, bool upRight) {
	if (!left && !right) return BlockTileSrc(gConfig.layout.bottomBand_isolated);
	if (left && right) {
		if (!upLeft && !upRight) return BlockTileSrc(gConfig.layout.bottomBand_innerTop);
		if (!upLeft && upRight) return BlockTileSrc(gConfig.layout.bottomBand_innerTopLeft);
		if (upLeft && !upRight) return BlockTileSrc(gConfig.layout.bottomBand_innerTopRight);
		return BlockTileSrc(gConfig.layout.bottomBand_edge);
	}
	if (!left) {
		if (!upRight) return BlockTileSrc(gConfig.layout.bottomBand_innerTopRightNoUpRight);
		return BlockTileSrc(gConfig.layout.bottomBand_bottomLeft);
	}
	// !right
	if (!upLeft) return BlockTileSrc(gConfig.layout.bottomBand_innerTopLeftNoUpLeft);
	return BlockTileSrc(gConfig.layout.bottomBand_bottomRight);
}

static Rectangle ChooseInteriorWithSides(bool upLeft, bool upRight, bool downLeft, bool downRight) {
	if (!downLeft && !downRight && !upLeft && !upRight) return BlockTileSrc(gConfig.layout.interior_allDiagonalsOpen);
	if (upLeft && upRight && !downLeft && !downRight) return BlockTileSrc(gConfig.layout.interior_upDiagonals);
	if (!upLeft && !downLeft && upRight && downRight) return BlockTileSrc(gConfig.layout.interior_rightDiagonals);
	if (upLeft && downLeft && !upRight && !downRight) return BlockTileSrc(gConfig.layout.interior_leftDiagonals);
	if (!upLeft && !upRight && !downRight && downLeft) return BlockTileSrc(gConfig.layout.interior_downLeft);
	if (!upLeft && !upRight && downRight && !downLeft) return BlockTileSrc(gConfig.layout.interior_downRight);
	if (upLeft && downRight && !upRight && !downLeft) return BlockTileSrc(gConfig.layout.interior_upLeftDownRight);
	if (!upLeft && !downRight && upRight && downLeft) return BlockTileSrc(gConfig.layout.interior_upRightDownLeft);
	if (!downRight && !downLeft && !upRight && upLeft) return BlockTileSrc(gConfig.layout.interior_upLeft);
	if (!downRight && !downLeft && upRight && !upLeft) return BlockTileSrc(gConfig.layout.interior_upRight);
	if (!upLeft && !upRight) return BlockTileSrc(gConfig.layout.interior_upDiagonalsOpen);
	if (!upLeft) return BlockTileSrc(gConfig.layout.interior_upLeftOpen);
	if (!upRight) return BlockTileSrc(gConfig.layout.interior_upRightOpen);
	if (!downRight) return BlockTileSrc(gConfig.layout.interior_downRightOpen);
	return BlockTileSrc(gConfig.layout.interior_full);
}

static Rectangle ChooseOpenLeft(bool upRight, bool downRight) {
	if (!upRight && !downRight) return BlockTileSrc(gConfig.layout.openLeft_allOpen);
	if (!downRight) return BlockTileSrc(gConfig.layout.openLeft_downRightOpen);
	if (!upRight) return BlockTileSrc(gConfig.layout.openLeft_upRightOpen);
	return BlockTileSrc(gConfig.layout.openLeft_leftEdge);
}

static Rectangle ChooseOpenRight(bool upLeft, bool downLeft) {
	if (!upLeft && !downLeft) return BlockTileSrc(gConfig.layout.openRight_allOpen);
	if (!downLeft) return BlockTileSrc(gConfig.layout.openRight_downLeftOpen);
	if (!upLeft) return BlockTileSrc(gConfig.layout.openRight_upLeftOpen);
	return BlockTileSrc(gConfig.layout.openRight_rightEdge);
}

Rectangle Autotiler_GetBlockTile(const void *context, int cx, int cy) {
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
	if (!right && !left) return BlockTileSrc(gConfig.layout.isolated_vertical);

	return BlockTileSrc(gConfig.layout.isolated_full);
}

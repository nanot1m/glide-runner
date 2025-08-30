// UI helpers (lists, messages) and input gate
#pragma once
#include "raylib.h"
#include <stdbool.h>
#include "game.h"

typedef struct {
  float startY;
  float stepY;
  float itemHeight;
  int fontSize;
} UiListSpec;

typedef const char *(*LabelAtFn)(int index, void *ud);

void UiListHandle(const UiListSpec *spec, int *selected, int itemCount, bool *outActivated);
void UiListRenderCB(const UiListSpec *spec, int selected, int itemCount, LabelAtFn labelAt, void *ud,
                    const char *title, const char *emptyMsg, const char *hint);

void RenderMessageScreen(const char *firstText, Color firstColor, int firstFontSize, ...);
void RenderVictory(const GameState *game);
void RenderDeath(void);

// Input gating for debouncing across screens
typedef enum { IG_FREE = 0, IG_BLOCK_ONCE = 1, IG_LATCHED = 2 } InputGateState;
bool InputGate_BeginFrameBlocked(void);
void InputGate_RequestBlockOnce(void);
void InputGate_LatchIfEdgeOccurred(bool edgePressed);

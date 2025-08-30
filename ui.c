#include "ui.h"
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include "game.h"
#include "input_config.h"
#include "raylib.h"

// Hover suppression
static bool gUiSuppressHover = false;
static Vector2 gUiLastMouse = {-9999.0f, -9999.0f};

// Input gate
static InputGateState gInputGate = IG_FREE;

static Rectangle UiListItemRect(const UiListSpec *spec, int index) {
	float x = 20.0f;
	float y = spec->startY + index * spec->stepY;
	float w = (float)(GetScreenWidth() - 40);
	float h = spec->itemHeight;
	return (Rectangle){x, y, w, h};
}

static int UiListIndexAtMouse(Vector2 m, const UiListSpec *spec, int itemCount) {
	for (int i = 0; i < itemCount; ++i)
		if (CheckCollisionPointRec(m, UiListItemRect(spec, i))) return i;
	return -1;
}

static inline bool UiListIsKeyActivatePressed(void) { return InputPressed(ACT_ACTIVATE); }

static bool AnyInputDown(void) {
	bool anyKeyHeld = InputDown(ACT_ACTIVATE) || InputDown(ACT_BACK) || InputDown(ACT_NAV_UP) || InputDown(ACT_NAV_DOWN) || InputDown(ACT_NAV_LEFT) || InputDown(ACT_NAV_RIGHT) || InputDown(ACT_LEFT) || InputDown(ACT_RIGHT) || InputDown(ACT_DOWN);
	bool anyMouseHeld = IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_RIGHT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
	return anyKeyHeld || anyMouseHeld;
}

bool InputGate_BeginFrameBlocked(void) {
	if (gInputGate == IG_BLOCK_ONCE || gInputGate == IG_LATCHED) {
		if (AnyInputDown()) return true; // keep blocking while anything is held
		gInputGate = IG_FREE;
		return false;
	}
	return false;
}
void InputGate_RequestBlockOnce(void) { gInputGate = IG_BLOCK_ONCE; }
void InputGate_LatchIfEdgeOccurred(bool edgePressed) {
	if (edgePressed || IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) || IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) gInputGate = IG_LATCHED;
}

void UiListHandle(const UiListSpec *spec, int *selected, int itemCount, bool *outActivated) {
	if (outActivated) *outActivated = false;
	if (InputGate_BeginFrameBlocked()) return;

	Vector2 m = GetMousePosition();
	if (fabsf(m.x - gUiLastMouse.x) > 1.0f || fabsf(m.y - gUiLastMouse.y) > 1.0f) {
		gUiSuppressHover = false;
		gUiLastMouse = m;
	}

	bool up = InputPressed(ACT_NAV_UP) || InputPressed(ACT_NAV_LEFT);
	bool down = InputPressed(ACT_NAV_DOWN) || InputPressed(ACT_NAV_RIGHT);
	bool keyActivate = UiListIsKeyActivatePressed();
	if (up || down) { gUiSuppressHover = true; }
	if (up) {
		if (*selected > 0)
			(*selected)--;
		else
			*selected = itemCount > 0 ? itemCount - 1 : 0;
	}
	if (down) {
		if (*selected < itemCount - 1)
			(*selected)++;
		else
			*selected = 0;
	}

	int hover = gUiSuppressHover ? -1 : UiListIndexAtMouse(m, spec, itemCount);
	if (hover != -1) *selected = hover;
	if ((hover != -1 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || keyActivate) {
		if (outActivated) *outActivated = true;
		InputGate_LatchIfEdgeOccurred(true);
	}
}

void UiListRenderCB(const UiListSpec *spec, int selected, int itemCount, LabelAtFn labelAt, void *ud, const char *title, const char *emptyMsg, const char *hint) {
	if (title) DrawText(title, 20, 20, 32, DARKGRAY);
	if (itemCount == 0 && emptyMsg) DrawText(emptyMsg, 20, (int)spec->startY, 24, RED);

	Vector2 m = GetMousePosition();
	int hover = gUiSuppressHover ? -1 : UiListIndexAtMouse(m, spec, itemCount);
	for (int i = 0; i < itemCount; ++i) {
		Rectangle r = UiListItemRect(spec, i);
		if (hover == i) {
			Rectangle hr = r;
			hr.y -= 2;
			DrawRectangleRec(hr, (Color){230, 230, 230, 255});
		}
		Color c = (selected == i) ? RED : BLUE;
		const char *label = labelAt ? labelAt(i, ud) : "";
		DrawText(label, (int)r.x, (int)r.y, spec->fontSize, c);
	}
	if (hint) DrawText(hint, 20, (int)(spec->startY + itemCount * spec->stepY + 10), 18, DARKGRAY);
}

void RenderMessageScreen(const char *firstText, Color firstColor, int firstFontSize, ...) {
	if (!firstText) return;
	typedef struct {
		const char *text;
		Color color;
		int size;
	} Line;
	Line lines[16];
	int count = 0;
	lines[count++] = (Line){firstText, firstColor, firstFontSize};
	va_list ap;
	va_start(ap, firstFontSize);
	while (count < (int)(sizeof(lines) / sizeof(lines[0]))) {
		const char *t = va_arg(ap, const char *);
		if (t == NULL) break;
		Color c = va_arg(ap, Color);
		int sz = va_arg(ap, int);
		lines[count++] = (Line){t, c, sz};
	}
	va_end(ap);
	const int spacing = 10;
	int totalH = 0;
	for (int i = 0; i < count; ++i) totalH += lines[i].size;
	if (count > 1) totalH += spacing * (count - 1);
	int cx = GetScreenWidth() / 2;
	int y = GetScreenHeight() / 2 - totalH / 2;
	for (int i = 0; i < count; ++i) {
		int w = MeasureText(lines[i].text, lines[i].size);
		DrawText(lines[i].text, cx - w / 2, y, lines[i].size, lines[i].color);
		y += lines[i].size + spacing;
	}
}

void RenderVictory(const struct GameState *game) {
	float seconds = (float)game->score / 1000.0f;
	const char *scoreTxt = TextFormat("Score: %.2f s", seconds);

	// clang-format off
	RenderMessageScreen(
	    "VICTORY!", GREEN, 40,
		"You reached the exit.", DARKGRAY, 24,
		scoreTxt, BLUE, 28,
		"Enter: restart | Space/Esc: menu", BLUE, 20,
		NULL);
	// clang-format on
}
void RenderDeath(void) {
	// clang-format off
	RenderMessageScreen(
	    "YOU DIED!", RED, 40,
		"You touched a laser.", DARKGRAY, 24,
		"Enter: restart | Space/Esc: menu", BLUE, 20,
		NULL);
	// clang-format on
}

#include "fps_meter.h"
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <direct.h>
#endif
#include "config.h"
#include "raylib.h"

#if !ENABLE_FPS_METER

void FpsMeter_Init(void) {}
void FpsMeter_BeginFrame(void) {}
void FpsMeter_Draw(void) {}
void FpsMeter_SetEnabled(bool enabled) { (void)enabled; }
bool FpsMeter_IsEnabled(void) { return false; }

#else

#define FPS_HISTORY_SECONDS 30.0f
#define FPS_MAX_SAMPLES 4000
#define FPS_GRAPH_WIDTH 110
#define FPS_GRAPH_HEIGHT 36
#define FPS_GRAPH_MARGIN 6
#define FPS_LABEL_HEIGHT 12

typedef struct {
	float t;
	float fps;
} FpsSample;

static FpsSample gHistory[FPS_MAX_SAMPLES];
static int gHead = 0;
static int gCount = 0;
static float gLabelFps = 0.0f;
static float gLastLabelTime = 0.0f;
static bool gEnabled = true;

static void EnsureConfigDir(void) {
#ifndef _WIN32
	mkdir("config", 0755);
#else
	_mkdir("config");
#endif
}

static void SaveSettings(void) {
	EnsureConfigDir();
	FILE *f = fopen("config/settings.cfg", "w");
	if (!f) return;
	fprintf(f, "fps_meter=%d\n", gEnabled ? 1 : 0);
	fclose(f);
}

static void LoadSettings(void) {
	FILE *f = fopen("config/settings.cfg", "r");
	if (!f) return;
	char key[64] = {0};
	int val = 1;
	while (fscanf(f, "%63[^=]=%d\n", key, &val) == 2) {
		if (strcmp(key, "fps_meter") == 0) {
			gEnabled = (val != 0);
		}
	}
	fclose(f);
}

static int IndexAt(int offset) { return (gHead + offset) % FPS_MAX_SAMPLES; }

static void PruneOld(float now) {
	float cutoff = now - FPS_HISTORY_SECONDS;
	while (gCount > 0 && gHistory[gHead].t < cutoff) {
		gHead = (gHead + 1) % FPS_MAX_SAMPLES;
		gCount--;
	}
}

void FpsMeter_Init(void) {
	gHead = 0;
	gCount = 0;
	gLabelFps = 0.0f;
	gLastLabelTime = 0.0f;
	gEnabled = true;
	LoadSettings();
}

void FpsMeter_BeginFrame(void) {
	if (!gEnabled) return;
	float now = (float)GetTime();
	float dt = GetFrameTime();
	float fps = (dt > 0.0001f) ? (1.0f / dt) : (float)GetFPS();

	if (gCount == FPS_MAX_SAMPLES) {
		// Drop the oldest when the ring buffer is full
		gHead = (gHead + 1) % FPS_MAX_SAMPLES;
		gCount--;
	}
	int writeIdx = IndexAt(gCount);
	gHistory[writeIdx].t = now;
	gHistory[writeIdx].fps = fps;
	gCount++;
	PruneOld(now);
}

static float HistoryMaxFps(float now) {
	PruneOld(now);
	float maxFps = BASE_FPS;
	for (int i = 0; i < gCount; ++i) {
		int idx = IndexAt(i);
		if (gHistory[idx].fps > maxFps) maxFps = gHistory[idx].fps;
	}
	if (maxFps < 30.0f) maxFps = 30.0f;
	return maxFps;
}

void FpsMeter_Draw(void) {
	if (!gEnabled) return;
	if (gCount < 2) return;
	float now = (float)GetTime();
	PruneOld(now);
	if (gCount < 2) return;

	int screenW = GetScreenWidth();
	int boxX = screenW - FPS_GRAPH_MARGIN - FPS_GRAPH_WIDTH;
	int boxY = FPS_GRAPH_MARGIN;
	int boxW = FPS_GRAPH_WIDTH;
	int boxH = FPS_GRAPH_HEIGHT + FPS_LABEL_HEIGHT + 6;

	// Panel background and outline
	DrawRectangle(boxX - 3, boxY - 3, boxW + 6, boxH + 6, (Color){0, 0, 0, 170});
	DrawRectangleLines(boxX - 3, boxY - 3, boxW + 6, boxH + 6, (Color){80, 80, 80, 200});

	float latestFps = gHistory[IndexAt(gCount - 1)].fps;
	if (gLabelFps <= 0.0f || (now - gLastLabelTime) >= 0.25f) {
		gLabelFps = latestFps;
		gLastLabelTime = now;
	}
	// Fixed width to reduce text jitter when values change
	DrawText(TextFormat("FPS %03.0f", gLabelFps), boxX, boxY - 2, 12, RAYWHITE);

	int graphY = boxY + FPS_LABEL_HEIGHT + 4;
	float maxFps = HistoryMaxFps(now);
	float pxPerSec = (float)FPS_GRAPH_WIDTH / FPS_HISTORY_SECONDS;

	// Target line at BASE_FPS
	float targetY = graphY + FPS_GRAPH_HEIGHT - (BASE_FPS / maxFps) * FPS_GRAPH_HEIGHT;
	if (targetY < graphY) targetY = graphY;
	if (targetY > graphY + FPS_GRAPH_HEIGHT) targetY = graphY + FPS_GRAPH_HEIGHT;
	DrawLine(boxX, (int)targetY, boxX + FPS_GRAPH_WIDTH, (int)targetY, (Color){100, 100, 120, 180});

	// Plot history from oldest to newest
	Vector2 prev = {0};
	bool havePrev = false;
	for (int i = 0; i < gCount; ++i) {
		const FpsSample *s = &gHistory[IndexAt(i)];
		float age = now - s->t;
		if (age < 0.0f) age = 0.0f;
		float x = (float)boxX + (float)FPS_GRAPH_WIDTH - age * pxPerSec;
		if (x < boxX) continue;
		float norm = s->fps / maxFps;
		if (norm > 1.0f) norm = 1.0f;
		if (norm < 0.0f) norm = 0.0f;
		float y = (float)graphY + (float)FPS_GRAPH_HEIGHT - norm * (float)FPS_GRAPH_HEIGHT;
		Vector2 cur = {x, y};
		if (havePrev) {
			DrawLineV(prev, cur, (Color){0, 255, 180, 230});
		}
		prev = cur;
		havePrev = true;
	}
}

void FpsMeter_SetEnabled(bool enabled) {
	gEnabled = enabled;
	SaveSettings();
}
bool FpsMeter_IsEnabled(void) { return gEnabled; }

#endif // ENABLE_FPS_METER

#include "input_config.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	int count;
	int keys[8];
} KeyList;
static KeyList gActions[ACT__COUNT];

typedef struct {
	const char *name;
	int key;
} KeyName;

// Minimal key name dictionary for the keys we use
static const KeyName KEY_NAMES[] = {
    {"SPACE", KEY_SPACE},
    {"ENTER", KEY_ENTER},
    {"RETURN", KEY_ENTER},
    {"ESCAPE", KEY_ESCAPE},
    {"TAB", KEY_TAB},
    {"UP", KEY_UP},
    {"DOWN", KEY_DOWN},
    {"LEFT", KEY_LEFT},
    {"RIGHT", KEY_RIGHT},
    {"W", KEY_W},
    {"A", KEY_A},
    {"S", KEY_S},
    {"D", KEY_D},
    {"ONE", KEY_ONE},
    {"TWO", KEY_TWO},
    {"THREE", KEY_THREE},
    {"FOUR", KEY_FOUR},
    {"FIVE", KEY_FIVE},
};

static int FindKeyByName(const char *s) {
	if (!s) return 0;
	char buf[32];
	size_t n = strlen(s);
	if (n >= sizeof(buf)) n = sizeof(buf) - 1;
	for (size_t i = 0; i < n; i++) buf[i] = (char)toupper((unsigned char)s[i]);
	buf[n] = '\0';
	// trim spaces
	while (n > 0 && isspace((unsigned char)buf[n - 1])) buf[--n] = '\0';
	const char *p = buf;
	while (*p && isspace((unsigned char)*p)) p++;
	for (size_t i = 0; i < sizeof(KEY_NAMES) / sizeof(KEY_NAMES[0]); ++i) {
		if (strcmp(p, KEY_NAMES[i].name) == 0) return KEY_NAMES[i].key;
	}
	return 0;
}

static void AddKey(InputAction a, int key) {
	if (key == 0) return;
	KeyList *kl = &gActions[a];
	if (kl->count >= (int)(sizeof(kl->keys) / sizeof(kl->keys[0]))) return;
	// prevent duplicates
	for (int i = 0; i < kl->count; i++)
		if (kl->keys[i] == key) return;
	kl->keys[kl->count++] = key;
}

static void ClearAll(void) {
	for (int i = 0; i < ACT__COUNT; i++) gActions[i].count = 0;
}

static void LoadDefaults(void) {
	ClearAll();
	// Core gameplay/menu defaults
	AddKey(ACT_ACTIVATE, KEY_ENTER);
	AddKey(ACT_ACTIVATE, KEY_SPACE);
	AddKey(ACT_BACK, KEY_ESCAPE);
	AddKey(ACT_NAV_UP, KEY_UP);
	AddKey(ACT_NAV_UP, KEY_W);
	AddKey(ACT_NAV_DOWN, KEY_DOWN);
	AddKey(ACT_NAV_DOWN, KEY_S);
	AddKey(ACT_NAV_LEFT, KEY_LEFT);
	AddKey(ACT_NAV_LEFT, KEY_A);
	AddKey(ACT_NAV_RIGHT, KEY_RIGHT);
	AddKey(ACT_NAV_RIGHT, KEY_D);
	AddKey(ACT_LEFT, KEY_LEFT);
	AddKey(ACT_LEFT, KEY_A);
	AddKey(ACT_RIGHT, KEY_RIGHT);
	AddKey(ACT_RIGHT, KEY_D);
	AddKey(ACT_DOWN, KEY_DOWN);
	AddKey(ACT_DOWN, KEY_S);
	AddKey(ACT_JUMP, KEY_SPACE);
	AddKey(ACT_JUMP, KEY_W);
	AddKey(ACT_JUMP, KEY_UP);
}

static const char *ActionName(InputAction a) {
	switch (a) {
	case ACT_ACTIVATE:
		return "activate";
	case ACT_BACK:
		return "back";
	case ACT_NAV_UP:
		return "nav_up";
	case ACT_NAV_DOWN:
		return "nav_down";
	case ACT_NAV_LEFT:
		return "nav_left";
	case ACT_NAV_RIGHT:
		return "nav_right";
	case ACT_LEFT:
		return "left";
	case ACT_RIGHT:
		return "right";
	case ACT_DOWN:
		return "down";
	case ACT_JUMP:
		return "jump";
	default:
		return "";
	}
}

static InputAction ActionByName(const char *name) {
	if (!name) return ACT__COUNT;
	char buf[32];
	size_t n = strlen(name);
	if (n >= sizeof(buf)) n = sizeof(buf) - 1;
	for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)name[i]);
	buf[n] = '\0';
	// trim
	char *p = buf;
	while (*p && isspace((unsigned char)*p)) p++;
	char *e = p + strlen(p);
	while (e > p && isspace((unsigned char)e[-1])) *--e = '\0';

	for (int a = 0; a < ACT__COUNT; a++)
		if (strcmp(p, ActionName((InputAction)a)) == 0) return (InputAction)a;
	return ACT__COUNT;
}

static void TryLoadFile(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) return;
	char line[256];
	while (fgets(line, sizeof(line), f)) {
		// skip comments and blanks
		char *s = line;
		while (*s && isspace((unsigned char)*s)) s++;
		if (*s == '#' || *s == '\0') continue;
		// key = values | values
		char *eq = strchr(s, '=');
		if (!eq) continue;
		*eq = '\0';
		char *keyName = s;
		char *vals = eq + 1;
		// strip end of key
		char *end = keyName + strlen(keyName);
		while (end > keyName && isspace((unsigned char)end[-1])) *--end = '\0';
		InputAction act = ActionByName(keyName);
		if (act == ACT__COUNT) continue;
		// clear current for this action (override defaults)
		gActions[act].count = 0;
		// parse values separated by '|'
		char *tok = strtok(vals, "|\r\n");
		while (tok) {
			// trim token
			while (*tok && isspace((unsigned char)*tok)) tok++;
			char *te = tok + strlen(tok);
			while (te > tok && isspace((unsigned char)te[-1])) *--te = '\0';
			int key = FindKeyByName(tok);
			AddKey(act, key);
			tok = strtok(NULL, "|\r\n");
		}
	}
	fclose(f);
}

void InputConfig_Init(void) {
	LoadDefaults();
	// Try load from repo config
	TryLoadFile("config/input.cfg");
}

bool InputDown(InputAction a) {
	if (a < 0 || a >= ACT__COUNT) return false;
	KeyList *kl = &gActions[a];
	for (int i = 0; i < kl->count; i++)
		if (IsKeyDown(kl->keys[i])) return true;
	return false;
}

bool InputPressed(InputAction a) {
	if (a < 0 || a >= ACT__COUNT) return false;
	KeyList *kl = &gActions[a];
	for (int i = 0; i < kl->count; i++)
		if (IsKeyPressed(kl->keys[i])) return true;
	return false;
}

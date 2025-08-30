#include "audio.h"
#include "raylib.h"

static Sound sfxJump, sfxVictory, sfxDeath, sfxMenu, sfxHover;
static bool sfxJumpLoaded = false, sfxVictoryLoaded = false, sfxDeathLoaded = false, sfxMenuLoaded = false, sfxHoverLoaded = false;

static Music musicMenu;
static bool musicMenuLoaded = false;
static bool musicMenuPlaying = false;

#define MENU_MUSIC_VOL 0.6f
#define MENU_MUSIC_FADE 1.5f
static float musicMenuVol = 0.0f;
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#endif
static float musicMenuTargetVol = MENU_MUSIC_VOL;
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

static bool LoadSoundIfExists(const char *path, Sound *out) {
  if (FileExists(path)) { *out = LoadSound(path); return true; }
  return false;
}
static bool LoadMusicIfExists(const char *path, Music *out) {
  if (FileExists(path)) { *out = LoadMusicStream(path); return true; }
  return false;
}

void Audio_Init(void) {
  // SFX
  sfxJumpLoaded = LoadSoundIfExists("assets/jump.wav", &sfxJump);
  sfxVictoryLoaded = LoadSoundIfExists("assets/victory.wav", &sfxVictory);
  sfxDeathLoaded = LoadSoundIfExists("assets/death.wav", &sfxDeath);
  sfxMenuLoaded = LoadSoundIfExists("assets/menu.wav", &sfxMenu);
  sfxHoverLoaded = LoadSoundIfExists("assets/hover.wav", &sfxHover);

  // Warmup: play/stop at 0 volume to avoid first-latency hiccup
  if (sfxJumpLoaded) { SetSoundVolume(sfxJump, 0.0f); PlaySound(sfxJump); StopSound(sfxJump); SetSoundVolume(sfxJump, 1.0f);} 
  if (sfxVictoryLoaded) { SetSoundVolume(sfxVictory, 0.0f); PlaySound(sfxVictory); StopSound(sfxVictory); SetSoundVolume(sfxVictory, 1.0f);} 
  if (sfxDeathLoaded) { SetSoundVolume(sfxDeath, 0.0f); PlaySound(sfxDeath); StopSound(sfxDeath); SetSoundVolume(sfxDeath, 1.0f);} 
  if (sfxMenuLoaded) { SetSoundVolume(sfxMenu, 0.0f); PlaySound(sfxMenu); StopSound(sfxMenu); SetSoundVolume(sfxMenu, 1.0f);} 

  // Menu music
  musicMenuLoaded = LoadMusicIfExists("assets/menu.mp3", &musicMenu);
  if (musicMenuLoaded) {
    musicMenu.looping = true;
    SetMusicVolume(musicMenu, 0.0f);
    musicMenuVol = 0.0f;
    musicMenuTargetVol = MENU_MUSIC_VOL;
  }
}

void Audio_Deinit(void) {
  if (musicMenuLoaded) {
    if (musicMenuPlaying) StopMusicStream(musicMenu);
    UnloadMusicStream(musicMenu);
    musicMenuLoaded = false;
    musicMenuPlaying = false;
  }
  if (sfxJumpLoaded) UnloadSound(sfxJump);
  if (sfxVictoryLoaded) UnloadSound(sfxVictory);
  if (sfxDeathLoaded) UnloadSound(sfxDeath);
  if (sfxMenuLoaded) UnloadSound(sfxMenu);
  if (sfxHoverLoaded) UnloadSound(sfxHover);
}

void Audio_PlayHover(void) { if (sfxHoverLoaded) PlaySound(sfxHover); }
void Audio_PlayMenuClick(void) { if (sfxMenuLoaded) PlaySound(sfxMenu); }
void Audio_PlayVictory(void) { if (sfxVictoryLoaded) PlaySound(sfxVictory); }
void Audio_PlayDeath(void) { if (sfxDeathLoaded) PlaySound(sfxDeath); }

void Audio_MenuMusicUpdate(bool inMenuScreens, float dt) {
  if (!musicMenuLoaded) return;
  if (inMenuScreens) {
    if (!musicMenuPlaying) { PlayMusicStream(musicMenu); musicMenuPlaying = true; }
    musicMenuTargetVol = MENU_MUSIC_VOL;
  } else {
    musicMenuTargetVol = 0.0f;
  }

  if (musicMenuVol < musicMenuTargetVol) {
    musicMenuVol += MENU_MUSIC_FADE * dt;
    if (musicMenuVol > musicMenuTargetVol) musicMenuVol = musicMenuTargetVol;
  } else if (musicMenuVol > musicMenuTargetVol) {
    musicMenuVol -= MENU_MUSIC_FADE * dt;
    if (musicMenuVol < musicMenuTargetVol) musicMenuVol = musicMenuTargetVol;
  }
  SetMusicVolume(musicMenu, musicMenuVol);
  if (musicMenuPlaying) UpdateMusicStream(musicMenu);
  if (!inMenuScreens && musicMenuPlaying && musicMenuVol <= 0.001f) {
    StopMusicStream(musicMenu);
    musicMenuPlaying = false;
  }
}


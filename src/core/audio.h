#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

// Global audio system (SDL_mixer 3.x). Audio is cross-cutting -- it fires from
// player/enemy/boss/game code -- so it lives behind a tiny global API instead
// of being threaded through every Update() signature. One mixer, one music
// track, a fixed set of predecoded sound effects.

// Sound-effect ids. The order MUST match SFX_PATHS[] in audio.c.
typedef enum {
    SFX_JUMP,
    SFX_ATTACK,
    SFX_SKILL,
    SFX_HURT,          // player took damage
    SFX_ENEMY_HIT,     // an enemy or the boss took damage
    SFX_ENEMY_DEATH,
    SFX_PLAYER_DEATH,
    SFX_VICTORY,
    SFX_COUNT
} Sfx;

// Opens the default audio device and loads music + every SFX from
// assets/audio/. Missing files are logged and skipped (that sound stays
// silent) the same way a missing texture is tolerated, so the game still runs
// with partial or no audio. Returns false only if the device can't be opened
// at all (e.g. no audio hardware) -- callers should treat that as non-fatal.
bool Audio_Init(void);

void Audio_PlaySfx(Sfx id);
void Audio_PlayMusic(void);   // start (or restart) the looping background track
void Audio_StopMusic(void);

void Audio_ToggleMute(void);
bool Audio_IsMuted(void);

void Audio_Shutdown(void);

#endif // AUDIO_H

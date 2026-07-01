#include "audio.h"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

// One mixer bound to the default playback device; SFX are fire-and-forget,
// music plays on its own looping track so we can stop/restart it.
static MIX_Mixer *g_mixer = NULL;
static MIX_Audio *g_sfx[SFX_COUNT] = {0};
static MIX_Audio *g_music = NULL;      // ambient/level background
static MIX_Audio *g_bossMusic = NULL;  // boss-fight theme
static MIX_Track *g_musicTrack = NULL; // one track; whichever music is active plays on it
static MIX_Audio *g_curMusic = NULL;   // which MIX_Audio is currently on g_musicTrack
static bool g_muted = false;

// Indexed by Sfx -- keep in lockstep with the enum in audio.h.
static const char *SFX_PATHS[SFX_COUNT] = {
    [SFX_JUMP]         = "assets/audio/jump.wav",
    [SFX_ATTACK]       = "assets/audio/attack.wav",
    [SFX_SKILL]        = "assets/audio/skill.wav",
    [SFX_HURT]         = "assets/audio/hurt.wav",
    [SFX_ENEMY_HIT]    = "assets/audio/enemy_hit.wav",
    [SFX_ENEMY_DEATH]  = "assets/audio/enemy_death.wav",
    [SFX_PLAYER_DEATH] = "assets/audio/player_death.wav",
    [SFX_VICTORY]      = "assets/audio/victory.wav",
};
static const char *MUSIC_PATH = "assets/audio/music.wav";
static const char *BOSS_MUSIC_PATH = "assets/audio/boss theme.wav";

// Music sits a bit under the SFX so hits/jumps still cut through.
#define MUSIC_GAIN 0.5f

bool Audio_Init(void) {
    if (!MIX_Init()) {
        SDL_Log("Audio: MIX_Init failed: %s", SDL_GetError());
        return false;
    }

    g_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!g_mixer) {
        SDL_Log("Audio: could not open audio device (running silent): %s", SDL_GetError());
        MIX_Quit();
        return false;
    }

    // Predecode SFX (short, played often); missing files are non-fatal.
    for (int i = 0; i < SFX_COUNT; i++) {
        g_sfx[i] = MIX_LoadAudio(g_mixer, SFX_PATHS[i], true);
        if (!g_sfx[i]) {
            SDL_Log("Audio: missing SFX '%s' (skipped): %s", SFX_PATHS[i], SDL_GetError());
        }
    }

    // Stream the (longer) music rather than predecoding it.
    g_music = MIX_LoadAudio(g_mixer, MUSIC_PATH, false);
    if (!g_music) {
        SDL_Log("Audio: missing music '%s' (skipped): %s", MUSIC_PATH, SDL_GetError());
    }
    g_bossMusic = MIX_LoadAudio(g_mixer, BOSS_MUSIC_PATH, false);
    if (!g_bossMusic) {
        SDL_Log("Audio: missing boss music '%s' (skipped): %s", BOSS_MUSIC_PATH, SDL_GetError());
    }

    SDL_Log("Audio: initialized");
    return true;
}

void Audio_PlaySfx(Sfx id) {
    if (!g_mixer || id < 0 || id >= SFX_COUNT || !g_sfx[id]) return;
    MIX_PlayAudio(g_mixer, g_sfx[id]); // fire-and-forget; overlaps are fine
}

// Start `audio` looping on the single music track, swapping out whatever was
// playing. Idempotent: if the requested track is already the one playing this is
// a no-op, so callers can call it every frame (e.g. while the boss is aggroed)
// without restarting the loop.
static void PlayMusicTrack(MIX_Audio *audio) {
    if (!g_mixer || !audio) return;
    if (g_curMusic == audio && g_musicTrack && MIX_TrackPlaying(g_musicTrack)) return;

    if (!g_musicTrack) {
        g_musicTrack = MIX_CreateTrack(g_mixer);
        if (!g_musicTrack) return;
        MIX_SetTrackGain(g_musicTrack, MUSIC_GAIN);
    }

    MIX_StopTrack(g_musicTrack, 0);
    MIX_SetTrackAudio(g_musicTrack, audio);
    g_curMusic = audio;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1); // loop forever
    MIX_PlayTrack(g_musicTrack, props);
    SDL_DestroyProperties(props);
}

void Audio_PlayMusic(void)     { PlayMusicTrack(g_music); }
void Audio_PlayBossMusic(void) { PlayMusicTrack(g_bossMusic); }

void Audio_StopMusic(void) {
    if (g_musicTrack) MIX_StopTrack(g_musicTrack, 0);
    g_curMusic = NULL; // force the next Play* to actually restart a track
}

void Audio_ToggleMute(void) {
    g_muted = !g_muted;
    // Master gain of 0 silences everything (music + SFX) without stopping it.
    if (g_mixer) MIX_SetMixerGain(g_mixer, g_muted ? 0.0f : 1.0f);
}

bool Audio_IsMuted(void) {
    return g_muted;
}

void Audio_Shutdown(void) {
    if (g_musicTrack) { MIX_DestroyTrack(g_musicTrack); g_musicTrack = NULL; }
    for (int i = 0; i < SFX_COUNT; i++) {
        if (g_sfx[i]) { MIX_DestroyAudio(g_sfx[i]); g_sfx[i] = NULL; }
    }
    if (g_music) { MIX_DestroyAudio(g_music); g_music = NULL; }
    if (g_bossMusic) { MIX_DestroyAudio(g_bossMusic); g_bossMusic = NULL; }
    if (g_mixer) { MIX_DestroyMixer(g_mixer); g_mixer = NULL; }
    MIX_Quit();
}

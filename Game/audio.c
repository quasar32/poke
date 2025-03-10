#include "audio.h"
#include <stdio.h>

#include "misc.h"
#include <SDL3_mixer/SDL_mixer.h>

static const char *g_MusicPaths[COUNTOF_MUS] = { 
    NULL,
    "Music/01_title.ogg",
    "Music/02_oakspeech.ogg",
    "Music/03_pallettown.ogg",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "Music/09_route1.ogg"
};

static const char *const g_SoundEffectPaths[COUNTOF_SFX] = {
    "SoundEffects/SFX_COLLISION.ogg",
    "SoundEffects/SFX_LEDGE.ogg", 
    "SoundEffects/SFX_START_MENU.ogg", 
    "SoundEffects/SFX_PRESS_AB.ogg", 
    "SoundEffects/SFX_GO_OUTSIDE.ogg", 
    "SoundEffects/SFX_GO_INSIDE.ogg", 
    "SoundEffects/SFX_SAVE.ogg", 
    "SoundEffects/SFX_TURN_ON_PC.ogg",
    "SoundEffects/SFX_TURN_OFF_PC.ogg",
    "SoundEffects/SFX_WITHDRAW_DEPOSIT.ogg",
    "SoundEffects/SFX_SHRINK.ogg",
    "SoundEffects/SFX_TINK.ogg"
};

static Mix_Music *g_MusList[COUNTOF_MUS];
static Mix_Chunk *g_SfxList[COUNTOF_SFX];
static Mix_Chunk *g_CryList[COUNTOF_CRY];

void InitAudio(void) {
    if (!Mix_OpenAudio(0, NULL)) {
        FatalSDL("Mix_OpenAudio");
    }
    Mix_Init(MIX_INIT_OGG);
    for (int I = 0; I < COUNTOF_MUS; I++) {
        if (g_MusicPaths[I]) {
            g_MusList[I] = Mix_LoadMUS(g_MusicPaths[I]);
            if (!g_MusList[I]) {
                fprintf(stderr, "Mix_LoadMUS: %s\n", SDL_GetError());
            }
        }
    }
    for (int I = 0; I < COUNTOF_SFX; I++) {
        if (g_SoundEffectPaths[I]) {
            g_SfxList[I] = Mix_LoadWAV(g_SoundEffectPaths[I]);
            if (!g_SfxList[I]) {
                fprintf(stderr, "Mix_LoadMUS: %s\n", SDL_GetError());
            }
        }
    }
    for (int I = 0; I < COUNTOF_CRY; I++) {
        char Path[32];
        sprintf(Path, "Cries/%03d.ogg", I);
        g_CryList[I] = Mix_LoadWAV(Path);
        if (I == CRY_NIDORINA && !g_CryList[I]) {
            fprintf(stderr, "Mix_LoadMUS: %s\n", SDL_GetError());
        }
    }
}

void PlayMusic(music_path_i I) {
    Mix_VolumeMusic(MIX_MAX_VOLUME);
    Mix_PlayMusic(g_MusList[I], -1);
}

void PlaySoundEffect(sound_effect_i SoundEffectI) {
    Mix_PlayChannel(-1, g_SfxList[SoundEffectI], 0);
}

void PlayCry(cry_i CryI) {
    Mix_PlayChannel(-1, g_CryList[CryI], 0);
}

void MusicSetVolume(float Volume) {
    Mix_VolumeMusic(MIX_MAX_VOLUME * Volume);
}

bool IsSoundPlaying(void) {
    return Mix_Playing(-1);
}

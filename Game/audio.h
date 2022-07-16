#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <windows.h>

#define COBJMACROS
#include <xaudio2.h>

#include "stb_vorbis.h"

typedef enum music_path_i {
    MUS_INVALID = -1,
    MUS_TITLE = 1,
    MUS_OAK_SPEECH = 2,
    MUS_PALLETE_TOWN = 3,
    MUS_ROUTE_1 = 9
} music_path_i;

typedef enum sound_effect_i {
    SFX_COLLISION,
    SFX_LEDGE,
    SFX_START_MENU,
    SFX_PRESS_AB,
    SFX_GO_OUTSIDE,
    SFX_GO_INSIDE,
    SFX_SAVE,
    SFX_TURN_ON_PC,
    SFX_TURN_OFF_PC,
    SFX_WITHDRAW_DEPOSIT,
    SFX_SHRINK 
} sound_effect_i;

typedef enum cry_i {
    CRY_NIDORINA = 30,
} cry_i; 

__attribute__((constructor))
BOOL CreateXAudio2(void);

__attribute__((destructor))
void DestroyXAudio2(void);

BOOL PlayMusic(music_path_i I);
BOOL PlaySoundEffect(sound_effect_i SoundEffectI);
BOOL PlayCry(cry_i CryI);

BOOL MusicSetVolume(float Volume);

BOOL IsSoundPlaying(void);

#endif

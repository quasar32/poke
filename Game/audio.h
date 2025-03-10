#ifndef AUDIO_H
#define AUDIO_H

//#include <windows.h>

typedef enum music_path_i {
    MUS_INVALID = -1,
    MUS_TITLE = 1,
    MUS_OAK_SPEECH = 2,
    MUS_PALLET_TOWN = 3,
    MUS_ROUTE_1 = 9,
    COUNTOF_MUS
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
    SFX_SHRINK,
    SFX_TINK,
    COUNTOF_SFX
} sound_effect_i;

typedef enum cry_i {
    CRY_NIDORINA = 30,
    COUNTOF_CRY
} cry_i; 

void InitAudio(void);
void PlayMusic(music_path_i I);
void PlaySoundEffect(sound_effect_i SoundEffectI);
void PlayCry(cry_i CryI);
void MusicSetVolume(float Volume);
bool IsSoundPlaying(void);

#endif

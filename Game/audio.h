#ifndef AUDIO_H
#define AUDIO_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#define COBJMACROS
#include <xaudio2.h>

#include "stb_vorbis.h"

typedef HRESULT WINAPI xaudio2_create(IXAudio2 **, UINT32, XAUDIO2_PROCESSOR);
typedef void WINAPI co_uninitialize(void);
typedef HRESULT WINAPI co_initialize_ex(LPVOID, DWORD);

typedef struct com {
    HMODULE Library;
    co_initialize_ex *CoInitializeEx; 
    co_uninitialize *CoUninitialize;
} com;

typedef struct xaudio2 {
    HMODULE Library;
    xaudio2_create *Create;
    IXAudio2 *Engine;
    IXAudio2MasteringVoice *MasterVoice;

    IXAudio2VoiceCallback StreamCallback;
    IXAudio2SourceVoice *SourceVoice;

    HANDLE StreamStart;
    HANDLE StreamEnd;
    HANDLE StreamThread; 
    HANDLE BufferEndEvent;

    stb_vorbis *Vorbis;
    _Atomic bool IsPlaying;
} xaudio2;

typedef struct sound {
    IXAudio2SourceVoice *Voice;
    XAUDIO2_BUFFER Collision;
    XAUDIO2_BUFFER Ledge; 
    XAUDIO2_BUFFER StartMenu; 
    XAUDIO2_BUFFER PressAB;
    XAUDIO2_BUFFER GoOutside;
    XAUDIO2_BUFFER GoInside; 
    XAUDIO2_BUFFER Save;
    XAUDIO2_BUFFER TurnOnPC;
    XAUDIO2_BUFFER TurnOffPC;
    XAUDIO2_BUFFER WithdrawDeposit; 
} sound;

BOOL InitCom(com *Com);
void DestroyCom(com *Com);
BOOL InitXAudio2(xaudio2 *XAudio2);
void DestroyXAudio2(xaudio2 *XAudio2);
BOOL ReadSound(LPCSTR Path, XAUDIO2_BUFFER *XBuf);
BOOL PlayWave(IXAudio2SourceVoice *Voice, const XAUDIO2_BUFFER *Buffer);
BOOL PlayMusic(xaudio2 *XAudio2, int I);
HRESULT SetVolume(IXAudio2SourceVoice *Voice, float Volume);
int GetQueueCount(IXAudio2SourceVoice *Voice);
HRESULT CreateGenericVoice(IXAudio2 *Engine, IXAudio2SourceVoice **Voice);
bool PlayOgg(xaudio2 *XAudio2, int MusicI);

extern uint32_t MusicTick;
extern sound Sound;
extern int MusicI[2];

#endif

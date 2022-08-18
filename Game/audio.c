#include "audio.h"
#include "scalar.h"
#include "stb_vorbis.h"
#include "win32.h"

#define COBJMACROS
#include <xaudio2.h>

#define STREAM_VOICE_COUNT 2

#define STREAM_BUFFER_COUNT 8
#define STREAM_BUFFER_SAMPLE_COUNT 3200

typedef enum source_i {
    SVI_MUSIC,
    SVI_SOUND,
} source_i;

typedef struct source {
    IXAudio2VoiceCallback Callback;
    IXAudio2SourceVoice *Voice;

    stb_vorbis *Vorbis;
    stb_vorbis *volatile Queued;

    volatile BOOL IsPushing;
    int BufI;
    short Bufs[STREAM_BUFFER_COUNT][STREAM_BUFFER_SAMPLE_COUNT]; 
} source;

static const WAVEFORMATEX WaveFormat = {
    .wFormatTag = WAVE_FORMAT_PCM, 
    .nChannels = 1,
    .nSamplesPerSec = 48000,
    .nAvgBytesPerSec = 96000,
    .nBlockAlign = 2,
    .wBitsPerSample = 16
};

static const char *const g_MusicPaths[10] = { 
    "",
    "Music/01_title.ogg",
    "Music/02_oakspeech.ogg",
    "Music/03_pallettown.ogg",
    "",
    "",
    "",
    "",
    "",
    "Music/09_route1.ogg"
};

static const char *const g_SoundEffectPaths[] = {
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

typedef void WINAPI co_uninitialize(void);
typedef HRESULT WINAPI co_initialize_ex(LPVOID, DWORD);

typedef HRESULT WINAPI xaudio2_create(IXAudio2 **, UINT32, XAUDIO2_PROCESSOR);

/*Com Globals*/
static BOOL g_ComSuccess;
static HMODULE g_ComLibrary;
static co_initialize_ex *g_CoInitializeEx; 
static co_uninitialize *g_CoUninitialize;

/*XAudio2 Main Thread Globals*/
static BOOL g_XAudio2Success;
static HMODULE g_XAudio2Library;
static xaudio2_create *g_XAudio2Create;
static IXAudio2 *g_Engine;
static IXAudio2MasteringVoice *g_MasterVoice;

/*XAudio2 Stream Thread Globals*/
static HANDLE g_BufferEndEvent;
static HANDLE g_StreamStart;
static HANDLE g_StreamThread; 
static source g_Sources[STREAM_VOICE_COUNT];

/*Sound Globals*/
static volatile BOOL g_InSound;

/*StreamCallback*/
static void STDMETHODCALLTYPE StreamOnBufferEnd(
    IXAudio2VoiceCallback *This, 
    [[maybe_unused]] void *Context
) {
    source *Source = CONTAINER_OF(This, source, Callback);
    SetEvent(g_BufferEndEvent);
    if(Source == &g_Sources[SVI_MUSIC] && !Source->IsPushing) {
        IXAudio2SourceVoice_Stop(Source->Voice, 0, XAUDIO2_COMMIT_NOW);
        IXAudio2SourceVoice_FlushSourceBuffers(Source->Voice);
    }
}

void STDMETHODCALLTYPE StreamStub1(IXAudio2VoiceCallback *) {}
void STDMETHODCALLTYPE StreamStub2(IXAudio2VoiceCallback *, void *) {}
void STDMETHODCALLTYPE StreamStub3(IXAudio2VoiceCallback *, void *, HRESULT) {}
void STDMETHODCALLTYPE StreamStub4(IXAudio2VoiceCallback *, UINT32) {}

static IXAudio2VoiceCallbackVtbl StreamCallbackVTBL = {
    .OnBufferEnd = StreamOnBufferEnd,
    .OnBufferStart = StreamStub2,
    .OnLoopEnd = StreamStub2,
    .OnStreamEnd = StreamStub1,
    .OnVoiceError = StreamStub3,
    .OnVoiceProcessingPassEnd = StreamStub1,
    .OnVoiceProcessingPassStart = StreamStub4
};

/*XAudio2 Queue Functions*/
static int GetQueueCount(IXAudio2SourceVoice *Voice) {
    XAUDIO2_VOICE_STATE VoiceState;
    IXAudio2SourceVoice_GetState(Voice, &VoiceState, 0);
    return VoiceState.BuffersQueued; 
}

static BOOL IsQueueFull(IXAudio2SourceVoice *Voice) {
    int QueueCount = GetQueueCount(Voice); 
    return QueueCount >= STREAM_BUFFER_COUNT - 1;
}

/*Sample Wait Functions*/
static BOOL AnySamplesToWait(void) {
    for(int I = 0; I < STREAM_VOICE_COUNT; I++) {
        source *Source = &g_Sources[I];
        if(Source->IsPushing && IsQueueFull(Source->Voice)) {
            return TRUE;
        }
    }
    return FALSE;
}

static void WaitForAllSamples(void) {
    while(AnySamplesToWait()) {
        WaitForSingleObject(g_BufferEndEvent, INFINITE);
    }
}

static BOOL AnyBufferInUse(void) {
    for(int I = 0; I < STREAM_VOICE_COUNT; I++) {
        IXAudio2SourceVoice *Voice = g_Sources[I].Voice;
        int QueueCount = GetQueueCount(Voice); 
        if(QueueCount > 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static void WaitUntilAllEmpty(void) {
    while(AnyBufferInUse()) {
        WaitForSingleObject(g_BufferEndEvent, INFINITE);
    }
}

/*Vorbis Queue Functions*/
static stb_vorbis *PopVorbis(source *Source) {
    return (stb_vorbis *) InterlockedAnd64((volatile LONG64 *) &Source->Queued, 0); 
}

static void StopOldVorbis(source *Source) {
    Source->IsPushing = FALSE;
    stb_vorbis *Old = PopVorbis(Source);
    if(Old) {
        stb_vorbis_close(Old);
    }
}

static BOOL StartNewVorbis(source *Source, const char *Path) {
    BOOL Success = FALSE;
    stb_vorbis *New = stb_vorbis_open_filename(Path, NULL, NULL);
    if(New) {
        Source->Queued = New;
        Success = TRUE;
    }
    SetEvent(g_StreamStart);
    return Success;
}

static BOOL PushVorbisSoundEffect(source *Source, const char *Path) {
    BOOL Success = !Source->Queued && StartNewVorbis(Source, Path);
    g_InSound = Success;
    return Success;
}

static void UpdateSoundIsPlaying(void) {
    if(
        g_InSound &&
        !g_Sources[SVI_SOUND].IsPushing && 
        !g_Sources[SVI_SOUND].Vorbis &&
        !g_Sources[SVI_SOUND].Queued &&
        GetQueueCount(g_Sources[SVI_SOUND].Voice) <= 0
    ) {
        g_InSound = FALSE;
    }
}

/*Sample Submission Functions*/
static void MoveToNextBuffer(source *Source) {
    Source->BufI++;
    if(Source->BufI >= STREAM_BUFFER_COUNT) {
        Source->BufI = 0;
    }
}

static void SubmitSamples(source *Source, int SampleCount, const short *Buf) {
    XAUDIO2_BUFFER XBuf = {
        .Flags = XAUDIO2_END_OF_STREAM, 
        .pAudioData = (BYTE *) Buf,
        .AudioBytes = SampleCount * 2, 
    }; 
    IXAudio2SourceVoice_SubmitSourceBuffer(Source->Voice, &XBuf, NULL);
    IXAudio2SourceVoice_Start(Source->Voice, 0, 0);
}

static BOOL AttemptToGetNewVorbis(source *Source, float Volume) {
    if(!Source->IsPushing) {
        if(Source->Vorbis) {
            stb_vorbis_close(Source->Vorbis);
            Source->Vorbis = NULL;
        }

        Source->Vorbis = PopVorbis(Source); 
        if(Source->Vorbis) { 
            Source->IsPushing = TRUE;
            IXAudio2Voice_SetVolume(Source->Voice, Volume, 0);
        }
    }
    return Source->IsPushing;
}

static BOOL SubmitMusicSamples(source *Source) {
    BOOL Success = AttemptToGetNewVorbis(Source, 1.0F);
    if(Success && !IsQueueFull(Source->Voice)) {
        BOOL HasZero = FALSE;
        int RemainingSamples = STREAM_BUFFER_SAMPLE_COUNT;
        short *Buf = Source->Bufs[Source->BufI];
        short *CurBuf = Buf;
        while(RemainingSamples > 0) {
            int SampleCount = stb_vorbis_get_samples_short(
                Source->Vorbis, 
                1, 
                &CurBuf, 
                RemainingSamples
            );
            if(SampleCount == 0) {
                if(HasZero) {
                    memset(CurBuf, 0, RemainingSamples * sizeof(short));
                    RemainingSamples = 0;
                } else {
                    stb_vorbis_seek_start(Source->Vorbis);
                    HasZero = TRUE;
                }
            }
            CurBuf += SampleCount;
            RemainingSamples -= SampleCount;
        }
        SubmitSamples(Source, STREAM_BUFFER_SAMPLE_COUNT, Buf);
        MoveToNextBuffer(Source);
    }
    return Success;
}

static BOOL SubmitSoundBufferSamples(source *Source) {
    BOOL Success = AttemptToGetNewVorbis(Source, 0.5F);
    if(Success && !IsQueueFull(Source->Voice) && Source->Vorbis) { 
        short *CurBuf = Source->Bufs[Source->BufI];
        int SampleCount = stb_vorbis_get_samples_short(
            Source->Vorbis, 
            1, 
            &CurBuf,
            STREAM_BUFFER_SAMPLE_COUNT
        );
        if(SampleCount == 0) {
            stb_vorbis_close(Source->Vorbis);
            Source->Vorbis = NULL;
            Source->IsPushing = FALSE;
        } else {
            SubmitSamples(Source, SampleCount, CurBuf);
            MoveToNextBuffer(Source);
        }
    }
    return Success;
}

/*Stream Function*/
static DWORD StreamProc(LPVOID Ptr) {
    while(WaitForSingleObject(g_StreamStart, INFINITE) == WAIT_OBJECT_0) {
        BOOL IsActive;
        do {
            IsActive = FALSE;
            IsActive |= SubmitMusicSamples(&g_Sources[SVI_MUSIC]);
            IsActive |= SubmitSoundBufferSamples(&g_Sources[SVI_SOUND]);
            WaitForAllSamples();
            UpdateSoundIsPlaying();
        } while(IsActive);
        WaitUntilAllEmpty();
        UpdateSoundIsPlaying();
        g_InSound = FALSE;
    }
    return EXIT_SUCCESS;
}

/*Com Functions*/
static void CreateCom(void) {
    g_ComSuccess = FALSE;

    FARPROC Procs[2];
    g_ComLibrary = LoadProcs(
        "ole32.dll",
        2,
        (const char *[]) {
            "CoInitializeEx", 
            "CoUninitialize" 
        },
        Procs
    );
    if(g_ComLibrary) {
        g_CoInitializeEx = (co_initialize_ex *) Procs[0]; 
        if(SUCCEEDED(g_CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
            g_ComSuccess = TRUE;
        } else {
            FreeLibrary(g_ComLibrary);
        }
        g_CoUninitialize = (co_uninitialize *) Procs[1];
    }
    g_ComSuccess = TRUE;
}

static void DestroyCom(void) {
    if(g_ComSuccess) {
        g_CoUninitialize();
        FreeLibrary(g_ComLibrary);
        g_ComSuccess = FALSE;
    }
}

/*Engine Functions*/
static BOOL InitStreamVoice(source *Source) {
    Source->Callback.lpVtbl = &StreamCallbackVTBL;
    HRESULT SourceResult = IXAudio2_CreateSourceVoice(
        g_Engine, 
        &Source->Voice,
        &WaveFormat,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        &Source->Callback,
        NULL,
        NULL
    );
    BOOL Success = SUCCEEDED(SourceResult);
    if(Success) {
        Source->Vorbis = NULL;
        Source->Queued = NULL;

        Source->IsPushing = FALSE; 
    }
    return Success;
}

static void DestroyStreamVoice(source *Source) {
    stb_vorbis_close(Source->Vorbis);
    stb_vorbis_close(Source->Queued);
    IXAudio2SourceVoice_DestroyVoice(Source->Voice);
}

BOOL CreateXAudio2(void) {
    CreateCom();
    if(!g_ComSuccess) {
        goto library_fail;
    }

    FARPROC Proc;

    g_XAudio2Library = LoadProcsVersioned(
        3,
        (const char *[]) {
            "XAudio2_9.dll",
            "XAudio2_8.dll",
            "XAudio2_7.dll" 
        },
        1,
        (const char *[]) {
            "XAudio2Create"
        },
        &Proc
    );
    if(!g_XAudio2Library) {
        goto library_fail;
    } 

    g_XAudio2Create = (xaudio2_create *) Proc;
    HRESULT CreateResult = g_XAudio2Create(&g_Engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if(FAILED(CreateResult)) {
        goto create_fail;
    }

    HRESULT MasterResult = IXAudio2_CreateMasteringVoice(
        g_Engine, 
        &g_MasterVoice,
        XAUDIO2_DEFAULT_CHANNELS,
        XAUDIO2_DEFAULT_SAMPLERATE,
        0,
        NULL,
        NULL,
        AudioCategory_GameEffects
    ); 
    if(FAILED(MasterResult)) {
        goto master_fail;
    }

    g_BufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(!g_BufferEndEvent) {
        goto buffer_fail;
    }

    g_StreamStart = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(!g_StreamStart) {
        goto stream_start_fail;
    }

    int SourceI;
    for(SourceI = 0; SourceI < STREAM_VOICE_COUNT; SourceI++) {
        if(!InitStreamVoice(&g_Sources[SourceI])) {
            goto source_init_fail;
        }
    }

    g_StreamThread = CreateThread(NULL, 0, StreamProc, NULL, 0, 0);
    if(!g_StreamThread) {
        goto stream_thread_fail;
    }

    g_XAudio2Success = TRUE;
    return TRUE;

stream_thread_fail:
    CloseHandle(g_StreamThread);
source_init_fail:
    for(int I = 0; I < SourceI; I++) {
        DestroyStreamVoice(&g_Sources[I]);
    }
stream_start_fail:
    CloseHandle(g_StreamStart);
buffer_fail:
    CloseHandle(g_BufferEndEvent); 
master_fail:
    IXAudio2_Release(g_Engine);
create_fail:
    FreeLibrary(g_XAudio2Library);
library_fail:
    g_XAudio2Success = FALSE;
    return FALSE;
}

void DestroyXAudio2(void) {
    if(g_XAudio2Success) {
        CloseHandle(g_StreamThread);
        for(int I = 0; I < STREAM_VOICE_COUNT; I++) {
            DestroyStreamVoice(&g_Sources[I]);
        }
        CloseHandle(g_BufferEndEvent); 
        CloseHandle(g_StreamStart);
        IXAudio2_Release(g_Engine);
        FreeLibrary(g_XAudio2Library);
        DestroyCom();
        g_XAudio2Success = FALSE;
    }
}

/*Play Functions*/
BOOL PlaySoundEffect(sound_effect_i SoundEffectI) {
    BOOL Success = FALSE;
    if(g_XAudio2Success) {
        source *Source = &g_Sources[SVI_SOUND];
        const char *Path = g_SoundEffectPaths[SoundEffectI];
        Success = PushVorbisSoundEffect(Source, Path);
    }
    return Success;
}

BOOL PlayMusic(music_path_i MusicI) {
    BOOL Success = FALSE;
    if(g_XAudio2Success) {
        source *Source = &g_Sources[SVI_MUSIC];
        StopOldVorbis(Source);
        const char *Path = SAFE_INDEX(g_MusicPaths, MusicI, NULL);
        if(Path) {
            Success = StartNewVorbis(Source, Path);
        }
    }
    return Success;
}

BOOL PlayCry(cry_i CryI) {
    BOOL Success = FALSE;
    if(g_XAudio2Success) {
        source *Source = &g_Sources[SVI_SOUND];
        char Path[32];
        snprintf(Path, _countof(Path), "Cries/%03d.ogg", CryI);
        Success = PushVorbisSoundEffect(Source, Path); 
    }
    return Success;
}

/*Misc Functions*/
BOOL MusicSetVolume(float Volume) {
    return (
        g_XAudio2Success && 
        SUCCEEDED(IXAudio2Voice_SetVolume(g_Sources[SVI_MUSIC].Voice, Volume, 0))
    );
}

BOOL IsSoundPlaying(void) {
    return g_InSound;
}

#include "audio.h"
#include "procs.h"
#include "read.h"
#include "stb_vorbis.h"

#define LE_RIFF 0x46464952 
#define LE_WAVE 0x45564157 
#define LE_FMT 0x20746D66 
#define LE_DATA 0x61746164

#define STREAM_BUFFER_COUNT 8
#define STREAM_BUFFER_SAMPLE_COUNT 3200

uint32_t MusicTick;
int MusicI[2];
sound Sound;

static const WAVEFORMATEX WaveFormat = {
    .wFormatTag = WAVE_FORMAT_PCM, 
    .nChannels = 1,
    .nSamplesPerSec = 48000,
    .nAvgBytesPerSec = 96000,
    .nBlockAlign = 2,
    .wBitsPerSample = 16
};

static void STDMETHODCALLTYPE StreamOnBufferEnd(
    [[maybe_unused]] IXAudio2VoiceCallback *This, 
    void *Context
) { 
    xaudio2 *XAudio2 = Context;
    SetEvent(XAudio2->BufferEndEvent);
    if(!atomic_load(&XAudio2->IsPlaying)) {
        IXAudio2SourceVoice_Stop(XAudio2->SourceVoice, 0, XAUDIO2_COMMIT_NOW);
        IXAudio2SourceVoice_FlushSourceBuffers(XAudio2->SourceVoice);
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

static const char *const MusicPaths[10] = { 
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

static void WaitForSamples(xaudio2 *XAudio2, HANDLE BufferEndEvent) {
    XAUDIO2_VOICE_STATE VoiceState;
    while(
        IXAudio2SourceVoice_GetState(XAudio2->SourceVoice, &VoiceState, 0), 
        VoiceState.BuffersQueued >= STREAM_BUFFER_COUNT - 1
    ) {
        WaitForSingleObject(BufferEndEvent, INFINITE);
    }
}

static void WaitUntilEmpty(xaudio2 *XAudio2, HANDLE BufferEndEvent) {
    XAUDIO2_VOICE_STATE VoiceState;
    while(
        IXAudio2SourceVoice_GetState(XAudio2->SourceVoice, &VoiceState, 0), 
        VoiceState.BuffersQueued > 0
    ) {
        WaitForSingleObject(BufferEndEvent, INFINITE);
    }
}

static DWORD StreamProc(LPVOID Ptr) {
    xaudio2 *XAudio2 = Ptr; 
    while(WaitForSingleObject(XAudio2->StreamStart, INFINITE) == WAIT_OBJECT_0) {
        short Bufs[STREAM_BUFFER_COUNT][3200];
        int BufI = 0;

        while(atomic_load(&XAudio2->IsPlaying)) {
            bool HasZero = false;
            int TotalSampleCount = 0;
            do {
                short *CurBuf = &Bufs[BufI][TotalSampleCount];
                int SampleCount = stb_vorbis_get_samples_short(
                    XAudio2->Vorbis, 
                    1, 
                    &CurBuf,
                    _countof(Bufs[BufI])
                );
                if(SampleCount == 0) {
                    if(HasZero) {
                        stb_vorbis_seek_start(XAudio2->Vorbis);
                        HasZero = true;
                    } else {
                        SampleCount = STREAM_BUFFER_COUNT - TotalSampleCount;
                        memset(CurBuf, 0, SampleCount * sizeof(short));
                    }
                }
                TotalSampleCount += SampleCount;
            } while(TotalSampleCount < STREAM_BUFFER_COUNT);

            XAUDIO2_BUFFER XBuf = {
                .Flags = XAUDIO2_END_OF_STREAM, 
                .pAudioData = (BYTE *) &Bufs[BufI],
                .AudioBytes = TotalSampleCount * 2, 
                .pContext = XAudio2
            }; 

            if(
                FAILED(IXAudio2SourceVoice_SubmitSourceBuffer(XAudio2->SourceVoice, &XBuf, NULL)) |
                FAILED(IXAudio2SourceVoice_Start(XAudio2->SourceVoice, 0, 0))
            ) {
                break;
            }

            BufI++;
            if(BufI >= (int) STREAM_BUFFER_COUNT) BufI = 0;

            WaitForSamples(XAudio2, XAudio2->BufferEndEvent);
        }
        WaitUntilEmpty(XAudio2, XAudio2->BufferEndEvent);
        stb_vorbis_close(XAudio2->Vorbis);
        XAudio2->Vorbis = NULL;
        SetEvent(XAudio2->StreamEnd);
    }
    return EXIT_SUCCESS;
}

BOOL InitCom(com *Com) {
    BOOL Success = FALSE;
    FARPROC Procs[2];
    Com->Library = LoadProcs(
        "ole32.dll",
        2,
        (const char *[]) {
            "CoInitializeEx", 
            "CoUninitialize" 
        },
        Procs
    );
    if(!Com->Library) goto out; 

    Com->CoInitializeEx = (co_initialize_ex *) Procs[0]; 
    if(FAILED(Com->CoInitializeEx(NULL, COINIT_MULTITHREADED))) goto out;
    Com->CoUninitialize = (co_uninitialize *) Procs[1];

    Success = TRUE;
out:
    if(!Success) DestroyCom(Com);
    return Success;
}

void DestroyCom(com *Com) {
    if(Com->CoUninitialize) Com->CoUninitialize();
    if(Com->Library) FreeLibrary(Com->Library);
    *Com = (com) {0};
}

BOOL InitXAudio2(xaudio2 *XAudio2) {
    FARPROC Proc;

    BOOL Success = FALSE;
    XAudio2->Library = LoadProcsVersioned(
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
    if(!XAudio2->Library) goto out; 

    XAudio2->Create = (xaudio2_create *) Proc;
    HRESULT CreateResult = XAudio2->Create(&XAudio2->Engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if(FAILED(CreateResult)) goto out;

    HRESULT MasterResult = IXAudio2_CreateMasteringVoice(
        XAudio2->Engine, 
        &XAudio2->MasterVoice,
        XAUDIO2_DEFAULT_CHANNELS,
        XAUDIO2_DEFAULT_SAMPLERATE,
        0,
        NULL,
        NULL,
        AudioCategory_GameEffects
    ); 
    if(FAILED(MasterResult)) goto out;

    XAudio2->StreamCallback.lpVtbl = &StreamCallbackVTBL;
    HRESULT SourceResult = IXAudio2_CreateSourceVoice(
        XAudio2->Engine, 
        &XAudio2->SourceVoice,
        &WaveFormat,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        &XAudio2->StreamCallback,
        NULL,
        NULL
    );
    if(FAILED(SourceResult)) goto out;

    XAudio2->StreamStart = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(!XAudio2->StreamStart) goto out;

    XAudio2->StreamEnd = CreateEvent(NULL, FALSE, FALSE, NULL);
    if(!XAudio2->StreamEnd) goto out;

    XAudio2->StreamThread = CreateThread(NULL, 0, StreamProc, XAudio2, 0, 0);
    if(!XAudio2->StreamThread) goto out;

    Success = TRUE;
out:
    if(!Success) DestroyXAudio2(XAudio2);
    return Success;
}

void DestroyXAudio2(xaudio2 *XAudio2) {
    if(XAudio2->StreamThread) CloseHandle(XAudio2->StreamThread); 
    if(XAudio2->StreamEnd) CloseHandle(XAudio2->StreamEnd);
    if(XAudio2->StreamStart) CloseHandle(XAudio2->StreamStart);
    if(XAudio2->Engine) IXAudio2_Release(XAudio2->Engine);
    if(XAudio2->Library) FreeLibrary(XAudio2->Library);
    *XAudio2 = (xaudio2) {};
}

BOOL ReadSound(LPCSTR Path, XAUDIO2_BUFFER *XBuf) {
    int ChannelCount;
    int SampleRate;
    short *Samples;

    *XBuf = (XAUDIO2_BUFFER) {
        .Flags = XAUDIO2_END_OF_STREAM,
    };

    int SampleCount = stb_vorbis_decode_filename(Path, &ChannelCount, &SampleRate, &Samples);
    if(SampleCount < 0) {
        __builtin_printf("Failed %s\n", Path);
        return FALSE;
    }
    XBuf->pAudioData = (BYTE *) Samples;
    XBuf->AudioBytes = SampleCount * 2;
    return TRUE; 
}

BOOL PlayWave(IXAudio2SourceVoice *Voice, const XAUDIO2_BUFFER *Buffer) { 
    return (
        Voice &&
        SUCCEEDED(IXAudio2SourceVoice_SubmitSourceBuffer(Voice, Buffer, NULL)) &&
        SUCCEEDED(IXAudio2SourceVoice_Start(Voice, 0, XAUDIO2_COMMIT_NOW)) 
    );
}

static void StopMusic(xaudio2 *XAudio2) {
    if(atomic_load(&XAudio2->IsPlaying)) { 
        atomic_store(&XAudio2->IsPlaying, false);
        WaitForSingleObject(XAudio2->StreamEnd, INFINITE);
    }
}

BOOL PlayMusic(xaudio2 *XAudio2, int MusicI) {
    StopMusic(XAudio2);
    const char *Path = MusicPaths[MusicI];
    XAudio2->Vorbis = stb_vorbis_open_filename(Path, NULL, NULL);
    if(!XAudio2->Vorbis) return false;
    atomic_store(&XAudio2->IsPlaying, true);
    SetEvent(XAudio2->StreamStart);
    return true;
}

HRESULT SetVolume(IXAudio2SourceVoice *Voice, float Volume) {
    HRESULT Result = S_OK;
    if(Voice) {
        Result = IXAudio2SourceVoice_SetVolume(Voice, Volume, XAUDIO2_COMMIT_NOW);
    }
    return Result;
}

int GetQueueCount(IXAudio2SourceVoice *Voice) {
    int Result = 0;
    if(Voice) {
        XAUDIO2_VOICE_STATE VoiceState;
        IXAudio2SourceVoice_GetState(Voice, &VoiceState, 0);
        Result = VoiceState.BuffersQueued; 
    }
    return Result;
}

HRESULT CreateGenericVoice(IXAudio2 *Engine, IXAudio2SourceVoice **Voice) {
     return IXAudio2_CreateSourceVoice(
        Engine,
        Voice, 
        &WaveFormat,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        NULL,
        NULL,
        NULL
    );
}


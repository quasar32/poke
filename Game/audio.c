#include "audio.h"
#include "procs.h"
#include "read.h"

#define LE_RIFF 0x46464952 
#define LE_WAVE 0x45564157 
#define LE_FMT 0x20746D66 
#define LE_DATA 0x61746164

IXAudio2SourceVoice *MusicVoice;
XAUDIO2_BUFFER Music[2];
uint32_t MusicData[2][1024 * 1024 * 16];
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

static const char *const MusicPaths[10] = { 
    "",
    "01_title.wav",
    "",
    "03_pallettown.wav",
    "",
    "",
    "",
    "",
    "",
    "09_route1.wav"
};

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

    Success = TRUE;
out:
    if(!Success) DestroyXAudio2(XAudio2);
    return Success;
}

void DestroyXAudio2(xaudio2 *XAudio2) {
    if(XAudio2->Engine) IXAudio2_Release(XAudio2->Engine);
    if(XAudio2->Library) FreeLibrary(XAudio2->Library);
    *XAudio2 = (xaudio2) {};
}

BOOL ReadWave(LPCSTR Path, XAUDIO2_BUFFER *XBuf, LPVOID Buf, SIZE_T BufSize) {
    HANDLE FileHandle = CreateFile(
        Path, 
        GENERIC_READ, 
        FILE_SHARE_READ, 
        NULL, 
        OPEN_EXISTING, 
        0, 
        NULL
    );
    if(FileHandle == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    *XBuf = (XAUDIO2_BUFFER) {
        .Flags = XAUDIO2_END_OF_STREAM,
    };

    struct {
        DWORD idChunk;
        DWORD cbChunk;
        DWORD idFormat;
        DWORD idSubChunk;
        DWORD cbSubChunk;
        WORD wFormatTag;
        WORD nChannels;
        DWORD nSamplesPerSec;
        DWORD nAvgBytesPerSec;
        WORD nBlockAlign;
        WORD wBitsPerSample;
        DWORD idDataChunk;
        DWORD cbData;
    } Header;

    int Success = (
        ReadObject(FileHandle, &Header, sizeof(Header)) &&

        Header.idChunk == LE_RIFF &&
        Header.idFormat == LE_WAVE &&
        Header.idSubChunk == LE_FMT &&
        Header.wFormatTag == WaveFormat.wFormatTag &&
        Header.nChannels == WaveFormat.nChannels && 
        Header.nSamplesPerSec == WaveFormat.nSamplesPerSec &&
        Header.nBlockAlign == WaveFormat.nBlockAlign &&
        Header.nAvgBytesPerSec == WaveFormat.nAvgBytesPerSec &&
        Header.wBitsPerSample == WaveFormat.wBitsPerSample &&
        Header.idDataChunk == LE_DATA &&

        Header.cbData < BufSize &&
        ReadObject(FileHandle, Buf, Header.cbData)
    );
    if(Success) {
        XBuf->pAudioData = Buf; 
        XBuf->AudioBytes = Header.cbData; 
    }

    CloseHandle(FileHandle);
    return Success;
}

BOOL ReadMusic(size_t PathI, int I) {
    const char *Path = (PathI < _countof(MusicPaths) ? MusicPaths[PathI] : "");
    MusicI[I] = PathI;
    Music[I].LoopCount = XAUDIO2_LOOP_INFINITE;
    return ReadWave(Path, &Music[I], MusicData[I], sizeof(MusicData[I]));
} 

BOOL ReadSound(LPCSTR Path, XAUDIO2_BUFFER *XBuf) {
    static uint32_t WaveData[1024 * 1024];
    static size_t WaveIndex; 

    BOOL Success = ReadWave(Path, XBuf, &WaveData[WaveIndex], sizeof(WaveData) - WaveIndex * sizeof(*WaveData));
    WaveIndex += (XBuf->AudioBytes + 3) >> 2;
    return Success; 
}


BOOL PlayWave(IXAudio2SourceVoice *Voice, const XAUDIO2_BUFFER *Buffer) { 
    return (
        Voice &&
        SUCCEEDED(IXAudio2SourceVoice_SubmitSourceBuffer(Voice, Buffer, NULL)) &&
        SUCCEEDED(IXAudio2SourceVoice_Start(Voice, 0, XAUDIO2_COMMIT_NOW)) 
    );
}

BOOL PlayMusic(int I) {
    return (
        MusicVoice &&
        SUCCEEDED(IXAudio2SourceVoice_Stop(MusicVoice, 0, XAUDIO2_COMMIT_NOW)) &&
        SUCCEEDED(IXAudio2SourceVoice_FlushSourceBuffers(MusicVoice)) &&
        PlayWave(MusicVoice, &Music[I])
    );
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


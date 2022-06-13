#define UNICODE
#include <stdio.h>
#include <windows.h>

typedef struct buffer {
    void *Data;
    size_t Size;
} buffer;

typedef struct wave_header {
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
} wave_header;

static const WAVEFORMATEX g_SoundEffectFormat = {
    .wFormatTag = WAVE_FORMAT_PCM, 
    .nChannels = 2,
    .nSamplesPerSec = 48000,
    .nAvgBytesPerSec = 192000,
    .nBlockAlign = 4,
    .wBitsPerSample = 16
};

static HRESULT ReadObject(HANDLE FileHandle, void *Object, DWORD ObjectSize) {
    HRESULT Result = S_OK;
    DWORD BytesRead;
    if(ReadFile(FileHandle, Object, ObjectSize, &BytesRead, NULL)) {
        if(BytesRead != ObjectSize) {
            Result = E_FAIL;
        }
    } else {
        Result = HRESULT_FROM_WIN32(GetLastError()); 
    } 
    return Result;
}

static DWORD Win32FromHResult(HRESULT HResult) {
    DWORD Error = ERROR_CAN_NOT_COMPLETE; 
    if(HResult == S_OK) {
        Error = ERROR_SUCCESS;
    } else if((HResult & 0xFFFF0000) == MAKE_HRESULT(SEVERITY_ERROR, FACILITY_WIN32, 0)) {
        Error = HRESULT_CODE(HResult);
    }
    return Error;
}

static HRESULT GetWin32ErrStr(DWORD ErrorCode, char *Buffer, DWORD Size) {
    DWORD Length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL,
                                 ErrorCode,
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 Buffer,
                                 Size,
                                 NULL);
    return Length == 0 ? HRESULT_FROM_WIN32(GetLastError()) : S_OK;
}

static HRESULT GetHResultErrStr(HRESULT HResult, char *Buffer, DWORD Size) {
    return GetWin32ErrStr(Win32FromHResult(HResult), Buffer, Size); 
}

static HRESULT CreateBuffer(buffer *Buffer, size_t Size) {
    Buffer->Size = Size; 
    Buffer->Data = VirtualAlloc(NULL, Buffer->Size, MEM_COMMIT | MEM_RESERVE, 
                                PAGE_READWRITE);
    return Buffer->Data ? S_OK : HRESULT_FROM_WIN32(GetLastError()); 
}

static HRESULT ReadSoundEffect(const wchar_t *Path, buffer *Buffer) {
    HRESULT Result = S_OK;
    HANDLE FileHandle = CreateFile(
        Path, 
        GENERIC_READ, 
        FILE_SHARE_READ, 
        NULL, 
        OPEN_EXISTING, 
        0, 
        NULL
    );
    if(FileHandle != INVALID_HANDLE_VALUE) {
        wave_header Header;
        Result = ReadObject(FileHandle, &Header, sizeof(Header));
        if(SUCCEEDED(Result)) {
            if(Header.idChunk == 0x46464952 &&
               Header.idFormat == 0x45564157 &&
               Header.idSubChunk == 0x20746D66 &&
               Header.wFormatTag == g_SoundEffectFormat.wFormatTag &&
               Header.nChannels == g_SoundEffectFormat.nChannels && 
               Header.nSamplesPerSec == g_SoundEffectFormat.nSamplesPerSec &&
               Header.nBlockAlign == g_SoundEffectFormat.nBlockAlign &&
               Header.nAvgBytesPerSec == g_SoundEffectFormat.nAvgBytesPerSec &&
               Header.wBitsPerSample == g_SoundEffectFormat.wBitsPerSample &&
               Header.idDataChunk == 0x61746164) {
                if(SUCCEEDED(CreateBuffer(Buffer, Header.cbData))) {
                    Result = ReadObject(FileHandle, Buffer->Data, Buffer->Size);
                    if(FAILED(Result)) {
                        VirtualFree(Buffer->Data, 0, MEM_RELEASE);
                    }
                }
            } else {
                Result = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            } 
        } 
    } else {
        Result = HRESULT_FROM_WIN32(GetLastError());
    }
    return Result;
}

static int FindMatches(buffer *Buffer, int MatchI, int MatchSize, int *MatchValue) {
    int Size = Buffer->Size / MatchSize;
    int Number = 0;
    for(int I = 0; I < Size; I++) {
        char *Data = Buffer->Data;
        if(I != MatchI) {
            if(memcmp(&Data[I * MatchSize], &Data[MatchI], MatchSize) == 0) {
                *MatchValue = I * MatchSize;
                Number++;
            } 
        }
    }

    return Number;
} 

static int FindLargestMatch(buffer *Buffer, int MatchI) {
    int I;
    int MatchValue;
    for(I = 1; I <= 4096; I++) {
        int Number = FindMatches(Buffer, MatchI, I, &MatchValue);
        if(Number == 0) {
            break;
        }
    }

    DWORD BytesWritten;
    char Buf[256];
    HANDLE Handle = GetStdHandle(STD_OUTPUT_HANDLE);
    int Length = snprintf(Buf, sizeof(Buf), "Largest set %d %d %f\n", I--, MatchValue, MatchValue / 480000.0f / 4.0f);
    WriteFile(Handle, Buf, Length, &BytesWritten, NULL);
} 

static HRESULT GetSoundEffectDeltaNoise(buffer *DstBuffer, const buffer *SrcBuffer) {
    HRESULT Result = S_OK;
    if(SrcBuffer->Size % 4 == 0) {
        Result = CreateBuffer(DstBuffer, SrcBuffer->Size);
        if(SUCCEEDED(Result)) {
            WORD (*SrcPairs)[2] = SrcBuffer->Data;
            WORD (*DstPairs)[2] = DstBuffer->Data;
            DstPairs[0][0] = SrcPairs[0][1];
            DstPairs[0][1] = SrcPairs[0][1];
            int SamplePairCount = SrcBuffer->Size / sizeof(*SrcPairs);
            for(int I = 1; I < SamplePairCount; I++) {
                DstPairs[I][0] = SrcPairs[I][0] - SrcPairs[I - 1][0];
                DstPairs[I][1] = SrcPairs[I][1] - SrcPairs[I - 1][1];
            }
        }
    } else {
        Result = HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
    }
    return Result;
}

static DWORD WriteAll(const char *Path, void *Bytes, DWORD BytesToWrite) {
    DWORD BytesWritten = 0;
    HANDLE FileHandle = CreateFileA(Path, GENERIC_WRITE, FILE_SHARE_READ, NULL, 
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(FileHandle != INVALID_HANDLE_VALUE) {
        WriteFile(FileHandle, Bytes, BytesToWrite, &BytesWritten, NULL);
        CloseHandle(FileHandle);
    }
    return BytesWritten;
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE Prev, LPSTR CmdLine, int CmdShow) {
    HRESULT Result = S_OK;

    int ArgCount;
    wchar_t **Args = CommandLineToArgvW(GetCommandLineW(), &ArgCount);
    if(Args) {  
        if(ArgCount == 2) {
            buffer Buffer = {};
            Result = ReadSoundEffect(Args[1], &Buffer);
            if(SUCCEEDED(Result)) {
                for(int I = 2048; I < Buffer.Size; I++) {
                    FindLargestMatch(&Buffer, I);
                }
            }
        } else {
            Result = HRESULT_FROM_WIN32(ERROR_BAD_ARGUMENTS);
        }
    } else {
        Result = HRESULT_FROM_WIN32(GetLastError());
    }

    if(FAILED(Result)) {
        HANDLE Handle = GetStdHandle(STD_ERROR_HANDLE);
        if(Handle != INVALID_HANDLE_VALUE) {
            char Buffer[256];
            DWORD BytesWritten;
            GetHResultErrStr(Result, Buffer, sizeof(Buffer));
            int Len = strlen(Buffer);
            Buffer[Len] = '\n';
            WriteFile(Handle, Buffer, Len, &BytesWritten, NULL); 
        } else {
            Result = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return FAILED(Result);
}

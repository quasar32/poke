#include "audio.h"
#include "save.h"
#include "read.h"
#include "read_buffer.h"
#include "options.h"
#include "write_buffer.h"
#include "window_map.h"
#include "scalar.h"

bool IsSaveComplete;

int SaveSec;
int StartSaveSec;

save_rect ContinueSaveRect = {
    .WindowTask.Type = TT_SAVE, 
    .Rect = {4, 7, 20, 17}
};

save_rect StartSaveRect = {
    .WindowTask.Type = TT_SAVE, 
    .Rect = {4, 0, 20, 10} 
};

static BOOL WriteAll(LPCSTR Path, LPCVOID Data, DWORD Size) {
    BOOL Success = FALSE;
    HANDLE FileHandle = CreateFile(
        Path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    ); 
    if(FileHandle != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten;
        WriteFile(FileHandle, Data, Size, &BytesWritten, NULL);
        if(BytesWritten == Size) {
            Success = TRUE;
        }
        CloseHandle(FileHandle);
    }
    return Success;
}

BOOL WriteSaveHeader(void) {
    write_buffer WriteBuffer = {0};
    WriteBufferPushObject(&WriteBuffer, &SaveSec, sizeof(SaveSec));
    WriteBufferPushByte(&WriteBuffer, Options.E[0].I); 
    WriteBufferPushByte(&WriteBuffer, Options.E[1].I); 
    WriteBufferPushByte(&WriteBuffer, Options.E[2].I); 
    return WriteAll("SaveHeader", WriteBuffer.Data, WriteBuffer.Index);
}

BOOL WriteSave(const world *World) {
    write_buffer WriteBuffer = {0};
    WriteInventory(&WriteBuffer, &Bag);
    WriteInventory(&WriteBuffer, &RedPC);

    const map *CurMap = &World->Maps[World->MapI];
    const map *OthMap = &World->Maps[!World->MapI];

    WriteBufferPushString(&WriteBuffer, CurMap->Path, CurMap->PathLen);
    WriteBufferPushString(&WriteBuffer, OthMap->Path, OthMap->PathLen);

    WriteBufferPushByte(&WriteBuffer, World->Player.Tile);
    WriteBufferPushSaveObject(&WriteBuffer, &World->Player);
    WriteBufferPushSaveObjects(&WriteBuffer, CurMap);
    WriteBufferPushSaveObjects(&WriteBuffer, OthMap);
    WriteBufferPushByte(&WriteBuffer, MusicI[World->MapI]);
    WriteBufferPushByte(&WriteBuffer, MusicI[!World->MapI]);

    return WriteAll("Save", WriteBuffer.Data, WriteBuffer.Index);
}

void ReadSaveHeader(void) {
    read_buffer ReadBuffer = {
        .Size = ReadAll("SaveHeader", ReadBuffer.Data, sizeof(ReadBuffer.Data))
    };
    ReadBufferPopObject(&ReadBuffer, &SaveSec, sizeof(SaveSec)); 
    Options.E[0].I = ReadBufferPopByte(&ReadBuffer) % Options.E[0].Count;
    Options.E[1].I = ReadBufferPopByte(&ReadBuffer) % Options.E[1].Count;
    Options.E[2].I = ReadBufferPopByte(&ReadBuffer) % Options.E[2].Count;
    StartSaveSec = SaveSec;
}

void ReadSave(world *World) {
    read_buffer ReadBuffer = {
        .Size = ReadAll("Save", ReadBuffer.Data, sizeof(ReadBuffer.Data))
    };
    ReadBufferPopInventory(&ReadBuffer, &Bag);
    ReadBufferPopInventory(&ReadBuffer, &RedPC);

    map *CurMap = &World->Maps[World->MapI];
    map *OthMap = &World->Maps[!World->MapI];

    ReadBufferPopString(&ReadBuffer, CurMap->Path);
    ReadBufferPopString(&ReadBuffer, OthMap->Path);

    if(CurMap->Path[0]) {
        ReadMap(World, World->MapI, CurMap->Path);
        GetLoadedPoint(World, World->MapI, CurMap->Path); 
    } else {
        memset(CurMap, 0, sizeof(*CurMap));
    }

    if(OthMap->Path[0]) {
        ReadMap(World, !World->MapI, OthMap->Path);
        GetLoadedPoint(World, !World->MapI, OthMap->Path); 
    } else {
        memset(OthMap, 0, sizeof(*OthMap));
    }

    World->Player.Tile = ReadBufferPopByte(&ReadBuffer) & 0xF0;
    ReadBufferPopSaveObject(&ReadBuffer, &World->Player);
    ReadBufferPopSaveObjects(&ReadBuffer, CurMap);
    ReadBufferPopSaveObjects(&ReadBuffer, OthMap);
    MusicI[0] = ReadBufferPopByte(&ReadBuffer); 
    MusicI[1] = ReadBufferPopByte(&ReadBuffer); 
}

void PlaceSave(rect Rect) {
    int SaveMin = MinInt(SaveSec / 60, 99 * 59 + 60); 
    PlaceTextBox(Rect);
    PlaceTextF(
        (point) {Rect.Left + 1, Rect.Top + 2},
        "PLAYER %s\nBADGES       %d\nPOK" "\xE9" "DEX    %3d\nTIME     %2d:%02d",
        "RED",
        0,
        0,
        SaveMin / 60,
        SaveMin % 60
    );
}


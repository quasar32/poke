#include "options.h"
#include "save.h"
#include "scalar.h"
#include "text.h"
#include "buffer.h"
#include "misc.h"

#include <limits.h>

static int g_SaveSec;
static int g_StartSaveSec;
static Uint64 g_StartCounter;

save_rect g_ContinueSaveRect = {
    .WindowTask.Type = TT_SAVE, 
    .Rect = {4, 7, 20, 17}
};

char g_Name[8];
char g_Rival[8];

bool WriteSaveHeader(void) {
    write_buffer WriteBuffer = {0};
    WriteBufferPushObject(&WriteBuffer, &g_SaveSec, sizeof(g_SaveSec));
    WriteBufferPushCString(&WriteBuffer, g_Name);
    WriteBufferPushCString(&WriteBuffer, g_Rival);
    WriteBufferPushByte(&WriteBuffer, g_Options[0].I); 
    WriteBufferPushByte(&WriteBuffer, g_Options[1].I); 
    WriteBufferPushByte(&WriteBuffer, g_Options[2].I); 
    return WriteAll("SaveHeader", WriteBuffer.Data, WriteBuffer.Index);
}

bool WriteSave(void) {
    write_buffer WriteBuffer = {0};
    WriteInventory(&WriteBuffer, &g_Bag);
    WriteInventory(&WriteBuffer, &g_RedPC);

    const map *CurMap = &g_Maps[g_MapI];
    const map *OthMap = &g_Maps[!g_MapI];

    WriteBufferPushCString(&WriteBuffer, CurMap->Path);
    WriteBufferPushCString(&WriteBuffer, OthMap->Path);
    WriteBufferPushCString(&WriteBuffer, g_RestorePath);

    WriteBufferPushByte(&WriteBuffer, g_Player.Tile);
    WriteBufferPushSaveObject(&WriteBuffer, &g_Player);
    WriteBufferPushSaveObjects(&WriteBuffer, CurMap);
    WriteBufferPushSaveObjects(&WriteBuffer, OthMap);
    WriteBufferPushByte(&WriteBuffer, g_MusicI);

    return WriteAll("Save", WriteBuffer.Data, WriteBuffer.Index);
}

void ReadSaveHeader(void) {
    read_buffer ReadBuffer; 
    ReadBufferFromFile(&ReadBuffer, "SaveHeader");
    ReadBufferPopObject(&ReadBuffer, &g_SaveSec, sizeof(g_SaveSec)); 
    ReadBufferPopCString(&ReadBuffer, countof(g_Name), g_Name);
    ReadBufferPopCString(&ReadBuffer, countof(g_Rival), g_Rival);
    g_Options[0].I = ReadBufferPopByte(&ReadBuffer) % g_Options[0].Count;
    g_Options[1].I = ReadBufferPopByte(&ReadBuffer) % g_Options[1].Count;
    g_Options[2].I = ReadBufferPopByte(&ReadBuffer) % g_Options[2].Count;
    g_StartSaveSec = g_SaveSec;
}

void ReadSave(void) {
    read_buffer ReadBuffer;
    ReadBufferFromFile(&ReadBuffer, "Save");
    ReadBufferPopInventory(&ReadBuffer, &g_Bag);
    ReadBufferPopInventory(&ReadBuffer, &g_RedPC);

    map *CurMap = &g_Maps[g_MapI];
    map *OthMap = &g_Maps[!g_MapI];

    ReadBufferPopCString(&ReadBuffer, countof(CurMap->Path), CurMap->Path);
    ReadBufferPopCString(&ReadBuffer, countof(OthMap->Path), OthMap->Path);
    ReadBufferPopCString(&ReadBuffer, countof(g_RestorePath), g_RestorePath);

    if(CurMap->Path[0]) {
        ReadMap(g_MapI, CurMap->Path);
        GetLoadedPoint(g_MapI, CurMap->Path); 
    } else {
        memset(CurMap, 0, sizeof(*CurMap));
    }

    if(OthMap->Path[0]) {
        ReadMap(!g_MapI, OthMap->Path);
        GetLoadedPoint(!g_MapI, OthMap->Path); 
    } else {
        memset(OthMap, 0, sizeof(*OthMap));
    }

    g_Player.Tile = ReadBufferPopByte(&ReadBuffer) & 0xF0;
    ReadBufferPopSaveObject(&ReadBuffer, &g_Player);
    ReadBufferPopSaveObjects(&ReadBuffer, CurMap);
    ReadBufferPopSaveObjects(&ReadBuffer, OthMap);
    g_MusicI = ReadBufferPopByte(&ReadBuffer); 
}

void PlaceSave(rect Rect) {
    int SaveMin = MIN(g_SaveSec / 60, 99 * 59 + 60); 
    PlaceBox(Rect);
    PlaceTextF(
        Rect.Left + 1, 
        Rect.Top + 2,
        "PLAYER @RED\nBADGES       %d\nPOK�DEX    %3d\nTIME     %2d:%02d",
        0,
        0,
        SaveMin / 60,
        SaveMin % 60
    );
}

static Uint64 GetSecondsElapsed(Uint64 Counter) {
    return (Counter - SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();
}

void UpdateSaveSec(void) {
    int64_t Elapsed = GetSecondsElapsed(g_StartCounter); 
    g_SaveSec = (int) MIN(INT_MAX, g_StartSaveSec + Elapsed);
}

void ResetSaveSec(void) {
    g_StartSaveSec = 0;
}

void StartSaveCounter(void) {
    g_StartCounter = SDL_GetPerformanceCounter();
}

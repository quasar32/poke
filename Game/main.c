#define COBJMACROS

/*Includes*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <xaudio2.h>
#include <xinput.h>

/*Macro Constants*/
#define ANIM_PLAYER 0
#define ANIM_SEAL 128

#define BM_WIDTH 160
#define BM_HEIGHT 144

#define WORLD_WIDTH 1
#define WORLD_HEIGHT 4 

#define SPR_HORZ_FLAG 1
#define SPR_VERT_FLAG 2

#define TILE_LENGTH 8
#define TILE_SIZE 64

#define LE_RIFF 0x46464952 
#define LE_WAVE 0x45564157 
#define LE_FMT 0x20746D66 
#define LE_DATA 0x61746164

/*Typedefs*/
typedef uint8_t tile_map[32][32];
typedef uint8_t bm_pixels[BM_HEIGHT][BM_WIDTH];

typedef HRESULT WINAPI xaudio2_create(IXAudio2** pxaudio2, UINT32 flags, XAUDIO2_PROCESSOR processor);
typedef MMRESULT WINAPI winmm_func(UINT);
typedef HRESULT WINAPI co_uninitialize(void);
typedef HRESULT WINAPI co_initialize_ex(LPVOID pvReserved, DWORD dwCoInit);
typedef DWORD WINAPI xinput_get_state(DWORD, XINPUT_STATE *);
typedef DWORD WINAPI xinput_set_state(DWORD, XINPUT_VIBRATION *);

/*Enum*/
typedef enum quad_prop {
    QUAD_PROP_NONE,
    QUAD_PROP_SOLID, 
    QUAD_PROP_EDGE, 
    QUAD_PROP_MESSAGE, 
    QUAD_PROP_WATER, 
    QUAD_PROP_DOOR,
    QUAD_PROP_EXIT,
    QUAD_PROP_TV,
    QUAD_PROP_SHELF,
    QUAD_PROP_RED_PC
} quad_prop;
#define COUNTOF_QUAD_PROP 10 

typedef enum dir {
    DIR_UP,
    DIR_LEFT,
    DIR_DOWN,
    DIR_RIGHT
} dir;

typedef enum buttons {
    BT_UP,
    BT_LEFT,
    BT_DOWN,
    BT_RIGHT,
    BT_A,
    BT_B,
    BT_START,
    BT_FULLSCREEN,
} buttons;
#define COUNTOF_BT 8

typedef enum menu_tile {
    MT_FULL_HORZ_ARROW = 174,
    MT_BLANK = 176,
    MT_EMPTY_HORZ_ARROW = 177,
    MT_TIMES = 178,
    MT_QUESTION = 179
} menu_tile;

typedef enum game_state {
    /*Start*/
    GS_MAIN_MENU,
    GS_CONTINUE,

    /*Game*/
    GS_NORMAL,
    GS_LEAPING,
    GS_TEXT,
    GS_TRANSITION,

    /*Menu*/
    GS_START_MENU,
    GS_ITEM,
    GS_SAVE,
    GS_OPTIONS,

    GS_USE_TOSS,
    GS_TOSS,
    GS_CONFIRM_TOSS,

    /*RedPC*/
    GS_RED_PC
} game_state; 

typedef enum option_names {
    OPT_TEXT_SPEED,
    OPT_BATTLE_ANIMATION, 
    OPT_BATTLE_STYLE,
    OPT_CANCEL
} option_names;

typedef enum item_id {
    ITEM_POTION
} item_id;

typedef enum inventory_state {
    IS_ITEM,
    IS_DEPOSIT,
    IS_WITHDRAW,
    IS_TOSS
} inventory_state;

/*Coord Structures*/
typedef struct point {
    short X;
    short Y;
} point;

typedef struct dim_rect {
    int X;
    int Y;
    int Width;
    int Height;
} dim_rect;

typedef struct rect {
    short Left;
    short Top;
    short Right;
    short Bottom;
} rect;

/*Render Structures*/
typedef struct sprite {
    uint8_t X;
    uint8_t Y;
    uint8_t Tile;
    uint8_t Flags;
} sprite;

typedef struct bitmap {
    union {
        BITMAPINFO WinStruct;
        struct {
            BITMAPINFOHEADER Header;
            RGBQUAD Colors[4];    
        };
    } Info;
    uint8_t Pixels[BM_HEIGHT][BM_WIDTH];
} bitmap;

/*World Structures*/
typedef struct quad_info {
    point Point;
    int MapI;
    int Quad;
    int Prop;
} quad_info;

typedef struct door {
    point Pos;
    point DstPos;
    char Path[256];
} door;

typedef struct text {
    point Pos;
    char Str[256];
} text;

typedef struct object {
    point Pos;

    uint8_t StartingDir;
    uint8_t Dir;
    uint8_t Speed;
    uint8_t Tile;

    uint8_t IsMoving;
    uint8_t IsRight;
    uint8_t Tick;

    char Str[256];
} object;

typedef struct map {
    char Path[256];
    int PathLen;

    int16_t Width;
    int16_t Height;
    int16_t DefaultQuad;
    int16_t PalleteI;
    uint8_t Quads[256][256];
    point Loaded;

    int TextCount;
    int DoorCount;
    int ObjectCount;

    text Texts[256];
    door Doors[256];
    object Objects[256];
} map;

typedef struct data_path {
    const char *Tile;
    const char *Quad;
    const char *Prop;
} data_path; 

typedef struct world {
    uint8_t TileData[256][TILE_SIZE];
    uint8_t QuadProps[128];
    uint8_t QuadData[128][4];

    const char *OverworldMapPaths[WORLD_HEIGHT][WORLD_WIDTH];

    int DataPathI;
    data_path DataPaths[2];

    int MapI;
    map Maps[2];

    object Player;

    int IsOverworld;
} world;

/*IO Structs*/
typedef struct read_buffer {
    uint8_t Data[65536];
    int Index;
    int Size;
} read_buffer;

typedef struct write_buffer {
    uint8_t Data[4096];
    int Index;
} write_buffer;

/*Library Structs*/
typedef struct lib_struct {
    HMODULE Library;
    FARPROC Procs[];
} lib_struct;

typedef struct multi_lib {
    size_t VersionCount; 
    const char **Versions;

    size_t ProcCount; 
    const char **ProcNames;
    lib_struct *LibStruct;
} multi_lib;
    
typedef union com {
    struct {
        HMODULE Library;
        co_initialize_ex *CoInitializeEx; 
        co_uninitialize *CoUninitialize;
    };
    lib_struct LibStruct;
} com;

typedef union winmm {
    struct {
        HMODULE Library;
        winmm_func *TimeBeginPeriod;
        winmm_func *TimeEndPeriod;
        int IsGranular;
    };
    lib_struct LibStruct;
} winmm;

typedef union xaudio2 {
    struct {
        HMODULE Library;
        xaudio2_create *Create;
        IXAudio2 *Engine;
        IXAudio2MasteringVoice *MasterVoice;
    };
    lib_struct LibStruct;
} xaudio2;

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

/*Menu Structs*/
typedef struct menu {
    const char *Text;
    rect Rect;
    int TextY;
    int SelectI;
    int EndI;
} menu;

typedef struct option {
    int Y;
    int Xs[3];
    int I;
    int Count;
} option;

typedef struct item {
    uint8_t ID;
    uint8_t Count;
} item;

typedef struct inventory {
    item *Items;
    int ItemCapacity;

    int ItemCount;
    int ItemSelect;
    int Y;
} inventory;

typedef struct options_menu {
   option E[4]; 
   int I;
} options_menu;

typedef struct active_text {
    const char *Str;
    const char *Restore;
    uint64_t Tick;
    point TilePt; 
    int BoxDelay;
    int IsImmediate;
} active_text;

/*Global Constants*/
static const point g_DirPoints[] = {
    [DIR_UP] = {0, -1},
    [DIR_LEFT] = {-1, 0},
    [DIR_DOWN] = {0, 1},
    [DIR_RIGHT] = {1, 0}
};

static const point g_NextPoints[] = {
    [DIR_UP] = {0, 1},
    [DIR_LEFT] = {1, 0},
    [DIR_DOWN] = {0, 1},
    [DIR_RIGHT] = {1, 0}
};

static const WAVEFORMATEX g_WaveFormat = {
    .wFormatTag = WAVE_FORMAT_PCM, 
    .nChannels = 2,
    .nSamplesPerSec = 48000,
    .nAvgBytesPerSec = 192000,
    .nBlockAlign = 4,
    .wBitsPerSample = 16
};

static const char g_ItemNames[256][8] = {
    [ITEM_POTION] = "POTION"
};

/*Math Functions*/
static int MinInt(int A, int B) {
    return A < B ? A : B;
}

static int MaxInt(int A, int B) {
    return A > B ? A : B;
}

static int AbsInt(int Val) {
    return Val < 0 ? -Val : Val; 
}

static int64_t MinInt64(int64_t A, int64_t B) {
    return A < B ? A : B;
}

static int PosIntMod(int A, int B) {
    return A % B < 0 ? A % B + AbsInt(B) : A % B; 
}

static int ReverseDir(int Dir) {
    return (Dir + 2) % 4;
}

static int CorrectTileCoord(int TileCoord) {
    return PosIntMod(TileCoord, 32);
}

/*String Functions*/
static int DoesStartStringMatch(const char *Dst, const char *Src) {
    for(int I = 0; Src[I] != '\0'; I++) {
        if(Dst[I] != Src[I]) {
            return 0;
        }
    }
    return 1; 
}

static int AreStringsEqual(const char *A, const char *B) {
    return A && B ? strcmp(A, B) == 0 : A != B;
}

/*Timing*/
static int64_t GetPerfFreq(void) {
    LARGE_INTEGER PerfFreq;
    QueryPerformanceFrequency(&PerfFreq);
    return PerfFreq.QuadPart;
}

static int64_t GetPerfCounter(void) {
    LARGE_INTEGER PerfCounter;
    QueryPerformanceCounter(&PerfCounter);
    return PerfCounter.QuadPart;
}

static int64_t GetDeltaCounter(int64_t BeginCounter) {
    return GetPerfCounter() - BeginCounter;
}

static int64_t GetSecondsElapsed(int64_t BeginCounter, int64_t PerfFreq) {
    return GetDeltaCounter(BeginCounter) / PerfFreq;
}

static void SleepTillNextFrame(int64_t Counter, int64_t Delta, int64_t PerfFreq, int IsGranular) {
    int64_t InitDeltaCounter = GetDeltaCounter(Counter);
    if(InitDeltaCounter < Delta) {
        if(IsGranular) {
            int64_t RemainCounter = Delta - InitDeltaCounter;
            uint32_t SleepMS = 1000 * RemainCounter / PerfFreq;
            if(SleepMS > 0) {
                Sleep(SleepMS);
            }
        }
        while(GetDeltaCounter(Counter) < Delta);
    }
} 

/*Point Functions*/ 
static point AddPoints(point A, point B)  {
    return (point) {
        .X = A.X + B.X,
        .Y = A.Y + B.Y
    };
}

static point SubPoints(point A, point B)  {
    return (point) {
        .X = A.X - B.X,
        .Y = A.Y - B.Y
    };
}

static point MulPoint(point A, short B) {
    return (point) {
        .X = A.X * B,
        .Y = A.Y * B 
    };
}

static int EqualPoints(point A, point B) {
    return A.X == B.X && A.Y == B.Y;
}

static int PointInWorld(point Pt) {
    int InX = Pt.X >= 0 && Pt.X < WORLD_WIDTH;
    int InY = Pt.Y >= 0 && Pt.Y < WORLD_HEIGHT;
    return InX && InY;
}

/*DimRect Functions*/
static dim_rect WinToDimRect(RECT WinRect) {
    return (dim_rect) {
        .X = WinRect.left,
        .Y = WinRect.top,
        .Width = WinRect.right - WinRect.left,
        .Height = WinRect.bottom - WinRect.top
    };
}

static dim_rect GetDimClientRect(HWND Window) {
    RECT WinClientRect;
    GetClientRect(Window, &WinClientRect);
    return WinToDimRect(WinClientRect);
}

static dim_rect GetDimWindowRect(HWND Window) {
    RECT WinWindowRect;
    GetWindowRect(Window, &WinWindowRect);
    return WinToDimRect(WinWindowRect);
}

/*Point Conversion Functions*/
static point PtToQuadPt(point Point) {
    point QuadPoint = {
        .X = Point.X / 16,
        .Y = Point.Y / 16
    };
    return QuadPoint;
}

static point QuadPtToPt(point Point) {
    point QuadPoint = {
        .X = Point.X * 16,
        .Y = Point.Y * 16
    };
    return QuadPoint;
}

static point GetFacingPoint(point Pos, int Dir) {
    point OldPoint = PtToQuadPt(Pos);
    point DirPoint = g_DirPoints[Dir];
    return AddPoints(OldPoint, DirPoint);
}

/*Quad Functions*/
static void SetToTiles(tile_map TileMap, int TileX, int TileY, const uint8_t Set[4]) {
    TileMap[TileY][TileX] = Set[0];
    TileMap[TileY][TileX + 1] = Set[1];
    TileMap[TileY + 1][TileX] = Set[2];
    TileMap[TileY + 1][TileX + 1] = Set[3];
}

static int GetMapDir(const map Maps[2], int Map) {
    point DirPoint = SubPoints(Maps[!Map].Loaded, Maps[Map].Loaded);
    for(size_t I = 0; I < _countof(g_DirPoints); I++) {
        if(EqualPoints(DirPoint, g_DirPoints[I])) {
            return I;
        }
    }
    return -1; 
}

/*Render Functions*/
static void RenderTileMap(bm_pixels Pixels, tile_map TileMap, 
                          uint8_t TileData[256][TILE_SIZE], int ScrollX, int ScrollY) {
    uint8_t (*BmRow)[BM_WIDTH] = Pixels;

    for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
        int PixelX = 8 - (ScrollX & 7);
        int SrcYDsp = ((PixelY + ScrollY) & 7) << 3;
        int TileX = ScrollX >> 3;
        int TileY = (PixelY + ScrollY) / 8 % 32;
        int StartOff = SrcYDsp | (ScrollX & 7);
        memcpy(*BmRow, &TileData[TileMap[TileY][TileX]][StartOff], 8);

        for(int Repeat = 1; Repeat < 20; Repeat++) {
            TileX = (TileX + 1) % 32;
            uint8_t *Pixel = *BmRow + PixelX;
            memcpy(Pixel, &TileData[TileMap[TileY][TileX]][SrcYDsp], 8);

            PixelX += 8;
        }
        TileX = (TileX + 1) % 32;
        uint8_t *Pixel = *BmRow + PixelX;
        int RemainX = BM_WIDTH - PixelX;
        memcpy(Pixel, &TileData[TileMap[TileY][TileX]][SrcYDsp], RemainX);
        BmRow++;
    }
}

static void RenderWindowMap(bm_pixels Pixels, tile_map TileMap, uint8_t TileData[256][TILE_SIZE]) {
    for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
        int PixelX = 0;
        int SrcYDsp = (PixelY & 7) << 3;
        int TileY = PixelY / 8;

        for(int TileX = 0; TileX < 20; TileX++) {
            if(TileMap[TileY][TileX]) {
                memcpy(&Pixels[PixelY][PixelX], &TileData[TileMap[TileY][TileX]][SrcYDsp], 8); 
            }
            PixelX += 8;
        }
    }
}

static void RenderSprites(bm_pixels Pixels, sprite *Sprites, int Count, uint8_t Data[256][TILE_SIZE]) {
    for(int I = 0; I < Count; I++) {
        int RowsToRender = 8;
        if(Sprites[I].Y < 8) {
            RowsToRender = Sprites[I].Y;
        } else if(Sprites[I].Y > BM_HEIGHT) {
            RowsToRender = MaxInt(BM_HEIGHT + 8 - Sprites[I].Y, 0);
        }

        int ColsToRender = 8;
        if(Sprites[I].X < 8) {
            ColsToRender = Sprites[I].X;
        } else if(Sprites[I].X > BM_WIDTH) {
            ColsToRender = MaxInt(BM_WIDTH + 8 - Sprites[I].X, 0);
        }

        int DstX = MaxInt(Sprites[I].X - 8, 0);
        int DstY = MaxInt(Sprites[I].Y - 8, 0);

        int SrcX = MaxInt(8 - Sprites[I].X, 0);
        int DispX = 1;
        if(Sprites[I].Flags & SPR_HORZ_FLAG) {
            SrcX = MinInt(Sprites[I].X, 7);
            DispX = -1;
        }
        int DispY = 8;
        int SrcY = MaxInt(8 - Sprites[I].Y, 0);
        if(Sprites[I].Flags & SPR_VERT_FLAG) {
            SrcY = MinInt(Sprites[I].Y, 7);
            DispY = -8;
        }

        uint8_t *Src = &Data[Sprites[I].Tile][SrcY * TILE_LENGTH + SrcX];

        for(int Y = 0; Y < RowsToRender; Y++) {
            uint8_t *Tile = Src;
            for(int X = 0; X < ColsToRender; X++) {
                if(*Tile != 2) {
                    Pixels[Y + DstY][X + DstX] = *Tile;
                }
                Tile += DispX;
            }
            Src += DispY;
        }
    }
}

/*WriteBuffer Functions*/
static void WriteBufferPushByte(write_buffer *WriteBuffer, int Byte) {
    if(WriteBuffer->Index < _countof(WriteBuffer->Data)) {
        WriteBuffer->Data[WriteBuffer->Index] = Byte;
        WriteBuffer->Index++;
    } 
}

static void WriteBufferPushObject(write_buffer *WriteBuffer, const void *Object, size_t Size) {
    if(WriteBuffer->Index + Size <= _countof(WriteBuffer->Data)) {
        memcpy(&WriteBuffer->Data[WriteBuffer->Index], Object, Size);
        WriteBuffer->Index += Size;
    }
}

static void WriteBufferPushSaveObject(write_buffer *WriteBuffer, const object *Object) {
    WriteBufferPushByte(WriteBuffer, Object->Pos.X / 16);
    WriteBufferPushByte(WriteBuffer, Object->Pos.Y / 16);
    WriteBufferPushByte(WriteBuffer, Object->Speed == 0 ? Object->StartingDir : Object->Dir);
}

static void WriteBufferPushSaveObjects(write_buffer *WriteBuffer, const map *Map) {
    for(int I = 0; I < Map->ObjectCount; I++) {
        WriteBufferPushSaveObject(WriteBuffer, &Map->Objects[I]);
    }
}

static void WriteBufferPushString(write_buffer *WriteBuffer, const char *Str, int Length) {
    WriteBufferPushByte(WriteBuffer, Length);
    WriteBufferPushObject(WriteBuffer, Str, Length);
}

/*Write Functions*/
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

static void WriteInventory(write_buffer *WriteBuffer, const inventory *Inventory) {
    WriteBufferPushByte(WriteBuffer, Inventory->ItemCount);
    for(int I = 0; I < Inventory->ItemCount; I++) {
        WriteBufferPushByte(WriteBuffer, Inventory->Items[I].ID);
        WriteBufferPushByte(WriteBuffer, Inventory->Items[I].Count);
    }
}

static BOOL WriteSave(const world *World, const inventory *Bag, const inventory *RedPC, int SaveSec) {
    write_buffer WriteBuffer = {0};
    WriteBufferPushObject(&WriteBuffer, &SaveSec, sizeof(SaveSec));
    WriteInventory(&WriteBuffer, Bag);
    WriteInventory(&WriteBuffer, RedPC);

    const map *CurMap = &World->Maps[World->MapI];
    const map *OthMap = &World->Maps[!World->MapI];

    WriteBufferPushString(&WriteBuffer, CurMap->Path, CurMap->PathLen);
    WriteBufferPushString(&WriteBuffer, OthMap->Path, OthMap->PathLen);

    WriteBufferPushByte(&WriteBuffer, World->Player.Tile);
    WriteBufferPushSaveObject(&WriteBuffer, &World->Player);
    WriteBufferPushSaveObjects(&WriteBuffer, CurMap);
    WriteBufferPushSaveObjects(&WriteBuffer, OthMap);

    return WriteAll("Save", WriteBuffer.Data, WriteBuffer.Index);
}

/*ReadBuffer Functions*/
static uint8_t ReadBufferPopByte(read_buffer *ReadBuffer) {
    uint8_t Result = 0;
    if(ReadBuffer->Index < ReadBuffer->Size) {
        Result = ReadBuffer->Data[ReadBuffer->Index]; 
        ReadBuffer->Index++;
    } 
    return Result;
}

static void ReadBufferPopObject(read_buffer *ReadBuffer, void *Object, size_t Size) {
    if(ReadBuffer->Index + Size <= ReadBuffer->Size) {
        memcpy(Object, &ReadBuffer->Data[ReadBuffer->Index], Size); 
        ReadBuffer->Index += Size;
    } else {
        memset(Object, 0, Size);
    } 
}

static void ReadBufferPopString(read_buffer *ReadBuffer, char Str[256]) {
    uint8_t Length = ReadBufferPopByte(ReadBuffer); 
    int Left = ReadBuffer->Size - ReadBuffer->Index; 
    if(Left < Length) {
        Length = Left;
    }
    memcpy(Str, &ReadBuffer->Data[ReadBuffer->Index], Length);
    Str[Length] = '\0';
    ReadBuffer->Index += Length;
}

static void ReadBufferPopSaveObject(read_buffer *ReadBuffer, object *Object) {
    Object->Pos.X = ReadBufferPopByte(ReadBuffer) * 16;
    Object->Pos.Y = ReadBufferPopByte(ReadBuffer) * 16;
    Object->Dir = ReadBufferPopByte(ReadBuffer) % 4;
}

static void ReadBufferPopSaveObjects(read_buffer *ReadBuffer, map *Map) {
    for(int I = 0; I < Map->ObjectCount; I++) {
        ReadBufferPopSaveObject(ReadBuffer, &Map->Objects[I]);
    }
}

/*Read Functions*/
static HRESULT ReadObject(HANDLE FileHandle, LPVOID Object, DWORD ObjectSize) {
    HRESULT Result = S_OK;
    DWORD BytesRead;
    if(ReadFile(FileHandle, Object, ObjectSize, &BytesRead, NULL)) {
        if(BytesRead < ObjectSize) {
            Result = E_FAIL;
        }
    } else {
        Result = HRESULT_FROM_WIN32(GetLastError()); 
    } 
    return Result;
}

static DWORD ReadAll(LPCTSTR Path, LPVOID Bytes, DWORD ToRead) {
    DWORD BytesRead = 0;
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
        ReadFile(FileHandle, Bytes, ToRead, &BytesRead, NULL);
        CloseHandle(FileHandle);
    }
                 
    return BytesRead;
}

static void ReadTileData(const char *Path, uint8_t TileData[][TILE_SIZE], int TileCount) {
    uint8_t CompData[256][16] = {0};
    ReadAll(Path, CompData, TileCount * 16);

    for(int TileI = 0; TileI < TileCount; TileI++) {
        for(int I = 0; I < 16; I++) {
            TileData[TileI][I * 4] = (CompData[TileI][I] >> 6) & 3;
            TileData[TileI][I * 4 + 1] = (CompData[TileI][I] >> 4) & 3;
            TileData[TileI][I * 4 + 2] = (CompData[TileI][I] >> 2) & 3;
            TileData[TileI][I * 4 + 3] = CompData[TileI][I] & 3;
        }
    }
}

static void ReadMap(world *World, int MapI, const char *Path) {
    read_buffer ReadBuffer = {
        .Size = ReadAll(Path, ReadBuffer.Data, sizeof(ReadBuffer.Data))
    };

    /*SaveMapPath*/
    map *Map = &World->Maps[MapI];
    Map->PathLen = strlen(Path);
    memcpy(Map->Path, Path, Map->PathLen + 1);

    /*ReadQuadSize*/
    Map->Width = ReadBufferPopByte(&ReadBuffer) + 1;
    Map->Height = ReadBufferPopByte(&ReadBuffer) + 1;
    int Size = Map->Width * Map->Height;

    /*ReadQuads*/
    int QuadIndex = 0;
    while(QuadIndex < Size) {
        int QuadRaw = ReadBufferPopByte(&ReadBuffer);
        int Quad = QuadRaw & 127;
        int Repeat = 0;

        if(Quad == QuadRaw) {
            Repeat = 1;
        } else { 
            Repeat = ReadBufferPopByte(&ReadBuffer) + 1;
        }

        Repeat = MinInt(Size - QuadIndex, Repeat);

        for(int I = QuadIndex; I < QuadIndex + Repeat; I++) {
            Map->Quads[I / Map->Width][I % Map->Width] = Quad; 
        }

        QuadIndex += Repeat;
    }

    /*DefaultQuad*/
    Map->DefaultQuad = ReadBufferPopByte(&ReadBuffer) & 127;

    /*ReadText*/
    Map->TextCount = ReadBufferPopByte(&ReadBuffer);
    for(int I = 0; I < Map->TextCount; I++) {
        Map->Texts[I].Pos.X = ReadBufferPopByte(&ReadBuffer);
        Map->Texts[I].Pos.Y = ReadBufferPopByte(&ReadBuffer);
        ReadBufferPopString(&ReadBuffer, Map->Texts[I].Str);
    }

    /*ReadObjects*/
    Map->ObjectCount = ReadBufferPopByte(&ReadBuffer);
    for(int I = 0; I < Map->ObjectCount; I++) {
        Map->Objects[I].Pos.X = ReadBufferPopByte(&ReadBuffer) * 16;
        Map->Objects[I].Pos.Y = ReadBufferPopByte(&ReadBuffer) * 16;
        Map->Objects[I].StartingDir = ReadBufferPopByte(&ReadBuffer) & 3;
        Map->Objects[I].Dir = Map->Objects[I].StartingDir;  
        Map->Objects[I].Speed = ReadBufferPopByte(&ReadBuffer) % 2;
        Map->Objects[I].Tile = ReadBufferPopByte(&ReadBuffer) & 0xF0;
        ReadBufferPopString(&ReadBuffer, Map->Objects[I].Str);
    }

    /*ReadDoors*/
    Map->DoorCount = ReadBufferPopByte(&ReadBuffer);
    for(int I = 0; I < Map->DoorCount; I++) {
        Map->Doors[I].Pos.X = ReadBufferPopByte(&ReadBuffer);
        Map->Doors[I].Pos.Y = ReadBufferPopByte(&ReadBuffer);
        Map->Doors[I].DstPos.X = ReadBufferPopByte(&ReadBuffer);
        Map->Doors[I].DstPos.Y = ReadBufferPopByte(&ReadBuffer);
        ReadBufferPopString(&ReadBuffer, Map->Doors[I].Path);
    }

    /*ReadTileSet*/
    int NewDataPathI = ReadBufferPopByte(&ReadBuffer) % _countof(World->DataPaths); 
    if(World->DataPathI != NewDataPathI) {
        World->DataPathI = NewDataPathI;

        ReadTileData(World->DataPaths[NewDataPathI].Tile, World->TileData, 96);
        ReadAll(World->DataPaths[NewDataPathI].Quad, World->QuadData, sizeof(World->QuadData)); 
        ReadAll(World->DataPaths[NewDataPathI].Prop, World->QuadProps, sizeof(World->QuadProps));
    }

    /*ReadPalleteI*/
    Map->PalleteI = ReadBufferPopByte(&ReadBuffer);
}

static void ReadOverworldMap(world *World, int MapI, point Load) { 
    const char *MapPath = World->OverworldMapPaths[Load.Y][Load.X]; 
    if(PointInWorld(Load) && MapPath) {
        const char *CurMapPath = MapPath;
        ReadMap(World, MapI, CurMapPath);
        World->IsOverworld = 1;
        World->Maps[MapI].Loaded = Load; 
    }
}

static int GetLoadedPoint(world *World, int MapI, const char *MapPath) {
    for(int Y = 0; Y < WORLD_HEIGHT; Y++) {
        for(int X = 0; X < WORLD_WIDTH; X++) {
            if(AreStringsEqual(World->OverworldMapPaths[Y][X], MapPath)) {
                World->Maps[MapI].Loaded = (point) {X, Y};
                return 1;
            } 
        }
    }
    return 0;
}

static void ReadInventory(read_buffer *ReadBuffer, inventory *Inventory) {
    Inventory->ItemCount = MinInt(ReadBufferPopByte(ReadBuffer), Inventory->ItemCapacity);
    for(int I = 0; I < Inventory->ItemCount; I++) {
        Inventory->Items[I].ID = ReadBufferPopByte(ReadBuffer);
        Inventory->Items[I].Count = ReadBufferPopByte(ReadBuffer);
    }
}

static void ReadSave(world *World, inventory *Bag, inventory *RedPC, int *SaveSec) {
    read_buffer ReadBuffer = {
        .Size = ReadAll("Save", ReadBuffer.Data, sizeof(ReadBuffer.Data))
    };
    ReadBufferPopObject(&ReadBuffer, SaveSec, sizeof(*SaveSec)); 
    ReadInventory(&ReadBuffer, Bag);
    ReadInventory(&ReadBuffer, RedPC);

    map *CurMap = &World->Maps[World->MapI];
    map *OthMap = &World->Maps[!World->MapI];

    ReadBufferPopString(&ReadBuffer, CurMap->Path);
    ReadBufferPopString(&ReadBuffer, OthMap->Path);

    if(CurMap->Path[0]) {
        ReadMap(World, World->MapI, CurMap->Path);
        World->IsOverworld = GetLoadedPoint(World, World->MapI, CurMap->Path); 
    } else {
        memset(CurMap, 0, sizeof(*CurMap));
    }

    if(OthMap->Path[0]) {
        ReadMap(World, !World->MapI, OthMap->Path);
        World->IsOverworld = GetLoadedPoint(World, !World->MapI, OthMap->Path); 
    } else {
        memset(OthMap, 0, sizeof(*OthMap));
    }

    World->Player.Tile = ReadBufferPopByte(&ReadBuffer) & 0xF0;
    ReadBufferPopSaveObject(&ReadBuffer, &World->Player);
    ReadBufferPopSaveObjects(&ReadBuffer, CurMap);
    ReadBufferPopSaveObjects(&ReadBuffer, OthMap);
}

/*Map Functions*/
static int PointInMap(const map *Map, point Pt) {
    int InX = Pt.X >= 0 && Pt.X < Map->Width; 
    int InY = Pt.Y >= 0 && Pt.Y < Map->Height; 
    return InX && InY;
}

static const char *GetTextFromPos(const map *Map, point Point) {
    for(int I = 0; I < Map->TextCount; I++) {
        if(EqualPoints(Map->Texts[I].Pos, Point)) {
            return Map->Texts[I].Str;
        } 
    } 
    return "ERROR: NO TEXT";
}

static const door *GetDoorFromPos(const map *Map, point Point) {
    for(int I = 0; I < Map->DoorCount; I++) {
        if(EqualPoints(Map->Doors[I].Pos, Point)) {
            return &Map->Doors[I];
        } 
    }
    return NULL;
}

/*Object Functions*/
static void MoveEntity(object *Object) {
    if(Object->IsMoving && Object->Tick % 2 == 0) {
        point DeltaPoint = g_DirPoints[Object->Dir];
        DeltaPoint = MulPoint(DeltaPoint, Object->Speed);
        Object->Pos = AddPoints(Object->Pos, DeltaPoint);
    }
    Object->Tick--;
}

static int WillObjectCollide(const object *OtherObject, point NewPoint) {
    int WillCollide = 0;
    point OtherPoint = PtToQuadPt(OtherObject->Pos);

    if(EqualPoints(NewPoint, OtherPoint)) {
        WillCollide = 1;
    } else if(OtherObject->IsMoving) {
        point NextPoint = g_NextPoints[OtherObject->Dir];
        point OtherNextPoint = AddPoints(OtherPoint, NextPoint);
        if(EqualPoints(OtherNextPoint, NewPoint)) {
            WillCollide = 1;
        }
    }
    return WillCollide;
}

static void RandomMove(const world *World, int MapI, object *Object) {
    const map *Map = &World->Maps[MapI];
    int Seed = rand();
    if(Seed % 64 == 0) {
        Object->Dir = Seed / 64 % 4;

        point NewPoint = GetFacingPoint(Object->Pos, Object->Dir);

        /*FetchQuadPropSingleMap*/
        int Quad = -1;
        int Prop = QUAD_PROP_SOLID;
        if(PointInMap(Map, NewPoint)) {
            Quad = Map->Quads[NewPoint.Y][NewPoint.X];
            Prop = World->QuadProps[Quad];
        }
        
        /*CheckForCollisions*/
        int WillCollide = WillObjectCollide(&World->Player, NewPoint);
        for(int I = 0; I < World->Maps[MapI].ObjectCount && !WillCollide; I++) {
            const object *OtherObject = &World->Maps[MapI].Objects[I]; 
            if(Object != OtherObject) {
                WillCollide = WillObjectCollide(OtherObject, NewPoint); 
            }
        } 

        /*StartMovingEntity*/
        if(Prop == QUAD_PROP_NONE && !WillCollide) {
            Object->Tick = 32;
            Object->IsMoving = 1;
        }
    }
}

static void UpdateAnimation(const object *Object, sprite *SpriteQuad, point QuadPt) {
    uint8_t QuadX = QuadPt.X;
    uint8_t QuadY = QuadPt.Y;
    if(Object->Tick % 16 < 8 || Object->Speed == 0) {
        /*SetStillAnimation*/
        switch(Object->Dir) {
        case DIR_UP:
            SpriteQuad[0] = (sprite) {0, 0, 0, 0};
            SpriteQuad[1] = (sprite) {8, 0, 0, SPR_HORZ_FLAG};
            SpriteQuad[2] = (sprite) {0, 8, 1, 0};
            SpriteQuad[3] = (sprite) {8, 8, 1, SPR_HORZ_FLAG};
            break;
        case DIR_LEFT:
            SpriteQuad[0] = (sprite) {0, 0, 4, 0};
            SpriteQuad[1] = (sprite) {8, 0, 5, 0};
            SpriteQuad[2] = (sprite) {0, 8, 6, 0};
            SpriteQuad[3] = (sprite) {8, 8, 7, 0};
            break;
        case DIR_DOWN:
            SpriteQuad[0] = (sprite) {0, 0, 10, 0};
            SpriteQuad[1] = (sprite) {8, 0, 10, SPR_HORZ_FLAG};
            SpriteQuad[2] = (sprite) {0, 8, 11, 0};
            SpriteQuad[3] = (sprite) {8, 8, 11, SPR_HORZ_FLAG};
            break;
        case DIR_RIGHT:
            SpriteQuad[0] = (sprite) {0, 0, 5, SPR_HORZ_FLAG};
            SpriteQuad[1] = (sprite) {8, 0, 4, SPR_HORZ_FLAG};
            SpriteQuad[2] = (sprite) {0, 8, 7, SPR_HORZ_FLAG};
            SpriteQuad[3] = (sprite) {8, 8, 6, SPR_HORZ_FLAG};
            break;
        }
    } else {
        /*StepAnimation*/
        switch(Object->Dir) {
        case DIR_UP:
            SpriteQuad[0] = (sprite) {0, 1, 0, 0};
            SpriteQuad[1] = (sprite) {8, 1, 0, SPR_HORZ_FLAG};
            if(Object->IsRight) {
                SpriteQuad[2] = (sprite) {0, 8, 2, 0};
                SpriteQuad[3] = (sprite) {8, 8, 3, 0};
            } else {
                SpriteQuad[2] = (sprite) {0, 8, 3, SPR_HORZ_FLAG};
                SpriteQuad[3] = (sprite) {8, 8, 2, SPR_HORZ_FLAG};
            }
            break;
        case DIR_LEFT:
            SpriteQuad[0] = (sprite) {0, 0, 14, 0};
            SpriteQuad[1] = (sprite) {8, 0, 15, 0};
            SpriteQuad[2] = (sprite) {0, 8, 8, 0};
            SpriteQuad[3] = (sprite) {8, 8, 9, 0};
            break;
        case DIR_DOWN:
            SpriteQuad[0] = (sprite) {0, 1, 10, 0};
            SpriteQuad[1] = (sprite) {8, 1, 10, SPR_HORZ_FLAG};
            if(Object->IsRight) {
                SpriteQuad[2] = (sprite) {0, 8, 13, SPR_HORZ_FLAG};
                SpriteQuad[3] = (sprite) {8, 8, 12, SPR_HORZ_FLAG};
            } else {
                SpriteQuad[2] = (sprite) {0, 8, 12, 0};
                SpriteQuad[3] = (sprite) {8, 8, 13, 0};
            }
            break;
        case DIR_RIGHT:
            SpriteQuad[0] = (sprite) {0, 0, 15, SPR_HORZ_FLAG};
            SpriteQuad[1] = (sprite) {8, 0, 14, SPR_HORZ_FLAG};
            SpriteQuad[2] = (sprite) {0, 8, 9, SPR_HORZ_FLAG};
            SpriteQuad[3] = (sprite) {8, 8, 8, SPR_HORZ_FLAG};
            break;
        }
    }

    /*SetSpriteQuad*/
    for(int SpriteI = 0; SpriteI < 4; SpriteI++) {
        SpriteQuad[SpriteI].X += QuadX;
        SpriteQuad[SpriteI].Y += QuadY - 4;
        SpriteQuad[SpriteI].Tile += Object->Tile;
    }
}

static int ObjectInUpdateBounds(point QuadPt) {
    return QuadPt.X > -16 && QuadPt.X < 176 && QuadPt.Y > -16 && QuadPt.Y < 160;
}

/*World Functions*/
static quad_info GetQuadInfo(const world *World, point Point) {
    quad_info QuadInfo = {.MapI = World->MapI, .Point = Point};

    int OldWidth = World->Maps[QuadInfo.MapI].Width;
    int OldHeight = World->Maps[QuadInfo.MapI].Height;

    int NewWidth = World->Maps[!QuadInfo.MapI].Width;
    int NewHeight = World->Maps[!QuadInfo.MapI].Height;

    switch(GetMapDir(World->Maps, QuadInfo.MapI)) {
    case DIR_UP:
        if(QuadInfo.Point.Y < 0) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y += NewHeight; 
            QuadInfo.MapI ^= 1;
        }
        break;
    case DIR_LEFT:
        if(QuadInfo.Point.X < 0) {
            QuadInfo.MapI ^= 1;
            QuadInfo.Point.X += NewHeight; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
        }
        break;
    case DIR_DOWN:
        if(QuadInfo.Point.Y >= World->Maps[QuadInfo.MapI].Height) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y -= OldHeight; 
            QuadInfo.MapI ^= 1;
        }
        break;
    case DIR_RIGHT:
        if(QuadInfo.Point.X >= World->Maps[QuadInfo.MapI].Width) {
            QuadInfo.Point.X -= OldWidth; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
            QuadInfo.MapI ^= 1;
        }
        break;
    }

    if(PointInMap(&World->Maps[QuadInfo.MapI], QuadInfo.Point)) {
        QuadInfo.Quad = World->Maps[QuadInfo.MapI].Quads[QuadInfo.Point.Y][QuadInfo.Point.X];
        QuadInfo.Prop = World->QuadProps[QuadInfo.Quad];
    } else {
        QuadInfo.Quad = World->Maps[World->MapI].DefaultQuad;
        QuadInfo.Prop = QUAD_PROP_SOLID;
    }

    return QuadInfo;
}

static const char *GetOverworldMapPath(world *World, point Point) {
    return PointInWorld(Point) ? World->OverworldMapPaths[Point.X][Point.Y] : NULL; 
}

static int LoadAdjacentMap(world *World, int DeltaX, int DeltaY) {
    int Result = 0;
    point CurMapPt = World->Maps[World->MapI].Loaded;
    if(DeltaX || DeltaY) {
        point NewMapPt = {CurMapPt.X + DeltaX, CurMapPt.Y + DeltaY}; 
        point OldMapPt = World->Maps[!World->MapI].Loaded;
        if(GetOverworldMapPath(World, NewMapPt) && !EqualPoints(NewMapPt, OldMapPt)) {
            ReadOverworldMap(World, !World->MapI, NewMapPt);
            Result = 1;
        }
    }
    return Result;
}

static void LoadNextMap(world *World, int DeltaX, int DeltaY) {
    if(!LoadAdjacentMap(World, DeltaX, 0)) {
        LoadAdjacentMap(World, 0, DeltaY);
    }
}

static void PlaceMap(const world *World, tile_map TileMap, point TileMin, rect QuadRect) {
    int TileY = CorrectTileCoord(TileMin.Y);
    for(int QuadY = QuadRect.Top; QuadY < QuadRect.Bottom; QuadY++) {
        int TileX = CorrectTileCoord(TileMin.X);
        for(int QuadX = QuadRect.Left; QuadX < QuadRect.Right; QuadX++) {
            int Quad = GetQuadInfo(World, (point) {QuadX, QuadY}).Quad;
            SetToTiles(TileMap, TileX, TileY, World->QuadData[Quad]); 
            TileX = (TileX + 2) % 32;
        }
        TileY = (TileY + 2) % 32;
    }
}

static void PlaceViewMap(const world *World, tile_map TileMap, int IsDown) {
    rect QuadRect = {
        .Left = World->Player.Pos.X / 16 - 4,
        .Top = World->Player.Pos.Y / 16 - 4,
        .Right = QuadRect.Left + 10,
        .Bottom = QuadRect.Top + 9 + !!IsDown
    };
    point TileMin = {0};
    PlaceMap(World, TileMap, TileMin, QuadRect);
}

/*Tile Functions*/
static void PlaceTextBox(tile_map TileMap, rect Rect) {
    /*HeadRow*/
    TileMap[Rect.Top][Rect.Left] = 96;
    memset(&TileMap[Rect.Top][Rect.Left + 1], 97, Rect.Right - Rect.Left - 2);
    TileMap[Rect.Top][Rect.Right - 1] = 98;

    /*BodyRows*/
    for(int Y = Rect.Top + 1; Y < Rect.Bottom - 1; Y++) {
        TileMap[Y][Rect.Left] = 99;
        memset(&TileMap[Y][Rect.Left + 1], MT_BLANK, Rect.Right - Rect.Left - 2);
        TileMap[Y][Rect.Right - 1] = 101;
    }

    /*FootRow*/
    TileMap[Rect.Bottom - 1][Rect.Left] = 100;
    memset(&TileMap[Rect.Bottom - 1][Rect.Left + 1], 97, Rect.Right - Rect.Left - 2);
    TileMap[Rect.Bottom - 1][Rect.Right - 1] = 102;
}

static int CharToTile(int Char) {
    int Output = Char;
    if(Output == '*') {
        Output = MT_TIMES;
    } else if(Output == '.') {
        Output = 175;
    } else if(Output >= '0' && Output <= ':') {
        Output += 103 - '0';
    } else if(Output >= 'A' && Output <= 'Z') {
        Output += 114 - 'A';
    } else if(Output >= 'a' && Output <= 'z') {
        Output += 140 - 'a';
    } else if(Output == '!') {
        Output = 168;
    } else if(Output == 'é') {
        Output = 166;
    } else if(Output == '\'') {
        Output = 169;
    } else if(Output == '~') {
        Output = 172;
    } else if(Output == '-') {
        Output = 170;
    } else if(Output == ',') {
        Output = 173; 
    } else if(Output == '?') {
       Output = MT_QUESTION; 
    } else {
        Output = MT_BLANK;
    }
    return Output;
}

static void PlaceText(tile_map TileMap, point TileMin, const char *Text) {
    int X = TileMin.X;
    int Y = TileMin.Y;
    for(int I = 0; Text[I] != '\0'; I++) {
        switch(Text[I]) {
        case '\n':
            X = TileMin.X;
            Y += 2;
            break;
        case '\r':
            X = TileMin.X;
            Y += 3; 
            break;
        default:
            TileMap[Y][X] = CharToTile(Text[I]);
            X++;
        }
    }
}

static void PlaceTextF(tile_map TileMap, point TileMin, const char *Format, ...) {
    char Text[256];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Text, sizeof(Text), Format, Args);
    va_end(Args);
    PlaceText(TileMap, TileMin, Text);
}

static void PlaceStaticText(tile_map TileMap, const char *Text) {
    PlaceTextBox(TileMap, (rect) {0, 12, 20, 18});
    PlaceText(TileMap, (point) {1, 14}, Text);
}

/*Menu Functions*/
static void PlaceMenuCursor(tile_map TileMap, const menu *Menu, int MenuTile) {
    TileMap[Menu->SelectI * 2 + Menu->TextY][Menu->Rect.Left + 1] = MenuTile; 
}

static void PlaceMenu(tile_map TileMap, const menu *Menu) {
    if(Menu->EndI != 2) {
        memset(TileMap, 0, sizeof(tile_map));
    }
    PlaceTextBox(TileMap, Menu->Rect); 
    PlaceText(TileMap, (point) {Menu->Rect.Left + 2, Menu->TextY}, Menu->Text); 
    PlaceMenuCursor(TileMap, Menu, MT_FULL_HORZ_ARROW);
}

static void PlaceOptionCursor(tile_map TileMap, const option *Option, int Tile) {
    TileMap[Option->Y][Option->Xs[Option->I]] = Tile;
}

static void ChangeOptionX(tile_map TileMap, option *Option, int NewOptionI) {
    PlaceOptionCursor(TileMap, Option, MT_BLANK);
    Option->I = NewOptionI;
    PlaceOptionCursor(TileMap, Option, MT_FULL_HORZ_ARROW);
}

static void PlaceSave(tile_map TileMap, rect Rect, int SaveSec) {
    int SaveMin = MinInt(SaveSec / 60, 99 * 59 + 60); 
    PlaceTextBox(TileMap, Rect);
    PlaceTextF(
        TileMap, 
        (point) {Rect.Left + 1, Rect.Top + 2},
        "PLAYER %s\nBADGES       %d\nPOKéDEX    %3d\nTIME     %2d:%02d",
        "RED",
        0,
        0,
        SaveMin / 60,
        SaveMin % 60
    );
}

static game_state PlaceInventory(tile_map TileMap, const inventory *Inventory) {
    PlaceTextBox(TileMap, (rect) {4, 2, 20, 13});
    int MinItem = Inventory->ItemSelect - Inventory->Y;
    for(int I = 0; I < 4; I++) {
        int ItemI = MinItem + I;
        int Y = I * 2 + 4;

        /*PlaceItem*/
        if(ItemI < Inventory->ItemCount) {
            PlaceText(TileMap, (point) {6, Y}, g_ItemNames[Inventory->Items[ItemI].ID]);
            PlaceTextF(TileMap, (point) {14, Y + 1}, "*%2d", Inventory->Items[ItemI].Count);
        } else if(ItemI == Inventory->ItemCount) {
            PlaceText(TileMap, (point) {6, Y}, "CANCEL");
        }
    }
    
    /*PlaceInventoryCursor*/
    TileMap[Inventory->Y * 2 + 4][5] = MT_FULL_HORZ_ARROW;
    return GS_ITEM;
}

static game_state RemoveStartMenu(tile_map TileMap) {
    memset(TileMap, 0, sizeof(tile_map));
    return GS_NORMAL; 
}

static void MoveMenuCursor(tile_map TileMap, menu *Menu, const int *VirtButtons) {
    if(VirtButtons[BT_UP] == 1 && Menu->SelectI > 0) {
        PlaceMenuCursor(TileMap, Menu, MT_BLANK);
        Menu->SelectI--; 
        PlaceMenuCursor(TileMap, Menu, MT_FULL_HORZ_ARROW);
    } else if(VirtButtons[BT_DOWN] == 1 && Menu->SelectI < Menu->EndI - 1) {
        PlaceMenuCursor(TileMap, Menu, MT_BLANK);
        Menu->SelectI++; 
        PlaceMenuCursor(TileMap, Menu, MT_FULL_HORZ_ARROW);
    }
}

static void MoveMenuCursorWrap(tile_map TileMap, menu *Menu, const int *VirtButtons) { 
    if(VirtButtons[BT_UP] == 1) {
        PlaceMenuCursor(TileMap, Menu, MT_BLANK);
        Menu->SelectI = PosIntMod(Menu->SelectI - 1, Menu->EndI); 
        PlaceMenuCursor(TileMap, Menu, MT_FULL_HORZ_ARROW);
    } else if(VirtButtons[BT_DOWN] == 1) {
        PlaceMenuCursor(TileMap, Menu, MT_BLANK);
        Menu->SelectI = PosIntMod(Menu->SelectI + 1, Menu->EndI); 
        PlaceMenuCursor(TileMap, Menu, MT_FULL_HORZ_ARROW);
    }
}

static game_state PlaceOptionsMenu(tile_map TileMap, options_menu *Options) {
    memset(TileMap, MT_BLANK, sizeof(tile_map));
    PlaceTextBox(TileMap, (rect) {0, 0, 20, 5});
    PlaceTextBox(TileMap, (rect) {0, 5, 20, 10});
    PlaceTextBox(TileMap, (rect) {0, 10, 20, 15});
    PlaceText(
        TileMap, 
        (point) {1, 1}, 
        "TEXT SPEED\n FAST  MEDIUM SLOW\r" 
        "BATTLE ANIMATION\n ON       OFF\r" 
        "BATTLE STYLE\n SHIFT    SET\r"
        " CANCEL"
    ); 
    Options->I = 0;
    PlaceOptionCursor(TileMap, &Options->E[0], MT_FULL_HORZ_ARROW); 
    for(int I = 1; I < _countof(Options->E); I++) {
        PlaceOptionCursor(TileMap, &Options->E[I], MT_EMPTY_HORZ_ARROW);
    }
    return GS_OPTIONS;
}

/*Inventory Functions*/
static item RemoveItem(inventory *Inventory, int TossCount) {
    item *SelectedItem = &Inventory->Items[Inventory->ItemSelect]; 
    item RetItem = *SelectedItem;
    if(TossCount < SelectedItem->Count) { 
        SelectedItem->Count -= TossCount;
        RetItem.Count = TossCount;
    } else {
        Inventory->ItemCount--;
        for(int I = Inventory->ItemSelect; I < Inventory->ItemCount; I++) {
            Inventory->Items[I] = Inventory->Items[I + 1]; 
        }
        Inventory->ItemSelect = MinInt(Inventory->ItemSelect, Inventory->ItemCount);
    }
    return RetItem;
}

static int FindItem(const inventory *Inventory, int ID) {
    for(int I = 0; I < Inventory->ItemCount; I++) {
        if(Inventory->Items[I].ID == ID) { 
            return I;
        }
    }
    return -1;
}

static void AddItem(inventory *Inventory, item Item) {
    int ExistingItemI = FindItem(Inventory, Item.ID); 
    if(ExistingItemI >= 0 && Inventory->Items[ExistingItemI].Count + Item.Count < 100) {
        Inventory->Items[ExistingItemI].Count += Item.Count;
    } else if(Inventory->ItemCount < Inventory->ItemCapacity) {
        Inventory->Items[Inventory->ItemCount++] = Item;
    } 
}

static void MoveItem(inventory *Dest, inventory *Src) {
    if(Dest->ItemCount < Dest->ItemCapacity) {
        AddItem(Dest, RemoveItem(Src, 1));
    }
} 

static int PlaceTossCount(tile_map TileMap, const inventory *Inventory, int TossCount) {
    TossCount = PosIntMod(TossCount - 1, Inventory->Items[Inventory->ItemSelect].Count) + 1; 
    PlaceTextF(TileMap, (point) {16, 10}, "*%02d", TossCount);
    return TossCount;
}

/*Library Functions*/
static HRESULT LoadProc(lib_struct *Struct, const char *Name, const char **ProcNames, int ProcCount) {
    HRESULT Result = S_OK;
    Struct->Library = LoadLibrary(Name);
    if(Struct->Library) {
        for(int I = 0; I < ProcCount; I++) {
            Struct->Procs[I] = GetProcAddress(Struct->Library, ProcNames[I]);
            if(!Struct->Procs[I]) {
                memset(Struct, 0, sizeof(Struct) + ProcCount * sizeof(FARPROC));
                Result = HRESULT_FROM_WIN32(GetLastError());
                break;
            } 
        }
    } else {
        Result = HRESULT_FROM_WIN32(GetLastError());
    }
    return Result;
} 

static HRESULT LoadProcsVersioned(multi_lib *LibData) {
    HRESULT Result = E_FAIL;
    for(int I = 0; I < LibData->VersionCount && FAILED(Result); I++) {
        Result = LoadProc(LibData->LibStruct, LibData->Versions[I], 
                          LibData->ProcNames, LibData->ProcCount);
    }
    return Result;
} 

static HRESULT StartWinmm(winmm *Winmm) {
    const char *WinmmProcs[2] = {
        "timeBeginPeriod", 
        "timeEndPeriod" 
    };
    HRESULT Result = LoadProc(&Winmm->LibStruct, "winmm.dll", WinmmProcs, _countof(WinmmProcs)); 
    if(SUCCEEDED(Result)) {
        Winmm->IsGranular = Winmm->TimeBeginPeriod(1) == TIMERR_NOERROR;
        if(!Winmm->IsGranular) {
            Result = E_FAIL; 
            FreeLibrary(Winmm->Library);
            *Winmm = (winmm) {0};
        }
    }
    return Result;
}

static void EndWinmm(winmm *Winmm) {
    if(Winmm->Library) {
        Winmm->TimeEndPeriod(1);
        FreeLibrary(Winmm->Library);
    }
}

static HRESULT StartCom(com *Com) {
    const char *ComProcs[] = {
        "CoInitializeEx", 
        "CoUninitialize" 
    };
    HRESULT Result = LoadProc((lib_struct *) Com, "ole32.dll", ComProcs, _countof(ComProcs));
    if(SUCCEEDED(Result)) {
        Result = Com->CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if(FAILED(Result)) {
            FreeLibrary(Com->Library); 
        }
    }
    if(FAILED(Result)) {
        *Com = (com) {0};
    }
    return Result;
}

static void EndCom(com *Com) {
    if(Com->Library) {
        Com->CoUninitialize();
        FreeLibrary(Com->Library);
    }
}

static HRESULT CreateXAudio2(xaudio2 *XAudio2, const com *Com) {
    multi_lib LibData = {
        .VersionCount = 3,
        .Versions = (const char *[]) {
            "XAudio2_9.dll",
            "XAudio2_8.dll",
            "XAudio2_7.dll" 
        },
        .ProcCount = 1,
        .ProcNames = (const char *[]) {
            "XAudio2Create", 
        },
        .LibStruct = &XAudio2->LibStruct
    };
    HRESULT Result = LoadProcsVersioned(&LibData);
    if(SUCCEEDED(Result)) {
        Result = XAudio2->Create(&XAudio2->Engine, 0, XAUDIO2_DEFAULT_PROCESSOR);
        if(SUCCEEDED(Result)) {
            Result = IXAudio2_CreateMasteringVoice(
                XAudio2->Engine, 
                &XAudio2->MasterVoice,
                XAUDIO2_DEFAULT_CHANNELS,
                XAUDIO2_DEFAULT_SAMPLERATE,
                0,
                NULL,
                NULL,
                AudioCategory_GameEffects
            ); 
            if(FAILED(Result)) {
                IXAudio2_Release(XAudio2->Engine);
            } 
        }
        if(FAILED(Result)) {
            FreeLibrary(XAudio2->Library);
        }
    }
    if(FAILED(Result)) {
        *XAudio2 = (xaudio2) {0}; 
    }
    return Result;
}

static void EndXAudio2(xaudio2 *XAudio2) {
    if(XAudio2->Engine) {
        IXAudio2_Release(XAudio2->Engine);
    }
}

/*XAudio2 Functions*/
static HRESULT ReadWave(const char *Path, XAUDIO2_BUFFER *Buffer) {
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
            if(Header.idChunk == LE_RIFF &&
               Header.idFormat == LE_WAVE &&
               Header.idSubChunk == LE_FMT &&
               Header.wFormatTag == g_WaveFormat.wFormatTag &&
               Header.nChannels == g_WaveFormat.nChannels && 
               Header.nSamplesPerSec == g_WaveFormat.nSamplesPerSec &&
               Header.nBlockAlign == g_WaveFormat.nBlockAlign &&
               Header.nAvgBytesPerSec == g_WaveFormat.nAvgBytesPerSec &&
               Header.wBitsPerSample == g_WaveFormat.wBitsPerSample &&
               Header.idDataChunk == LE_DATA) {
                void *DataBuf = HeapAlloc(GetProcessHeap(), 0, Header.cbData);
                if(DataBuf) {
                    Result = ReadObject(FileHandle, DataBuf, Header.cbData);
                    if(SUCCEEDED(Result)) {
                        *Buffer = (XAUDIO2_BUFFER) {
                            .Flags = XAUDIO2_END_OF_STREAM,
                            .AudioBytes = Header.cbData,  
                            .pAudioData = DataBuf
                        };
                    } else {
                        HeapFree(GetProcessHeap(), 0, DataBuf);
                    }
                } else {
                    Result = HRESULT_FROM_WIN32(GetLastError());
                }
            } else {
                Result = E_FAIL;
            }
        } 
    } else {
        Result = HRESULT_FROM_WIN32(GetLastError());
    }
    return Result;
}

static HRESULT PlayWave(IXAudio2SourceVoice *Voice, XAUDIO2_BUFFER *Buffer) { 
    HRESULT Result = S_OK;
    if(Buffer->pAudioData) {
        Result = IXAudio2SourceVoice_SubmitSourceBuffer(Voice, Buffer, NULL);
        if(SUCCEEDED(Result)) {
            Result = IXAudio2SourceVoice_Start(Voice, 0, XAUDIO2_COMMIT_NOW); 
        }
    }
    return Result;
}

static HRESULT PlayMusic(IXAudio2SourceVoice *Voice, XAUDIO2_BUFFER *Buffer) {
    HRESULT Result = S_OK;
    if(Buffer->pAudioData) {
        Result = IXAudio2SourceVoice_Stop(Voice, 0, XAUDIO2_COMMIT_NOW);
        if(SUCCEEDED(Result)) {
            Result = IXAudio2SourceVoice_FlushSourceBuffers(Voice);
            if(SUCCEEDED(Result)) {
                Result = PlayWave(Voice, Buffer);
            }
        }
    }
    return Result;
}

static HRESULT SetVolume(IXAudio2SourceVoice *Voice, float Volume) {
    HRESULT Result = S_OK;
    if(Voice) {
        Result = IXAudio2SourceVoice_SetVolume(Voice, Volume, XAUDIO2_COMMIT_NOW);
    }
    return Result;
}

static int GetQueueCount(IXAudio2SourceVoice *Voice) {
    int Result = 0;
    if(Voice) {
        XAUDIO2_VOICE_STATE VoiceState;
        IXAudio2SourceVoice_GetState(Voice, &VoiceState, 0);
        Result = VoiceState.BuffersQueued; 
    }
    return Result;
}

static HRESULT CreateGenericVoice(IXAudio2 *Engine, IXAudio2SourceVoice **Voice) {
     return IXAudio2_CreateSourceVoice(
        Engine,
        Voice, 
        &g_WaveFormat,
        0,
        XAUDIO2_DEFAULT_FREQ_RATIO,
        NULL,
        NULL,
        NULL
    );
}

/*Win32 Functions*/
static void SetBitmapForWindow(HWND Window, bitmap *Bitmap) {
    SetWindowLongPtr(Window, GWLP_USERDATA, (LONG_PTR) Bitmap);
}

static bitmap *GetBitmapFromWindow(HWND Window) {
    return (bitmap *) GetWindowLongPtr(Window, GWLP_USERDATA);
}

static void SetMyWindowPos(HWND Window, DWORD Style, dim_rect Rect) {
    SetWindowLongPtr(Window, GWL_STYLE, Style | WS_VISIBLE);
    SetWindowPos(Window, 
                 HWND_TOP, 
                 Rect.X, Rect.Y, Rect.Width, Rect.Height, 
                 SWP_FRAMECHANGED | SWP_NOREPOSITION);
}

static void UpdateFullscreen(HWND Window) {
    dim_rect ClientRect = GetDimClientRect(Window);

    HMONITOR Monitor = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO MonitorInfo = {
        .cbSize = sizeof(MonitorInfo)
    };
    GetMonitorInfo(Monitor, &MonitorInfo);

    dim_rect MonitorRect = WinToDimRect(MonitorInfo.rcMonitor);

    if(MonitorRect.Width != ClientRect.Width || MonitorRect.Height != ClientRect.Height) {
        SetMyWindowPos(Window, WS_POPUP, MonitorRect);
    }
}

static void PaintFrame(HWND Window, bitmap *Bitmap) {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    dim_rect ClientRect = GetDimClientRect(Window);

    int RenderWidth = ClientRect.Width - ClientRect.Width % BM_WIDTH;
    int RenderHeight = ClientRect.Height - ClientRect.Height % BM_HEIGHT;

    int RenderColSize = RenderWidth * BM_HEIGHT;
    int RenderRowSize = RenderHeight * BM_WIDTH;
    if(RenderColSize > RenderRowSize) {
        RenderWidth = RenderRowSize / BM_HEIGHT;
    } else {
        RenderHeight = RenderColSize / BM_WIDTH;
    }
    int RenderX = (ClientRect.Width - RenderWidth) / 2;
    int RenderY = (ClientRect.Height - RenderHeight) / 2;

    if(Bitmap) {
        StretchDIBits(DeviceContext, 
                      RenderX, RenderY, RenderWidth, RenderHeight,
                      0, 0, BM_WIDTH, BM_HEIGHT, 
                      Bitmap->Pixels, &Bitmap->Info.WinStruct, 
                      DIB_RGB_COLORS, SRCCOPY);
    }
    PatBlt(DeviceContext, 0, 0, RenderX, ClientRect.Height, BLACKNESS);
    int RenderRight = RenderX + RenderWidth;
    PatBlt(DeviceContext, RenderRight, 0, RenderX, ClientRect.Height, BLACKNESS);
    PatBlt(DeviceContext, RenderX, 0, RenderWidth, RenderY, BLACKNESS);
    int RenderBottom = RenderY + RenderHeight;
    PatBlt(DeviceContext, RenderX, RenderBottom, RenderWidth, RenderY + 1, BLACKNESS);
    EndPaint(Window, &Paint);
}

static LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    switch(Message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        DestroyWindow(Window);
        return 0;
    case WM_PAINT:
        PaintFrame(Window, GetBitmapFromWindow(Window));
        return 0;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE Prev, LPSTR CmdLine, int CmdShow) {
    int TimeStart = time(NULL);
    srand(TimeStart);
    SetCurrentDirectory("../Shared");
    int VirtButtons[COUNTOF_BT] = {0};

    /*MuteModeLaunch*/
    int IsMute = strcmp(CmdLine, "mute") == 0;

    /*InitPalletes*/
    const RGBQUAD Palletes[256][4] = {
        {
            {0xFF, 0xEF, 0xFF, 0x00},
            {0xA8, 0xA8, 0xA8, 0x00},
            {0x80, 0x80, 0x80, 0x00},
            {0x10, 0x10, 0x18, 0x00}
        },
        {
            {0xFF, 0xEF, 0xFF, 0x00},
            {0xDE, 0xE7, 0xCE, 0x00},
            {0xFF, 0xD6, 0xA5, 0x00},
            {0x10, 0x10, 0x18, 0x00}
        },
        {
            {0xFF, 0xEF, 0xFF, 0x00},
            {0x5A, 0xE7, 0xAD, 0x00},
            {0xFF, 0xD6, 0xA5, 0x00},
            {0x10, 0x10, 0x18, 0x00}
        }
    };

    /*InitBitmap*/
    bitmap Bitmap = {
        .Info.Header = {
            .biSize = sizeof(Bitmap.Info.Header),
            .biWidth = BM_WIDTH,
            .biHeight = -BM_HEIGHT,
            .biPlanes = 1,
            .biBitCount = 8,
            .biCompression = BI_RGB,
            .biClrUsed = 4 
        }
    };

    /*InitWindow*/
    WNDCLASS WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = Instance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = "PokeWindowClass"
    };
    if(!RegisterClass(&WindowClass)) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR); 
        return 1;
    }

    RECT WinWindowRect = {0, 0, BM_WIDTH, BM_HEIGHT};
    AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, 0);

    dim_rect WindowRect = WinToDimRect(WinWindowRect);

    HWND Window = CreateWindow(
        WindowClass.lpszClassName, 
        "PokeGame", 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        WindowRect.Width, 
        WindowRect.Height, 
        NULL, 
        NULL, 
        Instance, 
        NULL
    );
    if(!Window) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR); 
        return 1;
    }
    SetBitmapForWindow(Window, &Bitmap);

    /*InitXAudio2*/
    HRESULT XAudio2Result = E_FAIL;
    com Com = {0};
    xaudio2 XAudio2 = {0};

    if(!IsMute) {
        XAudio2Result = StartCom(&Com);
        if(SUCCEEDED(XAudio2Result)) {
            XAudio2Result = CreateXAudio2(&XAudio2, &Com);
            if(FAILED(XAudio2Result)) {
                EndCom(&Com); 
            }
        }
        if(FAILED(XAudio2Result)) {
            MessageBox(Window, "XAudio2 could not be initalized", "Error", MB_ICONERROR);
        }
    }

    /*GetSoundsAndMusic*/
    XAUDIO2_BUFFER CollisionBuffer = {0};
    XAUDIO2_BUFFER LedgeBuffer = {0}; 
    XAUDIO2_BUFFER StartMenuBuffer = {0}; 
    XAUDIO2_BUFFER SoundEffectPressAB = {0};
    XAUDIO2_BUFFER GoOutside = {0};
    XAUDIO2_BUFFER GoInside = {0}; 
    XAUDIO2_BUFFER SaveSound = {0};
    XAUDIO2_BUFFER SoundEffectTurnOnPC = {0};
    XAUDIO2_BUFFER SoundEffectTurnOffPC = {0};

    struct {
        XAUDIO2_BUFFER Title;
        XAUDIO2_BUFFER PalleteTown;
    } Music = {0}; 

    IXAudio2SourceVoice *SoundEffectVoice = NULL;
    IXAudio2SourceVoice *MusicVoice = NULL;

    if(SUCCEEDED(XAudio2Result)) {
        HRESULT SoundEffectResult = CreateGenericVoice(XAudio2.Engine, &SoundEffectVoice);
        if(SUCCEEDED(SoundEffectResult)) {
            ReadWave("SFX_COLLISION.wav", &CollisionBuffer);
            ReadWave("SFX_LEDGE.wav", &LedgeBuffer);
            ReadWave("SFX_START_MENU.wav", &StartMenuBuffer);
            ReadWave("SFX_PRESS_AB.wav", &SoundEffectPressAB);
            ReadWave("SFX_GO_OUTSIDE.wav", &GoOutside);
            ReadWave("SFX_GO_INSIDE.wav", &GoInside);
            ReadWave("SFX_SAVE.wav", &SaveSound);
            ReadWave("SFX_TURN_ON_PC.wav", &SoundEffectTurnOnPC);
            ReadWave("SFX_TURN_OFF_PC.wav", &SoundEffectTurnOffPC);
        }
 
        HRESULT MusicResult = CreateGenericVoice(XAudio2.Engine, &MusicVoice);
        if(SUCCEEDED(MusicResult)) {
            ReadWave("01_title.wav", &Music.Title);
            Music.Title.LoopCount = XAUDIO2_LOOP_INFINITE;
            ReadWave("03_pallettown.wav", &Music.PalleteTown);
            Music.PalleteTown.LoopCount = XAUDIO2_LOOP_INFINITE;
        }
    }

    /*InitXInput*/
    union {
        struct {
            HMODULE Library;
            xinput_get_state *GetState;
            xinput_set_state *SetState;
        };
        lib_struct LibStruct;
    } XInput = {0};

    multi_lib XInputLibData = {
        .VersionCount = 3,
        .Versions = (const char *[]) {
            "xinput1_4.dll",
            "xinput1_3.dll",
            "xinput9_1_0.dll"
        },
        .ProcCount = 2,
        .ProcNames = (const char *[]) {
            "XInputGetState", 
            "XInputSetState" 
        },
        .LibStruct = &XInput.LibStruct
    };
    LoadProcsVersioned(&XInputLibData);

    /*InitWorld*/
    world World = {
        .OverworldMapPaths = {
            {"VirdianCity"},
            {"Route1"},
            {"PalleteTown"},
            {"Route21"}
        },
        .DataPathI = -1,
        .DataPaths = {
            {
                .Tile = "TileData00",
                .Quad = "QuadData00",
                .Prop = "QuadProps00"
            }, 
            {
                .Tile = "TileData01",
                .Quad = "QuadData01",
                .Prop = "QuadProps01"
            }
        },
        .Player = {
            .Pos = {80, 96},
            .Dir = DIR_DOWN,
            .Speed = 2,
            .Tile = ANIM_PLAYER
        }
    };

    /*LoadTileData*/
    uint8_t SpriteData[256][TILE_SIZE];
    uint8_t FlowerData[3][TILE_SIZE];
    uint8_t WaterData[7][TILE_SIZE];

    ReadTileData("Menu", &World.TileData[96], 152);
    ReadTileData("SpriteData", SpriteData, 256);
    ReadTileData("FlowerData", FlowerData, 3);
    ReadTileData("WaterData", WaterData, 7);
    ReadTileData("ShadowData", &SpriteData[255], 1);

    /*GameBoyGraphics*/
    sprite Sprites[40] = {0};

    tile_map TileMap;
    uint8_t ScrollX = 0;
    uint8_t ScrollY = 0;

    tile_map WindowMap = {0};

    /*InitGameState*/
    game_state GameState = GS_MAIN_MENU;
    game_state RestoreState = GS_MAIN_MENU;
    int TransitionTick = 0;
    point DoorPoint = {0};

    /*InitMenuData*/
    const rect SaveRect = {4, 0, 20, 10};

    int IsPauseAttempt = 0;
    int OptionI = 0; 

    options_menu Options = {
        .E = {
            [OPT_TEXT_SPEED] = {
                .Y = 3,
                .Xs = {1, 7, 14},
                .Count = 3
            },
            [OPT_BATTLE_ANIMATION] = {
                .Y = 8,
                .Xs = {1, 10},
                .Count = 2
            },
            [OPT_BATTLE_STYLE] = {
                .Y = 13,
                .Xs = {1, 10},
                .Count = 2
            },
            [OPT_CANCEL] = {
                .Y = 16,
                .Xs = {1},
                .Count = 1
            }
        }
    };

    int SaveSec = 0;
    int StartSaveSec = 0;
    int IsSaveComplete = 0;

    menu StartMenu = {
        .Text = "POKéMON\nITEM\nRED\nSAVE\nOPTION\nEXIT", 
        .Rect = {10, 0, 20, 14},
        .TextY = 2,
        .EndI = 6
    };

    menu RedPCMenu = {
        .Text = "WITHDRAW ITEM\nDEPOSIT ITEM\nTOSS ITEM\nLOG OFF",
        .Rect = {0, 0, 16, 10},
        .TextY = 2,
        .EndI = 4
    };

    menu YesNoMenu = {
        .Text = "YES\nNO",
        .Rect = {0, 7, 6, 12},
        .TextY = 8,
        .EndI = 2
    };
    menu UseTossMenu = {
        .Text = "USE\nTOSS",
        .Rect = {13, 10, 20, 15},
        .TextY = 11,
        .EndI = 2
    };
    menu ConfirmTossMenu = {
        .Text = "YES\nNO",
        .Rect = {14, 7, 6, 12},
        .TextY = 8,
        .EndI = 2
    };

    /*InitInventory*/
    inventory Bag = {
        .ItemCapacity = 20,
        .Items = (item[20]) {{0}}
    };
    inventory RedPC = {
        .ItemCapacity = 50,
        .Items = (item[50]) {{ITEM_POTION, 1}},
        .ItemCount = 1 
    };
    inventory_state InventoryState = IS_ITEM;
    inventory *Inventory = &Bag;

    int TossCount = 1;

    /*InitMainMenu*/
    menu MainMenu = { 
        .Text = "NEW GAME\nOPTION",
        .Rect = {0, 0, 15, 6},
        .EndI = 2,
        .TextY = 2
    };
    if(GetFileAttributes("Save") != INVALID_FILE_ATTRIBUTES) {
        MainMenu = (menu) {
            .Rect = {0, 0, 15, 8},
            .Text = "CONTINUE\nNEW GAME\nOPTION",
            .EndI = 3,
            .TextY = 2
        };
    }

    memset(TileMap, MT_BLANK, sizeof(TileMap));
    PlaceMenu(WindowMap, &MainMenu);
    memcpy(Bitmap.Info.Colors, Palletes[0], sizeof(Palletes[0]));
    PlayWave(MusicVoice, &Music.Title); 
    
    /*InitNonGameState*/
    dim_rect RestoreRect = {0};
    int HasQuit = 0;
    int IsFullscreen = 0;

    /*Text*/
    active_text ActiveText = {0};

    /*Timing*/
    int64_t Tick = 0;
    int64_t BeginCounter = 0;
    int64_t PerfFreq = GetPerfFreq();
    int64_t FrameDelta = PerfFreq / 60;

    winmm Winmm; 
    StartWinmm(&Winmm); 

    int64_t StartCounter = 0;

    /*MainLoop*/
    ShowWindow(Window, CmdShow);
    while(!HasQuit) {
        BeginCounter = GetPerfCounter();

        /*ProcessMessages*/
        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
            switch(Message.message) {
            case WM_QUIT:
                HasQuit = 1;
                break;
            default:
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }
        }

        /*UpdateInput*/
        if(GetActiveWindow() == Window) {
            /*UpdateXInput*/
            XINPUT_STATE XInputState;
            if(XInput.GetState && XInput.GetState(0, &XInputState) == ERROR_SUCCESS) { 
                if(XInputState.Gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                    XInputState.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
                } 
                if(XInputState.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                    XInputState.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
                }
                if(XInputState.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                    XInputState.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
                }
                if(XInputState.Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                    XInputState.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
                } 
            } else {
                XInputState = (XINPUT_STATE) {0}; 
            }

            /*UpdateVirtButtons*/
            struct {
                int Button;
                int Key;
            } VirtButtonMap[] = {
                [BT_UP] = {
                    .Button = XINPUT_GAMEPAD_DPAD_UP,
                    .Key = VK_UP
                },
                [BT_LEFT] = {
                    .Button = XINPUT_GAMEPAD_DPAD_LEFT,
                    .Key = VK_LEFT,
                },
                [BT_DOWN] = {
                    .Button = XINPUT_GAMEPAD_DPAD_DOWN,
                    .Key = VK_DOWN
                },
                [BT_RIGHT] = {
                    .Button = XINPUT_GAMEPAD_DPAD_RIGHT,
                    .Key = VK_RIGHT
                },
                [BT_A] = {
                    .Button = XINPUT_GAMEPAD_A,
                    .Key = 'X' 
                },
                [BT_B] = {
                    .Button = XINPUT_GAMEPAD_B,
                    .Key = 'Z' 
                },
                [BT_START] = {
                    .Button = XINPUT_GAMEPAD_START,
                    .Key = VK_RETURN
                },
                [BT_FULLSCREEN] = {
                    .Button = XINPUT_GAMEPAD_BACK,
                    .Key = VK_F11 
                }
            };
            int IsAltDown = GetAsyncKeyState(VK_MENU);
            for(int I = 0; I < _countof(VirtButtonMap); I++) {
                int IsButtonDown = XInputState.Gamepad.wButtons & VirtButtonMap[I].Button; 
                int IsKeyDown = !IsAltDown && GetAsyncKeyState(VirtButtonMap[I].Key); 
                VirtButtons[I] = IsButtonDown || IsKeyDown ? VirtButtons[I] + 1 : 0;                  
            }
        } else {
            memset(VirtButtons, 0, sizeof(VirtButtons));
        } 

        /*UpdateFullscreen*/
        if(VirtButtons[BT_FULLSCREEN] == 1) {
            ShowCursor(IsFullscreen);
            if(IsFullscreen) {
                SetMyWindowPos(Window, WS_OVERLAPPEDWINDOW, RestoreRect);
            } else {
                RestoreRect = GetDimWindowRect(Window);
            }
            IsFullscreen ^= 1;
        }

        /*UpdatePlayer*/
        int SpriteI = 0;
        switch(GameState) {
        case GS_MAIN_MENU:
            MoveMenuCursor(WindowMap, &MainMenu, VirtButtons);
            if(VirtButtons[BT_A] == 1) {
                switch(MainMenu.EndI - MainMenu.SelectI) {
                case 3: /*CONTINUE*/
                    ReadSave(&World, &Bag, &RedPC, &StartSaveSec);
                    PlaceSave(WindowMap, (rect) {4, 7, 20, 17}, StartSaveSec); 
                    GameState = GS_CONTINUE;
                    break;
                case 2: /*NEW GAME*/
                    StartCounter = GetPerfCounter();
                    World.Player = (object) {
                        .Pos = {80, 96},
                        .Dir = DIR_DOWN,
                        .Speed = 2,
                        .Tile = ANIM_PLAYER
                    };
                    memset(WindowMap, 0, sizeof(WindowMap));
                    ReadOverworldMap(&World, 0, (point) {0, 2});
                    PlaceViewMap(&World, TileMap, 0);
                    ScrollX = 0;
                    ScrollY = 0;
                    GameState = GS_NORMAL;
                    break;
                case 1: /*OPTIONS*/
                    GameState = PlaceOptionsMenu(WindowMap, &Options);
                    RestoreState = GS_MAIN_MENU;
                    break;
                }
                PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
            }
            break;
        case GS_CONTINUE:
            if(ActiveText.Tick > 0) {
                ActiveText.Tick--;
                if(RestoreState == GS_NORMAL) {
                    SetVolume(MusicVoice, (float) ActiveText.Tick / 60.0f);
                }
                if(ActiveText.Tick == 0) {
                    switch(RestoreState) {
                    case GS_NORMAL:
                        StartCounter = GetPerfCounter();
                        PlaceViewMap(&World, TileMap, 0);
                        ScrollX = 0;
                        ScrollY = 0;
                        SetVolume(MusicVoice, 1.0f);
                        PlayMusic(MusicVoice, &Music.PalleteTown); 
                        break;
                    default:
                        PlaceMenu(WindowMap, &MainMenu);
                        break;
                    }
                    GameState = RestoreState;
                }
            } else if(VirtButtons[BT_A] == 1) {
                ActiveText.Tick = 60;
                memset(WindowMap, 0, sizeof(WindowMap));
                RestoreState = GS_NORMAL;
            } else if(VirtButtons[BT_B] == 1) {
                ActiveText.Tick = 30;
                memset(WindowMap, 0, sizeof(WindowMap));
                RestoreState = GS_MAIN_MENU;
            }
            break;
        case GS_NORMAL:
            /*PlayerUpdate*/
            IsPauseAttempt |= (VirtButtons[BT_START] == 1);
            if(World.Player.Tick == 0) {
                if(IsPauseAttempt) {
                    if(GetQueueCount(SoundEffectVoice) == 0) {
                        IsPauseAttempt = 0;
                        PlayWave(SoundEffectVoice, &StartMenuBuffer); 
                        GameState = GS_START_MENU;
                        PlaceMenu(WindowMap, &StartMenu);
                    }
                } else { 
                    int AttemptLeap = 0;

                    /*PlayerKeyboard*/
                    int HasMoveKey = 1;
                    if(VirtButtons[BT_UP]) {
                        World.Player.Dir = DIR_UP;
                    } else if(VirtButtons[BT_LEFT]) {
                        World.Player.Dir = DIR_LEFT;
                    } else if(VirtButtons[BT_DOWN]) {
                        World.Player.Dir = DIR_DOWN;
                        AttemptLeap = 1;
                    } else if(VirtButtons[BT_RIGHT]) {
                        World.Player.Dir = DIR_RIGHT;
                    } else {
                        HasMoveKey = 0;
                    }

                    point NewPoint = GetFacingPoint(World.Player.Pos, World.Player.Dir);

                    /*FetchQuadProp*/
                    quad_info OldQuadInfo = GetQuadInfo(&World, PtToQuadPt(World.Player.Pos));
                    quad_info NewQuadInfo = GetQuadInfo(&World, NewPoint);
                    NewPoint = NewQuadInfo.Point;

                    point ReverseDirPoint = g_DirPoints[ReverseDir(World.Player.Dir)];
                    point StartPos = AddPoints(NewPoint, ReverseDirPoint); 
                    StartPos.X *= 16;
                    StartPos.Y *= 16;

                    /*FetchObject*/
                    AttemptLeap &= !!(NewQuadInfo.Prop == QUAD_PROP_EDGE);
                    point TestPoint = NewPoint; 
                    if(AttemptLeap) {
                        TestPoint.Y++;
                    }

                    object *FacingObject = NULL; 
                    for(int ObjI = 0; ObjI < World.Maps[World.MapI].ObjectCount; ObjI++) {
                        object *Object = &World.Maps[World.MapI].Objects[ObjI];
                        point ObjOldPt = PtToQuadPt(Object->Pos);
                        if(Object->Tick > 0) {
                            point ObjDirPt = g_NextPoints[Object->Dir];
                            point ObjNewPt = AddPoints(ObjOldPt, ObjDirPt);
                            if(EqualPoints(TestPoint, ObjOldPt) || 
                               EqualPoints(TestPoint, ObjNewPt)) {
                                NewQuadInfo.Prop = QUAD_PROP_SOLID;
                                FacingObject = Object;
                                break;
                            }
                        } else if(EqualPoints(TestPoint, ObjOldPt)) {
                            NewQuadInfo.Prop = AttemptLeap ? QUAD_PROP_SOLID : QUAD_PROP_MESSAGE;
                            FacingObject = Object;
                            break;
                        }
                    }

                    /*PlayerTestProp*/
                    if(VirtButtons[BT_A] == 1) {
                        int IsTV = NewQuadInfo.Prop == QUAD_PROP_TV;
                        int IsShelf = NewQuadInfo.Prop == QUAD_PROP_SHELF && 
                                      World.Player.Dir == DIR_UP;
                        int IsComputer = (NewQuadInfo.Prop == QUAD_PROP_RED_PC && 
                                       World.Player.Dir == DIR_UP);
                        if(NewQuadInfo.Prop == QUAD_PROP_MESSAGE || IsTV || IsShelf || IsComputer) {
                            GameState = GS_TEXT;
                            RestoreState = GS_NORMAL;

                            ActiveText = (active_text) {
                                .Tick = 16
                            };
                            /*GetActiveText*/
                            if(IsShelf) {
                                ActiveText.Str = "Crammed full of\nPOKéMON books!"; 
                            } else if(IsTV && World.Player.Dir != DIR_UP) {
                                ActiveText.Str = "Oops, wrong side."; 
                            } else if(FacingObject) {
                                ActiveText.Str = FacingObject->Str;
                                if(FacingObject->Speed == 0) {
                                    FacingObject->Tick = 120;
                                }
                                FacingObject->Dir = ReverseDir(World.Player.Dir);
                            } else if(IsComputer) {
                                ActiveText.Str = "RED turned on\nthe PC";
                                ActiveText.IsImmediate = 1;
                                PlayWave(SoundEffectVoice, &SoundEffectTurnOnPC);
                            } else {
                                ActiveText.Str = GetTextFromPos(&World.Maps[NewQuadInfo.MapI], NewPoint);
                            }

                            HasMoveKey = 0;
                        } else if(NewQuadInfo.Prop == QUAD_PROP_WATER) {
                            if(World.Player.Tile == ANIM_PLAYER) {
                                World.Player.Tile = ANIM_SEAL;
                                HasMoveKey = 1;
                            }
                        } else if(World.Player.Tile == ANIM_SEAL && 
                                  NewQuadInfo.Prop == QUAD_PROP_NONE) {
                            World.Player.Tile = ANIM_PLAYER;
                            HasMoveKey = 1;
                        }
                    }

                    if(HasMoveKey) {
                        GameState = GS_NORMAL;
                        /*MovePlayer*/
                        if(NewQuadInfo.Prop == QUAD_PROP_DOOR && 
                           (!World.IsOverworld || World.Player.Dir == DIR_UP)) {
                            World.Player.IsMoving = 1;
                            World.Player.Tick = 16; 
                            DoorPoint = NewQuadInfo.Point;
                            GameState = GS_TRANSITION;
                            if(World.IsOverworld) {
                                PlayWave(SoundEffectVoice, &GoInside);
                            } else {
                                PlayWave(SoundEffectVoice, &GoOutside);
                            }
                            NewPoint.Y--;
                        } else if(World.Player.Dir == DIR_DOWN && 
                                  NewQuadInfo.Prop == QUAD_PROP_EDGE) {
                            World.Player.IsMoving = 1;
                            World.Player.Tick = 32;
                            GameState = GS_LEAPING;
                            PlayWave(SoundEffectVoice, &LedgeBuffer);
                        } else if(World.Player.Tile == ANIM_SEAL ? 
                                  NewQuadInfo.Prop == QUAD_PROP_WATER : 
                                  NewQuadInfo.Prop == QUAD_PROP_NONE || NewQuadInfo.Prop == QUAD_PROP_EXIT) { 
                            World.Player.IsMoving = 1;
                            World.Player.Tick = 16;
                        } else {
                            World.Player.IsMoving = 0;
                            World.Player.Tick = 16;
                            if(World.Player.Dir == DIR_DOWN && OldQuadInfo.Prop == QUAD_PROP_EXIT) {
                                World.Player.Tick = 0;
                                TransitionTick = 32;
                                DoorPoint = OldQuadInfo.Point;
                                GameState = GS_TRANSITION;
                                PlayWave(SoundEffectVoice, &GoOutside);
                            } else if(GetQueueCount(SoundEffectVoice) < 2) {
                                PlayWave(SoundEffectVoice, &CollisionBuffer);
                            }
                        }
                    } else {
                        World.Player.IsMoving = 0;
                    }

                    if(World.Player.IsMoving) {
                        World.Player.Pos = StartPos;
                        World.MapI = NewQuadInfo.MapI;

                        /*UpdateLoadedMaps*/
                        if(World.IsOverworld) {
                            int AddToX = 0;
                            int AddToY = 0;
                            if(NewPoint.X == 4) {
                                AddToX = -1;
                            } else if(NewPoint.X == World.Maps[World.MapI].Width - 4) {
                                AddToX = 1;
                            }
                            if(NewPoint.Y == 4) {
                                AddToY = -1;
                            } else if(NewPoint.Y == World.Maps[World.MapI].Height - 4) {
                                AddToY = 1;
                            }

                            if(AddToX || AddToY) {
                                LoadNextMap(&World, AddToX, AddToY);
                            }
                        }

                        /*TranslateQuadRectToTiles*/
                        point TileMin = {0};
                        rect QuadRect = {0};

                        switch(World.Player.Dir) {
                        case DIR_UP:
                            TileMin.X = (ScrollX / 8) & 31;
                            TileMin.Y = (ScrollY / 8 + 30) & 31;

                            QuadRect = (rect) {
                                .Left = NewPoint.X - 4,
                                .Top = NewPoint.Y - 4, 
                                .Right = NewPoint.X + 6,
                                .Bottom = NewPoint.Y - 3
                            };
                            if(World.IsOverworld && GameState == GS_TRANSITION) {
                                QuadRect.Bottom += 4;
                                TileMin.Y -= 2;
                            }
                            break;
                        case DIR_LEFT:
                            TileMin.X = (ScrollX / 8 + 30) & 31;
                            TileMin.Y = (ScrollY / 8) & 31;

                            QuadRect = (rect) {
                                .Left = NewPoint.X - 4,
                                .Top = NewPoint.Y - 4,
                                .Right = NewPoint.X - 3,
                                .Bottom = NewPoint.Y + 5
                            };
                            break;
                        case DIR_DOWN:
                            TileMin.X = (ScrollX / 8) & 31;
                            TileMin.Y = (ScrollY / 8 + 18) & 31;

                            QuadRect = (rect) {
                                .Left = NewPoint.X - 4,
                                .Top = NewPoint.Y + 4,
                                .Right = NewPoint.X + 6,
                                .Bottom = NewPoint.Y + (GameState == GS_LEAPING ? 6 : 5)
                            };
                            break;
                        case DIR_RIGHT:
                            TileMin.X = (ScrollX / 8 + 20) & 31;
                            TileMin.Y = (ScrollY / 8) & 31;

                            QuadRect = (rect) {
                                .Left = NewPoint.X + 5,
                                .Top = NewPoint.Y - 4,
                                .Right = NewPoint.X + 6,
                                .Bottom = NewPoint.Y + 5
                            };
                            break;
                        }
                        PlaceMap(&World, TileMap, TileMin, QuadRect);
                    }
                }
            }
            break;
        case GS_LEAPING:
            IsPauseAttempt |= (VirtButtons[BT_START] == 1);
            break;
        case GS_TEXT:
            if(ActiveText.BoxDelay >= 0) {
                if(ActiveText.BoxDelay == 0) {
                    if(IsSaveComplete) {
                        PlayWave(SoundEffectVoice, &SaveSound);
                    }
                    PlaceTextBox(WindowMap, (rect) {0, 12, 20, 18});
                    ActiveText.TilePt = (point) {1, 14};
                }
                ActiveText.BoxDelay--;
            } else { 
                if(!ActiveText.Str[0] && ActiveText.Restore) {
                    ActiveText.Str = ActiveText.Restore;
                    ActiveText.Restore = NULL;
                }
                if(ActiveText.Str[0]) {
                    switch(ActiveText.TilePt.Y) {
                    case 17:
                        /*MoreTextDisplay*/
                        if(ActiveText.Tick > 0) {
                            ActiveText.Tick--;
                        } else switch(ActiveText.Str[-1]) {
                        case '\n':
                            memcpy(&WindowMap[14][1], &WindowMap[15][1], 17);
                            memset(&WindowMap[13][1], MT_BLANK, 17);
                            memset(&WindowMap[15][1], MT_BLANK, 17);
                            ActiveText.TilePt.Y = 16;
                            break;
                        case '\f':
                            ActiveText.TilePt.Y = 14;
                            break;
                        }
                        break;
                    case 18:
                        /*MoreTextWait*/
                        WindowMap[16][18] = ActiveText.Tick / 30 % 2 ? MT_BLANK : 171;
                        if(VirtButtons[BT_A] == 1 || VirtButtons[BT_B] == 1) {
                            if(ActiveText.IsImmediate) {
                                GameState = GS_RED_PC;
                                RedPCMenu.SelectI = 0;
                                PlaceMenu(WindowMap, &RedPCMenu);
                                PlaceStaticText(WindowMap, "What do you want\nto do?");
                                PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
                            } else {
                                if(ActiveText.Str[-1] == '\n') {
                                    memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                                    memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                                }
                                memset(&WindowMap[14][1], MT_BLANK, 17);
                                memset(&WindowMap[16][1], MT_BLANK, 18);
                                ActiveText.TilePt.Y = 17;
                                ActiveText.Tick = 8;
                                PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
                            }
                        } else {
                            ActiveText.Tick++;
                        }
                        break;
                    default:
                        /*UpdateTextForNextChar*/
                        if(ActiveText.Tick > 0) {
                            ActiveText.Tick--;
                        } else if(ActiveText.IsImmediate) {
                            PlaceText(WindowMap, (point) {1, 14}, "RED turned on\nthe PC.");
                            ActiveText.TilePt.Y = 18;
                        } else {
                            if(ActiveText.Str[0] == '[') {
                                ActiveText.Restore = ActiveText.Str;
                                if(DoesStartStringMatch(ActiveText.Str, "[ITEM]")) {
                                    ActiveText.Restore += sizeof("[ITEM]") - 1;
                                    ActiveText.Str = Inventory->ItemSelect < Inventory->ItemCount ?
                                                        g_ItemNames[
                                                            Inventory->Items[Inventory->ItemSelect].ID
                                                        ]:
                                                        "???????";
                                } else {
                                    ActiveText.Restore++;
                                    ActiveText.Str = "???";
                                }
                            }
                            switch(ActiveText.Str[0]) {
                            case '\n':
                                ActiveText.TilePt.X = 1;
                                ActiveText.TilePt.Y += 2;
                                ActiveText.Str++;
                                break;
                            case '\f':
                                ActiveText.TilePt.X = 1;
                                ActiveText.TilePt.Y = 18;
                                ActiveText.Str++;
                                break;
                            default:
                                WindowMap[ActiveText.TilePt.Y][ActiveText.TilePt.X] = 
                                    CharToTile(*ActiveText.Str);
                                if(ActiveText.TilePt.X < 20) {
                                    ActiveText.TilePt.X++;
                                    ActiveText.Str++;
                                } else {
                                    ActiveText.Str = "";
                                }
                            }
                            if(ActiveText.TilePt.Y != 18) {
                                /*UseTextSpeed*/
                                if(!VirtButtons[BT_A]) {
                                    ActiveText.Tick = (uint64_t[]){0, 2, 3}[Options.E[OPT_TEXT_SPEED].I];
                                }
                            }
                        }
                    }
                } else {
                    switch(RestoreState) {
                    case GS_SAVE:
                        GameState = RestoreState;
                        YesNoMenu.SelectI = 0;
                        PlaceMenu(WindowMap, &YesNoMenu);
                        break;
                    case GS_RED_PC:
                        GameState = RestoreState;
                        break;
                    case GS_CONFIRM_TOSS:
                        WindowMap[16][18] = ActiveText.Tick / 30 % 2 ? MT_BLANK : 171;
                        ActiveText.Tick++;
                        if(VirtButtons[BT_A] == 1) {
                            GameState = RestoreState;
                            PlaceMenu(WindowMap, &ConfirmTossMenu);
                            PlayWave(SoundEffectVoice, &SoundEffectPressAB);
                        }
                        break;
                    default:
                        if(IsSaveComplete || VirtButtons[BT_A] == 1) {
                            GameState = RestoreState;
                            memset(WindowMap, 0, sizeof(WindowMap));
                            IsSaveComplete = 0;
                        }
                    }
                }
            }
            break;
        case GS_TRANSITION:
            /*EnterTransition*/
            if(World.Player.IsMoving) {
                if(World.Player.Tick <= 0) {
                    World.Player.IsMoving = 0;
                    TransitionTick = 32;
                }
            }

            /*ProgressTransition*/
            if(!World.Player.IsMoving) {
                if(TransitionTick-- > 0) {
                    switch(TransitionTick) {
                    case 24:
                        Bitmap.Info.Colors[0] = Palletes[World.Maps[World.MapI].PalleteI][1]; 
                        Bitmap.Info.Colors[1] = Palletes[World.Maps[World.MapI].PalleteI][2]; 
                        Bitmap.Info.Colors[2] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                        break;
                    case 16:
                        Bitmap.Info.Colors[0] = Palletes[World.Maps[World.MapI].PalleteI][2]; 
                        Bitmap.Info.Colors[1] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                        break;
                    case 8:
                        Bitmap.Info.Colors[0] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                        break;
                    case 0:
                        {
                            /*LoadDoorMap*/
                            quad_info QuadInfo = GetQuadInfo(&World, DoorPoint); 
                            map *OldMap = &World.Maps[QuadInfo.MapI]; 
                            const door *Door = GetDoorFromPos(OldMap, QuadInfo.Point);

                            if(Door) {
                                ReadMap(&World, !QuadInfo.MapI, Door->Path);

                                World.MapI = !QuadInfo.MapI;

                                ReadMap(&World, !QuadInfo.MapI, Door->Path);
                                World.Player.Pos = QuadPtToPt(Door->DstPos);
                                World.IsOverworld = GetLoadedPoint(&World, !QuadInfo.MapI, Door->Path);

                                /*CompleteTransition*/
                                ScrollX = 0;
                                ScrollY = 0;

                                memset(OldMap, 0, sizeof(*OldMap));
                                if(QuadInfo.Prop == QUAD_PROP_EXIT) {
                                    World.Player.IsMoving = 1;
                                    World.Player.Tick = 16;
                                    PlaceViewMap(&World, TileMap, 1);
                                } else {
                                    PlaceViewMap(&World, TileMap, 0);
                                }
                            } 
                            GameState = GS_NORMAL;
                            break;
                        }
                    }
                }
            }
            break;
        case GS_START_MENU:
            if(VirtButtons[BT_START] == 1 || VirtButtons[BT_B] == 1) {
                GameState = RemoveStartMenu(WindowMap);
                if(VirtButtons[BT_B] == 1) {
                    PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
                }
            } else { 
                MoveMenuCursorWrap(WindowMap, &StartMenu, VirtButtons);

                /*SelectMenuOption*/
                if(VirtButtons[BT_A] == 1) {
                    switch(StartMenu.SelectI) {
                    case 0: /*POKéMON*/
                        break;
                    case 1: /*ITEM*/
                        Inventory = &Bag;
                        InventoryState = IS_ITEM; 
                        GameState = PlaceInventory(WindowMap, Inventory);
                        RestoreState = GS_START_MENU;
                        break;
                    case 2: /*RED*/
                        break;
                    case 3: /*SAVE*/
                        SaveSec = (int) MinInt64(
                            INT_MAX, 
                            StartSaveSec + GetSecondsElapsed(StartCounter, PerfFreq)
                        );
                        PlaceSave(WindowMap, SaveRect, SaveSec);

                        ActiveText = (active_text) {
                            .Tick = 4,
                            .BoxDelay = 16, 
                            .Str = "Would you like to\nSAVE the game?"
                        };

                        GameState = GS_TEXT;
                        RestoreState = GS_SAVE;
                        break;
                    case 4: /*OPTIONS*/
                        GameState = PlaceOptionsMenu(WindowMap, &Options);
                        RestoreState = GS_START_MENU;
                        break;
                    case 5: /*EXIT*/
                        GameState = RemoveStartMenu(WindowMap);
                        break;
                    }
                    PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
                }
            }
            break;
        case GS_ITEM:
            if(VirtButtons[BT_B] == 1 || 
               (VirtButtons[BT_A] == 1 && Inventory->ItemSelect == Inventory->ItemCount)) {
                /*RemoveSubMenu*/
                GameState = RestoreState;
                switch(RestoreState) {
                case GS_START_MENU:
                    PlaceMenu(WindowMap, &StartMenu);
                    break;
                default:
                    PlaceMenu(WindowMap, &RedPCMenu);
                    PlaceTextBox(WindowMap, (rect) {0, 12, 20, 18});
                    ActiveText = (active_text) {
                        .Str = "What do you want\nto do?", 
                    }; 
                    GameState = GS_TEXT;
                }
                PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
            } else {
                /*MoveTextCursor*/
                if(VirtButtons[BT_A] == 1) {
                    PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
                    switch(InventoryState) {
                    case IS_ITEM:
                        WindowMap[Inventory->Y * 2 + 4][5] = MT_EMPTY_HORZ_ARROW;
                        UseTossMenu.SelectI = 0;
                        PlaceMenu(WindowMap, &UseTossMenu);
                        GameState = GS_USE_TOSS;
                        break;
                    case IS_WITHDRAW: 
                        MoveItem(&Bag, &RedPC);
                        break;
                    case IS_DEPOSIT:
                        MoveItem(&RedPC, &Bag);
                        break;
                    case IS_TOSS:
                        break;
                    } 
                } else if(VirtButtons[BT_UP] % 10 == 1 && Inventory->ItemSelect > 0) {
                    Inventory->ItemSelect--; 
                    if(Inventory->Y > 0) {
                        Inventory->Y--;
                    }
                    PlaceInventory(WindowMap, Inventory); 
                } else if(VirtButtons[BT_DOWN] % 10 == 1 && Inventory->ItemSelect < Inventory->ItemCount) {
                    Inventory->ItemSelect++; 
                    if(Inventory->Y < 2) {
                        Inventory->Y++;
                    }
                    PlaceInventory(WindowMap, Inventory); 
                } 

                /*MoreItemsWait*/
                if(Inventory->ItemSelect - Inventory->Y + 3 < Inventory->ItemCount) {
                    WindowMap[11][18] = ActiveText.Tick / 30 % 2 ? MT_BLANK : 171;
                    ActiveText.Tick++;
                } else {
                    ActiveText.Tick = 0;
                }
            }
            break;
        case GS_SAVE:
            if(VirtButtons[BT_A] == 1 && YesNoMenu.SelectI == 0) {
                PlayWave(SoundEffectVoice, &SoundEffectPressAB); 

                /*RemoveYesNoSavePrompt*/
                memset(WindowMap, 0, sizeof(WindowMap));
                PlaceMenu(WindowMap, &StartMenu);
                PlaceSave(WindowMap, SaveRect, SaveSec);
                PlaceStaticText(WindowMap, "Now saving...");

                if(WriteSave(&World, &Bag, &RedPC, SaveSec)) {
                    /*ArtificalSaveDelay*/
                    ActiveText = (active_text) {
                        .Str = "RED saved\nthe game!",
                        .BoxDelay = 120
                    };
                    GameState = GS_TEXT;
                    RestoreState = GS_NORMAL;
                    IsSaveComplete = 1;
                } else {
                    /*Error*/
                    ActiveText = (active_text) {
                        .Str = "ERROR:\nWriteSave failed",
                        .BoxDelay = 60,
                    };
                    GameState = GS_TEXT;
                    RestoreState = GS_NORMAL;
                } 
            } else if(VirtButtons[BT_A] == 1 || VirtButtons[BT_B] == 1) {
                GameState = RemoveStartMenu(WindowMap);
                PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
            } else {
                MoveMenuCursor(WindowMap, &YesNoMenu, VirtButtons);
            }
            break;
        case GS_OPTIONS:
            if(VirtButtons[BT_START] == 1 || VirtButtons[BT_B] == 1 ||
               (VirtButtons[BT_A] == 1 && OptionI == OPT_CANCEL)) {
                /*RemoveSubMenu*/
                GameState = RestoreState;
                switch(RestoreState) {
                case GS_START_MENU:
                    PlaceMenu(WindowMap, &StartMenu);
                    break;
                default:
                    PlaceMenu(WindowMap, &MainMenu);
                }
                PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
            } else {
                /*MoveOptionsCursor*/
                if(VirtButtons[BT_UP] == 1) {
                    PlaceOptionCursor(WindowMap, &Options.E[OptionI], MT_EMPTY_HORZ_ARROW);
                    OptionI = PosIntMod(OptionI - 1, _countof(Options.E)); 
                    PlaceOptionCursor(WindowMap, &Options.E[OptionI], MT_FULL_HORZ_ARROW);
                } else if(VirtButtons[BT_LEFT] == 1) {
                    switch(Options.E[OptionI].Count) {
                    case 2:
                        /*SwapOptionSelected*/
                        ChangeOptionX(WindowMap, &Options.E[OptionI], Options.E[OptionI].I ^ 1); 
                        break;
                    case 3:
                        /*NextOptionLeft*/
                        if(Options.E[OptionI].I > 0) {
                            ChangeOptionX(WindowMap, &Options.E[OptionI], Options.E[OptionI].I - 1); 
                        }
                        break;
                    }
                } else if(VirtButtons[BT_DOWN] == 1) {
                    PlaceOptionCursor(WindowMap, &Options.E[OptionI], MT_EMPTY_HORZ_ARROW);
                    OptionI = PosIntMod(OptionI + 1, _countof(Options.E)); 
                    PlaceOptionCursor(WindowMap, &Options.E[OptionI], MT_FULL_HORZ_ARROW);
                } else if(VirtButtons[BT_RIGHT] == 1) {
                    switch(Options.E[OptionI].Count) {
                    case 2:
                        /*SwapOptionSelected*/
                        ChangeOptionX(WindowMap, &Options.E[OptionI], Options.E[OptionI].I ^ 1); 
                        break;
                    case 3:
                        /*NextOptionRight*/
                        if(Options.E[OptionI].I < 2) {
                            ChangeOptionX(WindowMap, &Options.E[OptionI], Options.E[OptionI].I + 1); 
                        }
                        break;
                    }
                }
            }
            break;
        case GS_USE_TOSS:
            MoveMenuCursor(WindowMap, &UseTossMenu, VirtButtons);
            if(VirtButtons[BT_A] == 1) {
                if(UseTossMenu.SelectI == 0) {
                    ActiveText = (active_text) {
                       .Str = "ERROR: TODO" 
                    };
                    RestoreState = GS_NORMAL;
                    GameState = GS_TEXT;
                } else {
                    PlaceMenuCursor(WindowMap, &UseTossMenu, MT_EMPTY_HORZ_ARROW);
                    PlaceTextBox(WindowMap, (rect) {15, 9, 20, 12});
                    TossCount = PlaceTossCount(WindowMap, Inventory, 1); 
                    GameState = GS_TOSS;
                }
                PlayWave(SoundEffectVoice, &SoundEffectPressAB);
            } else if(VirtButtons[BT_B] == 1) {
                PlaceMenu(WindowMap, &StartMenu); 
                PlaceInventory(WindowMap, Inventory);
                PlayWave(SoundEffectVoice, &SoundEffectPressAB);
                GameState = GS_ITEM;
            }
            break;
        case GS_TOSS:
            /*
                    RemoveItem(Inventory); 
                    PlaceMenu(WindowMap, &StartMenu); 
                    PlaceInventory(WindowMap, Inventory);
                    GameState = GS_ITEM;
            */
            if(VirtButtons[BT_UP] == 1) {
                TossCount = PlaceTossCount(WindowMap, Inventory, TossCount + 1); 
            } else if(VirtButtons[BT_DOWN] == 1) {
                TossCount = PlaceTossCount(WindowMap, Inventory, TossCount - 1); 
            } 
            if(VirtButtons[BT_A] == 1) {
                ActiveText = (active_text) {
                    .Str = "Is it OK to toss\n[ITEM]?"
                };
                /*
                RemoveItem(Inventory, TossCount); 
                PlaceMenu(WindowMap, &StartMenu); 
                PlaceInventory(WindowMap, Inventory);
                GameState = GS_ITEM;
                */
                PlayWave(SoundEffectVoice, &SoundEffectPressAB);
                GameState = GS_TEXT;
                RestoreState = GS_CONFIRM_TOSS;
            } else if(VirtButtons[BT_B] == 1) {
                PlaceMenu(WindowMap, &StartMenu); 
                PlaceInventory(WindowMap, Inventory);
                PlayWave(SoundEffectVoice, &SoundEffectPressAB);
                GameState = GS_ITEM;
            }
            break;
        case GS_CONFIRM_TOSS:
            MoveMenuCursor(WindowMap, &ConfirmTossMenu, VirtButtons);
            if(VirtButtons[BT_A] == 1) {
                PlayWave(SoundEffectVoice, &SoundEffectPressAB);
            } else if(VirtButtons[BT_B] == 1) {
                GameState = GS_ITEM;
                PlayWave(SoundEffectVoice, &SoundEffectPressAB);
                PlaceMenu(WindowMap, &StartMenu); 
                PlaceInventory(WindowMap, Inventory);
                PlayWave(SoundEffectVoice, &SoundEffectPressAB);
            }
            break;
        case GS_RED_PC:
            MoveMenuCursor(WindowMap, &RedPCMenu, VirtButtons); 
            if(VirtButtons[BT_A] == 1) {
                switch(RedPCMenu.SelectI) {
                case 0: /*WITHDRAW ITEM*/ 
                    Inventory = &RedPC;
                    Inventory->ItemSelect = 0;
                    Inventory->Y = 0;
                    InventoryState = IS_WITHDRAW; 
                    GameState = PlaceInventory(WindowMap, Inventory);
                    RestoreState = GS_RED_PC;
                    PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
                    break;
                case 1: /*DEPOSIT ITEM*/
                    Inventory = &Bag;
                    Inventory->ItemSelect = 0;
                    Inventory->Y = 0;
                    InventoryState = IS_DEPOSIT; 
                    GameState = PlaceInventory(WindowMap, Inventory);
                    RestoreState = GS_RED_PC;
                    PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
                    break;
                case 2: /*TOSS ITEM*/
                    Inventory = &RedPC;
                    Inventory->ItemSelect = 0;
                    Inventory->Y = 0;
                    InventoryState = IS_TOSS; 
                    GameState = PlaceInventory(WindowMap, Inventory);
                    RestoreState = GS_RED_PC;
                    PlayWave(SoundEffectVoice, &SoundEffectPressAB); 
                    break;
                case 3: /*LOG OFF*/
                    GameState = GS_NORMAL;
                    memset(WindowMap, 0, sizeof(WindowMap));
                    PlayWave(SoundEffectVoice, &SoundEffectTurnOffPC);
                    break;
                }
            }
            if(VirtButtons[BT_B] == 1) {
                GameState = GS_NORMAL;
                memset(WindowMap, 0, sizeof(WindowMap));
                PlayWave(SoundEffectVoice, &SoundEffectTurnOffPC);
            }
            break;
        }

        if(GameState != GS_MAIN_MENU && GameState != GS_CONTINUE && GameState != GS_OPTIONS) {
            /*UpdatePlayer*/
            int CanPlayerMove = GameState == GS_NORMAL || 
                                GameState == GS_LEAPING || 
                                GameState == GS_TRANSITION;
            if(CanPlayerMove && World.Player.Tick > 0) {
                if(World.Player.Tick % 16 == 8) {
                    World.Player.IsRight ^= 1;
                }
                if(World.Player.IsMoving && World.Player.Tick % 2 == 0) {
                    switch(World.Player.Dir) {
                    case DIR_UP:
                        ScrollY -= World.Player.Speed;
                        break;
                    case DIR_LEFT:
                        ScrollX -= World.Player.Speed;
                        break;
                    case DIR_RIGHT:
                        ScrollX += World.Player.Speed;
                        break;
                    case DIR_DOWN:
                        ScrollY += World.Player.Speed;
                        break;
                    }
                }
                MoveEntity(&World.Player);
            }
            sprite *SpriteQuad = &Sprites[SpriteI];
            UpdateAnimation(&World.Player, SpriteQuad, (point) {72, 72});
            SpriteI += 4;

            /*UpdateLeaping*/
            if(GameState == GS_LEAPING) {
                /*PlayerUpdateJumpingAnimation*/
                uint8_t PlayerDeltas[] = {0, 4, 6, 8, 9, 10, 11, 12};
                uint8_t HalfTick = World.Player.Tick / 2;
                uint8_t DeltaI = HalfTick < 8 ? HalfTick: 15 - HalfTick;
                for(int I = 0; I < 4; I++) {
                    Sprites[I].Y -= PlayerDeltas[DeltaI];
                }

                /*CreateShadowQuad*/
                if(HalfTick) {
                    sprite *ShadowQuad  = &Sprites[SpriteI]; 
                    ShadowQuad[0] = (sprite) {72, 72, 255, 0};
                    ShadowQuad[1] = (sprite) {80, 72, 255, SPR_HORZ_FLAG};
                    ShadowQuad[2] = (sprite) {72, 80, 255, SPR_VERT_FLAG};
                    ShadowQuad[3] = (sprite) {80, 80, 255, SPR_HORZ_FLAG | SPR_VERT_FLAG};
                    SpriteI += 4;
                } else {
                    GameState = GS_NORMAL;
                }
            }
       
            /*UpdateObjects*/
            for(int MapI = 0; MapI < _countof(World.Maps); MapI++) {
                for(int ObjI = 0; ObjI < World.Maps[MapI].ObjectCount; ObjI++) {
                    object *Object = &World.Maps[MapI].Objects[ObjI];

                    /*TickFrame*/
                    if(GameState == GS_NORMAL) {
                        if(Object->Speed == 0) {
                            if(Object->Tick > 0) {
                                Object->Tick--; 
                            } else {
                                Object->Dir = Object->StartingDir;
                            }
                            if(Object->Tick % 16 == 8) {
                                Object->IsRight ^= 1;
                            }
                        } else if(Object->Tick > 0) {
                            MoveEntity(Object);
                        } else if(!IsPauseAttempt) {
                            RandomMove(&World, MapI, Object);
                        }
                    }

                    /*GetObjectRenderPos*/
                    point QuadPt = {
                        Object->Pos.X - World.Player.Pos.X + 72,
                        Object->Pos.Y - World.Player.Pos.Y + 72
                    };

                    if(World.MapI != MapI) {
                        switch(GetMapDir(World.Maps, World.MapI)) {
                        case DIR_UP:
                            QuadPt.Y -= World.Maps[MapI].Height * 16;
                            break;
                        case DIR_LEFT:
                            QuadPt.X -= World.Maps[MapI].Width * 16;
                            break;
                        case DIR_DOWN:
                            QuadPt.Y += World.Maps[World.MapI].Height * 16;
                            break;
                        case DIR_RIGHT:
                            QuadPt.X += World.Maps[World.MapI].Width * 16;
                            break;
                        }
                    }

                    /*RenderObject*/
                    if(ObjectInUpdateBounds(QuadPt) && SpriteI < 40) {
                        sprite *SpriteQuad = &Sprites[SpriteI];
                        UpdateAnimation(Object, SpriteQuad, QuadPt);
                        SpriteI += 4;
                    }
                }
            }

            /*MutTileUpdate*/
            int TickCycle = Tick / 32 % 9;
            if(Tick % 16 == 0 && World.IsOverworld) {
                /*FlowersUpdate*/
                memcpy(World.TileData[3], FlowerData[TickCycle % 3], TILE_SIZE);

                /*WaterUpdate*/
                int TickMod = TickCycle < 5 ? TickCycle : 9 - TickCycle;
                memcpy(World.TileData[20], WaterData[TickMod], TILE_SIZE);
            }

            /*UpdatePallete*/
            if(GameState != GS_TRANSITION && World.Player.Tick <= 0) {
                int PalleteI = World.Maps[World.MapI].PalleteI;
                memcpy(Bitmap.Info.Colors, Palletes[PalleteI], sizeof(Palletes[PalleteI]));
            }
        }

        /*Render*/
        RenderTileMap(Bitmap.Pixels, TileMap, World.TileData, ScrollX, ScrollY);
        RenderSprites(Bitmap.Pixels, Sprites, SpriteI, SpriteData);
        RenderWindowMap(Bitmap.Pixels, WindowMap, World.TileData);

        /*RefreshWindowSize*/
        if(IsFullscreen) {
            UpdateFullscreen(Window);
        }

        /*NextFrame*/
        SleepTillNextFrame(BeginCounter, FrameDelta, PerfFreq, Winmm.IsGranular);
        InvalidateRect(Window, NULL, 0);
        UpdateWindow(Window);
        Tick++;
    }
    EndCom(&Com); 
    EndXAudio2(&XAudio2);

    EndWinmm(&Winmm); 
    return 0;
}


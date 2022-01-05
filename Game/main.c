#define COBJMACROS

/*Includes*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>
#include <xaudio2.h>

/*Macro Constants*/
#define ANIM_PLAYER 0
#define ANIM_SEAL 128

#define BM_WIDTH 160
#define BM_HEIGHT 144

#define WORLD_WIDTH 1
#define WORLD_HEIGHT 4 

#define SPR_HORZ_FLAG 1
#define SPR_VERT_FLAG 2

#define QUAD_PROP_SOLID   1
#define QUAD_PROP_EDGE    2
#define QUAD_PROP_MESSAGE 4
#define QUAD_PROP_WATER   8
#define QUAD_PROP_DOOR   16
#define QUAD_PROP_EXIT   32
#define QUAD_PROP_TV     64
#define QUAD_PROP_SHELF 128

#define TILE_LENGTH 8
#define TILE_SIZE 64

#define LE_RIFF 'FFIR'
#define LE_WAVE 'EVAW'
#define LE_FMT 'tmf'
#define LE_DATA 'atad'

/*Typedefs*/
typedef uint8_t tile_map[32][32];
typedef HRESULT WINAPI xaudio2_create(IXAudio2** pxaudio2, UINT32 flags, XAUDIO2_PROCESSOR processor);
typedef MMRESULT WINAPI winmm_func (UINT);
typedef HRESULT WINAPI co_uninitialize(void);
typedef HRESULT WINAPI co_initialize_ex(LPVOID pvReserved, DWORD dwCoInit);

/*Enum*/
typedef enum dir {
    DIR_UP,
    DIR_LEFT,
    DIR_DOWN,
    DIR_RIGHT
} dir;

typedef enum menu_tile {
    MT_FULL_HORZ_ARROW = 174,
    MT_BLANK = 176,
    MT_EMPTY_HORZ_ARROW = 177,
    MT_TIMES = 178,
    MT_QUESTION = 179
} menu_tile;

typedef enum game_state {
    GS_NORMAL,
    GS_LEAPING,
    GS_TEXT,
    GS_TRANSITION,
    GS_START_MENU,

    /*SubMenu*/
    GS_ITEM,
    GS_SAVE,
    GS_OPTIONS,
} game_state; 

/*Structures*/
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

typedef struct sprite {
    uint8_t X;
    uint8_t Y;
    uint8_t Tile;
    uint8_t Flags;
} sprite;

typedef struct quad_info {
    point Point;
    int Map;
    int Quad;
    int Prop;
} quad_info;

typedef struct error { 
    const char *Func;
    const char *Error;
} error;

typedef struct data_path {
    const char *Tile;
    const char *Quad;
    const char *Prop;
} data_path; 

typedef struct world {
    uint8_t TileData[256][TILE_SIZE];
    uint8_t QuadProps[128];
    uint8_t QuadData[128][4];

    const char *MapPaths[WORLD_HEIGHT][WORLD_WIDTH];

    int DataPathI;
    data_path DataPaths[2];

    int MapI;
    map Maps[2];

    object Player;

    int IsOverworld;
} world;

typedef struct run_buffer {
    uint8_t Data[65536];
    int Index;
    int Size;
} run_buffer;

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

typedef struct bag {
    const char *ItemNames[256];
    item Items[20];
    int ItemCount;
    int ItemSelect;
    int Y;
} bag;

typedef struct proc {
    const char *Name;
    FARPROC Func;
} proc;

typedef struct multi_lib {
    size_t VersionCount; 
    const char **Versions;

    size_t ProcCount; 
    proc *Procs;
} multi_lib;
    
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

typedef struct com {
    HMODULE Library;
    co_initialize_ex *CoInitializeEx; 
    co_uninitialize *CoUninitialize;
} com;

typedef struct winmm {
    HMODULE Library;
    winmm_func *TimeBeginPeriod;
    winmm_func *TimeEndPeriod;
    int IsGranular;
} winmm;

typedef struct xaudio2 {
    HMODULE Library;
    xaudio2_create *Create;
    IXAudio2 *Engine;
    IXAudio2MasteringVoice *MasterVoice;
} xaudio2;

typedef struct active_text {
    const char *Str;
    point TilePt;
    uint64_t Tick;
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

/*Globals*/
static uint8_t g_BmPixels[BM_HEIGHT][BM_WIDTH];
static BITMAPINFO *g_BmInfo;

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

static int PosIntMod(int A, int B) {
    return A % B < 0 ? A % B + AbsInt(B) : A % B; 
}

static int ReverseDir(int Dir) {
    return (Dir + 2) % 4;
}

static int CorrectTileCoord(int TileCoord) {
    return PosIntMod(TileCoord, 32);
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

static int PointInMap(const map *Map, point Pt) {
    int InX = Pt.X >= 0 && Pt.X < Map->Width; 
    int InY = Pt.Y >= 0 && Pt.Y < Map->Height; 
    return InX && InY;
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

static int GetMapDir(const map Maps[static 2], int Map) {
    point DirPoint = SubPoints(Maps[!Map].Loaded, Maps[Map].Loaded);
    for(size_t I = 0; I < _countof(g_DirPoints); I++) {
        if(EqualPoints(DirPoint, g_DirPoints[I])) {
            return I;
        }
    }
    return -1; 
}

static quad_info GetQuad(const world *World, point Point) {
    quad_info QuadInfo = {.Map = World->MapI, .Point = Point};

    int OldWidth = World->Maps[QuadInfo.Map].Width;
    int OldHeight = World->Maps[QuadInfo.Map].Height;

    int NewWidth = World->Maps[!QuadInfo.Map].Width;
    int NewHeight = World->Maps[!QuadInfo.Map].Height;

    switch(GetMapDir(World->Maps, QuadInfo.Map)) {
    case DIR_UP:
        if(QuadInfo.Point.Y < 0) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y += NewHeight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DIR_LEFT:
        if(QuadInfo.Point.X < 0) {
            QuadInfo.Map ^= 1;
            QuadInfo.Point.X += NewHeight; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
        }
        break;
    case DIR_DOWN:
        if(QuadInfo.Point.Y >= World->Maps[QuadInfo.Map].Height) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y -= OldHeight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DIR_RIGHT:
        if(QuadInfo.Point.X >= World->Maps[QuadInfo.Map].Width) {
            QuadInfo.Point.X -= OldWidth; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
            QuadInfo.Map ^= 1;
        }
        break;
    }

    if(PointInMap(&World->Maps[QuadInfo.Map], QuadInfo.Point)) {
        QuadInfo.Quad = World->Maps[QuadInfo.Map].Quads[QuadInfo.Point.Y][QuadInfo.Point.X];
        QuadInfo.Prop = World->QuadProps[QuadInfo.Quad];
    } else {
        QuadInfo.Quad = World->Maps[World->MapI].DefaultQuad;
        QuadInfo.Prop = QUAD_PROP_SOLID;
    }

    return QuadInfo;
}

/*RunBuffer Functions*/
static uint8_t RunBufferGetByte(run_buffer *RunBuffer) {
    uint8_t Result = 0;
    if(RunBuffer->Index < RunBuffer->Size) {
        Result = RunBuffer->Data[RunBuffer->Index]; 
        RunBuffer->Index++;
    } 
    return Result;
}

static void RunBufferGetString(run_buffer *RunBuffer, char Str[256]) {
    uint8_t Length = RunBufferGetByte(RunBuffer); 
    int Left = RunBuffer->Size - RunBuffer->Index; 
    if(Left < Length) {
        Length = Left;
    }
    memcpy(Str, &RunBuffer->Data[RunBuffer->Index], Length);
    Str[Length] = '\0';
    RunBuffer->Index += Length;
}

/*Data Loader*/
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

static int ReadAll(const char *Path, void *Bytes, int ToRead) {
    int Result = 0;
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
        DWORD BytesRead;     
        ReadFile(FileHandle, Bytes, ToRead, &BytesRead, NULL);
        Result = BytesRead;
        CloseHandle(FileHandle);
    }
                 
    return Result;
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
    run_buffer RunBuffer = {0};
    RunBuffer.Size = ReadAll(Path, RunBuffer.Data, sizeof(RunBuffer.Data));

    /*ReadQuadSize*/
    map *Map = &World->Maps[MapI];
    Map->Width = RunBufferGetByte(&RunBuffer) + 1;
    Map->Height = RunBufferGetByte(&RunBuffer) + 1;
    int Size = Map->Width * Map->Height;

    /*ReadQuads*/
    int QuadIndex = 0;
    while(QuadIndex < Size) {
        int QuadRaw = RunBufferGetByte(&RunBuffer);
        int Quad = QuadRaw & 127;
        int Repeat = 0;

        if(Quad == QuadRaw) {
            Repeat = 1;
        } else { 
            Repeat = RunBufferGetByte(&RunBuffer) + 1;
        }

        Repeat = MinInt(Size - QuadIndex, Repeat);

        for(int I = QuadIndex; I < QuadIndex + Repeat; I++) {
            Map->Quads[I / Map->Width][I % Map->Width] = Quad; 
        }

        QuadIndex += Repeat;
    }

    /*DefaultQuad*/
    Map->DefaultQuad = RunBufferGetByte(&RunBuffer) & 127;

    /*ReadText*/
    Map->TextCount = RunBufferGetByte(&RunBuffer);
    for(int I = 0; I < Map->TextCount; I++) {
        Map->Texts[I].Pos.X = RunBufferGetByte(&RunBuffer);
        Map->Texts[I].Pos.Y = RunBufferGetByte(&RunBuffer);
        RunBufferGetString(&RunBuffer, Map->Texts[I].Str);
    }

    /*ReadObjects*/
    Map->ObjectCount = RunBufferGetByte(&RunBuffer);
    for(int I = 0; I < Map->ObjectCount; I++) {
        Map->Objects[I].Pos.X = RunBufferGetByte(&RunBuffer) * 16;
        Map->Objects[I].Pos.Y = RunBufferGetByte(&RunBuffer) * 16;
        Map->Objects[I].StartingDir = RunBufferGetByte(&RunBuffer) & 3;
        Map->Objects[I].Dir = Map->Objects[I].StartingDir;  
        Map->Objects[I].Speed = RunBufferGetByte(&RunBuffer) % 2;
        Map->Objects[I].Tile = RunBufferGetByte(&RunBuffer) & 0xF0;
        RunBufferGetString(&RunBuffer, Map->Objects[I].Str);
    }

    /*ReadDoors*/
    Map->DoorCount = RunBufferGetByte(&RunBuffer);
    for(int I = 0; I < Map->DoorCount; I++) {
        Map->Doors[I].Pos.X = RunBufferGetByte(&RunBuffer);
        Map->Doors[I].Pos.Y = RunBufferGetByte(&RunBuffer);
        Map->Doors[I].DstPos.X = RunBufferGetByte(&RunBuffer);
        Map->Doors[I].DstPos.Y = RunBufferGetByte(&RunBuffer);
        RunBufferGetString(&RunBuffer, Map->Doors[I].Path);
    }

    /*ReadTileSet*/
    int NewDataPathI = RunBufferGetByte(&RunBuffer) % _countof(World->DataPaths); 
    if(World->DataPathI != NewDataPathI) {
        World->DataPathI = NewDataPathI;

        ReadTileData(World->DataPaths[NewDataPathI].Tile, World->TileData, 96);
        ReadAll(World->DataPaths[NewDataPathI].Quad, World->QuadData, sizeof(World->QuadData)); 
        ReadAll(World->DataPaths[NewDataPathI].Prop, World->QuadProps, sizeof(World->QuadProps));
    }

    /*ReadPalleteI*/
    Map->PalleteI = RunBufferGetByte(&RunBuffer);
}

static void ReadOverworldMap(world *World, int MapI, point Load) {  
    const char *MapPath = World->MapPaths[Load.Y][Load.X]; 
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
            if(World->MapPaths[Y][X] && strcmp(World->MapPaths[Y][X], MapPath) == 0) {
                World->Maps[MapI].Loaded = (point) {X, Y};
                return 1;
            } 
        }
    }
    return 0;
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
        if(!(Prop & QUAD_PROP_SOLID) && !WillCollide) {
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
    return QuadPt.X > -16 && QuadPt.X < 1176 && QuadPt.Y > -16 && QuadPt.Y < 160;
}

/*World Functions*/
static int LoadAdjacentMap(world *World, int DeltaX, int DeltaY) {
    int Result = 0;
    point CurMapPt = World->Maps[World->MapI].Loaded;
    if(DeltaX || DeltaY) {
        point NewMapPt = {CurMapPt.X + DeltaX, CurMapPt.Y + DeltaY}; 
        if(PointInWorld(NewMapPt) && World->MapPaths[NewMapPt.X][NewMapPt.Y] &&
           !EqualPoints(World->Maps[!World->MapI].Loaded, NewMapPt)) {
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
            int Quad = GetQuad(World, (point) {QuadX, QuadY}).Quad;
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
    } else if(Output == 'È') {
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

static void PlaceMenu(tile_map TileMap, int MenuSelectI) {
    memset(TileMap, 0, sizeof(tile_map));
    PlaceTextBox(TileMap, (rect) {10, 0, 20, 14}); 
    PlaceText(
        TileMap, 
        (point) {12, 2}, 
        "POKÈMON\nITEM\nRED\nSAVE\nOPTION\nEXIT" 
    ); 
    TileMap[MenuSelectI * 2 + 2][11] = MT_FULL_HORZ_ARROW;
}

static void PlaceOptionCursor(tile_map TileMap, const option *Option, int Tile) {
    TileMap[Option->Y][Option->Xs[Option->I]] = Tile;
}

static void PlaceBag(tile_map TileMap, const bag *Bag) {
    PlaceTextBox(TileMap, (rect) {4, 2, 20, 13});
    int MinItem = Bag->ItemSelect - Bag->Y;
    for(int I = 0; I < 4; I++) {
        int ItemI = MinItem + I;
        int Y = I * 2 + 4;

        /*PlaceItem*/
        if(ItemI < Bag->ItemCount) {
            PlaceText(TileMap, (point) {6, Y}, Bag->ItemNames[Bag->Items[ItemI].ID]);
            PlaceTextF(TileMap, (point) {14, Y + 1}, "*%2d", Bag->Items[ItemI].Count);
        } else if(ItemI == Bag->ItemCount) {
            PlaceText(TileMap, (point) {6, Y}, "CANCEL");
        }
    }
    
    /*PlaceBagCursor*/
    TileMap[Bag->Y * 2 + 4][5] = MT_FULL_HORZ_ARROW;
}

static game_state RemoveStartMenu(tile_map TileMap) {
    memset(TileMap, 0, sizeof(tile_map));
    return GS_NORMAL; 
}

/*Library Functions*/
static HRESULT LoadProc(HMODULE *Library, const char *LibraryName, proc *Procs, int ProcCount) {
    HRESULT Result = S_OK;
    *Library = LoadLibrary(LibraryName);
    if(*Library) {
        for(int I = 0; I < ProcCount; I++) {
            Procs[I].Func = GetProcAddress(*Library, Procs[I].Name);
            if(!Procs[I].Func) {
                FreeLibrary(*Library);
                Result = HRESULT_FROM_WIN32(GetLastError());
                *Library = NULL;
                break;
            } 
        }
    } else {
        Result = HRESULT_FROM_WIN32(GetLastError());
    }
    return Result;
} 

static HRESULT LoadProcsVersioned(HMODULE *Library, multi_lib *LibData) {
    HRESULT Result = S_OK;
    for(int I = 0; I < LibData->VersionCount; I++) {
        Result = LoadProc(Library, LibData->Versions[I], LibData->Procs, LibData->ProcCount);
        if(SUCCEEDED(Result)) {
            break;
        }
    }
    return Result;
} 

static HRESULT StartWinmm(winmm *Winmm) {
    proc WinmmProcs[2] = {{"timeBeginPeriod"}, {"timeEndPeriod"}};
    HRESULT Result = LoadProc(&Winmm->Library, "winmm.dll", WinmmProcs, _countof(WinmmProcs)); 
    if(SUCCEEDED(Result)) {
        Winmm->TimeBeginPeriod = (winmm_func *) WinmmProcs[0].Func;
        Winmm->TimeEndPeriod = (winmm_func *) WinmmProcs[1].Func;
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
    proc ComProcs[] = {{"CoInitializeEx"}, {"CoUninitialize"}};
    HRESULT Result = LoadProc(&Com->Library, "ole32.dll", ComProcs, _countof(ComProcs));
    if(SUCCEEDED(Result)) {
        Com->CoInitializeEx = (co_initialize_ex *) ComProcs[0].Func;
        Com->CoUninitialize = (co_uninitialize *) ComProcs[1].Func;
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
        .Procs = (proc []) {{"XAudio2Create"}}
    };
    HRESULT Result = LoadProcsVersioned(&XAudio2->Library, &LibData);
    if(SUCCEEDED(Result)) {
        XAudio2->Create = (xaudio2_create *) LibData.Procs[0].Func;
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
            if(Header.idChunk == 0x46464952 &&
               Header.idFormat == 0x45564157 &&
               Header.idSubChunk == 0x20746D66 &&
               Header.wFormatTag == g_WaveFormat.wFormatTag &&
               Header.nChannels == g_WaveFormat.nChannels && 
               Header.nSamplesPerSec == g_WaveFormat.nSamplesPerSec &&
               Header.nBlockAlign == g_WaveFormat.nBlockAlign &&
               Header.nAvgBytesPerSec == g_WaveFormat.nAvgBytesPerSec &&
               Header.wBitsPerSample == g_WaveFormat.wBitsPerSample &&
               Header.idDataChunk == 0x61746164) {
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

static int IsVoiceEmpty(IXAudio2SourceVoice *Voice) {
    int Result = 1;
    if(Voice) {
        XAUDIO2_VOICE_STATE VoiceState;
        IXAudio2SourceVoice_GetState(Voice, &VoiceState, 0);
        Result = VoiceState.BuffersQueued == 0; 
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
static void SetMyWindowPos(HWND Window, DWORD Style, dim_rect Rect) {
    SetWindowLongPtr(Window, GWL_STYLE, Style | WS_VISIBLE);
    SetWindowPos(Window, 
                 HWND_TOP, 
                 Rect.X, Rect.Y, Rect.Width, Rect.Height, 
                 SWP_FRAMECHANGED | SWP_NOREPOSITION);
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
        {
            /*PaintFrame*/
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

            StretchDIBits(DeviceContext, RenderX, RenderY, RenderWidth, RenderHeight,
                          0, 0, BM_WIDTH, BM_HEIGHT, g_BmPixels, g_BmInfo, 
                          DIB_RGB_COLORS, SRCCOPY);
            PatBlt(DeviceContext, 0, 0, RenderX, ClientRect.Height, BLACKNESS);
            int RenderRight = RenderX + RenderWidth;
            PatBlt(DeviceContext, RenderRight, 0, RenderX, ClientRect.Height, BLACKNESS);
            PatBlt(DeviceContext, RenderX, 0, RenderWidth, RenderY, BLACKNESS);
            int RenderBottom = RenderY + RenderHeight;
            PatBlt(DeviceContext, RenderX, RenderBottom, RenderWidth, RenderY + 1, BLACKNESS);
            EndPaint(Window, &Paint);
        }
        return 0;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE Prev, LPSTR CmdLine, int CmdShow) {
    int TimeStart = time(NULL);
    srand(TimeStart);
    SetCurrentDirectory("../Shared");
    int Keys[256] = {0};

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
    g_BmInfo = _alloca(sizeof(BITMAPINFOHEADER) + 4 * sizeof(RGBQUAD));
    g_BmInfo->bmiHeader = (BITMAPINFOHEADER) {
        .biSize = sizeof(g_BmInfo->bmiHeader),
        .biWidth = BM_WIDTH,
        .biHeight = -BM_HEIGHT,
        .biPlanes = 1,
        .biBitCount = 8,
        .biCompression = BI_RGB,
        .biClrUsed = 4
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

    /*InitXAudio2*/
    HRESULT XAudio2Result = E_FAIL;
    com Com;
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

    /*WaveTest*/
    XAUDIO2_BUFFER CollisionBuffer = {0};
    XAUDIO2_BUFFER LedgeBuffer = {0}; 
    XAUDIO2_BUFFER StartMenuBuffer = {0}; 
    XAUDIO2_BUFFER PressBuffer = {0};
    XAUDIO2_BUFFER GoOutside = {0};
    XAUDIO2_BUFFER GoInside = {0}; 
    XAUDIO2_BUFFER PalleteTownMusic = {0};
    IXAudio2SourceVoice *SoundEffectVoice = NULL;
    IXAudio2SourceVoice *MusicVoice = NULL;
    if(SUCCEEDED(XAudio2Result)) {
        HRESULT SoundEffectResult = CreateGenericVoice(XAudio2.Engine, &SoundEffectVoice);
        if(SUCCEEDED(SoundEffectResult)) {
            ReadWave("SFX_COLLISION.wav", &CollisionBuffer);
            ReadWave("SFX_LEDGE.wav", &LedgeBuffer);
            ReadWave("SFX_START_MENU.wav", &StartMenuBuffer);
            ReadWave("SFX_PRESS_AB.wav", &PressBuffer);
            ReadWave("SFX_GO_OUTSIDE.wav", &GoOutside);
            ReadWave("SFX_GO_INSIDE.wav", &GoInside);
        }
 
        HRESULT MusicResult = CreateGenericVoice(XAudio2.Engine, &MusicVoice);
        if(SUCCEEDED(MusicResult)) {
            ReadWave("03_pallettown.wav", &PalleteTownMusic);
            PalleteTownMusic.LoopCount = XAUDIO2_LOOP_INFINITE;
            PlayWave(MusicVoice, &PalleteTownMusic); 
        }
    }

    /*InitWorld*/
    world World = {
        .MapPaths = {
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

    /*InitMaps*/
    ReadOverworldMap(&World, 0, (point) {0, 2});

    /*GameBoyGraphics*/
    sprite Sprites[40] = {0};

    tile_map TileMap;
    uint8_t ScrollX = 0;
    uint8_t ScrollY = 0;
    PlaceViewMap(&World, TileMap, 0);

    tile_map WindowMap = {0};

    /*InitGameState*/
    game_state GameState = GS_NORMAL;
    game_state RestoreState = GS_NORMAL;
    int TransitionTick = 0;
    point DoorPoint = {0};

    /*InitMenuData*/
    int IsPauseAttempt = 0;
    int MenuSelectI = 0;
    int OptionI = 0; 

    enum option_names {
        OPT_TEXT_SPEED,
        OPT_BATTLE_ANIMATION, 
        OPT_BATTLE_STYLE,
        OPT_CANCEL,
    };

    option Options[] = {
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
    };

    int IsSaveYes = 0;

    /*InitBag*/
    bag Bag = {0};
    
    /*InitNonGameState*/
    dim_rect RestoreRect = {0};
    int HasQuit = 0;
    int IsFullscreen = 0;

    /*Text*/
    const char *ActiveText = "ERROR: NO TEXT";
    point TextTilePt = {1, 14};
    uint64_t TextTick = 0;
    int BoxDelay = 0;

    /*Timing*/
    int64_t Tick = 0;
    int64_t BeginCounter = 0;
    int64_t PerfFreq = GetPerfFreq();
    int64_t FrameDelta = PerfFreq / 60;

    winmm Winmm; 
    StartWinmm(&Winmm); 

    int64_t StartCounter = GetPerfCounter(); 

    /*MainLoop*/
    ShowWindow(Window, CmdShow);
    while(!HasQuit) {
        BeginCounter = GetPerfCounter();

        /*ProcessMessage*/
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

        /*UpdateKeys*/
        if(GetActiveWindow() == Window) {
            for(int I = 0; I < _countof(Keys); I++) {
                if(!GetAsyncKeyState(I)) {
                    Keys[I] = 0;
                } else if(Keys[I] < INT_MAX) {
                    Keys[I]++;
                }
            }
        } else {
            memset(Keys, 0, sizeof(Keys));
        } 

        /*UpdateFullscreen*/
        if(Keys[VK_F11] == 1) {
            ShowCursor(IsFullscreen);
            if(IsFullscreen) {
                SetMyWindowPos(Window, WS_OVERLAPPEDWINDOW, RestoreRect);
            } else {
                RestoreRect = GetDimWindowRect(Window);
            }
            IsFullscreen ^= 1;
        }

        int SpriteI = 0;

        /*UpdatePlayer*/
        switch(GameState) {
        case GS_NORMAL:
            /*PlayerUpdate*/
            IsPauseAttempt |= (Keys[VK_RETURN] == 1);
            if(World.Player.Tick == 0) {
                if(IsPauseAttempt) {
                    if(IsVoiceEmpty(SoundEffectVoice)) {
                        IsPauseAttempt = 0;
                        PlayWave(SoundEffectVoice, &StartMenuBuffer); 
                        GameState = GS_START_MENU;
                        PlaceMenu(WindowMap, MenuSelectI);
                    }
                } else { 
                    int AttemptLeap = 0;

                    /*PlayerKeyboard*/
                    int HasMoveKey = 1;
                    if(Keys[VK_UP]) {
                        World.Player.Dir = DIR_UP;
                    } else if(Keys[VK_LEFT]) {
                        World.Player.Dir = DIR_LEFT;
                    } else if(Keys[VK_DOWN]) {
                        World.Player.Dir = DIR_DOWN;
                        AttemptLeap = 1;
                    } else if(Keys[VK_RIGHT]) {
                        World.Player.Dir = DIR_RIGHT;
                    } else {
                        HasMoveKey = 0;
                    }

                    point NewPoint = GetFacingPoint(World.Player.Pos, World.Player.Dir);

                    /*FetchQuadProp*/
                    quad_info OldQuadInfo = GetQuad(&World, PtToQuadPt(World.Player.Pos));
                    quad_info NewQuadInfo = GetQuad(&World, NewPoint);
                    NewPoint = NewQuadInfo.Point;

                    point ReverseDirPoint = g_DirPoints[ReverseDir(World.Player.Dir)];
                    point StartPos = AddPoints(NewPoint, ReverseDirPoint); 
                    StartPos.X *= 16;
                    StartPos.Y *= 16;

                    /*FetchObject*/
                    AttemptLeap &= !!(NewQuadInfo.Prop & QUAD_PROP_EDGE);
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
                            NewQuadInfo.Prop = QUAD_PROP_SOLID;
                            if(!AttemptLeap) {
                                NewQuadInfo.Prop |= QUAD_PROP_MESSAGE;
                            }
                            FacingObject = Object;
                            break;
                        }
                    }

                    /*PlayerTestProp*/
                    if(Keys['X'] == 1) {
                        int IsTV = NewQuadInfo.Prop & QUAD_PROP_TV;
                        int IsShelf = NewQuadInfo.Prop & QUAD_PROP_SHELF && 
                                      World.Player.Dir == DIR_UP;
                        if(NewQuadInfo.Prop & QUAD_PROP_MESSAGE || IsTV || IsShelf) {
                            GameState = GS_TEXT;
                            RestoreState = GS_NORMAL;

                            /*GetActiveText*/
                            if(IsShelf) {
                                ActiveText = "Crammed full of\nPOKÈMON books!"; 
                            } else if(IsTV && World.Player.Dir != DIR_UP) {
                               ActiveText = "Oops, wrong side."; 
                            } else if(FacingObject) {
                                ActiveText = FacingObject->Str;
                                if(FacingObject->Speed == 0) {
                                    FacingObject->Tick = 120;
                                }
                                FacingObject->Dir = ReverseDir(World.Player.Dir);
                            } else {
                                ActiveText = "ERROR: NO TEXT";
                                for(int I = 0; I < World.Maps[World.MapI].TextCount; I++) {
                                    text *Text = &(World.Maps[World.MapI].Texts[I]);
                                    if(EqualPoints(Text->Pos, NewPoint)) {
                                       ActiveText = Text->Str; 
                                       break;
                                    } 
                                } 
                            }

                            BoxDelay = 0;
                            TextTick = 16;

                            HasMoveKey = 0;
                        } else if(NewQuadInfo.Prop & QUAD_PROP_WATER) {
                            if(World.Player.Tile == ANIM_PLAYER) {
                                World.Player.Tile = ANIM_SEAL;
                                HasMoveKey = 1;
                            }
                        } else if(World.Player.Tile == ANIM_SEAL && 
                                  !(NewQuadInfo.Prop & QUAD_PROP_SOLID)) {
                            World.Player.Tile = ANIM_PLAYER;
                            HasMoveKey = 1;
                        }
                    }

                    if(World.Player.Tile == ANIM_SEAL) {
                        if(NewQuadInfo.Prop & QUAD_PROP_WATER) {
                            NewQuadInfo.Prop &= ~QUAD_PROP_SOLID;
                        } else {
                            NewQuadInfo.Prop |= QUAD_PROP_SOLID;
                        }
                    } else if(NewQuadInfo.Prop & QUAD_PROP_WATER) {
                        NewQuadInfo.Prop |= QUAD_PROP_SOLID;
                    }

                    if(HasMoveKey) {
                        GameState = GS_NORMAL;
                        /*MovePlayer*/
                        if(NewQuadInfo.Prop & QUAD_PROP_DOOR && 
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
                                  NewQuadInfo.Prop & QUAD_PROP_EDGE) {
                            World.Player.IsMoving = 1;
                            World.Player.Tick = 32;
                            GameState = GS_LEAPING;
                            PlayWave(SoundEffectVoice, &LedgeBuffer);
                        } else if(NewQuadInfo.Prop & QUAD_PROP_SOLID) {
                            World.Player.IsMoving = 0;
                            World.Player.Tick = 16;
                            if(World.Player.Dir == DIR_DOWN && OldQuadInfo.Prop & QUAD_PROP_EXIT) {
                                World.Player.IsMoving = 0;
                                TransitionTick = 32;
                                DoorPoint = OldQuadInfo.Point;
                                GameState = GS_TRANSITION;
                                PlayWave(SoundEffectVoice, &GoOutside);
                            } else { 
                                PlayWave(SoundEffectVoice, &CollisionBuffer);
                            }
                        } else {
                            World.Player.IsMoving = 1;
                            World.Player.Tick = 16;
                        }
                    } else {
                        World.Player.IsMoving = 0;
                    }

                    if(World.Player.IsMoving) {
                        World.Player.Pos = StartPos;
                        World.MapI = NewQuadInfo.Map;

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
            {
                IsPauseAttempt |= (Keys[VK_RETURN] == 1);
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
            break;
        case GS_TEXT:
            if(BoxDelay >= 0) {
                if(BoxDelay == 0) {
                    PlaceTextBox(WindowMap, (rect) {0, 12, 20, 18});
                    TextTilePt = (point) {1, 14};
                }
                BoxDelay--;
            } else if(ActiveText[0]) {
                switch(TextTilePt.Y) {
                case 17:
                    /*MoreTextDisplay*/
                    if(TextTick > 0) {
                        TextTick--;
                    } else switch(ActiveText[-1]) {
                    case '\n':
                        memcpy(&WindowMap[14][1], &WindowMap[15][1], 17);
                        memset(&WindowMap[13][1], MT_BLANK, 17);
                        memset(&WindowMap[15][1], MT_BLANK, 17);
                        TextTilePt.Y = 16;
                        break;
                    case '\f':
                        TextTilePt.Y = 14;
                        break;
                    }
                    break;
                case 18:
                    /*MoreTextWait*/
                    WindowMap[16][18] = TextTick / 30 % 2 ? MT_BLANK : 171;
                    if(Keys['X'] == 1) {
                        if(ActiveText[-1] == '\n') {
                            memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                            memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                        }
                        memset(&WindowMap[14][1], MT_BLANK, 17);
                        memset(&WindowMap[16][1], MT_BLANK, 18);
                        TextTilePt.Y = 17;
                        TextTick = 8;
                        PlayWave(SoundEffectVoice, &PressBuffer); 
                    } else {
                        TextTick++;
                    }
                    break;
                default:
                    /*UpdateTextForNextChar*/
                    if(TextTick > 0) {
                        TextTick--;
                    } else {
                        switch(ActiveText[0]) {
                        case '\n':
                            TextTilePt.X = 1;
                            TextTilePt.Y += 2;
                            break;
                        case '\f':
                            TextTilePt.X = 1;
                            TextTilePt.Y = 18;
                            break;
                        default:
                            WindowMap[TextTilePt.Y][TextTilePt.X] = CharToTile(*ActiveText);
                            TextTilePt.X++;
                        }
                        if(TextTilePt.Y != 18) {
                            /*UseTextSpeed*/
                            if(!Keys['X']) {
                                TextTick = (uint64_t[]){0, 2, 3}[Options[OPT_TEXT_SPEED].I];
                            }
                        }
                        ActiveText++;
                    }
                }
            } else {
                switch(RestoreState) {
                case GS_SAVE:
                    GameState = RestoreState;
                    PlaceTextBox(WindowMap, (rect) {0, 7, 6, 12});
                    PlaceText(WindowMap, (point) {2, 8}, "YES\nNO");
                    WindowMap[8][1] = MT_FULL_HORZ_ARROW; 
                    IsSaveYes = 1;
                    break;
                default:
                    if(Keys['X'] == 1) {
                        GameState = RestoreState;
                        memset(WindowMap, 0, sizeof(WindowMap));
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
                        g_BmInfo->bmiColors[0] = Palletes[World.Maps[World.MapI].PalleteI][1]; 
                        g_BmInfo->bmiColors[1] = Palletes[World.Maps[World.MapI].PalleteI][2]; 
                        g_BmInfo->bmiColors[2] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                        break;
                    case 16:
                        g_BmInfo->bmiColors[0] = Palletes[World.Maps[World.MapI].PalleteI][2]; 
                        g_BmInfo->bmiColors[1] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                        break;
                    case 8:
                        g_BmInfo->bmiColors[0] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                        break;
                    case 0:
                        {
                            /*LoadDoorMap*/
                            quad_info QuadInfo = GetQuad(&World, DoorPoint); 

                            map *OldMap = &World.Maps[QuadInfo.Map]; 
                            map *NewMap = &World.Maps[!QuadInfo.Map]; 

                            door *Door = NULL;
                            for(int I = 0; I < OldMap->DoorCount; I++) {
                                if(EqualPoints(OldMap->Doors[I].Pos, QuadInfo.Point)) {
                                    Door = &OldMap->Doors[I];
                                    break;
                                } 
                            }

                            if(Door) {
                                const char *NewPath = Door->Path;
                                ReadMap(&World, !QuadInfo.Map, NewPath);

                                if(!PointInMap(NewMap, Door->DstPos)) {
                                    Door->DstPos = (point) {0, 0};
                                }
                                World.MapI = !QuadInfo.Map;
                                World.Player.Pos = QuadPtToPt(Door->DstPos);
                                World.IsOverworld = GetLoadedPoint(&World, !QuadInfo.Map, NewPath);

                                /*CompleteTransition*/
                                ScrollX = 0;
                                ScrollY = 0;

                                memset(OldMap, 0, sizeof(*OldMap));
                                if(QuadInfo.Prop & QUAD_PROP_EXIT) {
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
            if(Keys[VK_RETURN] == 1 || Keys['Z'] == 1) {
                GameState = RemoveStartMenu(WindowMap);
                if(Keys['Z'] == 1) {
                    PlayWave(SoundEffectVoice, &PressBuffer); 
                }
            } else { 
                /*MoveMenuCursor*/
                if(Keys[VK_UP] == 1) {
                    WindowMap[MenuSelectI * 2 + 2][11] = MT_BLANK;
                    MenuSelectI = PosIntMod(MenuSelectI - 1, 6); 
                    WindowMap[MenuSelectI * 2 + 2][11] = MT_FULL_HORZ_ARROW;
                } else if(Keys[VK_DOWN] == 1) {
                    WindowMap[MenuSelectI * 2 + 2][11] = MT_BLANK;
                    MenuSelectI = PosIntMod(MenuSelectI + 1, 6); 
                    WindowMap[MenuSelectI * 2 + 2][11] = MT_FULL_HORZ_ARROW;
                }

                /*SelectMenuOption*/
                if(Keys['X'] == 1) {
                    switch(MenuSelectI) {
                    case 0: /*POKÈMON*/
                        break;
                    case 1: /*ITEM*/
                        GameState = GS_ITEM; 
                        PlaceBag(WindowMap, &Bag);
                        TextTick = 0;
                        break;
                    case 2: /*RED*/
                        break;
                    case 3: /*SAVE*/
                        {
                            int64_t MinutesElapsed = GetDeltaCounter(StartCounter) / PerfFreq / 60;
                            int64_t MinutesCorrected = MinInt(99 * 60 + 59, MinutesElapsed);
                            PlaceTextBox(WindowMap, (rect) {4, 0, 20, 10});
                            PlaceTextF(
                                WindowMap, 
                                (point) {5, 2},
                                "PLAYER %s\nBADGES       %d\nPOKÈDEX    %3d\nTIME     %2d:%02d",
                                "RED",
                                0,
                                0,
                                MinutesCorrected / 60,
                                MinutesCorrected % 60
                            );

                            TextTick = 4;
                            BoxDelay = 16; 
                            ActiveText = "Would you like to\nSAVE the game?";

                            GameState = GS_TEXT;
                            RestoreState = GS_SAVE;
                        }
                        break;
                    case 4: /*OPTION*/
                        GameState = GS_OPTIONS;
                        memset(WindowMap, MT_BLANK, sizeof(WindowMap));
                        PlaceTextBox(WindowMap, (rect) {0, 0, 20, 5});
                        PlaceTextBox(WindowMap, (rect) {0, 5, 20, 10});
                        PlaceTextBox(WindowMap, (rect) {0, 10, 20, 15});
                        PlaceText(
                            WindowMap, 
                            (point) {1, 1}, 
                            "TEXT SPEED\n FAST  MEDIUM SLOW\r" 
                            "BATTLE ANIMATION\n ON       OFF\r" 
                            "BATTLE STYLE\n SHIFT    SET\r"
                            " CANCEL"
                        ); 
                        OptionI = 0;
                        PlaceOptionCursor(WindowMap, &Options[0], MT_FULL_HORZ_ARROW); 
                        for(int I = 1; I < _countof(Options); I++) {
                            PlaceOptionCursor(WindowMap, &Options[I], MT_EMPTY_HORZ_ARROW);
                        }
                        break;
                    case 5: /*EXIT*/
                        GameState = RemoveStartMenu(WindowMap);
                        break;
                    }
                    PlayWave(SoundEffectVoice, &PressBuffer); 
                }
            }
            break;
        case GS_ITEM:
            if(Keys['Z'] == 1 || (Keys['X'] == 1 && Bag.ItemSelect == Bag.ItemCount)) {
                /*RemoveSubMenu*/
                GameState = GS_START_MENU;
                PlaceMenu(WindowMap, MenuSelectI);
                PlayWave(SoundEffectVoice, &PressBuffer); 
            } else {
                /*MoveTextCursor*/
                if(Keys['X'] == 1) {
                    PlayWave(SoundEffectVoice, &PressBuffer); 
                } else if(Keys[VK_UP] % 10 == 1 && Bag.ItemSelect > 0) {
                    Bag.ItemSelect--; 
                    if(Bag.Y > 0) {
                        Bag.Y--;
                    }
                    PlaceBag(WindowMap, &Bag); 
                } else if(Keys[VK_DOWN] % 10 == 1 && Bag.ItemSelect < Bag.ItemCount) {
                    Bag.ItemSelect++; 
                    if(Bag.Y < 2) {
                        Bag.Y++;
                    }
                    PlaceBag(WindowMap, &Bag); 
                } 

                /*MoreItemsWait*/
                if(Bag.ItemSelect - Bag.Y + 3 < Bag.ItemCount) {
                    WindowMap[11][18] = TextTick / 30 % 2 ? MT_BLANK : 171;
                    TextTick++;
                } else {
                    TextTick = 0;
                }
            }
            break;
        case GS_SAVE:
            if(Keys['X'] == 1 || Keys['Z'] == 1) {
                GameState = RemoveStartMenu(WindowMap);
                PlayWave(SoundEffectVoice, &PressBuffer); 
            } else if(Keys[VK_UP] == 1) {
                WindowMap[10][1] = MT_BLANK; 
                IsSaveYes = 1;
                WindowMap[8][1] = MT_FULL_HORZ_ARROW; 

            } else if(Keys[VK_DOWN] == 1) {
                WindowMap[8][1] = MT_BLANK; 
                IsSaveYes = 0;
                WindowMap[10][1] = MT_FULL_HORZ_ARROW; 
            }
            break;
        case GS_OPTIONS:
            if(Keys[VK_RETURN] == 1 || Keys['Z'] == 1 ||
               (Keys['X'] == 1 && OptionI == OPT_CANCEL)) {
                /*RemoveSubMenu*/
                GameState = GS_START_MENU;
                PlaceMenu(WindowMap, MenuSelectI);
                PlayWave(SoundEffectVoice, &PressBuffer); 
            } else {
                /*MoveOptionsCursor*/
                if(Keys[VK_UP] == 1) {
                    PlaceOptionCursor(WindowMap, &Options[OptionI], MT_EMPTY_HORZ_ARROW);
                    OptionI = PosIntMod(OptionI - 1, _countof(Options)); 
                    PlaceOptionCursor(WindowMap, &Options[OptionI], MT_FULL_HORZ_ARROW);
                } else if(Keys[VK_LEFT] == 1) {
                    switch(Options[OptionI].Count) {
                    case 2:
                        /*SwapOptionSelected*/
                        PlaceOptionCursor(WindowMap, &Options[OptionI], MT_BLANK);
                        Options[OptionI].I ^= 1;
                        PlaceOptionCursor(WindowMap, &Options[OptionI], MT_FULL_HORZ_ARROW);
                        break;
                    case 3:
                        /*NextOptionLeft*/
                        if(Options[OptionI].I > 0) {
                            PlaceOptionCursor(WindowMap, &Options[OptionI], MT_BLANK);
                            Options[OptionI].I--;
                            PlaceOptionCursor(WindowMap, &Options[OptionI], MT_FULL_HORZ_ARROW);
                        }
                        break;
                    }
                } else if(Keys[VK_DOWN] == 1) {
                    PlaceOptionCursor(WindowMap, &Options[OptionI], MT_EMPTY_HORZ_ARROW);
                    OptionI = PosIntMod(OptionI + 1, _countof(Options)); 
                    PlaceOptionCursor(WindowMap, &Options[OptionI], MT_FULL_HORZ_ARROW);
                } else if(Keys[VK_RIGHT] == 1) {
                    switch(Options[OptionI].Count) {
                    case 2:
                        /*SwapOptionSelected*/
                        PlaceOptionCursor(WindowMap, &Options[OptionI], MT_BLANK);
                        Options[OptionI].I ^= 1;
                        PlaceOptionCursor(WindowMap, &Options[OptionI], MT_FULL_HORZ_ARROW);
                        break;
                    case 3:
                        /*NextOptionRight*/
                        if(Options[OptionI].I < 2) {
                            PlaceOptionCursor(WindowMap, &Options[OptionI], MT_BLANK);
                            Options[OptionI].I++;
                            PlaceOptionCursor(WindowMap, &Options[OptionI], MT_FULL_HORZ_ARROW);
                        }
                        break;
                    }
                }
            }
            break;
        }

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
            memcpy(g_BmInfo->bmiColors, Palletes[PalleteI], sizeof(Palletes[PalleteI]));
        }

        /*RenderTiles*/
        uint8_t (*BmRow)[BM_WIDTH] = g_BmPixels;
        
        for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
            int PixelX = 8 - (ScrollX & 7);
            int SrcYDsp = ((PixelY + ScrollY) & 7) << 3;
            int TileX = ScrollX >> 3;
            int TileY = (PixelY + ScrollY) / 8 % 32;
            int StartOff = SrcYDsp | (ScrollX & 7);
            memcpy(*BmRow, &World.TileData[TileMap[TileY][TileX]][StartOff], 8);

            for(int Repeat = 1; Repeat < 20; Repeat++) {
                TileX = (TileX + 1) % 32;
                uint8_t *Pixel = *BmRow + PixelX;
                memcpy(Pixel, &World.TileData[TileMap[TileY][TileX]][SrcYDsp], 8);

                PixelX += 8;
            }
            TileX = (TileX + 1) % 32;
            uint8_t *Pixel = *BmRow + PixelX;
            int RemainX = BM_WIDTH - PixelX;
            memcpy(Pixel, &World.TileData[TileMap[TileY][TileX]][SrcYDsp], RemainX);
            BmRow++;
        }

        /*RenderSprites*/
        int MaxX = BM_WIDTH;
        int MaxY = BM_HEIGHT;

        for(int I = 0; I < SpriteI; I++) {
            int RowsToRender = 8;
            if(Sprites[I].Y < 8) {
                RowsToRender = Sprites[I].Y;
            } else if(Sprites[I].Y > MaxY) {
                RowsToRender = MaxInt(MaxY + 8 - Sprites[I].Y, 0);
            }

            int ColsToRender = 8;
            if(Sprites[I].X < 8) {
                ColsToRender = Sprites[I].X;
            } else if(Sprites[I].X > MaxX) {
                ColsToRender = MaxInt(MaxX + 8 - Sprites[I].X, 0);
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

            uint8_t *Src = &SpriteData[Sprites[I].Tile][SrcY * TILE_LENGTH + SrcX];

            for(int Y = 0; Y < RowsToRender; Y++) {
                uint8_t *Tile = Src;
                for(int X = 0; X < ColsToRender; X++) {
                    if(*Tile != 2) {
                        g_BmPixels[Y + DstY][X + DstX] = *Tile;
                    }
                    Tile += DispX;
                }
                Src += DispY;
            }
        }

        /*RenderWindowTiles*/
        for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
            int PixelX = 0;
            int SrcYDsp = (PixelY & 7) << 3;
            int TileY = PixelY / 8;

            for(int TileX = 0; TileX < 20; TileX++) {
                if(WindowMap[TileY][TileX]) {
                    uint8_t *Src = &World.TileData[WindowMap[TileY][TileX]][SrcYDsp];
                    memcpy(g_BmPixels[PixelY] + PixelX, Src, 8);
                }
                PixelX += 8;
            }
        }

        /*UpdateFullscreen*/
        if(IsFullscreen) {
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

        /*SleepTillNextFrame*/
        int64_t InitDeltaCounter = GetDeltaCounter(BeginCounter);
        if(InitDeltaCounter < FrameDelta) {
            if(Winmm.IsGranular) {
                int64_t RemainCounter = FrameDelta - InitDeltaCounter;
                uint32_t SleepMS = 1000 * RemainCounter / PerfFreq;
                if(SleepMS > 0) {
                    Sleep(SleepMS);
                }
            }
            while(GetDeltaCounter(BeginCounter) < FrameDelta);
        }

        /*NextFrame*/
        InvalidateRect(Window, NULL, 0);
        UpdateWindow(Window);
        Tick++;
    }
    EndCom(&Com); 
    EndXAudio2(&XAudio2);

    EndWinmm(&Winmm); 
    return 0;
}


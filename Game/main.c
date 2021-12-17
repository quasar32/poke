#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>

/*Typedefs*/
typedef MMRESULT WINAPI(winmm_func)(UINT);

/*Generic Structures*/
#define FC_VEC(Type, Size) struct {\
    int Count;\
    Type Data[Size];\
}

#define SELECT(Type, Size) struct {\
    int I;\
    Type Data[Size];\
}

/*Attribute Wrappers*/
#define NONNULL_PARAMS __attribute((nonnull))
#define UNUSED __attribute__((unused))
#define RET_ERROR __attribute__((warn_unused_result))
#define CLEANUP(Var, Func) Var __attribute__((cleanup(Func)))

/*Error Macros*/
#define TRY(Cond, Str, Val) do {\
    if(!(Cond)) {\
        LogError(__func__, (Str));\
        return (Val);\
    }\
} while(0)

/*Monad Macros*/
#define FOR_RANGE(Var, Ptr, BeginI, EndI) \
    for(typeof(*Ptr) *Var = (Ptr) + (BeginI), *End_ = Var + (EndI); Var < End_; Var++) 

#define FOR_VEC(Var, Vec) FOR_RANGE(Var, (Vec).Data, 0, (Vec).Count)
#define FOR_ARY(Var, Ary) FOR_RANGE(Var, (Ary), 0, _countof(Ary))

#define AUTO_VAR(Var, Val) typeof(Val) Var = (Val)
#define CUR(Select) ({\
    AUTO_VAR(Select_, (Select));\
    &Select_->Data[Select_->I];\
}) 
#define OTH(Select) ({\
    AUTO_VAR(Select_, (Select));\
    &Select_->Data[!Select_->I];\
}) 

#define READ_OBJECT(Path, Data) (ReadAll(Path, (Data), sizeof(Data)) == sizeof(Data))
#define COPY_OBJECT(Dst, Src) memcpy((Dst), (Src), sizeof(Src))

#define SEARCH_FROM_POS(Vec, InPos) ({\
    AUTO_VAR(Vec_, (Vec));\
    point Pos_ = (InPos);\
\
    typeof((Vec)->Data[0]) *Ret = NULL;\
    FOR_VEC(E, *Vec_) {\
        if(EqualPoints(E->Pos, Pos_)) {\
            Ret = E;\
            break;\
        }\
    }\
    Ret;\
})

/*Macro Constants*/
#define ANIM_PLAYER 0
#define ANIM_SEAL 112

#define BM_WIDTH 160
#define BM_HEIGHT 144

#define WORLD_WIDTH 1
#define WORLD_HEIGHT 4 

#define SPR_HORZ_FLAG 1
#define SPR_VERT_FLAG 2

#define DIR_UP 0
#define DIR_LEFT 1
#define DIR_DOWN 2
#define DIR_RIGHT 3

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

#define LPARAM_KEY_IS_HELD 0x30000000

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
    FC_VEC(char, 256) Path;
} door;

typedef struct text {
    point Pos;
    FC_VEC(char, 256) Str;
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

    FC_VEC(char, 256) Str;
} object;

typedef struct map {
    int16_t Width;
    int16_t Height;
    int16_t DefaultQuad;
    int16_t PalleteNum;
    uint8_t Quads[256][256];
    point Loaded;
    FC_VEC(text, 256) Texts;
    FC_VEC(object, 256) Objects;
    FC_VEC(door, 256) Doors;
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
    SELECT(data_path, 2) DataPaths;

    uint8_t TileData[256 * TILE_SIZE];
    uint8_t QuadProps[128];
    uint8_t QuadData[128][4];

    SELECT(map, 2) Maps;
    object Player;
    const char *MapPaths[WORLD_HEIGHT][WORLD_WIDTH];

    int IsOverworld;
} world;

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

/*Globals*/
static uint8_t g_Keys[256] = {};
static uint8_t g_BmPixels[BM_HEIGHT][BM_WIDTH];
static BITMAPINFO *g_BmInfo = NULL;
static winmm_func *g_TimeEndPeriod = NULL;
static FC_VEC(error, 32) g_Errors;

/*Math Functions*/
static int MinInt(int A, int B) {
    return A < B ? A : B;
}

static int MaxInt(int A, int B) {
    return A > B ? A : B;
}

static int ClampInt(int Value, int Bottom, int Top) {
    return MinInt(MaxInt(Value, Bottom), Top);
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
static void SetToTiles(uint8_t TileMap[32][32], int TileX, int TileY, const uint8_t Set[4]) {
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
    quad_info QuadInfo = {.Map = World->Maps.I, .Point = Point};

    int OldWidth = World->Maps.Data[QuadInfo.Map].Width;
    int OldHeight = World->Maps.Data[QuadInfo.Map].Height;

    int NewWidth = World->Maps.Data[!QuadInfo.Map].Width;
    int NewHeight = World->Maps.Data[!QuadInfo.Map].Height;

    switch(GetMapDir(World->Maps.Data, QuadInfo.Map)) {
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
        if(QuadInfo.Point.Y >= World->Maps.Data[QuadInfo.Map].Height) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y -= OldHeight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DIR_RIGHT:
        if(QuadInfo.Point.X >= World->Maps.Data[QuadInfo.Map].Width) {
            QuadInfo.Point.X -= OldWidth; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
            QuadInfo.Map ^= 1;
        }
        break;
    }

    if(PointInMap(&World->Maps.Data[QuadInfo.Map], QuadInfo.Point)) {
        QuadInfo.Quad = World->Maps.Data[QuadInfo.Map].Quads[QuadInfo.Point.Y][QuadInfo.Point.X];
        QuadInfo.Prop = World->QuadProps[QuadInfo.Quad];
    } else {
        QuadInfo.Quad = World->Maps.Data[World->Maps.I].DefaultQuad;
        QuadInfo.Prop = QUAD_PROP_SOLID;
    }

    return QuadInfo;
}

/*Error*/
NONNULL_PARAMS
static void LogError(const char *Func, const char *Error) {
    error *CurErr = &g_Errors.Data[g_Errors.Count];
    if(g_Errors.Count < _countof(g_Errors.Data)) {
        g_Errors.Count++;
    } else if(g_Errors.Count == _countof(g_Errors.Data)) { 
        Func = __func__; 
        Error = "Out of memory"; 
        CurErr--;
    }
    CurErr->Func = Func;
    CurErr->Error = Error;
}

static void DisplayAllErrors(HWND Window) {
    if(g_Errors.Count != 0) {
        char Buf[65536] = "";
        FOR_VEC(Error, g_Errors) {
            strcat_s(Buf, _countof(Buf), Error->Func);
            strcat_s(Buf, _countof(Buf), ": ");
            strcat_s(Buf, _countof(Buf), Error->Error);
            strcat_s(Buf, _countof(Buf), "\n");
        }
        MessageBox(NULL, Buf, "Error", MB_OK);
        g_Errors.Count = 0;
    }
}

/*String*/
NONNULL_PARAMS
static void CopyCharsWithNull(char *Dest, const char *Source, size_t Length) {
    memcpy(Dest, Source, Length);
    Dest[Length] = '\0';
}

/*Cleanup Functions*/
static void CleanUpTime(int *IsGranular) {
    if(*IsGranular) {
        g_TimeEndPeriod(1);
        *IsGranular = 0;
    }
}

static void CleanUpWindow(HWND *Window) {
    DisplayAllErrors(*Window); 
    if(Window) {
        DestroyWindow(*Window);
        *Window = NULL;
    }
}

static void CleanUpHandle(HANDLE *Handle) {
    if(*Handle != INVALID_HANDLE_VALUE) {
        CloseHandle(*Handle);
        *Handle = INVALID_HANDLE_VALUE;
    }
} 


/*Data loaders*/
static int64_t GetFullFileSize(HANDLE FileHandle) {
    LARGE_INTEGER FileSize;
    int Success = GetFileSizeEx(FileHandle, &FileSize);
    TRY(Success, "Could not obtain file size", -1);
    return FileSize.QuadPart;
}

NONNULL_PARAMS RET_ERROR
static int ReadAll(const char *Path, void *Bytes, int MaxBytesToRead) {
    DWORD BytesRead = 0;
    CLEANUP(HANDLE, CleanUpHandle) FileHandle = CreateFile(
        Path, 
        GENERIC_READ, 
        FILE_SHARE_READ, 
        NULL, 
        OPEN_EXISTING, 
        0, 
        NULL
    );
    TRY(FileHandle != INVALID_HANDLE_VALUE, "File could not be found", -1); 
    int64_t FileSize = GetFullFileSize(FileHandle);
    TRY(FileSize >= 0, "File size could not be obtained", -1);
    TRY(FileSize <= MaxBytesToRead, "File is too big for buffer", -1);
        
    TRY(ReadFile(FileHandle, Bytes, FileSize, &BytesRead, NULL), "Could not read file", -1); 
    TRY(BytesRead == FileSize, "Could not read all of file", -1);

    return BytesRead;
}

NONNULL_PARAMS RET_ERROR
static int ReadTileData(const char *Path, uint8_t *TileData, int MaxTileCount) {
    const int SizeOfCompressedTile = 16;
    int SizeOfCompressedTileData = MaxTileCount * SizeOfCompressedTile;

    uint8_t CompData[SizeOfCompressedTileData];
    int BytesRead = ReadAll(Path, CompData, sizeof(CompData));
    int TilesRead = BytesRead / SizeOfCompressedTile;
    int BytesValid = TilesRead * SizeOfCompressedTile;

    uint8_t *CompPtr = CompData;
    uint8_t *TilePtr = TileData;
    for(int ByteIndex = 0; ByteIndex < BytesValid; ByteIndex++) {
        uint8_t Comp = *CompPtr++;
        *TilePtr++ = (Comp >> 6) & 3;
        *TilePtr++ = (Comp >> 4) & 3;
        *TilePtr++ = (Comp >> 2) & 3;
        *TilePtr++ = (Comp >> 0) & 3;
    }
    return TilesRead;
}

NONNULL_PARAMS RET_ERROR
static int ReadMap(world *World, int MapI, const char *Path) {
    #define NEXT_RUN ({\
        TRY(RunIndex < BytesRead, "File is truncated", 0);\
        RunData[RunIndex++];\
    })

    #define MOVE_RUN(Count) ({\
        TRY(RunIndex + Count <= BytesRead, "File is truncated", 0);\
        void *Ret = &RunData[RunIndex];\
        RunIndex += (Count);\
        Ret;\
    })

    #define NEXT_STR(Str) do {\
        size_t Length = NEXT_RUN;\
        Str.Count = Length + 1;\
        CopyCharsWithNull(Str.Data, MOVE_RUN(Length), Length);\
    } while(0)

    uint8_t RunData[65536];
    int RunIndex = 0;
    int BytesRead = ReadAll(Path, RunData, sizeof(RunData));
    TRY(BytesRead >= 0, "Could not read map", 0);

    /*ReadQuadSize*/
    map *Map = &World->Maps.Data[MapI];
    Map->Width = NEXT_RUN + 1;
    Map->Height = NEXT_RUN + 1;
    int Size = Map->Width * Map->Height;

    /*ReadQuads*/
    int QuadIndex = 0;
    while(QuadIndex < Size) {
        int QuadRaw = NEXT_RUN;
        int Quad = QuadRaw & 127;
        int Repeat = 0;

        if(Quad == QuadRaw) {
            Repeat = 1;
        } else { 
            Repeat = NEXT_RUN + 1;
        }

        TRY(QuadIndex + Repeat <= Size, "Quads are truncated", 0);

        for(int I = QuadIndex; I < QuadIndex + Repeat; I++) {
            Map->Quads[I / Map->Width][I % Map->Width] = Quad; 
        }

        QuadIndex += Repeat;
    }
    TRY(QuadIndex >= Size, "Quads are incomplete", 0);

    /*DefaultQuad*/
    Map->DefaultQuad = NEXT_RUN;
    TRY(Map->DefaultQuad < 128, "Out of bounds default quad", 0);

    /*ReadText*/
    Map->Texts.Count = NEXT_RUN;
    FOR_VEC(Text, Map->Texts) {
        Text->Pos.X = NEXT_RUN;
        Text->Pos.Y = NEXT_RUN;
        NEXT_STR(Text->Str);
    }

    /*ReadObjects*/
    Map->Objects.Count = NEXT_RUN;
    FOR_VEC(Object, Map->Objects) {
        Object->Pos.X = NEXT_RUN * 16;
        Object->Pos.Y = NEXT_RUN * 16;
        Object->StartingDir = NEXT_RUN;
        Object->Dir = Object->StartingDir;  
        Object->Speed = NEXT_RUN;
        Object->Tile = NEXT_RUN;
        NEXT_STR(Object->Str);
    }

    /*ReadDoors*/
    Map->Doors.Count = NEXT_RUN;
    FOR_VEC(Door, Map->Doors) {
        Door->Pos.X = NEXT_RUN;
        Door->Pos.Y = NEXT_RUN;
        Door->DstPos.X = NEXT_RUN;
        Door->DstPos.Y = NEXT_RUN;
        NEXT_STR(Door->Path);
    }

    /*ReadTileSet*/
    int NewDataPath = NEXT_RUN; 
    if(World->DataPaths.I != NewDataPath) {
        World->DataPaths.I = NewDataPath;
        data_path *DataPath = CUR(&World->DataPaths);

        TRY(ReadTileData(DataPath->Tile, World->TileData, 96), "Could not load tile data", 0);

        /*ReadQuadData*/
        TRY(READ_OBJECT(DataPath->Quad, World->QuadData), "Could not load quad data", 0); 
        FOR_ARY(Set, World->QuadData) {
            FOR_ARY(Quad, *Set) {
                *Quad = ClampInt(*Quad, 0, 95);
            }
        }
        
        /*ReadQuadProps*/
        TRY(READ_OBJECT(DataPath->Prop, World->QuadProps), "Could not load quad props", 0);
    }

    /*ReadPalleteNum*/
    Map->PalleteNum = NEXT_RUN;

    /*ExtraneousDataCheck*/
    TRY(RunIndex >= BytesRead, "Extraneous data present", 0);
    
    /*Out*/
    return 1;
#undef NEXT_STR
#undef MOVE_RUN
#undef NEXT_RUN
}

NONNULL_PARAMS RET_ERROR
static int ReadOverworldMap(world *World, int MapI, point Load) {  
    if(PointInWorld(Load) && World->MapPaths[Load.Y][Load.X]) {
        const char *CurMapPath = World->MapPaths[Load.Y][Load.X];
        TRY(ReadMap(World, MapI, CurMapPath), "ReadMap failed", 0);
        World->IsOverworld = 1;
        World->Maps.Data[MapI].Loaded = Load; 
    }
    return 1; 
}

NONNULL_PARAMS 
static int GetLoadedPoint(world *World, int MapI, const char *MapPath) {
    for(int Y = 0; Y < WORLD_HEIGHT; Y++) {
        for(int X = 0; X < WORLD_WIDTH; X++) {
            if(World->MapPaths[Y][X] && strcmp(World->MapPaths[Y][X], MapPath) == 0) {
                World->Maps.Data[MapI].Loaded = (point) {X, Y};
                return 1;
            } 
        }
    }
    return 0;
}

/*Object Functions*/
static void MoveEntity(object *Object) {
    if(Object->IsMoving) {
        point DeltaPoint = g_DirPoints[Object->Dir];
        DeltaPoint = MulPoint(DeltaPoint, Object->Speed);
        Object->Pos = AddPoints(Object->Pos, DeltaPoint);
    }
    Object->Tick--;
}

static void RandomMove(const world *World, int MapI, object *Object) {
    const map *Map = &World->Maps.Data[MapI];
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
        
        /*IfWillColideWithPlayer*/
        point PlayerCurPoint = PtToQuadPt(World->Player.Pos);

        if(EqualPoints(PlayerCurPoint, NewPoint)) {
            Prop = QUAD_PROP_SOLID;
        } else if(World->Player.IsMoving) {
            point NextPoint = g_NextPoints[World->Player.Dir];
            point PlayerNextPoint = AddPoints(PlayerCurPoint, NextPoint);
            if(EqualPoints(PlayerNextPoint, NewPoint)) {
                Prop = QUAD_PROP_SOLID;
            }
        }

        /*StartMovingEntity*/
        if(!(Prop & QUAD_PROP_SOLID)) {
            Object->Tick = 16;
            Object->IsMoving = 1;
        }
    }
}

static void UpdateAnimation(object *Object, sprite *SpriteQuad, point QuadPt) {
    uint8_t QuadX = QuadPt.X;
    uint8_t QuadY = QuadPt.Y;
    if(Object->Tick % 8 < 4 || Object->Speed == 0) {
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
        /*ChangeFootAnimation*/
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
            SpriteQuad[0] = (sprite) {0, 1, 4, 0};
            SpriteQuad[1] = (sprite) {8, 1, 5, 0};
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
            SpriteQuad[0] = (sprite) {0, 1, 5, SPR_HORZ_FLAG};
            SpriteQuad[1] = (sprite) {8, 1, 4, SPR_HORZ_FLAG};
            SpriteQuad[2] = (sprite) {0, 8, 9, SPR_HORZ_FLAG};
            SpriteQuad[3] = (sprite) {8, 8, 8, SPR_HORZ_FLAG};
            break;
        }
        if(Object->Tick % 8 == 4) {
            Object->IsRight ^= 1;
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
NONNULL_PARAMS RET_ERROR
static int LoadNextMap(world *World, int DeltaX, int DeltaY) {
    point CurMapPt = CUR(&World->Maps)->Loaded;
    if(DeltaX) {
        point NewMapPtX = {CurMapPt.X + DeltaX, CurMapPt.Y}; 
        if(PointInWorld(NewMapPtX) && World->MapPaths[NewMapPtX.X][NewMapPtX.Y]) {
            TRY(ReadOverworldMap(World, !World->Maps.I, NewMapPtX), "Could not load map", 0);
            return 1;
        }
    }
    if(DeltaY) {
        point NewMapPtY = {CurMapPt.X, CurMapPt.Y + DeltaY}; 
        if(PointInWorld(NewMapPtY) && World->MapPaths[NewMapPtY.X][NewMapPtY.Y]) {
            TRY(ReadOverworldMap(World, !World->Maps.I, NewMapPtY), "Could not load map", 0);
            return 1;
        }
    }
    return 1;
}

NONNULL_PARAMS
static void PlaceMap(const world *World, uint8_t TileMap[32][32], point TileMin, rect QuadRect) {
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

NONNULL_PARAMS
static void PlaceViewMap(world *World, uint8_t TileMap[32][32], int IsDown) {
    rect QuadRect = {
        .Left = World->Player.Pos.X / 16 - 4,
        .Top = World->Player.Pos.Y / 16 - 4,
        .Right = QuadRect.Left + 10,
        .Bottom = QuadRect.Top + 9 + !!IsDown
    };
    point TileMin = {};
    PlaceMap(World, TileMap, TileMin, QuadRect);
}

/*Tile Functions*/
NONNULL_PARAMS
static void PlaceTextBox(uint8_t TileMap[32][32], rect Rect) {
    /*HeadRow*/
    TileMap[Rect.Top][Rect.Left] = 96;
    memset(&TileMap[Rect.Top][Rect.Left + 1], 97, Rect.Right - Rect.Left + 2);
    TileMap[Rect.Top][Rect.Right - 1] = 98;

    /*BodyRows*/
    for(int Y = Rect.Top + 1; Y < Rect.Bottom - 1; Y++) {
        TileMap[Y][Rect.Left] = 99;
        memset(&TileMap[Y][Rect.Left + 1], 176, Rect.Right - Rect.Left + 2);
        TileMap[Y][Rect.Right - 1] = 101;
    }

    /*FootRow*/
    TileMap[Rect.Bottom - 1][Rect.Left] = 100;
    memset(&TileMap[Rect.Bottom - 1][Rect.Left + 1], 97, Rect.Right - Rect.Left + 2);
    TileMap[Rect.Bottom - 1][Rect.Right - 1] = 102;
}

static int CharToTile(int Char) {
    int Output = Char;
    if(Output == '.') {
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
    } else {
        Output = 176;
    }
    return Output;
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
    case WM_KILLFOCUS:
        memset(g_Keys, 0, sizeof(g_Keys));
        return 0;
    case WM_CLOSE:
        DestroyWindow(Window);
        return 0;
    case WM_PAINT:
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
                      0, 0, BM_WIDTH, BM_HEIGHT, g_BmPixels, g_BmInfo, DIB_RGB_COLORS, SRCCOPY);
        PatBlt(DeviceContext, 0, 0, RenderX, ClientRect.Height, BLACKNESS);
        int RenderRight = RenderX + RenderWidth;
        PatBlt(DeviceContext, RenderRight, 0, RenderX, ClientRect.Height, BLACKNESS);
        PatBlt(DeviceContext, RenderX, 0, RenderWidth, RenderY, BLACKNESS);
        int RenderBottom = RenderY + RenderHeight;
        PatBlt(DeviceContext, RenderX, RenderBottom, RenderWidth, RenderY + 1, BLACKNESS);
        EndPaint(Window, &Paint);
        return 0;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

int WINAPI WinMain(HINSTANCE Instance, UNUSED HINSTANCE Prev, UNUSED LPSTR CmdLine, int CmdShow) {
    srand(time(NULL));

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
    g_BmInfo = alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 4);
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
    TRY(RegisterClass(&WindowClass), "Could not register class", 1);

    RECT WinWindowRect = {0, 0, BM_WIDTH, BM_HEIGHT};
    AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, 0);

    dim_rect WindowRect = WinToDimRect(WinWindowRect);

    CLEANUP(HWND, CleanUpWindow) Window = CreateWindow(
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
    TRY(Window, "Could not create window", 1);

    TRY(SetCurrentDirectory("../Shared"), "Could not find shared directory", 1);

    /*InitWorld*/
    world World = {
        .Player = {
            .Pos = {80, 96},
            .Dir = DIR_DOWN,
            .Speed = 2,
            .Tile = 0,
        },
        .MapPaths = {
            {"VirdianCity"},
            {"Route1"},
            {"PalleteTown"},
            {"Route21"}
        },
        .DataPaths = {
            .I = -1,
            .Data = {
                {
                    .Tile = "TileData00",
                    .Quad = "QuadData00",
                    .Prop = "QuadProps00"
                }, 
                {
                    .Tile = "TileData01",
                    .Quad = "QuadData01",
                    .Prop = "QuadProps01",
                }
            },
        }
    };

    /*LoadTileData*/
    uint8_t SpriteData[256 * TILE_SIZE];
    uint8_t FlowerData[3 * TILE_SIZE];
    uint8_t WaterData[7 * TILE_SIZE];

#define LOAD_TILE_DATA(Name, Data, Offset, Index) \
    TRY(\
        ReadTileData(Name, (Data) + (Offset) * TILE_SIZE, (Index)),\
        "Could not load tile data: " Name,\
        1\
    ) 

    LOAD_TILE_DATA("Menu", World.TileData, 96, 152);
    LOAD_TILE_DATA("SpriteData", SpriteData, 0, 256);
    LOAD_TILE_DATA("FlowerData", FlowerData, 0, 3);
    LOAD_TILE_DATA("WaterData", WaterData, 0, 7);
    LOAD_TILE_DATA("ShadowData", SpriteData, 255, 1);

    /*InitMaps*/
    TRY(ReadOverworldMap(&World, 0, (point) {0, 2}), "Could not load starting map", 1);

    /*GameBoyGraphics*/
    sprite Sprites[40] = {};

    uint8_t TileMap[32][32];

    uint8_t ScrollX = 0;
    uint8_t ScrollY = 0;

    /*TranslateFullMapToTiles*/
    PlaceViewMap(&World, TileMap, FALSE);

    /*WindowMap*/
    uint8_t WindowMap[32][32];

    /*PlayerState*/
    int IsLeaping = 0;
    int IsTransition = 0; 
    int IsMenuUp = 0;
    int TransitionTick = 0;
    point DoorPoint;
    
    /*NonGameState*/
    dim_rect RestoreRect = {};
    int HasQuit = 0;
    int IsFullscreen = 0;

    /*Text*/
    const char *ActiveText = NULL;
    point TextTilePt = {1, 14};
    uint64_t TextTick = 0;

    /*Timing*/
    int64_t Tick = 0;
    int64_t BeginCounter = 0;
    int64_t PerfFreq = GetPerfFreq();
    int64_t FrameDelta = PerfFreq / 30;

    /*LoadWinmm*/
    CLEANUP(int, CleanUpTime) IsGranular = 0;

    HMODULE WinmmLib = LoadLibrary("winmm.dll");
    if(WinmmLib) {
        winmm_func *TimeBeginPeriod = (winmm_func *) GetProcAddress(WinmmLib, "timeBeginPeriod"); 
        if(TimeBeginPeriod) {
            g_TimeEndPeriod = (winmm_func *) GetProcAddress(WinmmLib, "timeEndPeriod"); 
            if(g_TimeEndPeriod) {
                IsGranular = TimeBeginPeriod(1) == TIMERR_NOERROR;
            }
        }
        if(!IsGranular) {
            FreeLibrary(WinmmLib);
        }
    }

    /*MainLoop*/
    ShowWindow(Window, CmdShow);
    while(!HasQuit) {
        BeginCounter = GetPerfCounter();

        /*UpdateKeyFrameCount*/
        FOR_ARY(Key, g_Keys) {
            if(*Key > 0 && *Key < 255) {
                *Key += 1;
            }
        }

        /*ProcessMessage*/
        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
            switch(Message.message) {
            case WM_QUIT:
                HasQuit = 1;
                break;
            case WM_KEYDOWN:
                switch(Message.wParam) {
                case VK_F11:
                    /*ToggleFullscreen*/
                    ShowCursor(IsFullscreen);
                    if(IsFullscreen) {
                        SetMyWindowPos(Window, WS_OVERLAPPEDWINDOW, RestoreRect);
                    } else {
                        RestoreRect = GetDimWindowRect(Window);
                    }
                    IsFullscreen ^= 1;
                    break;
                default:
                    if(!(Message.lParam & LPARAM_KEY_IS_HELD) && g_Keys[Message.wParam] == 0) {
                        g_Keys[Message.wParam] = 1;
                    }
                }
                break;
            case WM_KEYUP:
                g_Keys[Message.wParam] = 0;
                break;
            default:
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            }
        }

        /*UpdatePlayer*/
        if(ActiveText) {
            if(ActiveText[0]) {
                switch(TextTilePt.Y) {
                case 17:
                    /*MoreTextDisplay*/
                    if(TextTick > 0) {
                        TextTick--;
                    } else switch(ActiveText[-1]) {
                    case '\n':
                        memcpy(&WindowMap[14][1], &WindowMap[15][1], 17);
                        memset(&WindowMap[13][1], 176, 17);
                        memset(&WindowMap[15][1], 176, 17);
                        TextTilePt.Y = 16;
                        break;
                    case '\f':
                        TextTilePt.Y = 14;
                        break;
                    }
                    break;
                case 18:
                    /*MoreTextWait*/
                    WindowMap[16][18] = TextTick / 16 % 2 ? 176 : 171;
                    if(g_Keys['X'] == 1) {
                        if(ActiveText[-1] == '\n') {
                            memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                            memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                        }
                        memset(&WindowMap[14][1], 176, 17);
                        memset(&WindowMap[16][1], 176, 18);
                        TextTilePt.Y = 17;
                        TextTick = 4;
                    } else {
                        TextTick++;
                    }
                    break;
                default:
                    /*UpdateTextForNextChar*/
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
                    ActiveText++;
                }
            } else if(g_Keys['X'] == 1) {
                ActiveText = NULL;
                memset(WindowMap, 0, sizeof(WindowMap));
            }
        } else if(IsTransition) {
            /*EnterTransition*/
            if(World.Player.IsMoving) {
                if(World.Player.Tick <= 0) {
                    World.Player.IsMoving = 0;
                    TransitionTick = 16;
                }
            }

            /*ProgressTransition*/
            if(!World.Player.IsMoving) {
                if(TransitionTick-- > 0) {
                    switch(TransitionTick) {
                    case 12:
                        g_BmInfo->bmiColors[0] = Palletes[CUR(&World.Maps)->PalleteNum][1]; 
                        g_BmInfo->bmiColors[1] = Palletes[CUR(&World.Maps)->PalleteNum][2]; 
                        g_BmInfo->bmiColors[2] = Palletes[CUR(&World.Maps)->PalleteNum][3]; 
                        break;
                    case 8:
                        g_BmInfo->bmiColors[0] = Palletes[CUR(&World.Maps)->PalleteNum][2]; 
                        g_BmInfo->bmiColors[1] = Palletes[CUR(&World.Maps)->PalleteNum][3]; 
                        break;
                    case 4:
                        g_BmInfo->bmiColors[0] = Palletes[CUR(&World.Maps)->PalleteNum][3]; 
                        break;
                    case 0:
                        /*LoadDoorMap*/
                        quad_info QuadInfo = GetQuad(&World, DoorPoint); 

                        map *OldMap = &World.Maps.Data[QuadInfo.Map]; 
                        map *NewMap = &World.Maps.Data[!QuadInfo.Map]; 

                        door *Door = SEARCH_FROM_POS(&OldMap->Doors, QuadInfo.Point);
                        TRY(Door, "Door is not allocated", 1);

                        const char *NewPath = Door->Path.Data;
                        TRY(ReadMap(&World, !QuadInfo.Map, NewPath), "Could not load map", 1);

                        TRY(PointInMap(NewMap, Door->DstPos), "Destination out of bounds", 1);
                        World.Maps.I = !QuadInfo.Map;
                        World.Player.Pos = QuadPtToPt(Door->DstPos);
                        World.IsOverworld = GetLoadedPoint(&World, !QuadInfo.Map, NewPath); 

                        /*CompleteTransition*/
                        ScrollX = 0;
                        ScrollY = 0;

                        memset(OldMap, 0, sizeof(*OldMap));
                        IsTransition = 0;

                        if(World.Player.Dir == DIR_DOWN) {
                            World.Player.IsMoving = 1;
                            World.Player.Tick = 8;
                            PlaceViewMap(&World, TileMap, TRUE);
                        } else {
                            PlaceViewMap(&World, TileMap, FALSE);
                        }
                        break;
                    }
                }
            }
        } else if(IsMenuUp) {
            if(g_Keys[VK_RETURN] == 1) {
                IsMenuUp = 0; 
                memset(WindowMap, 0, sizeof(WindowMap));
            } 
        } else if(g_Keys[VK_RETURN] == 1) {
            IsMenuUp = 1; 
            PlaceTextBox(WindowMap, (rect) {10, 0, 20, 14}); 
        } else {
            /*PlayerUpdate*/
            if(World.Player.Tick == 0) {
                int AttemptLeap = 0;

                /*PlayerKeyboard*/
                int HasMoveKey = 1;
                if(g_Keys['W']) {
                    World.Player.Dir = DIR_UP;
                } else if(g_Keys['A']) {
                    World.Player.Dir = DIR_LEFT;
                } else if(g_Keys['S']) {
                    World.Player.Dir = DIR_DOWN;
                    AttemptLeap = 1;
                } else if(g_Keys['D']) {
                    World.Player.Dir = DIR_RIGHT;
                } else {
                    HasMoveKey = 0;
                }

                point NewPoint = GetFacingPoint(World.Player.Pos, World.Player.Dir);

                /*FetchQuadProp*/
                quad_info OldQuadInfo = GetQuad(&World, PtToQuadPt(World.Player.Pos));
                quad_info NewQuadInfo = GetQuad(&World, NewPoint);
                NewPoint = NewQuadInfo.Point;

                point StartPos = AddPoints(NewPoint, g_DirPoints[ReverseDir(World.Player.Dir)]); 
                StartPos.X *= 16;
                StartPos.Y *= 16;

                /*FetchObject*/
                AttemptLeap &= !!(NewQuadInfo.Prop & QUAD_PROP_EDGE);
                point TestPoint = NewPoint; 
                if(AttemptLeap) {
                    TestPoint.Y++;
                }

                object *FacingObject = NULL; 
                FOR_VEC(Object, CUR(&World.Maps)->Objects) {
                    point ObjOldPt = PtToQuadPt(Object->Pos);
                    if(Object->Tick > 0) {
                        point ObjDirPt = g_NextPoints[Object->Dir];
                        point ObjNewPt = AddPoints(ObjOldPt, ObjDirPt);
                        if(EqualPoints(TestPoint, ObjOldPt) || EqualPoints(TestPoint, ObjNewPt)) {
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
                if(g_Keys['X'] == 1) {
                    int IsTV = NewQuadInfo.Prop & QUAD_PROP_TV;
                    int IsShelf = NewQuadInfo.Prop & QUAD_PROP_SHELF && World.Player.Dir == DIR_UP;
                    if(NewQuadInfo.Prop & QUAD_PROP_MESSAGE || IsTV || IsShelf) {
                        PlaceTextBox(WindowMap, (rect) {0, 12, 20, 18});

                        /*GetActiveText*/
                        if(IsShelf) {
                            ActiveText = "Crammed full of\nPOKéMON books!"; 
                        } else if(FacingObject) {
                            ActiveText = FacingObject->Str.Data;
                            if(FacingObject->Speed == 0) {
                                FacingObject->Tick = 60;
                            }
                            FacingObject->Dir = ReverseDir(World.Player.Dir);
                        } else if(IsTV && World.Player.Dir != DIR_UP) {
                           ActiveText = "Oops, wrong side."; 
                        } else {
                            text *Text = SEARCH_FROM_POS(&CUR(&World.Maps)->Texts, NewPoint);
                            ActiveText = Text ? Text->Str.Data : "ERROR: No text";
                        }

                        TextTilePt = (point) {1, 14};
                        TextTick = 0;

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

                if(World.Player.Tile != 0) {
                    if(NewQuadInfo.Prop & QUAD_PROP_WATER) {
                        NewQuadInfo.Prop &= ~QUAD_PROP_SOLID;
                    } else {
                        NewQuadInfo.Prop |= QUAD_PROP_SOLID;
                    }
                } else if(NewQuadInfo.Prop & QUAD_PROP_WATER) {
                    NewQuadInfo.Prop |= QUAD_PROP_SOLID;
                }

                if(HasMoveKey) {
                    IsLeaping = 0;
                    /*MovePlayer*/
                    if(NewQuadInfo.Prop & QUAD_PROP_DOOR) {
                        World.Player.IsMoving = 1;
                        World.Player.Tick = 8; 
                        DoorPoint = NewQuadInfo.Point;
                        IsTransition = 1;
                        NewPoint.Y--;
                    } else if(World.Player.Dir == DIR_DOWN && NewQuadInfo.Prop & QUAD_PROP_EDGE) {
                        World.Player.IsMoving = 1;
                        World.Player.Tick = 16;
                        IsLeaping = 1;
                    } else if(NewQuadInfo.Prop & QUAD_PROP_SOLID) {
                        World.Player.IsMoving = 0;
                        World.Player.Tick = 8;
                        if(World.Player.Dir == DIR_DOWN && OldQuadInfo.Prop & QUAD_PROP_EXIT) {
                            World.Player.IsMoving = 0;
                            TransitionTick = 16;
                            DoorPoint = OldQuadInfo.Point;
                            IsTransition = 1;
                        }
                    } else {
                        World.Player.IsMoving = 1;
                        World.Player.Tick = 8;
                    }
                } else {
                    World.Player.IsMoving = 0;
                }

                if(World.Player.IsMoving) {
                    World.Player.Pos = StartPos;
                    World.Maps.I = NewQuadInfo.Map;

                    /*UpdateLoadedMaps*/
                    if(World.IsOverworld) {
                        int AddToX = 0;
                        int AddToY = 0;
                        if(NewPoint.X == 4) {
                            AddToX = -1;
                        } else if(NewPoint.X == CUR(&World.Maps)->Width - 4) {
                            AddToX = 1;
                        }
                        if(NewPoint.Y == 4) {
                            AddToY = -1;
                        } else if(NewPoint.Y == CUR(&World.Maps)->Height - 4) {
                            AddToY = 1;
                        }

                        if(AddToX || AddToY) {
                            TRY(LoadNextMap(&World, AddToX, AddToY), "Could not load next map", 1);
                        }
                    }

                    /*TranslateQuadRectToTiles*/
                    point TileMin = {};
                    rect QuadRect = {};

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
                        if(IsTransition) {
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
                            .Bottom = NewPoint.Y + (IsLeaping ? 6 : 5)
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

        int SprI = 0;

        /*UpdatePlayer*/
        sprite *SpriteQuad = &Sprites[SprI];
        point PlayerPt = {72, 72};
        UpdateAnimation(&World.Player, SpriteQuad, PlayerPt);
        if(!ActiveText && World.Player.Tick > 0) {
            MoveEntity(&World.Player);
            if(World.Player.IsMoving) {
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
        } else {
            IsLeaping = 0;
        }
        SprI += 4;
   
        /*UpdateObjects*/
        for(int MapI = 0; MapI < _countof(World.Maps.Data); MapI++) {
            FOR_VEC(Object, World.Maps.Data[MapI].Objects) {
                sprite *SpriteQuad = &Sprites[SprI];
                point QuadPt = {
                    Object->Pos.X - World.Player.Pos.X + 72,
                    Object->Pos.Y - World.Player.Pos.Y + 72
                };

                if(World.Maps.I != MapI) {
                    switch(GetMapDir(World.Maps.Data, World.Maps.I)) {
                    case DIR_UP:
                        QuadPt.Y -= World.Maps.Data[MapI].Height * 16;
                        break;
                    case DIR_LEFT:
                        QuadPt.X -= World.Maps.Data[MapI].Width * 16;
                        break;
                    case DIR_DOWN:
                        QuadPt.Y += CUR(&World.Maps)->Height * 16;
                        break;
                    case DIR_RIGHT:
                        QuadPt.X += CUR(&World.Maps)->Width * 16;
                        break;
                    }
                }

                if(ObjectInUpdateBounds(QuadPt)) {
                    UpdateAnimation(Object, SpriteQuad, QuadPt);
                    /*TickFrame*/
                    if(!ActiveText) {
                        if(Object->Speed == 0) {
                            if(Object->Tick > 0) {
                                Object->Tick--; 
                            } else {
                                Object->Dir = Object->StartingDir;
                            }
                        } else if(Object->Tick > 0) {
                            MoveEntity(Object);
                        } else if(!IsTransition)  {
                            RandomMove(&World, MapI, Object);
                        }
                    }
                    SprI += 4;
                }
            }
        }

        /*UpdateLeapingAnimations*/
        if(IsLeaping) {
            /*PlayerUpdateJumpingAnimation*/
            uint8_t PlayerDeltas[] = {0, 4, 6, 8, 9, 10, 11, 12};
            uint8_t DeltaI = World.Player.Tick < 8 ? World.Player.Tick: 15 - World.Player.Tick;
            FOR_RANGE(Tile, Sprites, 0, 4) { 
                Tile->Y -= PlayerDeltas[DeltaI];
            }

            /*CreateShadowQuad*/
            sprite *ShadowQuad  = &Sprites[SprI]; 
            ShadowQuad[0] = (sprite) {72, 72, 255, 0};
            ShadowQuad[1] = (sprite) {80, 72, 255, SPR_HORZ_FLAG};
            ShadowQuad[2] = (sprite) {72, 80, 255, SPR_VERT_FLAG};
            ShadowQuad[3] = (sprite) {80, 80, 255, SPR_HORZ_FLAG | SPR_VERT_FLAG};
            SprI += 4;
        }

        /*MutTileUpdate*/
        int TickCycle = Tick / 16 % 9;
        if(Tick % 16 == 0 && World.IsOverworld) {
            /*FlowersUpdate*/
            uint8_t *FlowerDst = World.TileData + 3 * TILE_SIZE;
            uint8_t *FlowerSrc = FlowerData + TickCycle % 3 * TILE_SIZE;
            memcpy(FlowerDst, FlowerSrc, TILE_SIZE);

            /*WaterUpdate*/
            int TickMod = TickCycle < 5 ? TickCycle : 9 - TickCycle;
            uint8_t *WaterDst = World.TileData + 20 * TILE_SIZE;
            uint8_t *WaterSrc = WaterData + TickMod * TILE_SIZE;
            memcpy(WaterDst, WaterSrc, TILE_SIZE);
        }

        /*UpdatePallete*/
        if(!IsTransition && World.Player.Tick <= 0) {
            COPY_OBJECT(g_BmInfo->bmiColors, Palletes[CUR(&World.Maps)->PalleteNum]);
        }

        /*RenderTiles*/
        uint8_t (*BmRow)[BM_WIDTH] = g_BmPixels;
        
        for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
            #define GET_TILE_SRC(Off) (World.TileData + (Off) + TileMap[TileY][TileX] * TILE_SIZE)
            int PixelX = 8 - (ScrollX & 7);
            int SrcYDsp = ((PixelY + ScrollY) & 7) << 3;
            int TileX = ScrollX >> 3;
            int TileY = (PixelY + ScrollY) / 8 % 32;
            int StartOff = SrcYDsp | (ScrollX & 7);
            memcpy(*BmRow, GET_TILE_SRC(StartOff), 8);

            for(int Repeat = 1; Repeat < 20; Repeat++) {
                TileX = (TileX + 1) % 32;
                uint8_t *Pixel = *BmRow + PixelX;
                memcpy(Pixel, GET_TILE_SRC(SrcYDsp), 8);

                PixelX += 8;
            }
            TileX = (TileX + 1) % 32;
            uint8_t *Pixel = *BmRow + PixelX;
            memcpy(Pixel, GET_TILE_SRC(SrcYDsp), BM_WIDTH - PixelX);
            BmRow++;
        }

        /*RenderSprites*/
        int MaxX = BM_WIDTH;
        int MaxY = BM_HEIGHT;

        for(size_t I = SprI; I-- > 0; ) {
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

            uint8_t *Src = SpriteData + Sprites[I].Tile * TILE_SIZE + SrcY * TILE_LENGTH + SrcX;

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
        BmRow = g_BmPixels;
        for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
            int PixelX = 0;
            int SrcYDsp = (PixelY & 7) << 3;
            int TileX = 0;
            int TileY = PixelY / 8;

            for(int Repeat = 0; Repeat < 20; Repeat++) {
                if(WindowMap[TileY][TileX]) {
                    uint8_t *Src = World.TileData + SrcYDsp + WindowMap[TileY][TileX] * TILE_SIZE;
                    memcpy(*BmRow + PixelX, Src, 8);
                }
                TileX++;
                PixelX += 8;
            }
            BmRow++;
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
            if(IsGranular) {
                int64_t RemainCounter = FrameDelta - InitDeltaCounter;
                uint32_t SleepMS = 1000 * RemainCounter / PerfFreq;
                if(SleepMS) {
                    Sleep(SleepMS);
                }
            }
            while(GetDeltaCounter(BeginCounter) < FrameDelta);
        }

        /*NextFrame*/
        InvalidateRect(Window, NULL, FALSE);
        UpdateWindow(Window);
        Tick++;
    }

    return 0;
}


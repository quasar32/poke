#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>

/*Generic Structures*/
#define FC_VEC(Type, Size) struct {\
    int Count;\
    Type Data[Size];\
}

#define SELECT(Type, Size) struct {\
    int I;\
    Type Data[Size];\
}

/*Monad Macros*/
#define FOR_RANGE(Var, Ptr, BeginI, EndI) \
    for(typeof(*Ptr) *Var = (Ptr) + (BeginI), *End_ = Var + (EndI); Var < End_; Var++) 

#define FOR_VEC(Var, Vec) FOR_RANGE(Var, (Vec).Data, 0, (Vec).Count)

#define AUTO_VAR(Var, Val) typeof(Val) Var = (Val)
#define CUR(Select) ({\
    AUTO_VAR(Select_, (Select));\
    &Select_->Data[Select_->I];\
}) 
#define OTH(Select) ({\
    AUTO_VAR(Select_, (Select));\
    &Select_->Data[!Select_->I];\
}) 

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
#define ANIM_SEAL 128

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
    
#define MT_FULL_HORZ_ARROW 174
#define MT_BLANK 176 
#define MT_EMPTY_HORZ_ARROW 177

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

/*Data loaders*/
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

static void ReadTileData(const char *Path, uint8_t *TileData, int TileCount) {
    const int SizeOfCompressedTile = 16;
    int SizeOfCompressedTileData = TileCount * SizeOfCompressedTile;
    int SizeofTileData = TileCount;

    uint8_t CompData[SizeOfCompressedTileData];
    int BytesRead = ReadAll(Path, CompData, sizeof(CompData));

    for(int I = 0; I < BytesRead; I++) {
        TileData[I * 4] = (CompData[I] >> 6) & 3;
        TileData[I * 4 + 1] = (CompData[I] >> 4) & 3;
        TileData[I * 4 + 2] = (CompData[I] >> 2) & 3;
        TileData[I * 4 + 3] = CompData[I] & 3;
    }
    for(int I = BytesRead * 4; I < SizeofTileData; I++) {
        TileData[I] = 0;
    } 
}

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

static void ReadMap(world *World, int MapI, const char *Path) {
    run_buffer RunBuffer = {};
    RunBuffer.Size = ReadAll(Path, RunBuffer.Data, sizeof(RunBuffer.Data));

    /*ReadQuadSize*/
    map *Map = &World->Maps.Data[MapI];
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
    Map->Texts.Count = RunBufferGetByte(&RunBuffer);
    FOR_VEC(Text, Map->Texts) {
        Text->Pos.X = RunBufferGetByte(&RunBuffer);
        Text->Pos.Y = RunBufferGetByte(&RunBuffer);
        RunBufferGetString(&RunBuffer, Text->Str);
    }

    /*ReadObjects*/
    Map->Objects.Count = RunBufferGetByte(&RunBuffer);
    FOR_VEC(Object, Map->Objects) {
        Object->Pos.X = RunBufferGetByte(&RunBuffer) * 16;
        Object->Pos.Y = RunBufferGetByte(&RunBuffer) * 16;
        Object->StartingDir = RunBufferGetByte(&RunBuffer);
        Object->Dir = Object->StartingDir;  
        Object->Speed = RunBufferGetByte(&RunBuffer);
        Object->Tile = RunBufferGetByte(&RunBuffer) & 0xF0;
        RunBufferGetString(&RunBuffer, Object->Str);
    }

    /*ReadDoors*/
    Map->Doors.Count = RunBufferGetByte(&RunBuffer);
    FOR_VEC(Door, Map->Doors) {
        Door->Pos.X = RunBufferGetByte(&RunBuffer);
        Door->Pos.Y = RunBufferGetByte(&RunBuffer);
        Door->DstPos.X = RunBufferGetByte(&RunBuffer);
        Door->DstPos.Y = RunBufferGetByte(&RunBuffer);
        RunBufferGetString(&RunBuffer, Door->Path);
    }

    /*ReadTileSet*/
    int NewDataPath = RunBufferGetByte(&RunBuffer) % _countof(World->DataPaths.Data); 
    if(World->DataPaths.I != NewDataPath) {
        World->DataPaths.I = NewDataPath;
        data_path *DataPath = CUR(&World->DataPaths);

        ReadTileData(DataPath->Tile, World->TileData, 96);
        ReadAll(DataPath->Quad, World->QuadData, sizeof(World->QuadData)); 
        ReadAll(DataPath->Prop, World->QuadProps, sizeof(World->QuadProps));
    }

    /*ReadPalleteNum*/
    Map->PalleteNum = RunBufferGetByte(&RunBuffer);
    Map->PalleteNum = 1;
}

static void ReadOverworldMap(world *World, int MapI, point Load) {  
    if(PointInWorld(Load) && World->MapPaths[Load.Y][Load.X]) {
        const char *CurMapPath = World->MapPaths[Load.Y][Load.X];
        ReadMap(World, MapI, CurMapPath);
        World->IsOverworld = 1;
        World->Maps.Data[MapI].Loaded = Load; 
    }
}

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
    point CurMapPt = CUR(&World->Maps)->Loaded;
    if(DeltaX || DeltaY) {
        point NewMapPt = {CurMapPt.X + DeltaX, CurMapPt.Y + DeltaY}; 
        if(PointInWorld(NewMapPt) && World->MapPaths[NewMapPt.X][NewMapPt.Y]) {
            ReadOverworldMap(World, !World->Maps.I, NewMapPt);
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

static void PlaceViewMap(const world *World, uint8_t TileMap[32][32], int IsDown) {
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
static void PlaceTextBox(uint8_t TileMap[32][32], rect Rect) {
    /*HeadRow*/
    TileMap[Rect.Top][Rect.Left] = 96;
    memset(&TileMap[Rect.Top][Rect.Left + 1], 97, Rect.Right - Rect.Left + 2);
    TileMap[Rect.Top][Rect.Right - 1] = 98;

    /*BodyRows*/
    for(int Y = Rect.Top + 1; Y < Rect.Bottom - 1; Y++) {
        TileMap[Y][Rect.Left] = 99;
        memset(&TileMap[Y][Rect.Left + 1], MT_BLANK, Rect.Right - Rect.Left + 2);
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
    } else if(Output == '\xE9') {
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
        Output = MT_BLANK;
    }
    return Output;
}

static void PlaceText(uint8_t TileMap[32][32], point TileMin, const char *Text) {
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

static void PlaceMenu(uint8_t WindowMap[32][32], int MenuSelectI) {
    PlaceTextBox(WindowMap, (rect) {10, 0, 20, 14}); 
    PlaceText(
        WindowMap, 
        (point) {12, 2}, 
        "POKéMON\nITEM\nRED\nSAVE\nOPTION\nEXIT" 
    ); 
    WindowMap[MenuSelectI * 2 + 2][11] = MT_FULL_HORZ_ARROW;
}

static void PlaceOptionCursor(uint8_t WindowMap[32][32], option *Option, int Tile) {
    WindowMap[Option->Y][Option->Xs[Option->I]] = Tile;
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
    srand(time(NULL));
    uint8_t Keys[256] = {};

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
    
    /*SetLoadPath*/
    if(!SetCurrentDirectory("../Shared")) {
        MessageBox(NULL, "Failed to find shared directory", "Error", MB_ICONERROR); 
        return 1;
    }

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

    ReadTileData("Menu", &World.TileData[TILE_SIZE * 96], 152);
    ReadTileData("SpriteData", SpriteData, 256);
    ReadTileData("FlowerData", FlowerData, 3);
    ReadTileData("WaterData", WaterData, 7);
    ReadTileData("ShadowData", &SpriteData[TILE_SIZE * 255], 1);

    /*InitMaps*/
    ReadOverworldMap(&World, 0, (point) {0, 2});

    /*GameBoyGraphics*/
    sprite Sprites[40] = {};

    uint8_t TileMap[32][32];
    uint8_t ScrollX = 0;
    uint8_t ScrollY = 0;
    PlaceViewMap(&World, TileMap, 0);

    uint8_t WindowMap[32][32] = {};

    /*InitGameState*/
    typedef enum game_state {
        GS_NORMAL,
        GS_LEAPING,
        GS_TEXT,
        GS_TRANSITION,
        GS_MENU,
        GS_OPTIONS,
    } game_state; 
    game_state GameState = GS_NORMAL;
    int TransitionTick = 0;
    point DoorPoint = {};

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
    
    /*NonGameState*/
    dim_rect RestoreRect = {};
    int HasQuit = 0;
    int IsFullscreen = 0;

    /*Text*/
    const char *ActiveText = "ERROR: NO TEXT";
    point TextTilePt = {1, 14};
    uint64_t TextTick = 0;

    /*Timing*/
    int64_t Tick = 0;
    int64_t BeginCounter = 0;
    int64_t PerfFreq = GetPerfFreq();
    int64_t FrameDelta = PerfFreq / 60;

    /*LoadWinmm*/
    int IsGranular = 0;
    typedef MMRESULT WINAPI(winmm_func)(UINT);
    winmm_func *TimeEndPeriod = NULL;

    HMODULE WinmmLib = LoadLibrary("winmm.dll");
    if(WinmmLib) {
        winmm_func *TimeBeginPeriod = (winmm_func *) GetProcAddress(WinmmLib, "timeBeginPeriod"); 
        if(TimeBeginPeriod) {
            TimeEndPeriod = (winmm_func *) GetProcAddress(WinmmLib, "timeEndPeriod"); 
            if(TimeEndPeriod) {
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
                } else if(Keys[I] < 255) {
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

        /*UpdatePlayer*/
        switch(GameState) {
        case GS_NORMAL:
        case GS_LEAPING:
            if((IsPauseAttempt |= (Keys[VK_RETURN] == 1)) && World.Player.Tick == 0) {
                IsPauseAttempt = 0;
                GameState = GS_MENU;
                PlaceMenu(WindowMap, MenuSelectI);
            } else {
                /*PlayerUpdate*/
                if(World.Player.Tick == 0) {
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
                    if(Keys['X'] == 1) {
                        int IsTV = NewQuadInfo.Prop & QUAD_PROP_TV;
                        int IsShelf = NewQuadInfo.Prop & QUAD_PROP_SHELF && World.Player.Dir == DIR_UP;
                        if(NewQuadInfo.Prop & QUAD_PROP_MESSAGE || IsTV || IsShelf) {
                            GameState = GS_TEXT;
                            PlaceTextBox(WindowMap, (rect) {0, 12, 20, 18});

                            /*GetActiveText*/
                            if(IsShelf) {
                                ActiveText = "Crammed full of\nPOKéMON books!"; 
                            } else if(IsTV && World.Player.Dir != DIR_UP) {
                               ActiveText = "Oops, wrong side."; 
                            } else if(FacingObject) {
                                ActiveText = FacingObject->Str;
                                if(FacingObject->Speed == 0) {
                                    FacingObject->Tick = 60;
                                }
                                FacingObject->Dir = ReverseDir(World.Player.Dir);
                            } else {
                                text *Text = SEARCH_FROM_POS(&CUR(&World.Maps)->Texts, NewPoint);
                                ActiveText = Text ? Text->Str : "ERROR: NO TEXT";
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
                        GameState = GS_NORMAL;
                        /*MovePlayer*/
                        if(NewQuadInfo.Prop & QUAD_PROP_DOOR) {
                            World.Player.IsMoving = 1;
                            World.Player.Tick = 8; 
                            DoorPoint = NewQuadInfo.Point;
                            GameState = GS_TRANSITION;
                            NewPoint.Y--;
                        } else if(World.Player.Dir == DIR_DOWN && NewQuadInfo.Prop & QUAD_PROP_EDGE) {
                            World.Player.IsMoving = 1;
                            World.Player.Tick = 16;
                            GameState = GS_LEAPING;
                        } else if(NewQuadInfo.Prop & QUAD_PROP_SOLID) {
                            World.Player.IsMoving = 0;
                            World.Player.Tick = 8;
                            if(World.Player.Dir == DIR_DOWN && OldQuadInfo.Prop & QUAD_PROP_EXIT) {
                                World.Player.IsMoving = 0;
                                TransitionTick = 16;
                                DoorPoint = OldQuadInfo.Point;
                                GameState = GS_TRANSITION;
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
                                LoadNextMap(&World, AddToX, AddToY);
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
                            if(GameState == GS_TRANSITION) {
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
        case GS_TEXT:
            if(ActiveText[0]) {
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
                    WindowMap[16][18] = TextTick / 16 % 2 ? MT_BLANK : 171;
                    if(Keys['X'] == 1) {
                        if(ActiveText[-1] == '\n') {
                            memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                            memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                        }
                        memset(&WindowMap[14][1], MT_BLANK, 17);
                        memset(&WindowMap[16][1], MT_BLANK, 18);
                        TextTilePt.Y = 17;
                        TextTick = 4;
                    } else {
                        TextTick++;
                    }
                    break;
                default:
                    {
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
                                TextTick = (uint64_t[]){0, 2, 3}[Options[OPT_TEXT_SPEED].I];
                            }
                            ActiveText++;
                        }
                    }
                }
            } else if(Keys['X'] == 1) {
                GameState = GS_NORMAL;
                memset(WindowMap, 0, sizeof(WindowMap));
            }
            break;
        case GS_TRANSITION:
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
                        {
                            /*LoadDoorMap*/
                            quad_info QuadInfo = GetQuad(&World, DoorPoint); 

                            map *OldMap = &World.Maps.Data[QuadInfo.Map]; 
                            map *NewMap = &World.Maps.Data[!QuadInfo.Map]; 

                            door *Door = SEARCH_FROM_POS(&OldMap->Doors, QuadInfo.Point);
                            if(Door) {
                                const char *NewPath = Door->Path;
                                ReadMap(&World, !QuadInfo.Map, NewPath);

                                if(!PointInMap(NewMap, Door->DstPos)) {
                                    Door->DstPos = (point) {0, 0};
                                }
                                World.Maps.I = !QuadInfo.Map;
                                World.Player.Pos = QuadPtToPt(Door->DstPos);
                                World.IsOverworld = GetLoadedPoint(&World, !QuadInfo.Map, NewPath); 

                                /*CompleteTransition*/
                                ScrollX = 0;
                                ScrollY = 0;

                                memset(OldMap, 0, sizeof(*OldMap));
                                if(World.Player.Dir == DIR_DOWN) {
                                    World.Player.IsMoving = 1;
                                    World.Player.Tick = 8;
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
        case GS_MENU:
            if(Keys[VK_RETURN] == 1) {
                /*RemoveMenu*/
                GameState = GS_NORMAL;
                memset(WindowMap, 0, sizeof(WindowMap));
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
                        /*RemoveMenu*/
                        GameState = GS_NORMAL; 
                        memset(WindowMap, 0, sizeof(WindowMap));
                        break;
                    }
                }
            }
            break;
        case GS_OPTIONS:
            if(Keys[VK_RETURN] == 1 || (Keys['X'] == 1 && OptionI == OPT_CANCEL)) {
                /*RemoveOptions*/
                GameState = GS_MENU;
                memset(WindowMap, 0, sizeof(WindowMap));
                PlaceMenu(WindowMap, MenuSelectI);
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

        int SprI = 0;

        /*UpdatePlayer*/
        sprite *SpriteQuad = &Sprites[SprI];
        point PlayerPt = {72, 72};
        int CanPlayerMove = GameState == GS_NORMAL || 
                            GameState == GS_LEAPING || 
                            GameState == GS_TRANSITION;
        UpdateAnimation(&World.Player, SpriteQuad, PlayerPt);
        if(CanPlayerMove && World.Player.Tick > 0) {
            if(World.Player.Tick % 8 == 4) {
                World.Player.IsRight ^= 1;
            }
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
                    if(GameState == GS_NORMAL) {
                        if(Object->Speed == 0) {
                            if(Object->Tick > 0) {
                                Object->Tick--; 
                            } else {
                                Object->Dir = Object->StartingDir;
                            }
                            if(Object->Tick % 8 == 4) {
                                Object->IsRight ^= 1;
                            }
                        } else if(Object->Tick > 0) {
                            MoveEntity(Object);
                        } else if(!IsPauseAttempt) {
                            RandomMove(&World, MapI, Object);
                        }
                    }
                    SprI += 4;
                }
            }
        }

        /*UpdateLeapingAnimations*/
        if(GameState == GS_LEAPING) {
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
        if(GameState != GS_TRANSITION && World.Player.Tick <= 0) {
            memcpy(g_BmInfo->bmiColors, Palletes[CUR(&World.Maps)->PalleteNum], sizeof(*Palletes));
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
    if(IsGranular) {
        TimeEndPeriod(1);
    }

    return 0;
}


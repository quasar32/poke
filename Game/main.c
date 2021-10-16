#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>

/*Fixed Capacity Vector Macros*/
#define FC_VEC(Type, Size) struct {\
    int Count;\
    Type Data[Size];\
}

#define CREATE_VEC_I(Vec) ({\
    FC_VEC_I(_typeof((Vec)->Data), _countof((Vec)->Count))\
})

/*Attribute Wrappers*/
#define NONNULL_PARAMS __attribute((nonnull))
#define UNUSED __attribute__((unused))

#define MAX_WORLD_WIDTH 1
#define MAX_WORLD_HEIGHT 4 

/*Monad macros*/
#define TRY(Cond, Str) do {\
    if(!(Cond)) {\
        LogError(__func__, Str);\
        goto out;\
    }\
} while(0)

#define TRY_HERE(Cond, Str) do {\
    if(!(Cond)) {\
        LogError(__func__, Str);\
    }\
} while(0)

#define FOR_RANGE(Var, Ptr, BeginI, EndI) \
    for(typeof(*Ptr) *Var = (Ptr) + (BeginI), *End_ = Var + (EndI); Var < End_; Var++) 

#define FOR_ARY(Var, Ary) FOR_RANGE(Var, (Ary), 0, _countof(Ary)) 
#define FOR_VEC(Var, Vec) FOR_RANGE(Var, (Vec).Data, 0, (Vec).Count)

/*Macro Constants*/
#define AnimPlayer 0
#define AnimSeal 84

#define BmWidth 160
#define BmHeight 144

#define SprHorzFlag 1
#define SprVertFlag 2

#define DirUp 0
#define DirLeft 1
#define DirDown 2
#define DirRight 3

#define StateStop 0
#define StateMoving 1

#define QuadPropSolid   1
#define QuadPropEdge    2
#define QuadPropMessage 4
#define QuadPropWater   8
#define CountofQuadProp 4

#define TileLength 8
#define TileSize 64

#define LParamKeyIsHeld 0x30000000

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

typedef struct text {
    point Pos;
    FC_VEC(char, 256) Str;
} text;

typedef struct object {
    point Pos;

    uint8_t Dir;
    uint8_t Speed;
    uint8_t Tile;

    uint8_t State;
    uint8_t IsRight;
    uint8_t Tick;

    FC_VEC(char, 256) Str;
} object;

typedef struct map {
    int16_t Width;
    int16_t Height;
    int16_t DefaultQuad;
    uint8_t Quads[256][256];
    point Loaded;
    FC_VEC(text, 256) Texts;
    FC_VEC(object, 256) Objects;
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
    int Dir;
    int Quad;
} quad_info;

typedef struct error { 
    const char *Func;
    const char *Error;
} error;

/*Reusable Vec*/
typedef FC_VEC(sprite, 64) sprite_output;

/*Global Constants*/
static const point g_DirPoints[] = {
    [DirUp] = {0, -1},
    [DirLeft] = {-1, 0},
    [DirDown] = {0, 1},
    [DirRight] = {1, 0}
};

static const point NextPoints[] = {
    [DirUp] = {0, 1},
    [DirLeft] = {1, 0},
    [DirDown] = {0, 1},
    [DirRight] = {1, 0}
};

/*Globals (Shared between WinMain and WndProc)*/
static uint8_t g_Keys[256] = {};
static uint8_t g_BmPixels[BmHeight][BmWidth];
static BITMAPINFO *g_BmInfo = NULL;

/*Globals (Shared between all)*/
static FC_VEC(error, 32) g_Errors;

/*Globals (World)*/
typedef struct world {
    int Width;
    int Height;
    int MapI;
    map Maps[2];
    object Player;
    const char *MapPaths[MAX_WORLD_HEIGHT][MAX_WORLD_WIDTH];
} world;

/*Math Functions*/
static int Min(int A, int B) {
    return A < B ? A : B;
}

static inline int ReverseDir(int Dir) {
    return (Dir + 2) % 4;
}

static int Max(int A, int B) {
    return A > B ? A : B;
}

static int Clamp(int Value, int Bottom, int Top) {
    Value = Max(Value, Bottom);
    Value = Min(Value, Top);
    return Value;
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

static int ComparePoints(point A, point B) {
    return A.X == B.X && A.Y == B.Y;
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
static point ConvertToQuadPoint(point Point) {
    point QuadPoint = {
        .X = Point.X / 16,
        .Y = Point.Y / 16
    };
    return QuadPoint;
}

static point GetFacingPoint(point Pos, int Dir) {
    point OldPoint = ConvertToQuadPoint(Pos);
    point DirPoint = g_DirPoints[Dir];
    return AddPoints(OldPoint, DirPoint);
}

/*Quad Functions*/
static void SetToTiles(uint8_t TileMap[32][32], int TileX, int TileY, 
                       const uint8_t Set[static 4]) {
    TileMap[TileY][TileX] = Set[0];
    TileMap[TileY][TileX + 1] = Set[1];
    TileMap[TileY + 1][TileX] = Set[2];
    TileMap[TileY + 1][TileX + 1] = Set[3];
}

static int GetMapDir(const map Maps[static 2], int Map) {
    point DirPoint = SubPoints(Maps[!Map].Loaded, Maps[Map].Loaded);
    for(size_t I = 0; I < _countof(g_DirPoints); I++) {
        if(ComparePoints(DirPoint, g_DirPoints[I])) {
            return I;
        }
    }
    return -1; 
} 

static quad_info GetQuad(const map Maps[static 2], quad_info QuadInfo) {
    int OldWidth = Maps[QuadInfo.Map].Width;
    int OldHeight = Maps[QuadInfo.Map].Height;

    int NewWidth = Maps[!QuadInfo.Map].Width;
    int NewHeight = Maps[!QuadInfo.Map].Height;

    switch(QuadInfo.Dir) {
    case DirUp:
        if(QuadInfo.Point.Y < 0) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y += NewHeight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DirLeft:
        if(QuadInfo.Point.X < 0) {
            QuadInfo.Map ^= 1;
            QuadInfo.Point.X += NewHeight; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
        }
        break;
    case DirDown:
        if(QuadInfo.Point.Y >= Maps[QuadInfo.Map].Height) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y -= OldHeight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DirRight:
        if(QuadInfo.Point.X >= Maps[QuadInfo.Map].Width) {
            QuadInfo.Point.X -= OldWidth; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
            QuadInfo.Map ^= 1;
        } 
        break;
    }

    if(QuadInfo.Point.X >= 0 && QuadInfo.Point.X < Maps[QuadInfo.Map].Width && 
       QuadInfo.Point.Y >= 0 && QuadInfo.Point.Y < Maps[QuadInfo.Map].Height) {
        QuadInfo.Quad = Maps[QuadInfo.Map].Quads[QuadInfo.Point.Y][QuadInfo.Point.X];
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
        char Buf[65536];
        FOR_VEC(Error, g_Errors) {
            strcat_s(Buf, _countof(Buf), Error->Func);
            strcat_s(Buf, _countof(Buf), ": ");
            strcat_s(Buf, _countof(Buf), Error->Error);
            strcat_s(Buf, _countof(Buf), "\n");
        }
        MessageBox(Window, Buf, "Error", MB_OK);
        g_Errors.Count = 0;
    }
}

/*String*/
NONNULL_PARAMS
static void CopyCharsWithNull(char *Dest, const char *Source, size_t Length) {
    memcpy(Dest, Source, Length);
    Dest[Length] = '\0';
}

/*Data loaders*/
static int64_t GetFullFileSize(HANDLE FileHandle) {
    LARGE_INTEGER FileSize;
    int Success = GetFileSizeEx(FileHandle, &FileSize);
    TRY_HERE(Success, "Could not obtain file size");
    return Success ? FileSize.QuadPart: -1;
}

NONNULL_PARAMS __attribute__((warn_unused_result))
static int ReadAll(const char *Path, void *Bytes, int MaxBytesToRead) {
    int Success = 0;
    DWORD BytesRead = 0;
    HANDLE FileHandle = CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, 
                                   NULL, OPEN_EXISTING, 0, NULL);
    TRY(FileHandle != INVALID_HANDLE_VALUE, "File could not be found"); 
    int64_t FileSize = GetFullFileSize(FileHandle);
    TRY(FileSize >= 0, "File size could not be obtained");
    TRY(FileSize <= MaxBytesToRead, "File is too big for buffer");
        
    ReadFile(FileHandle, Bytes, FileSize, &BytesRead, NULL); 
    TRY(BytesRead == FileSize, "Could not read all of file");

    Success = 1;
out:
    if(FileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(FileHandle);
    }
    if(!Success) {
       BytesRead = -1; 
    }
    return BytesRead;
}

NONNULL_PARAMS __attribute__((warn_unused_result))
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

NONNULL_PARAMS __attribute__((warn_unused_result))
static int ReadMap(const char *Path, map *Map) {
    #define NEXT_RUN ({\
        TRY(RunIndex < BytesRead, "File is truncated");\
        RunData[RunIndex++];\
    })

    #define MOVE_RUN(Count) ({\
        TRY(RunIndex + Count <= BytesRead, "File is truncated");\
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
    int BytesRead = ReadAll(Path, RunData, sizeof(RunData));
    int Success = 0;
    int RunIndex = 0;

    /*ReadQuadSize*/
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

        TRY(QuadIndex + Repeat <= Size, "Quads are truncated");

        for(int I = QuadIndex; I < QuadIndex + Repeat; I++) {
            Map->Quads[I / Map->Width][I % Map->Width] = Quad; 
        }

        QuadIndex += Repeat;
    }
    TRY(QuadIndex >= Size, "Quads are incomplete");

    /*DefaultQuad*/
    Map->DefaultQuad = NEXT_RUN;
    TRY(Map->DefaultQuad < 128, "Out of bounds default quad");

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
        Object->Dir = NEXT_RUN;  
        Object->Speed = NEXT_RUN;
        Object->Tile = NEXT_RUN;
        NEXT_STR(Object->Str);
    }

    TRY(RunIndex >= BytesRead, "Extraneous data present");
    
    /*Out*/
    Success = 1;
out:
    if(!Success) {
        memset(Map, 0, sizeof(*Map));
    }
    return Success;
#undef NEXT_STR
#undef MOVE_RUN
#undef NEXT_RUN
}

/*Object Functions*/
static void MoveEntity(object *Object) {
    if(Object->State == StateMoving) {
        point DeltaPoint = g_DirPoints[Object->Dir];
        DeltaPoint.X *= Object->Speed;
        DeltaPoint.Y *= Object->Speed;
        Object->Pos = AddPoints(Object->Pos, DeltaPoint);
    }
    Object->Tick--;
}

static void RandomMove(const map *Map, object *Object, 
                       const object *Player, const uint8_t QuadProps[64]) {
    int Seed = rand();
    if(Seed % 64 == 0) {
        Object->Dir = Seed / 64 % 4;

        point NewPoint = GetFacingPoint(Object->Pos, Object->Dir);

        /*FetchQuadPropSingleMap*/
        int Quad = -1;
        int Prop = QuadPropSolid;
        int InHorzBounds = NewPoint.X >= 0 && NewPoint.X < Map->Width;
        int InVertBounds = NewPoint.Y >= 0 && NewPoint.Y < Map->Height;
        if(InHorzBounds && InVertBounds) {
            Quad = Map->Quads[NewPoint.Y][NewPoint.X];
            Prop = QuadProps[Quad];
        }
        
        /*IfWillColideWithPlayer*/
        point PlayerCurPoint = ConvertToQuadPoint(Player->Pos);

        if(ComparePoints(PlayerCurPoint, NewPoint)) {
            Prop = QuadPropSolid;
        } else if(Player->State != StateStop) {
            point NextPoint = NextPoints[Player->Dir];
            point PlayerNextPoint = AddPoints(PlayerCurPoint, NextPoint);
            if(ComparePoints(PlayerNextPoint, NewPoint)) {
                Prop = QuadPropSolid;
            }
        }

        /*StartMovingEntity*/
        if(!(Prop & QuadPropSolid)) {
            Object->Tick = 16;
            Object->State = StateMoving;
        }
    }
}

static void UpdateAnimation(object *Object, sprite *SpriteQuad, point QuadPt) {
    uint8_t QuadX = QuadPt.X;
    uint8_t QuadY = QuadPt.Y;
    if(Object->Tick % 8 < 4) {
        /*SetStillAnimation*/
        switch(Object->Dir) {
        case DirUp:
            SpriteQuad[0] = (sprite) {0, 0, 0, 0};
            SpriteQuad[1] = (sprite) {8, 0, 0, SprHorzFlag};
            SpriteQuad[2] = (sprite) {0, 8, 1, 0};
            SpriteQuad[3] = (sprite) {8, 8, 1, SprHorzFlag};
            break;
        case DirLeft:
            SpriteQuad[0] = (sprite) {0, 0, 4, 0};
            SpriteQuad[1] = (sprite) {8, 0, 5, 0};
            SpriteQuad[2] = (sprite) {0, 8, 6, 0};
            SpriteQuad[3] = (sprite) {8, 8, 7, 0};
            break;
        case DirDown:
            SpriteQuad[0] = (sprite) {0, 0, 10, 0};
            SpriteQuad[1] = (sprite) {8, 0, 10, SprHorzFlag};
            SpriteQuad[2] = (sprite) {0, 8, 11, 0};
            SpriteQuad[3] = (sprite) {8, 8, 11, SprHorzFlag};
            break;
        case DirRight:
            SpriteQuad[0] = (sprite) {0, 0, 5, SprHorzFlag};
            SpriteQuad[1] = (sprite) {8, 0, 4, SprHorzFlag};
            SpriteQuad[2] = (sprite) {0, 8, 7, SprHorzFlag};
            SpriteQuad[3] = (sprite) {8, 8, 6, SprHorzFlag};
            break;
        }
    } else {
        /*ChangeFootAnimation*/
        switch(Object->Dir) {
        case DirUp:
            SpriteQuad[0] = (sprite) {0, 1, 0, 0};
            SpriteQuad[1] = (sprite) {8, 1, 0, SprHorzFlag};
            if(Object->IsRight) {
                SpriteQuad[2] = (sprite) {0, 8, 2, 0};
                SpriteQuad[3] = (sprite) {8, 8, 3, 0};
            } else {
                SpriteQuad[2] = (sprite) {0, 8, 3, SprHorzFlag};
                SpriteQuad[3] = (sprite) {8, 8, 2, SprHorzFlag};
            }
            break;
        case DirLeft:
            SpriteQuad[0] = (sprite) {0, 1, 4, 0};
            SpriteQuad[1] = (sprite) {8, 1, 5, 0};
            SpriteQuad[2] = (sprite) {0, 8, 8, 0};
            SpriteQuad[3] = (sprite) {8, 8, 9, 0};
            break;
        case DirDown:
            SpriteQuad[0] = (sprite) {0, 1, 10, 0};
            SpriteQuad[1] = (sprite) {8, 1, 10, SprHorzFlag};
            if(Object->IsRight) {
                SpriteQuad[2] = (sprite) {0, 8, 13, SprHorzFlag};
                SpriteQuad[3] = (sprite) {8, 8, 12, SprHorzFlag};
            } else {
                SpriteQuad[2] = (sprite) {0, 8, 12, 0};
                SpriteQuad[3] = (sprite) {8, 8, 13, 0};
            }
            break;
        case DirRight:
            SpriteQuad[0] = (sprite) {0, 1, 5, SprHorzFlag};
            SpriteQuad[1] = (sprite) {8, 1, 4, SprHorzFlag};
            SpriteQuad[2] = (sprite) {0, 8, 9, SprHorzFlag};
            SpriteQuad[3] = (sprite) {8, 8, 8, SprHorzFlag};
            break;
        }
        if(Object->Tick % 8 == 4) {
            Object->IsRight ^= 1;
        }
    }

    /*SetSpriteQuad*/
    for(int SpriteI = 0; SpriteI < 4; SpriteI++) {
        SpriteQuad[SpriteI].X += QuadX;
        SpriteQuad[SpriteI].Y += QuadY;
        SpriteQuad[SpriteI].Tile += Object->Tile;
    }
}

static int ObjectInUpdateBounds(point QuadPt) {
    return QuadPt.X > -16 && QuadPt.X < 176 && QuadPt.Y > -16 && QuadPt.Y < 160;
}

/*World Functions*/
NONNULL_PARAMS __attribute__((warn_unused_result))
static const char *LoadNextMap(world *World, int DeltaX, int DeltaY) {
    const char *Path = NULL;
    point AddTo = {DeltaX, DeltaY};
    point CurMapPt = World->Maps[World->MapI].Loaded;
    point NewMapPt = AddPoints(CurMapPt, AddTo);
    if(!ComparePoints(World->Maps[!World->MapI].Loaded, NewMapPt)) {
        if(NewMapPt.X >= 0 && NewMapPt.X < World->Width &&
           NewMapPt.Y >= 0 && NewMapPt.Y < World->Height) {
            Path = World->MapPaths[NewMapPt.Y][NewMapPt.X];
            if(Path) {
                TRY(ReadMap(Path, &World->Maps[!World->MapI]), "Could not load map"); 
                World->Maps[!World->MapI].Loaded = NewMapPt;
            }
        }
    }
    return Path;
out:
    return NULL;
}

NONNULL_PARAMS
static void PlaceMap(world *World, 
                     uint8_t TileMap[32][32], point TileMin, 
                     const uint8_t QuadData[128][4], rect QuadRect) {
    int TileY = TileMin.Y;
    for(int QuadY = QuadRect.Top; QuadY < QuadRect.Bottom; QuadY++) {
        int TileX = TileMin.X;
        for(int QuadX = QuadRect.Left; QuadX < QuadRect.Right; QuadX++) {
            quad_info QuadInfo = {
                .Point = (point) {QuadX, QuadY},
                .Map = World->MapI,
                .Dir = GetMapDir(World->Maps, QuadInfo.Map),
                .Quad = -1
            };
            int Quad = GetQuad(World->Maps, QuadInfo).Quad;
            if(Quad < 0) {
                Quad = World->Maps[QuadInfo.Map].DefaultQuad;
            }
            SetToTiles(TileMap, TileX, TileY, QuadData[Quad]); 
            TileX = (TileX + 2) % 32;
        }
        TileY = (TileY + 2) % 32;
    } 
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
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            dim_rect ClientRect = GetDimClientRect(Window);

            int RenderWidth = ClientRect.Width - ClientRect.Width % BmWidth;
            int RenderHeight = ClientRect.Height - ClientRect.Height % BmHeight;

            int RenderColSize = RenderWidth * BmHeight;
            int RenderRowSize = RenderHeight * BmWidth;
            if(RenderColSize > RenderRowSize) {
                RenderWidth = RenderRowSize / BmHeight;
            } else {
                RenderHeight = RenderColSize / BmWidth;
            }
            int RenderX = (ClientRect.Width - RenderWidth) / 2;
            int RenderY = (ClientRect.Height - RenderHeight) / 2;

            StretchDIBits(DeviceContext, RenderX, RenderY, RenderWidth, RenderHeight,
                          0, 0, BmWidth, BmHeight, g_BmPixels, g_BmInfo, DIB_RGB_COLORS, SRCCOPY);
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

int WINAPI WinMain(HINSTANCE Instance, UNUSED HINSTANCE Prev, UNUSED LPSTR CmdLine, int CmdShow) {
    srand(time(NULL));

    /*Predeclared for error cleanup*/
    HWND Window = NULL;
    int IsGranular = 0;

    /*InitBitmap*/
    g_BmInfo = alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 4);
    g_BmInfo->bmiHeader = (BITMAPINFOHEADER) {
        .biSize = sizeof(g_BmInfo->bmiHeader),
        .biWidth = BmWidth,
        .biHeight = -BmHeight,
        .biPlanes = 1,
        .biBitCount = 8,
        .biCompression = BI_RGB,
        .biClrUsed = 4
    };
    g_BmInfo->bmiColors[0] = (RGBQUAD) {0xF8, 0xF8, 0xF8, 0x00};
    g_BmInfo->bmiColors[1] = (RGBQUAD) {0xA8, 0xA8, 0xA8, 0x00};
    g_BmInfo->bmiColors[2] = (RGBQUAD) {0x80, 0x80, 0x80, 0x00};
    g_BmInfo->bmiColors[3] = (RGBQUAD) {0x00, 0x00, 0x00, 0x00};

    /*InitWindow*/
    WNDCLASS WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = Instance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = "PokeWindowClass"
    };
    TRY(RegisterClass(&WindowClass), "Could not register class");

    RECT WinWindowRect = {0, 0, BmWidth, BmHeight};
    AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, 0);

    dim_rect WindowRect = WinToDimRect(WinWindowRect);

    Window = CreateWindow(
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
    TRY(Window, "Could not create window");

    TRY(SetCurrentDirectory("../Shared"), "Could not find shared directory");

    /*LoadQuadData*/
    uint8_t QuadData[128][4] = {};
    int SetsRead = ReadAll("QuadData", QuadData, sizeof(QuadData)) / 4; 
    for(int Set = 0; Set < SetsRead; Set++) {
        for(int I = 0; I < 4; I++) {
            QuadData[Set][I] = Clamp(QuadData[Set][I], 0, 95);
        }
    }
    
    /*ReadQuadProps*/
    uint8_t QuadProps[128] = {};
    TRY_HERE(ReadAll("QuadProps", QuadProps, sizeof(QuadProps)) == 128,  "Could not load quad props");

    /*LoadTileData*/
    uint8_t TileData[256 * TileSize];
    uint8_t SpriteData[256 * TileSize];
    uint8_t FlowerData[3 * TileSize];
    uint8_t WaterData[7 * TileSize];

#define LOAD_TILE_DATA(Name, Data, Offset, Index) \
    TRY_HERE(ReadTileData(Name, Data + (Offset) * TileSize, Index), "Could not load tile data: " Name) 

    LOAD_TILE_DATA("TileData", TileData, 0, 110);
    LOAD_TILE_DATA("Numbers", TileData, 110, 146);
    LOAD_TILE_DATA("SpriteData", SpriteData, 0, 256);
    LOAD_TILE_DATA("FlowerData", FlowerData, 0, 3);
    LOAD_TILE_DATA("WaterData", WaterData, 0, 7);
    LOAD_TILE_DATA("ShadowData", SpriteData, 255, 1);

    /*InitMaps*/
    world World = {
        .Width = 1,
        .Height = 4,
        .MapI = 0,
        .Player = {
            .Pos = {80, 96},
            .Dir = DirDown,
            .Speed = 2,
            .Tile = 0,
        },
        .Maps = {
            [0] = {
                .Loaded = {0, 2}
            }
        },
        .MapPaths = {
            {"VirdianCity"},
            {"Route1"},
            {"PalleteTown"},
            {"Route21"}
        }
    };

    TRY_HERE(ReadMap("PalleteTown", &World.Maps[0]), "Could not load map");

    /*InitObjects*/
    /*
    object Objs[] = {
        {
            .Pos = {80, 96},
            .Dir = DirDown,
            .Speed = 2,
            .Tile = 0,
            .Map = 2
        }, 
        
        {
            .Pos = {176, 224},
            .Dir = DirDown,
            .Speed = 1,
            .Tile = 14,
            .Map = 0
        },

        {
            .Pos = {48, 128},
            .Dir = DirDown,
            .Speed = 1,
            .Tile = 28,
            .Map = 0,
        },

        {
            .Pos = {80, 384},
            .Dir = DirDown,
            .Speed = 1,
            .Tile = 98,
            .Map = 1
        }
    };
    
    int ObjCount = 4;
    */
    DisplayAllErrors(Window);

    /*GameBoyGraphics*/
    sprite Sprites[40] = {};
    uint8_t TileMap[32][32];

    uint8_t ScrollX = 0;
    uint8_t ScrollY = 0;

    /*TranslateFullMapToTiles*/
    rect QuadRect = {
        .Left = World.Player.Pos.X / 16 - 4,
        .Top = World.Player.Pos.Y / 16 - 4,
        .Right = QuadRect.Left + 10,
        .Bottom = QuadRect.Top + 9
    };
    point TileMin = {};
    PlaceMap(&World, TileMap, TileMin, QuadData, QuadRect);

    /*WindowMap*/
    uint8_t WindowMap[32][32];
    uint8_t WindowY = 144;

    /*PlayerState*/
    int IsLeaping = 0;
    int IsPaused = 0;
    
    /*NonGameState*/
    dim_rect RestoreRect = {};
    int HasQuit = 0;
    int IsFullscreen = 0;

    /*Text*/
    const char *ActiveText = "Text not init";
    point TextTilePt = {1, 14};
    uint64_t TextTick = 0;

    /*Timing*/
    uint64_t Tick = 0;
    int64_t BeginCounter = 0;
    int64_t PerfFreq = GetPerfFreq();
    int64_t FrameDelta = PerfFreq / 30;

    /*LoadWinmm*/
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

        int TickCycle = Tick / 16;

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
                    if(!(Message.lParam & LParamKeyIsHeld)) {
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

        /*InitSharedUpdate*/
        sprite *PlayerQuad = Sprites;

        /*UpdatePlayer*/
        if(IsPaused) {
            if(ActiveText[0]) {
                switch(TextTilePt.Y) {
                case 17:
                    /*MoreTextDisplay*/
                    if(TextTick > 0) {
                        TextTick--;
                    } else switch(ActiveText[-1]) {
                    case '\n':
                        memcpy(&WindowMap[14][1], &WindowMap[15][1], 17);
                        memset(&WindowMap[13][1], 0, 17);
                        memset(&WindowMap[15][1], 0, 17);
                        TextTilePt.Y = 16;
                        break;
                    case '\f':
                        TextTilePt.Y = 14;
                        break;
                    }
                    break;
                case 18:
                    /*MoreTextWait*/
                    WindowMap[16][18] = TextTick / 16 % 2 ? 0 : 178;
                    if(g_Keys['X']) {
                        if(ActiveText[-1] == '\n') {
                            memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                            memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                        }
                        memset(&WindowMap[14][1], 0, 17);
                        memset(&WindowMap[16][1], 0, 18);
                        TextTilePt.Y = 17;
                        g_Keys['X'] = 0;
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
                        /*RenderNextChar*/
                        {
                            int Output = ActiveText[0];
                            if(Output >= '0' && Output <= ':') {
                                Output += 110 - '0';
                            } else if(Output >= 'A' && Output <= 'Z') {
                                Output += 121 - 'A';
                            } else if(Output >= 'a' && Output <= 'z') {
                                Output += 147 - 'a';
                            } else if(Output == '!') {
                                Output = 175;
                            } else if(Output == 'é') {
                                Output = 173;
                            } else if(Output == '\'') {
                                Output = 176;
                            } else if(Output == '~') {
                                Output = 179;
                            } else if(Output == '-') {
                                Output = 177;
                            } else if(Output == ',') {
                                Output = 180; 
                            } else {
                                Output = 0;
                            }
                            WindowMap[TextTilePt.Y][TextTilePt.X] = Output;
                        }
                        TextTilePt.X++;
                    }
                    ActiveText++;
                }
            } else if(g_Keys['X']) {
                IsPaused = 0;
                WindowY = 144;
                g_Keys['X'] = 0;
            }
        } else {
            sprite *ShadowQuad = Sprites + 36;

            /*PlayerUpdate*/
            if(World.Player.Tick == 0) {
                memset(ShadowQuad, 0, sizeof(*ShadowQuad) * 4);
                int AttemptLeap = 0;

                /*PlayerKeyboard*/
                int HasMoveKey = 1;
                if(g_Keys['W']) {
                    World.Player.Dir = DirUp;
                } else if(g_Keys['A']) {
                    World.Player.Dir = DirLeft;
                } else if(g_Keys['S']) {
                    World.Player.Dir = DirDown;
                    AttemptLeap = 1;
                } else if(g_Keys['D']) {
                    World.Player.Dir = DirRight;
                } else {
                    HasMoveKey = 0;
                }

                point NewPoint = GetFacingPoint(World.Player.Pos, World.Player.Dir);

                /*FetchQuadProp*/
                quad_info QuadInfo = {
                    .Point = NewPoint,
                    .Map = World.MapI,
                    .Dir = GetMapDir(World.Maps, QuadInfo.Map),
                    .Quad = -1,
                };
                QuadInfo = GetQuad(World.Maps, QuadInfo);

                NewPoint = QuadInfo.Point;
                int Quad = QuadInfo.Quad;
                int NewMap = QuadInfo.Map;

                point StartPos = AddPoints(NewPoint, g_DirPoints[ReverseDir(World.Player.Dir)]); 
                StartPos.X *= 16;
                StartPos.Y *= 16;

                int Prop = Quad < 0 ? QuadPropSolid : QuadProps[Quad];

                /*FetchObject*/
                AttemptLeap &= !!(Prop & QuadPropEdge);
                point TestPoint = NewPoint; 
                if(AttemptLeap) {
                    TestPoint.Y++;
                }

                object *FacingObject = NULL; 
                FOR_VEC(Object, World.Maps[World.MapI].Objects) {
                    point OtherOldPoint = ConvertToQuadPoint(Object->Pos);
                    if(Object->Tick > 0) {
                        point OtherDirPoint = NextPoints[Object->Dir];
                        point OtherNewPoint = AddPoints(OtherOldPoint, OtherDirPoint);
                        if(ComparePoints(TestPoint, OtherOldPoint) ||
                           ComparePoints(TestPoint, OtherNewPoint)) {
                            Prop = QuadPropSolid;
                            FacingObject = Object;
                            break;
                        }
                    } else if(ComparePoints(TestPoint, OtherOldPoint)) {
                        Prop = QuadPropSolid;
                        if(!AttemptLeap) {
                            Prop |= QuadPropMessage;
                        }
                        FacingObject = Object;
                        break;
                    }
                }

                /*PlayerTestProp*/
                if(g_Keys['X']) {
                    if(Prop & QuadPropMessage) {
                        IsPaused = 1;
                        WindowY = 96;
                        g_Keys['X'] = 0;

                        /*PlaceTextBox*/
                        uint8_t TextBoxHeadRow[] = {96, [1 ... 18] = 97, 98};
                        uint8_t TextBoxBodyRow[] = {99, [1 ... 18] = 0, 100};
                        uint8_t TextBoxFooterRow[] = {101, [1 ... 18] = 97, 103};
                        memcpy(WindowMap[12], TextBoxHeadRow, sizeof(TextBoxHeadRow));
                        for(int Y = 13; Y < 17; Y++) {
                            memcpy(WindowMap[Y], TextBoxBodyRow, sizeof(TextBoxBodyRow));
                        }
                        memcpy(WindowMap[17], TextBoxFooterRow, sizeof(TextBoxFooterRow));

                        /*GetActiveText*/
                        /*
                        const char *ObjectText[] = {
                            "Technology is\nincredible!\f"
                            "You can now store\nand recall items\nand POKéMON as\ndata via PC!",
                            
                            "I~ raising\nPOKéMON too!\f"
                            "When they get\nstrong, they can\nprotect me!",

                            "We also carry\nPOKé BALLS for\ncatching POKéMON!"
                        };
                        */

                        if(FacingObject) {
                            ActiveText = FacingObject->Str.Data;
                            FacingObject->Dir = ReverseDir(World.Player.Dir);
                        } else {
                            ActiveText = "No text found";
                            FOR_VEC(Text, World.Maps[World.MapI].Texts) {
                                if(ComparePoints(Text->Pos, NewPoint)) {
                                    ActiveText = Text->Str.Data;
                                    break;
                                }
                            }
                        }

                        TextTilePt = (point) {1, 14};
                        TextTick = 0;

                        HasMoveKey = 0;
                    } else if(Prop & QuadPropWater) {
                        if(World.Player.Tile == AnimPlayer) {
                            World.Player.Tile = AnimSeal;
                            HasMoveKey = 1;
                        } 
                    } else if(World.Player.Tile == AnimSeal && !(Prop & QuadPropSolid)) {
                        World.Player.Tile = AnimPlayer;
                        HasMoveKey = 1;
                    }
                } 

                if(World.Player.Tile != 0) {
                    if(Prop & QuadPropWater) {
                        Prop &= ~QuadPropSolid;
                    } else {
                        Prop |= QuadPropSolid;
                    }
                } else if(Prop & QuadPropWater) {
                    Prop |= QuadPropSolid;
                }

                if(HasMoveKey) {
                    /*MovePlayer*/
                    IsLeaping = 0;
                    if(World.Player.Dir == DirDown && Prop & QuadPropEdge) {
                        World.Player.State = StateMoving;
                        IsLeaping = 1;

                        /*CreateShadowQuad*/
                        ShadowQuad[0] = (sprite) {72, 72, 255, 0};
                        ShadowQuad[1] = (sprite) {80, 72, 255, SprHorzFlag};
                        ShadowQuad[2] = (sprite) {72, 80, 255, SprVertFlag};
                        ShadowQuad[3] = (sprite) {80, 80, 255, SprHorzFlag | SprVertFlag};
                    } else if(Prop & QuadPropSolid) {
                        World.Player.State = StateStop;
                    } else {
                        World.Player.State = StateMoving;
                    }
                    World.Player.Tick = IsLeaping ? 16 : 8;
                } else {
                    World.Player.State = StateStop;
                }

                if(World.Player.State == StateMoving) {
                    World.Player.Pos = StartPos;
                    World.MapI = NewMap;

                    /*UpdateLoadedMaps*/
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

                    const char *Path = NULL;
                    if(AddToX) {
                        Path = LoadNextMap(&World, AddToX, 0);
                    } 

                    if(AddToY && !Path) {
                        Path = LoadNextMap(&World, 0, AddToY);
                    }
                    DisplayAllErrors(Window);

                    /*TranslateQuadRectToTiles*/
                    point TileMin = {};
                    rect QuadRect = {};

                    switch(World.Player.Dir) {
                    case DirUp:
                        TileMin.X = (ScrollX / 8) & 31;
                        TileMin.Y = (ScrollY / 8 + 30) & 31;

                        QuadRect = (rect) {
                            .Left = NewPoint.X - 4,
                            .Top = NewPoint.Y - 4, 
                            .Right = NewPoint.X + 6,
                            .Bottom = NewPoint.Y - 3
                        };
                        break;
                    case DirLeft:
                        TileMin.X = (ScrollX / 8 + 30) & 31;
                        TileMin.Y = (ScrollY / 8) & 31;

                        QuadRect = (rect) {
                            .Left = NewPoint.X - 4,
                            .Top = NewPoint.Y - 4,
                            .Right = NewPoint.X - 3,
                            .Bottom = NewPoint.Y + 5
                        };
                        break;
                    case DirDown:
                        TileMin.X = (ScrollX / 8) & 31;
                        TileMin.Y = (ScrollY / 8 + 18) & 31;

                        QuadRect = (rect) {
                            .Left = NewPoint.X - 4,
                            .Top = NewPoint.Y + 4,
                            .Right = NewPoint.X + 6,
                            .Bottom = NewPoint.Y + (IsLeaping ? 6 : 5)
                        };
                        break;
                    case DirRight:
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
                    PlaceMap(&World, TileMap, TileMin, QuadData, QuadRect);
                }
            }
        }

        int SprI = 0;

        /*UpdatePlayer*/
        sprite *SpriteQuad = &Sprites[SprI];
        point PlayerPt = {72, 72};
        UpdateAnimation(&World.Player, SpriteQuad, PlayerPt);
        if(!IsPaused && World.Player.Tick > 0) {
            MoveEntity(&World.Player);
            if(World.Player.State == StateMoving) {
                switch(World.Player.Dir) {
                case DirUp:
                    ScrollY -= World.Player.Speed;
                    break;
                case DirLeft:
                    ScrollX -= World.Player.Speed;
                    break;
                case DirRight:
                    ScrollX += World.Player.Speed;
                    break;
                case DirDown:
                    ScrollY += World.Player.Speed;
                    break;
                }
            }
        }
        SprI += 4;
   
        /*UpdateObjects*/
        for(int MapI = 0; MapI < _countof(World.Maps); MapI++) {
            map *Map = &World.Maps[MapI];
            FOR_VEC(Object, Map->Objects) {
                sprite *SpriteQuad = &Sprites[SprI];
                point QuadPt = {
                    Object->Pos.X - World.Player.Pos.X + 72,
                    Object->Pos.Y - World.Player.Pos.Y + 72
                };

                if(World.MapI != MapI) {
                    switch(GetMapDir(World.Maps, World.MapI)) {
                    case DirUp:
                        QuadPt.Y -= World.Maps[MapI].Height * 16;
                        break;
                    case DirLeft:
                        QuadPt.X -= World.Maps[MapI].Width * 16;
                        break;
                    case DirDown:
                        QuadPt.Y += World.Maps[World.MapI].Height * 16;
                        break;
                    case DirRight:
                        QuadPt.X += World.Maps[World.MapI].Width * 16;
                        break;
                    }
                }

                if(ObjectInUpdateBounds(QuadPt)) {
                    UpdateAnimation(Object, SpriteQuad, QuadPt);
                    /*TickFrame*/
                    if(!IsPaused) {
                        if(Object->Tick > 0) {
                            MoveEntity(Object);
                        } else /*if(I != 0)*/ {
                            RandomMove(Map, Object, &World.Player, QuadProps);
                        } 
                    }
                    SprI += 4;
                }
            }
        }

        /*PlayerUpdateJumpingAnimation*/
        if(IsLeaping) {
            uint8_t PlayerDeltas[] = {0, 4, 6, 8, 9, 10, 11, 12};
            uint8_t DeltaI = World.Player.Tick < 8 ? World.Player.Tick: 15 - World.Player.Tick;
            FOR_RANGE(Tile, PlayerQuad, 0, 4) { 
                Tile->Y -= PlayerDeltas[DeltaI];
            }
        }

        /*MutTileUpdate*/
        if(Tick % 16 == 0) {
            /*FlowersUpdate*/
            uint8_t *FlowerDst = TileData + 3 * TileSize;
            uint8_t *FlowerSrc = FlowerData + TickCycle % 3 * TileSize;
            memcpy(FlowerDst, FlowerSrc, TileSize);

            /*WaterUpdate*/
            int TickMod = TickCycle % 9 < 5 ? TickCycle % 9 : 9 - TickCycle % 9;
            uint8_t *WaterDst = TileData + 20 * TileSize;
            uint8_t *WaterSrc = WaterData + TickMod * TileSize;
            memcpy(WaterDst, WaterSrc, TileSize);
        }

        /*RenderTiles*/
        uint8_t (*BmRow)[BmWidth] = g_BmPixels;

        int MaxPixelY = WindowY < BmHeight ? WindowY : BmHeight; 

        for(int PixelY = 0; PixelY < MaxPixelY; PixelY++) {
            int PixelX = 8 - (ScrollX & 7);
            int SrcYDsp = ((PixelY + ScrollY) & 7) << 3;
            int TileX = ScrollX >> 3;
            int TileY = (PixelY + ScrollY) / 8 % 32;
            int StartOff = SrcYDsp | (ScrollX & 7);
            memcpy(*BmRow, TileData + StartOff + TileMap[TileY][TileX] * TileSize, 8);

            for(int Repeat = 1; Repeat < 20; Repeat++) {
                TileX = (TileX + 1) % 32;
                uint8_t *Pixel = *BmRow + PixelX;
                memcpy(Pixel, TileData + SrcYDsp + TileMap[TileY][TileX] * TileSize, 8);

                PixelX += 8;
            }
            TileX = (TileX + 1) % 32;
            uint8_t *Pixel = *BmRow + PixelX;
            memcpy(Pixel, TileData + SrcYDsp + TileMap[TileY][TileX] * TileSize, BmWidth - PixelX);
            BmRow++;
        }

        for(int PixelY = MaxPixelY; PixelY < BmHeight; PixelY++) {
            int PixelX = 0;
            int SrcYDsp = (PixelY & 7) << 3;
            int TileX = 0;
            int TileY = PixelY / 8;

            for(int Repeat = 0; Repeat < 20; Repeat++) {
                memcpy(*BmRow + PixelX, TileData + SrcYDsp + WindowMap[TileY][TileX] * TileSize, 8);
                TileX++;
                PixelX += 8;
            }
            BmRow++;
        }

        /*RenderSprites*/
        int MaxX = BmWidth;
        int MaxY = WindowY < BmHeight ? WindowY : BmHeight;

        for(size_t I = SprI; I-- > 0; ) {
            int RowsToRender = 8;
            if(Sprites[I].Y < 8) {
                RowsToRender = Sprites[I].Y;
            } else if(Sprites[I].Y > MaxY) {
                RowsToRender = Max(MaxY + 8 - Sprites[I].Y, 0);
            }

            int ColsToRender = 8;
            if(Sprites[I].X < 8) {
                ColsToRender = Sprites[I].X;
            } else if(Sprites[I].X > MaxX) {
                ColsToRender = Max(MaxX + 8 - Sprites[I].X, 0);
            }

            int DstX = Max(Sprites[I].X - 8, 0);
            int DstY = Max(Sprites[I].Y - 8, 0);

            int SrcX = Max(8 - Sprites[I].X, 0);
            int DispX = 1;
            if(Sprites[I].Flags & SprHorzFlag) {
                SrcX = Min(Sprites[I].X, 7);
                DispX = -1;
            }
            int DispY = 8;
            int SrcY = Max(8 - Sprites[I].Y, 0);
            if(Sprites[I].Flags & SprVertFlag) {
                SrcY = Min(Sprites[I].Y, 7);
                DispY = -8;
            }

            uint8_t *Src = SpriteData + Sprites[I].Tile * TileSize + SrcY * TileLength + SrcX;

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

out: 
    /*CleanUp*/
    if(IsGranular) {
        TimeEndPeriod(1);
    }

    DisplayAllErrors(Window);
    return g_Errors.Count != 0;
}


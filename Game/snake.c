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
        log_error(__func__, (Str));\
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

#define READ_OBJECT(Path, Data) (read_all(Path, (Data), sizeof(Data)) == sizeof(Data))
#define COPY_OBJECT(Dst, Src) memcpy((Dst), (Src), sizeof(Src))

#define SEARCH_FROM_POS(Vec, InPos) ({\
    AUTO_VAR(Vec_, (Vec));\
    point Pos_ = (InPos);\
\
    typeof((Vec)->Data[0]) *Ret = NULL;\
    FOR_VEC(E, *Vec_) {\
        if(equal_points(E->Pos, Pos_)) {\
            Ret = E;\
            break;\
        }\
    }\
    Ret;\
})

/*Macro Constants*/
#define ANIM_PLAYER 0
#define ANIM_SEAL 84

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
    int width;
    int height;
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

    uint8_t Dir;
    uint8_t Speed;
    uint8_t Tile;

    uint8_t IsMoving;
    uint8_t IsRight;
    uint8_t Tick;

    FC_VEC(char, 256) Str;
} object;

typedef struct game_map {
    int16_t width;
    int16_t height;
    int16_t DefaultQuad;
    uint8_t quads[256][256];
    point loaded;
    FC_VEC(text, 256) Texts;
    FC_VEC(object, 256) Objects;
    FC_VEC(door, 256) Doors;
} game_map;

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

typedef struct game_world {
    SELECT(data_path, 2) DataPaths;

    uint8_t tile_data[256 * TILE_SIZE];
    uint8_t QuadProps[128];
    uint8_t QuadData[128][4];

    SELECT(game_map, 2) maps;
    object Player;
    const char *MapPaths[WORLD_HEIGHT][WORLD_WIDTH];

    int IsOverworld;
} game_world;

/*Global Constants*/
static const point g_dir_points[] = {
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
static int min_int(int A, int B) {
    return A < B ? A : B;
}

static int max_int(int A, int B) {
    return A > B ? A : B;
}

static int clamp_int(int Value, int Bottom, int Top) {
    return min_int(max_int(Value, Bottom), Top);
}

static int abs_int(int Val) {
    return Val < 0 ? -Val : Val; 
}

static int pos_int_mod(int A, int B) {
    return A % B < 0 ? A % B + abs_int(B) : A % B; 
}

static int reverse_dir(int Dir) {
    return (Dir + 2) % 4;
}

static int correct_tile_coord(int TileCoord) {
    return pos_int_mod(TileCoord, 32);
}

/*Timing*/
static int64_t get_perf_freq(void) {
    LARGE_INTEGER PerfFreq;
    QueryPerformanceFrequency(&PerfFreq);
    return PerfFreq.QuadPart;
}

static int64_t get_perf_counter(void) {
    LARGE_INTEGER PerfCounter;
    QueryPerformanceCounter(&PerfCounter);
    return PerfCounter.QuadPart;
}

static int64_t get_delta_counter(int64_t BeginCounter) {
    return get_perf_counter() - BeginCounter;
}

/*Point Functions*/ 
static point add_points(point A, point B)  {
    return (point) {
        .X = A.X + B.X,
        .Y = A.Y + B.Y
    };
}

static point sub_points(point A, point B)  {
    return (point) {
        .X = A.X - B.X,
        .Y = A.Y - B.Y
    };
}

static point mul_point(point A, short B) {
    return (point) {
        .X = A.X * B,
        .Y = A.Y * B 
    };
}

static int equal_points(point A, point B) {
    return A.X == B.X && A.Y == B.Y;
}

static int point_in_map(const game_map *Map, point Pt) {
    int InX = Pt.X >= 0 && Pt.X < Map->width; 
    int InY = Pt.Y >= 0 && Pt.Y < Map->height; 
    return InX && InY;
}

static int point_in_world(point Pt) {
    int InX = Pt.X >= 0 && Pt.X < WORLD_WIDTH;
    int InY = Pt.Y >= 0 && Pt.Y < WORLD_HEIGHT;
    return InX && InY;
}

/*DimRect Functions*/
static dim_rect win_to_dim_rect(RECT WinRect) {
    return (dim_rect) {
        .X = WinRect.left,
        .Y = WinRect.top,
        .width = WinRect.right - WinRect.left,
        .height = WinRect.bottom - WinRect.top
    };
}

static dim_rect get_dim_client_rect(HWND Window) {
    RECT WinClientRect;
    GetClientRect(Window, &WinClientRect);
    return win_to_dim_rect(WinClientRect);
}

static dim_rect get_dim_window_rect(HWND Window) {
    RECT WinWindowRect;
    GetWindowRect(Window, &WinWindowRect);
    return win_to_dim_rect(WinWindowRect);
}

/*Point Conversion Functions*/
static point pt_to_quad_pt(point Point) {
    point QuadPoint = {
        .X = Point.X / 16,
        .Y = Point.Y / 16
    };
    return QuadPoint;
}

static point quad_pt_to_pt(point Point) {
    point QuadPoint = {
        .X = Point.X * 16,
        .Y = Point.Y * 16
    };
    return QuadPoint;
}

static point get_facing_point(point Pos, int Dir) {
    point OldPoint = pt_to_quad_pt(Pos);
    point DirPoint = g_dir_points[Dir];
    return add_points(OldPoint, DirPoint);
}

/*Quad Functions*/
static void set_to_tiles(uint8_t TileMap[32][32], int TileX, int TileY, const uint8_t Set[4]) {
    TileMap[TileY][TileX] = Set[0];
    TileMap[TileY][TileX + 1] = Set[1];
    TileMap[TileY + 1][TileX] = Set[2];
    TileMap[TileY + 1][TileX + 1] = Set[3];
}

static int get_map_dir(const game_map maps[static 2], int map_i) {
    point dir_point = sub_points(maps[!map_i].loaded, maps[map_i].loaded);
    for(size_t I = 0; I < _countof(g_dir_points); I++) {
        if(equal_points(dir_point, g_dir_points[I])) {
            return I;
        }
    }
    return -1; 
}

static quad_info get_quad(const game_world *world, point Point) {
    quad_info QuadInfo = {.Map = world->maps.I, .Point = Point};

    int Oldwidth = world->maps.Data[QuadInfo.Map].width;
    int Oldheight = world->maps.Data[QuadInfo.Map].height;

    int Newwidth = world->maps.Data[!QuadInfo.Map].width;
    int Newheight = world->maps.Data[!QuadInfo.Map].height;

    switch(get_map_dir(world->maps.Data, QuadInfo.Map)) {
    case DIR_UP:
        if(QuadInfo.Point.Y < 0) {
            QuadInfo.Point.X += (Newwidth - Oldwidth) / 2;
            QuadInfo.Point.Y += Newheight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DIR_LEFT:
        if(QuadInfo.Point.X < 0) {
            QuadInfo.Map ^= 1;
            QuadInfo.Point.X += Newheight; 
            QuadInfo.Point.Y += (Newheight - Oldheight) / 2;
        }
        break;
    case DIR_DOWN:
        if(QuadInfo.Point.Y >= world->maps.Data[QuadInfo.Map].height) {
            QuadInfo.Point.X += (Newwidth - Oldwidth) / 2;
            QuadInfo.Point.Y -= Oldheight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DIR_RIGHT:
        if(QuadInfo.Point.X >= world->maps.Data[QuadInfo.Map].width) {
            QuadInfo.Point.X -= Oldwidth; 
            QuadInfo.Point.Y += (Newheight - Oldheight) / 2;
            QuadInfo.Map ^= 1;
        }
        break;
    }

    if(point_in_map(&world->maps.Data[QuadInfo.Map], QuadInfo.Point)) {
        QuadInfo.Quad = world->maps.Data[QuadInfo.Map].quads[QuadInfo.Point.Y][QuadInfo.Point.X];
        QuadInfo.Prop = world->QuadProps[QuadInfo.Quad];
    } else {
        QuadInfo.Quad = world->maps.Data[QuadInfo.Map].DefaultQuad;
        QuadInfo.Prop = QUAD_PROP_SOLID;
    }

    return QuadInfo;
}

/*Error*/
NONNULL_PARAMS
static void log_error(const char *Func, const char *Error) {
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

static void display_all_errors(HWND Window) {
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
static void copy_chars_with_null(char *Dest, const char *Source, size_t Length) {
    memcpy(Dest, Source, Length);
    Dest[Length] = '\0';
}

/*Cleanup Functions*/
static void clean_up_time(int *IsGranular) {
    if(*IsGranular) {
        g_TimeEndPeriod(1);
        *IsGranular = 0;
    }
}

static void clean_up_window(HWND *Window) {
    display_all_errors(*Window); 
    if(Window) {
        DestroyWindow(*Window);
    }
    *Window = NULL;
}

static void clean_up_handle(HANDLE *Handle) {
    if(*Handle != INVALID_HANDLE_VALUE) {
        CloseHandle(*Handle);
    }
} 


/*Data loaders*/
static int64_t get_full_file_size(HANDLE FileHandle) {
    LARGE_INTEGER FileSize;
    int Success = GetFileSizeEx(FileHandle, &FileSize);
    TRY(Success, "Could not obtain file size", -1);
    return FileSize.QuadPart;
}

NONNULL_PARAMS RET_ERROR
static int read_all(const char *Path, void *Bytes, int MaxBytesToRead) {
    DWORD BytesRead = 0;
    CLEANUP(HANDLE, clean_up_handle) FileHandle = CreateFile(
        Path, 
        GENERIC_READ, 
        FILE_SHARE_READ, 
        NULL, 
        OPEN_EXISTING, 
        0, 
        NULL
    );
    TRY(FileHandle != INVALID_HANDLE_VALUE, "File could not be found", -1); 
    int64_t FileSize = get_full_file_size(FileHandle);
    TRY(FileSize >= 0, "File size could not be obtained", -1);
    TRY(FileSize <= MaxBytesToRead, "File is too big for buffer", -1);
        
    ReadFile(FileHandle, Bytes, FileSize, &BytesRead, NULL); 
    TRY(BytesRead == FileSize, "Could not read all of file", -1);

    return BytesRead;
}

NONNULL_PARAMS RET_ERROR
static int read_tile_data(const char *Path, uint8_t *tile_data, int MaxTileCount) {
    const int SizeOfCompressedTile = 16;
    int SizeOfCompressedtile_data = MaxTileCount * SizeOfCompressedTile;

    uint8_t CompData[SizeOfCompressedtile_data];
    int BytesRead = read_all(Path, CompData, sizeof(CompData));
    int TilesRead = BytesRead / SizeOfCompressedTile;
    int BytesValid = TilesRead * SizeOfCompressedTile;

    uint8_t *CompPtr = CompData;
    uint8_t *TilePtr = tile_data;
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
static int read_map(game_world *world, int MapI, const char *Path) {
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
        copy_chars_with_null(Str.Data, MOVE_RUN(Length), Length);\
    } while(0)

    uint8_t RunData[65536];
    int RunIndex = 0;
    int BytesRead = read_all(Path, RunData, sizeof(RunData));
    TRY(BytesRead >= 0, "Could not read game_map", 0);

    /*ReadQuadSize*/
    game_map *Map = &world->maps.Data[MapI];
    Map->width = NEXT_RUN + 1;
    Map->height = NEXT_RUN + 1;
    int Size = Map->width * Map->height;

    /*Readquads*/
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

        TRY(QuadIndex + Repeat <= Size, "quads are truncated", 0);

        for(int I = QuadIndex; I < QuadIndex + Repeat; I++) {
            Map->quads[I / Map->width][I % Map->width] = Quad; 
        }

        QuadIndex += Repeat;
    }
    TRY(QuadIndex >= Size, "quads are incomplete", 0);

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
        Object->Dir = NEXT_RUN;  
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
    if(world->DataPaths.I != NewDataPath) {
        world->DataPaths.I = NewDataPath;
        data_path *DataPath = CUR(&world->DataPaths);

        TRY(read_tile_data(DataPath->Tile, world->tile_data, 96), "Could not load tile data", 0);

        /*ReadQuadData*/
        TRY(READ_OBJECT(DataPath->Quad, world->QuadData), "Could not load quad data", 0); 
        FOR_ARY(Set, world->QuadData) {
            FOR_ARY(Quad, *Set) {
                *Quad = clamp_int(*Quad, 0, 95);
            }
        }
        
        /*ReadQuadProps*/
        TRY(READ_OBJECT(DataPath->Prop, world->QuadProps), "Could not load quad props", 0);
    }

    /*ExtraneousDataCheck*/
    TRY(RunIndex >= BytesRead, "Extraneous data present", 0);
    
    /*Out*/
    return 1;
#undef NEXT_STR
#undef MOVE_RUN
#undef NEXT_RUN
}

NONNULL_PARAMS RET_ERROR
static int read_overworld_map(game_world *world, int MapI, point Load) {  
    if(point_in_world(Load) && world->MapPaths[Load.Y][Load.X]) {
        const char *CurMapPath = world->MapPaths[Load.Y][Load.X];
        TRY(read_map(world, MapI, CurMapPath), "read_map failed", 0);
        world->IsOverworld = 1;
        world->maps.Data[MapI].loaded = Load; 
    }
    return 1; 
}

NONNULL_PARAMS 
static int get_loaded_point(game_world *world, int MapI, const char *MapPath) {
    for(int Y = 0; Y < WORLD_HEIGHT; Y++) {
        for(int X = 0; X < WORLD_WIDTH; X++) {
            if(world->MapPaths[Y][X] && strcmp(world->MapPaths[Y][X], MapPath) == 0) {
                world->maps.Data[MapI].loaded = (point) {X, Y};
                return 1;
            } 
        }
    }
    return 0;
}

/*Object Functions*/
static void move_entity(object *Object) {
    if(Object->IsMoving) {
        point DeltaPoint = g_dir_points[Object->Dir];
        DeltaPoint = mul_point(DeltaPoint, Object->Speed);
        Object->Pos = add_points(Object->Pos, DeltaPoint);
    }
    Object->Tick--;
}

static void random_move(const game_world *world, int MapI, object *Object) {
    const game_map *Map = &world->maps.Data[MapI];
    int Seed = rand();
    if(Seed % 64 == 0) {
        Object->Dir = Seed / 64 % 4;

        point NewPoint = get_facing_point(Object->Pos, Object->Dir);

        /*FetchQuadPropSingleMap*/
        int Quad = -1;
        int Prop = QUAD_PROP_SOLID;
        if(point_in_map(Map, NewPoint)) {
            Quad = Map->quads[NewPoint.Y][NewPoint.X];
            Prop = world->QuadProps[Quad];
        }
        
        /*IfWillColideWithPlayer*/
        point PlayerCurPoint = pt_to_quad_pt(world->Player.Pos);

        if(equal_points(PlayerCurPoint, NewPoint)) {
            Prop = QUAD_PROP_SOLID;
        } else if(world->Player.IsMoving) {
            point NextPoint = g_NextPoints[world->Player.Dir];
            point PlayerNextPoint = add_points(PlayerCurPoint, NextPoint);
            if(equal_points(PlayerNextPoint, NewPoint)) {
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

static void update_animation(object *Object, sprite *SpriteQuad, point QuadPt) {
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

static int object_in_update_bounds(point QuadPt) {
    return QuadPt.X > -16 && QuadPt.X < 176 && QuadPt.Y > -16 && QuadPt.Y < 160;
}

/*world Functions*/
NONNULL_PARAMS RET_ERROR
static int load_next_map(game_world *world, int DeltaX, int DeltaY) {
    point AddTo = {DeltaX, DeltaY};
    point CurMapPt = CUR(&world->maps)->loaded;
    point NewMapPt = add_points(CurMapPt, AddTo);
    if(!equal_points(world->maps.Data[!world->maps.I].loaded, NewMapPt)) {
        if(point_in_world(NewMapPt)) {
            TRY(read_overworld_map(world, !world->maps.I, NewMapPt), "Could not load game_map", 0);
        }
    }
    return 1;
}

NONNULL_PARAMS
static void place_map(const game_world *world, uint8_t TileMap[32][32], point TileMin, rect QuadRect) {
    int TileY = correct_tile_coord(TileMin.Y);
    for(int QuadY = QuadRect.Top; QuadY < QuadRect.Bottom; QuadY++) {
        int TileX = correct_tile_coord(TileMin.X);
        for(int QuadX = QuadRect.Left; QuadX < QuadRect.Right; QuadX++) {
            int Quad = get_quad(world, (point) {QuadX, QuadY}).Quad;
            set_to_tiles(TileMap, TileX, TileY, world->QuadData[Quad]); 
            TileX = (TileX + 2) % 32;
        }
        TileY = (TileY + 2) % 32;
    }
}

static void place_view_map(game_world *world, uint8_t TileMap[32][32], int IsDown) {
    rect QuadRect = {
        .Left = world->Player.Pos.X / 16 - 4,
        .Top = world->Player.Pos.Y / 16 - 4,
        .Right = QuadRect.Left + 10,
        .Bottom = QuadRect.Top + 9 + !!IsDown
    };
    point TileMin = {};
    place_map(world, TileMap, TileMin, QuadRect);
}

/*Win32 Functions*/
static void set_my_window_pos(HWND Window, DWORD Style, dim_rect Rect) {
    SetWindowLongPtr(Window, GWL_STYLE, Style | WS_VISIBLE);
    SetWindowPos(Window, 
                 HWND_TOP, 
                 Rect.X, Rect.Y, Rect.width, Rect.height, 
                 SWP_FRAMECHANGED | SWP_NOREPOSITION);
}

static LRESULT CALLBACK wnd_proc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
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
        dim_rect ClientRect = get_dim_client_rect(Window);

        int Renderwidth = ClientRect.width - ClientRect.width % BM_WIDTH;
        int Renderheight = ClientRect.height - ClientRect.height % BM_HEIGHT;

        int RenderColSize = Renderwidth * BM_HEIGHT;
        int RenderRowSize = Renderheight * BM_WIDTH;
        if(RenderColSize > RenderRowSize) {
            Renderwidth = RenderRowSize / BM_HEIGHT;
        } else {
            Renderheight = RenderColSize / BM_WIDTH;
        }
        int RenderX = (ClientRect.width - Renderwidth) / 2;
        int RenderY = (ClientRect.height - Renderheight) / 2;

        StretchDIBits(DeviceContext, RenderX, RenderY, Renderwidth, Renderheight,
                      0, 0, BM_WIDTH, BM_HEIGHT, g_BmPixels, g_BmInfo, DIB_RGB_COLORS, SRCCOPY);
        PatBlt(DeviceContext, 0, 0, RenderX, ClientRect.height, BLACKNESS);
        int RenderRight = RenderX + Renderwidth;
        PatBlt(DeviceContext, RenderRight, 0, RenderX, ClientRect.height, BLACKNESS);
        PatBlt(DeviceContext, RenderX, 0, Renderwidth, RenderY, BLACKNESS);
        int RenderBottom = RenderY + Renderheight;
        PatBlt(DeviceContext, RenderX, RenderBottom, Renderwidth, RenderY + 1, BLACKNESS);
        EndPaint(Window, &Paint);
        return 0;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

int WINAPI WinMain(HINSTANCE Instance, UNUSED HINSTANCE Prev, UNUSED LPSTR CmdLine, int CmdShow) {
    srand(time(NULL));

    /*InitBitmap*/
    g_BmInfo = alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 4);
    g_BmInfo->bmiHeader = (BITMAPINFOHEADER) {
        .biSize = sizeof(g_BmInfo->bmiHeader),
        .biwidth = BM_WIDTH,
        .biheight = -BM_HEIGHT,
        .biPlanes = 1,
        .biBitCount = 8,
        .biCompression = BI_RGB,
        .biClrUsed = 4
    };

    const RGBQUAD GrayScalePallete[4] = { 
        {0xF8, 0xF8, 0xF8, 0x00},
        {0xA8, 0xA8, 0xA8, 0x00},
        {0x80, 0x80, 0x80, 0x00},
        {0x00, 0x00, 0x00, 0x00}
    };
    COPY_OBJECT(g_BmInfo->bmiColors, GrayScalePallete);

    /*InitWindow*/
    WNDCLASS WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = wnd_proc,
        .hInstance = Instance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = "PokeWindowClass"
    };
    TRY(RegisterClass(&WindowClass), "Could not register class", 1);

    RECT WinWindowRect = {0, 0, BM_WIDTH, BM_HEIGHT};
    AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, 0);

    dim_rect WindowRect = win_to_dim_rect(WinWindowRect);

    CLEANUP(HWND, clean_up_window) Window = CreateWindow(
        WindowClass.lpszClassName, 
        "PokeGame", 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        WindowRect.width, 
        WindowRect.height, 
        NULL, 
        NULL, 
        Instance, 
        NULL
    );
    TRY(Window, "Could not create window", 1);

    TRY(SetCurrentDirectory("../Shared"), "Could not find shared directory", 1);

    /*Initworld*/
    game_world world = {
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
                    .Tile = "tile_data00",
                    .Quad = "QuadData00",
                    .Prop = "QuadProps00"
                }, 
                {
                    .Tile = "tile_data01",
                    .Quad = "QuadData01",
                    .Prop = "QuadProps01",
                }
            },
        }
    };

    /*Loadtile_data*/
    uint8_t SpriteData[256 * TILE_SIZE];
    uint8_t FlowerData[3 * TILE_SIZE];
    uint8_t WaterData[7 * TILE_SIZE];

#define LOAD_TILE_DATA(Name, Data, Offset, Index) \
    TRY(\
        read_tile_data(Name, (Data) + (Offset) * TILE_SIZE, (Index)),\
        "Could not load tile data: " Name,\
        1\
    ) 

    LOAD_TILE_DATA("Menu", world.tile_data, 96, 152);
    LOAD_TILE_DATA("SpriteData", SpriteData, 0, 256);
    LOAD_TILE_DATA("FlowerData", FlowerData, 0, 3);
    LOAD_TILE_DATA("WaterData", WaterData, 0, 7);
    LOAD_TILE_DATA("ShadowData", SpriteData, 255, 1);

    /*Initmaps*/
    TRY(read_overworld_map(&world, 0, (point) {0, 2}), "Could not load starting game_map", 1);

    /*GameBoyGraphics*/
    sprite Sprites[40] = {};

    uint8_t TileMap[32][32];

    uint8_t ScrollX = 0;
    uint8_t ScrollY = 0;

    /*TranslateFullMapToTiles*/
    place_view_map(&world, TileMap, FALSE);

    /*WindowMap*/
    uint8_t WindowMap[32][32];
    uint8_t WindowY = 144;

    /*PlayerState*/
    int IsLeaping = 0;
    int IsDialog = 0;
    int IsTransition = 0; 
    int TransitionTick = 0;
    point DoorPoint;
    
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
    int64_t PerfFreq = get_perf_freq();
    int64_t FrameDelta = PerfFreq / 30;

    /*LoadWinmm*/
    CLEANUP(int, clean_up_time) IsGranular = 0;

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
        BeginCounter = get_perf_counter();

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
                        set_my_window_pos(Window, WS_OVERLAPPEDWINDOW, RestoreRect);
                    } else {
                        RestoreRect = get_dim_window_rect(Window);
                    }
                    IsFullscreen ^= 1;
                    break;
                default:
                    if(!(Message.lParam & LPARAM_KEY_IS_HELD)) {
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
        if(IsDialog) {
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
                    if(g_Keys['X']) {
                        if(ActiveText[-1] == '\n') {
                            memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                            memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                        }
                        memset(&WindowMap[14][1], 176, 17);
                        memset(&WindowMap[16][1], 176, 18);
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
                            WindowMap[TextTilePt.Y][TextTilePt.X] = Output;
                        }
                        TextTilePt.X++;
                    }
                    ActiveText++;
                }
            } else if(g_Keys['X']) {
                IsDialog = 0;
                WindowY = 144;
                g_Keys['X'] = 0;
            }
        } else if(IsTransition) {
            /*EnterTransition*/
            if(world.Player.IsMoving) {
                if(world.Player.Tick <= 0) {
                    world.Player.IsMoving = 0;
                    TransitionTick = 16;
                }
            }

            /*ProgressTransition*/
            if(!world.Player.IsMoving) {
                if(TransitionTick-- > 0) {
                    switch(TransitionTick) {
                    case 12:
                        g_BmInfo->bmiColors[0] = GrayScalePallete[1]; 
                        g_BmInfo->bmiColors[1] = GrayScalePallete[2]; 
                        g_BmInfo->bmiColors[2] = GrayScalePallete[3]; 
                        break;
                    case 8:
                        g_BmInfo->bmiColors[0] = GrayScalePallete[2]; 
                        g_BmInfo->bmiColors[1] = GrayScalePallete[3]; 
                        break;
                    case 4:
                        g_BmInfo->bmiColors[0] = GrayScalePallete[3]; 
                        break;
                    case 0:
                        /*LoadDoorMap*/
                        quad_info QuadInfo = get_quad(&world, DoorPoint); 

                        game_map *OldMap = &world.maps.Data[QuadInfo.Map]; 
                        game_map *NewMap = &world.maps.Data[!QuadInfo.Map]; 

                        door *Door = SEARCH_FROM_POS(&OldMap->Doors, QuadInfo.Point);
                        TRY(Door, "Door is not allocated", 1);

                        const char *NewPath = Door->Path.Data;
                        TRY(read_map(&world, !QuadInfo.Map, NewPath), "Could not load game_map", 1);

                        TRY(point_in_map(NewMap, Door->DstPos), "Destination out of bounds", 1);
                        world.maps.I = !QuadInfo.Map;
                        world.Player.Pos = quad_pt_to_pt(Door->DstPos);
                        world.IsOverworld = get_loaded_point(&world, !QuadInfo.Map, NewPath); 

                        /*CompleteTransition*/
                        ScrollX = 0;
                        ScrollY = 0;
                        COPY_OBJECT(g_BmInfo->bmiColors, GrayScalePallete);

                        memset(OldMap, 0, sizeof(*OldMap));
                        IsTransition = 0;

                        if(world.Player.Dir == DIR_DOWN) {
                            world.Player.IsMoving = 1;
                            world.Player.Tick = 8;
                            place_view_map(&world, TileMap, TRUE);
                        } else {
                            place_view_map(&world, TileMap, FALSE);
                        }
                        break;
                    }
                }
            }
        } else {
            /*PlayerUpdate*/
            if(world.Player.Tick == 0) {
                int AttemptLeap = 0;

                /*PlayerKeyboard*/
                int HasMoveKey = 1;
                if(g_Keys['W']) {
                    world.Player.Dir = DIR_UP;
                } else if(g_Keys['A']) {
                    world.Player.Dir = DIR_LEFT;
                } else if(g_Keys['S']) {
                    world.Player.Dir = DIR_DOWN;
                    AttemptLeap = 1;
                } else if(g_Keys['D']) {
                    world.Player.Dir = DIR_RIGHT;
                } else {
                    HasMoveKey = 0;
                }

                point NewPoint = get_facing_point(world.Player.Pos, world.Player.Dir);

                /*FetchQuadProp*/
                quad_info OldQuadInfo = get_quad(&world, pt_to_quad_pt(world.Player.Pos));
                quad_info NewQuadInfo = get_quad(&world, NewPoint);
                NewPoint = NewQuadInfo.Point;

                point StartPos = add_points(NewPoint, g_dir_points[reverse_dir(world.Player.Dir)]); 
                StartPos.X *= 16;
                StartPos.Y *= 16;

                /*FetchObject*/
                AttemptLeap &= !!(NewQuadInfo.Prop & QUAD_PROP_EDGE);
                point TestPoint = NewPoint; 
                if(AttemptLeap) {
                    TestPoint.Y++;
                }

                object *FacingObject = NULL; 
                FOR_VEC(Object, CUR(&world.maps)->Objects) {
                    point OtherOldPoint = pt_to_quad_pt(Object->Pos);
                    if(Object->Tick > 0) {
                        point OtherDirPoint = g_NextPoints[Object->Dir];
                        point OtherNewPoint = add_points(OtherOldPoint, OtherDirPoint);
                        if(equal_points(TestPoint, OtherOldPoint) ||
                           equal_points(TestPoint, OtherNewPoint)) {
                            NewQuadInfo.Prop = QUAD_PROP_SOLID;
                            FacingObject = Object;
                            break;
                        }
                    } else if(equal_points(TestPoint, OtherOldPoint)) {
                        NewQuadInfo.Prop = QUAD_PROP_SOLID;
                        if(!AttemptLeap) {
                            NewQuadInfo.Prop |= QUAD_PROP_MESSAGE;
                        }
                        FacingObject = Object;
                        break;
                    }
                }

                /*PlayerTestProp*/
                if(g_Keys['X']) {
                    int IsTV = NewQuadInfo.Prop & QUAD_PROP_TV;
                    int IsShelf = NewQuadInfo.Prop & QUAD_PROP_SHELF && world.Player.Dir == DIR_UP;
                    if(NewQuadInfo.Prop & QUAD_PROP_MESSAGE || IsTV || IsShelf) {
                        IsDialog = 1;
                        WindowY = 96;
                        g_Keys['X'] = 0;

                        /*PlaceTextBox*/
                        uint8_t TextBoxHeadRow[] = {96, [1 ... 18] = 97, 98};
                        uint8_t TextBoxBodyRow[] = {99, [1 ... 18] = 176, 101};
                        uint8_t TextBoxFooterRow[] = {100, [1 ... 18] = 97, 102};
                        COPY_OBJECT(WindowMap[12], TextBoxHeadRow);
                        for(int Y = 13; Y < 17; Y++) {
                            COPY_OBJECT(WindowMap[Y], TextBoxBodyRow);
                        }
                        COPY_OBJECT(WindowMap[17], TextBoxFooterRow);

                        /*GetActiveText*/
                        if(IsShelf) {
                            ActiveText = "Crammed full of\nPOKéMON books!"; 
                        } else if(FacingObject) {
                            ActiveText = FacingObject->Str.Data;
                            FacingObject->Dir = reverse_dir(world.Player.Dir);
                        } else if(IsTV && world.Player.Dir != DIR_UP) {
                           ActiveText = "Oops, wrong side."; 
                        } else {
                            text *Text = SEARCH_FROM_POS(&CUR(&world.maps)->Texts, NewPoint);
                            ActiveText = Text ? Text->Str.Data : "ERROR: No text";
                        }

                        TextTilePt = (point) {1, 14};
                        TextTick = 0;

                        HasMoveKey = 0;
                    } else if(NewQuadInfo.Prop & QUAD_PROP_WATER) {
                        if(world.Player.Tile == ANIM_PLAYER) {
                            world.Player.Tile = ANIM_SEAL;
                            HasMoveKey = 1;
                        }
                    } else if(world.Player.Tile == ANIM_SEAL && !(NewQuadInfo.Prop & QUAD_PROP_SOLID)) {
                        world.Player.Tile = ANIM_PLAYER;
                        HasMoveKey = 1;
                    }
                }

                if(world.Player.Tile != 0) {
                    if(NewQuadInfo.Prop & QUAD_PROP_WATER) {
                        NewQuadInfo.Prop &= ~QUAD_PROP_SOLID;
                    } else {
                        NewQuadInfo.Prop |= QUAD_PROP_SOLID;
                    }
                } else if(NewQuadInfo.Prop & QUAD_PROP_WATER) {
                    NewQuadInfo.Prop |= QUAD_PROP_SOLID;
                }

                if(HasMoveKey) {
                    /*MovePlayer*/
                    IsLeaping = 0;
                    if(NewQuadInfo.Prop & QUAD_PROP_DOOR) {
                        world.Player.IsMoving = 1;
                        world.Player.Tick = 8; 
                        DoorPoint = NewQuadInfo.Point;
                        IsTransition = 1;
                        NewPoint.Y--;
                    } else if(world.Player.Dir == DIR_DOWN && NewQuadInfo.Prop & QUAD_PROP_EDGE) {
                        world.Player.IsMoving = 1;
                        world.Player.Tick = 16;
                        IsLeaping = 1;
                    } else if(NewQuadInfo.Prop & QUAD_PROP_SOLID) {
                        world.Player.IsMoving = 0;
                        world.Player.Tick = 8;
                        if(world.Player.Dir == DIR_DOWN && OldQuadInfo.Prop & QUAD_PROP_EXIT) {
                            world.Player.IsMoving = 0;
                            TransitionTick = 16;
                            DoorPoint = OldQuadInfo.Point;
                            IsTransition = 1;
                        }
                    } else {
                        world.Player.IsMoving = 1;
                        world.Player.Tick = 8;
                    }
                } else {
                    world.Player.IsMoving = 0;
                }

                if(world.Player.IsMoving) {
                    world.Player.Pos = StartPos;
                    world.maps.I = NewQuadInfo.Map;

                    /*Updateloadedmaps*/
                    if(world.IsOverworld) {
                        int AddToX = 0;
                        int AddToY = 0;
                        if(NewPoint.X == 4) {
                            AddToX = -1;
                        } else if(NewPoint.X == CUR(&world.maps)->width - 4) {
                            AddToX = 1;
                        } else if(NewPoint.Y == 4) {
                            AddToY = -1;
                        } else if(NewPoint.Y == CUR(&world.maps)->height - 4) {
                            AddToY = 1;
                        }

                        if(AddToX || AddToY) {
                            TRY(load_next_map(&world, AddToX, AddToY), "Could not load next game_map", 1);
                        }
                    }

                    /*TranslateQuadRectToTiles*/
                    point TileMin = {};
                    rect QuadRect = {};

                    switch(world.Player.Dir) {
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
                    place_map(&world, TileMap, TileMin, QuadRect);
                }
            }
        }

        int SprI = 0;

        /*UpdatePlayer*/
        sprite *SpriteQuad = &Sprites[SprI];
        point PlayerPt = {72, 72};
        update_animation(&world.Player, SpriteQuad, PlayerPt);
        if(!IsDialog && world.Player.Tick > 0) {
            move_entity(&world.Player);
            if(world.Player.IsMoving) {
                switch(world.Player.Dir) {
                case DIR_UP:
                    ScrollY -= world.Player.Speed;
                    break;
                case DIR_LEFT:
                    ScrollX -= world.Player.Speed;
                    break;
                case DIR_RIGHT:
                    ScrollX += world.Player.Speed;
                    break;
                case DIR_DOWN:
                    ScrollY += world.Player.Speed;
                    break;
                }
            }
        }
        SprI += 4;
   
        /*UpdateObjects*/
        for(int MapI = 0; MapI < _countof(world.maps.Data); MapI++) {
            FOR_VEC(Object, world.maps.Data[MapI].Objects) {
                sprite *SpriteQuad = &Sprites[SprI];
                point QuadPt = {
                    Object->Pos.X - world.Player.Pos.X + 72,
                    Object->Pos.Y - world.Player.Pos.Y + 72
                };

                if(world.maps.I != MapI) {
                    switch(get_map_dir(world.maps.Data, world.maps.I)) {
                    case DIR_UP:
                        QuadPt.Y -= world.maps.Data[MapI].height * 16;
                        break;
                    case DIR_LEFT:
                        QuadPt.X -= world.maps.Data[MapI].width * 16;
                        break;
                    case DIR_DOWN:
                        QuadPt.Y += CUR(&world.maps)->height * 16;
                        break;
                    case DIR_RIGHT:
                        QuadPt.X += CUR(&world.maps)->width * 16;
                        break;
                    }
                }

                if(object_in_update_bounds(QuadPt)) {
                    update_animation(Object, SpriteQuad, QuadPt);
                    /*TickFrame*/
                    if(!IsDialog) {
                        if(Object->Tick > 0) {
                            move_entity(Object);
                        } else if(!IsTransition) {
                            random_move(&world, MapI, Object);
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
            uint8_t DeltaI = world.Player.Tick < 8 ? world.Player.Tick: 15 - world.Player.Tick;
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
        if(Tick % 16 == 0 && world.IsOverworld) {
            /*FlowersUpdate*/
            uint8_t *FlowerDst = world.tile_data + 3 * TILE_SIZE;
            uint8_t *FlowerSrc = FlowerData + TickCycle % 3 * TILE_SIZE;
            memcpy(FlowerDst, FlowerSrc, TILE_SIZE);

            /*WaterUpdate*/
            int TickMod = TickCycle % 9 < 5 ? TickCycle % 9 : 9 - TickCycle % 9;
            uint8_t *WaterDst = world.tile_data + 20 * TILE_SIZE;
            uint8_t *WaterSrc = WaterData + TickMod * TILE_SIZE;
            memcpy(WaterDst, WaterSrc, TILE_SIZE);
        }

        /*RenderTiles*/
        uint8_t (*BmRow)[BM_WIDTH] = g_BmPixels;

        int MaxPixelY = WindowY < BM_HEIGHT ? WindowY : BM_HEIGHT; 

        for(int PixelY = 0; PixelY < MaxPixelY; PixelY++) {
            #define GET_TILE_SRC(Off) (world.tile_data + (Off) + TileMap[TileY][TileX] * TILE_SIZE)
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

        for(int PixelY = MaxPixelY; PixelY < BM_HEIGHT; PixelY++) {
            int PixelX = 0;
            int SrcYDsp = (PixelY & 7) << 3;
            int TileX = 0;
            int TileY = PixelY / 8;

            for(int Repeat = 0; Repeat < 20; Repeat++) {
                uint8_t *Src = world.tile_data + SrcYDsp + WindowMap[TileY][TileX] * TILE_SIZE;
                memcpy(*BmRow + PixelX, Src, 8);
                TileX++;
                PixelX += 8;
            }
            BmRow++;
        }

        /*RenderSprites*/
        int MaxX = BM_WIDTH;
        int MaxY = WindowY < BM_HEIGHT ? WindowY : BM_HEIGHT;

        for(size_t I = SprI; I-- > 0; ) {
            int RowsToRender = 8;
            if(Sprites[I].Y < 8) {
                RowsToRender = Sprites[I].Y;
            } else if(Sprites[I].Y > MaxY) {
                RowsToRender = max_int(MaxY + 8 - Sprites[I].Y, 0);
            }

            int ColsToRender = 8;
            if(Sprites[I].X < 8) {
                ColsToRender = Sprites[I].X;
            } else if(Sprites[I].X > MaxX) {
                ColsToRender = max_int(MaxX + 8 - Sprites[I].X, 0);
            }

            int DstX = max_int(Sprites[I].X - 8, 0);
            int DstY = max_int(Sprites[I].Y - 8, 0);

            int SrcX = max_int(8 - Sprites[I].X, 0);
            int DispX = 1;
            if(Sprites[I].Flags & SPR_HORZ_FLAG) {
                SrcX = min_int(Sprites[I].X, 7);
                DispX = -1;
            }
            int DispY = 8;
            int SrcY = max_int(8 - Sprites[I].Y, 0);
            if(Sprites[I].Flags & SPR_VERT_FLAG) {
                SrcY = min_int(Sprites[I].Y, 7);
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

        /*UpdateFullscreen*/
        if(IsFullscreen) {
            dim_rect ClientRect = get_dim_client_rect(Window);

            HMONITOR Monitor = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
            MONITORINFO MonitorInfo = {
                .cbSize = sizeof(MonitorInfo)
            };
            GetMonitorInfo(Monitor, &MonitorInfo);

            dim_rect MonitorRect = win_to_dim_rect(MonitorInfo.rcMonitor);

            if(MonitorRect.width != ClientRect.width || MonitorRect.height != ClientRect.height) {
                set_my_window_pos(Window, WS_POPUP, MonitorRect);
            }
        }

        /*SleepTillNextFrame*/
        int64_t InitDeltaCounter = get_delta_counter(BeginCounter);
        if(InitDeltaCounter < FrameDelta) {
            if(IsGranular) {
                int64_t RemainCounter = FrameDelta - InitDeltaCounter;
                uint32_t SleepMS = 1000 * RemainCounter / PerfFreq;
                if(SleepMS) {
                    Sleep(SleepMS);
                }
            }
            while(get_delta_counter(BeginCounter) < FrameDelta);
        }

        /*NextFrame*/
        InvalidateRect(Window, NULL, FALSE);
        UpdateWindow(Window);
        Tick++;
    }

    return 0;
}


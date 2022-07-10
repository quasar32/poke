#include "audio.h"
#include "read_buffer.h"
#include "read.h"
#include "render.h"
#include "scalar.h"
#include "pallete.h"
#include "world.h"

#define OUT_OF_RANGE(Ary, I) ((size_t) (I)) >= _countof((Ary))

typedef struct data_path {
    const char *Tile;
    const char *Quad;
    const char *Prop;
} data_path; 

static uint8_t g_FlowerData[3][64];
static uint8_t g_WaterData[7][64];
    
static int64_t g_Tick;
static int64_t g_TickCycle;

static const char g_OverworldMapPaths[WORLD_HEIGHT][WORLD_WIDTH][16] = {
    {"VirdianCity"},
    {"Route1"},
    {"PalleteTown"},
    {"Route21"}
};

static const data_path *g_DataPath;

static const data_path g_DataPaths[] = {
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
};

static uint8_t g_QuadProps[128];
static uint8_t g_QuadData[128][4];

static BOOL g_IsWorldActive;

const int8_t OverworldMusic[WORLD_HEIGHT][WORLD_WIDTH] = {
    {MUS_INVALID},
    {MUS_ROUTE_1},
    {MUS_PALLETE_TOWN},
    {MUS_INVALID}
};

BOOL g_InOverworld;
object g_Player;

static void UpdateFlowerTiles(void) {
    memcpy(g_TileData[3], g_FlowerData[g_TickCycle % 3], 64);
}

static void UpdateWaterTiles(void) {
    int TickMod = g_TickCycle < 5 ? g_TickCycle : 9 - g_TickCycle;
    memcpy(g_TileData[20], g_WaterData[TickMod], 64);
}

static void MulTickUpdate(void) {
    if(g_InOverworld && g_Tick % 32 == 0) {
        g_TickCycle = g_Tick / 32 % 9;
        UpdateFlowerTiles();
        UpdateWaterTiles();
    }
    g_Tick++;
}

static int CorrectTileCoord(int TileCoord) {
    return PosIntMod(TileCoord, 32);
}

static void SetToTiles(int TileX, int TileY, const uint8_t Set[4]) {
    g_TileMap[TileY][TileX] = Set[0];
    g_TileMap[TileY][TileX + 1] = Set[1];
    g_TileMap[TileY + 1][TileX] = Set[2];
    g_TileMap[TileY + 1][TileX + 1] = Set[3];
}

static BOOL WillNPCCollide(map *Map, object *Object, point NewPoint) {
    if(WillObjectCollide(&g_Player, NewPoint)) {
        return TRUE;
    }
    for(int I = 0; I < Map->ObjectCount; I++) {
        const object *OtherObject = &Map->Objects[I]; 
        if(Object != OtherObject && WillObjectCollide(OtherObject, NewPoint)) { 
            return TRUE;
        }
    } 
    return FALSE;
}

static void ReadTileSet(read_buffer *ReadBuffer) {
    int PathI = ReadBufferPopByte(ReadBuffer);
    if(OUT_OF_RANGE(g_DataPaths, PathI)) {
        return;
    } else {
        const data_path *NewDataPath = &g_DataPaths[PathI]; 
        if(g_DataPath != NewDataPath) {
            g_DataPath = NewDataPath;
            ReadTileData(g_DataPath->Tile, g_TileData, 96);
            ReadAll(g_DataPath->Quad, g_QuadData, sizeof(g_QuadData)); 
            ReadAll(g_DataPath->Prop, g_QuadProps, sizeof(g_QuadProps));
        }
    }
}

static void ReadPalleteI(read_buffer *ReadBuffer, map *Map) {
    Map->PalleteI = ReadBufferPopByte(ReadBuffer);
    Map->PalleteI = OUT_OF_RANGE(g_Palletes, Map->PalleteI) ? 0 : Map->PalleteI; 
}

int GetMapDir(const map Maps[2], int Map) {
    point DirPoint = SubPoints(Maps[!Map].Loaded, Maps[Map].Loaded);
    for(size_t I = 0; I < _countof(DirPoints); I++) {
        if(EqualPoints(DirPoint, DirPoints[I])) {
            return I;
        }
    }
    return -1; 
}

int PointInMap(const map *Map, point Pt) {
    int InX = Pt.X >= 0 && Pt.X < Map->Width; 
    int InY = Pt.Y >= 0 && Pt.Y < Map->Height; 
    return InX && InY;
}

int PointInWorld(point Pt) {
    int InX = Pt.X >= 0 && Pt.X < WORLD_WIDTH;
    int InY = Pt.Y >= 0 && Pt.Y < WORLD_HEIGHT;
    return InX && InY;
}

void ReadMap(world *World, int MapI, const char *Path) {
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

    ReadTileSet(&ReadBuffer);
    ReadPalleteI(&ReadBuffer, Map);
}

void ReadOverworldMap(world *World, int MapI, point Load) { 
    const char *MapPath = g_OverworldMapPaths[Load.Y][Load.X]; 
    if(PointInWorld(Load) && MapPath) {
        ReadMap(World, MapI, MapPath);
        g_InOverworld = 1;

        map *Map = &World->Maps[MapI];
        Map->Loaded = Load; 
    }
}

void GetLoadedPoint(world *World, int MapI, const char *MapPath) {
    for(int Y = 0; Y < WORLD_HEIGHT; Y++) {
        for(int X = 0; X < WORLD_WIDTH; X++) {
            if(strcmp(g_OverworldMapPaths[Y][X], MapPath) == 0) {
                World->Maps[MapI].Loaded = (point) {X, Y};
                g_InOverworld = 1;
                return;
            } 
        }
    }
    g_InOverworld = 0;
}

quad_info GetQuadInfo(const world *World, point Point) {
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
        QuadInfo.Prop = g_QuadProps[QuadInfo.Quad];
    } else {
        QuadInfo.Quad = World->Maps[World->MapI].DefaultQuad;
        QuadInfo.Prop = QUAD_PROP_SOLID;
    }

    return QuadInfo;
}

int LoadAdjacentMap(world *World, int DeltaX, int DeltaY) {
    int Result = 0;
    point CurMapPt = World->Maps[World->MapI].Loaded;
    if(DeltaX || DeltaY) {
        point NewMapPt = {CurMapPt.X + DeltaX, CurMapPt.Y + DeltaY}; 
        point OldMapPt = World->Maps[!World->MapI].Loaded;
        if(
            PointInWorld(NewMapPt) &&
            g_OverworldMapPaths[NewMapPt.X][NewMapPt.Y] && 
            !EqualPoints(NewMapPt, OldMapPt)
        ) {
            ReadOverworldMap(World, !World->MapI, NewMapPt);
            Result = 1;
        }
    }
    return Result;
}

void PlaceMap(const world *World, point TileMin, rect QuadRect) {
    int TileY = CorrectTileCoord(TileMin.Y);
    for(int QuadY = QuadRect.Top; QuadY < QuadRect.Bottom; QuadY++) {
        int TileX = CorrectTileCoord(TileMin.X);
        for(int QuadX = QuadRect.Left; QuadX < QuadRect.Right; QuadX++) {
            int Quad = GetQuadInfo(World, (point) {QuadX, QuadY}).Quad;
            SetToTiles(TileX, TileY, g_QuadData[Quad]); 
            TileX = (TileX + 2) % 32;
        }
        TileY = (TileY + 2) % 32;
    }
}

void PlaceViewMap(const world *World, int IsDown) {
    rect QuadRect = {
        .Left = g_Player.Pos.X / 16 - 4,
        .Top = g_Player.Pos.Y / 16 - 4,
        .Right = QuadRect.Left + 10,
        .Bottom = QuadRect.Top + 9 + !!IsDown
    };
    point TileMin = {0};
    PlaceMap(World, TileMin, QuadRect);
}

void SetPlayerToDefault(void) {
    g_Player = (object) {
        .Pos = {80, 96},
        .Dir = DIR_DOWN,
        .Speed = 2,
        .Tile = ANIM_PLAYER
    };
}

void ActivateWorld(void) {
    if(!g_IsWorldActive) {
        g_IsWorldActive = TRUE;
        ReadTileData("FlowerData", g_FlowerData, 3);
        ReadTileData("WaterData", g_WaterData, 7);
        ReadTileData("ShadowData", &g_SpriteData[255], 1);
        ReadTileData("SpriteData", g_SpriteData, 255);
    }
}

void RandomMove(map *Map, object *Object) {
    int Seed = rand();
    if(Seed % 64 == 0) {
        Object->Dir = Seed / 64 % 4;

        point NewPoint = GetFacingPoint(Object->Pos, Object->Dir);

        /*FetchQuadPropSingleMap*/
        int Quad = -1;
        int Prop = QUAD_PROP_SOLID;
        if(PointInMap(Map, NewPoint)) {
            Quad = Map->Quads[NewPoint.Y][NewPoint.X];
            Prop = g_QuadProps[Quad];
        }
        
        if(!WillNPCCollide(Map, Object, NewPoint) && Prop == QUAD_PROP_NONE) {
            Object->Tick = 32;
            Object->IsMoving = 1;
        }
    }
}

void UpdatePlayerMovement(void) {
    if(g_Player.Tick > 0) {
        if(g_Player.Tick % 16 == 8) {
            g_Player.IsRight ^= 1;
        }
        if(g_Player.IsMoving && g_Player.Tick % 2 == 0) {
            switch(g_Player.Dir) {
            case DIR_UP:
                g_TileMapY -= g_Player.Speed;
                break;
            case DIR_LEFT:
                g_TileMapX -= g_Player.Speed;
                break;
            case DIR_RIGHT:
                g_TileMapX += g_Player.Speed;
                break;
            case DIR_DOWN:
                g_TileMapY += g_Player.Speed;
                break;
            }
        }
        MoveEntity(&g_Player);
    } 
}

void UpdateActiveWorld(void) {
    if(g_IsWorldActive) {
        MulTickUpdate(); 
    }    
}

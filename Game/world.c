#include "audio.h"
#include "buffer.h"
#include "frame.h"
#include "input.h"
#include "render.h"
#include "scalar.h"
#include "text.h"
#include "world.h"

#include <stdint.h>

#define COBJMACROS
#include <xaudio2.h>

typedef struct data_path {
    const char *Tile;
    const char *Quad;
    const char *Prop;
} data_path; 

static uint8_t g_FlowerData[3][64];
static uint8_t g_WaterData[7][64];

static int64_t g_Tick;

static const char g_OverworldMapPaths[WORLD_HEIGHT][WORLD_WIDTH][16] = {
    {"VirdianCity"},
    {"Route1"},
    {"PalletTown"},
    {"Route21"}
};

static int g_TileSetI = -1; 

static uint8_t g_QuadProps[128];
static uint8_t g_QuadData[128][4];

static BOOL g_IsWorldActive;

static int g_MusicTick;

static const int8_t g_OverworldMusic[WORLD_HEIGHT][WORLD_WIDTH] = {
    {MUS_INVALID},
    {MUS_ROUTE_1},
    {MUS_PALLET_TOWN},
    {MUS_INVALID}
};

const point DirPoints[] = {
    [DIR_UP] = {0, -1},
    [DIR_LEFT] = {-1, 0},
    [DIR_DOWN] = {0, 1},
    [DIR_RIGHT] = {1, 0}
};

const point NextPoints[] = {
    [DIR_UP] = {0, 1},
    [DIR_LEFT] = {1, 0},
    [DIR_DOWN] = {0, 1},
    [DIR_RIGHT] = {1, 0}
};

BOOL g_InOverworld;
object g_Player;

int g_MapI;
map g_Maps[2];
char g_RestorePath[MAX_MAP_PATH];

int g_MusicI;

void MoveEntity(object *Object) {
    if(Object->IsMoving && Object->Tick % 2 == 0) {
        point DeltaPoint = DirPoints[Object->Dir];
        DeltaPoint = MulPoint(DeltaPoint, Object->Speed);
        Object->Pos = AddPoints(Object->Pos, DeltaPoint);
    }
    Object->Tick--;
    if(Object->Tick == 0) {
        Object->IsMoving = 0;
    }
}

int WillObjectCollide(const object *OtherObject, point NewPoint) {
    int WillCollide = 0;
    point OtherPoint = PtToQuadPt(OtherObject->Pos);

    if(EqualPoints(NewPoint, OtherPoint)) {
        WillCollide = 1;
    } else if(OtherObject->IsMoving) {
        point NextPoint = NextPoints[OtherObject->Dir];
        point OtherNextPoint = AddPoints(OtherPoint, NextPoint);
        if(EqualPoints(OtherNextPoint, NewPoint)) {
            WillCollide = 1;
        }
    }
    return WillCollide;
}

static void SetSpriteQuad(sprite *Quad, int X, int Y, int Tile) {
    int N = 4; 
    sprite *Sprite = Quad;

    ARY_FOR_EACH(Sprite, N) {
        Sprite->X += X;
        Sprite->Y += Y;
        Sprite->Tile += Tile;
    }
}

static void SetStillAnimation(sprite *Quad, dir Dir) {
    switch(Dir) {
    case DIR_UP:
        Quad[0] = (sprite) {0, 0, 0, 0};
        Quad[1] = (sprite) {8, 0, 0, SPR_HORZ_FLAG};
        Quad[2] = (sprite) {0, 8, 1, 0};
        Quad[3] = (sprite) {8, 8, 1, SPR_HORZ_FLAG};
        break;
    case DIR_LEFT:
        Quad[0] = (sprite) {0, 0, 4, 0};
        Quad[1] = (sprite) {8, 0, 5, 0};
        Quad[2] = (sprite) {0, 8, 6, 0};
        Quad[3] = (sprite) {8, 8, 7, 0};
        break;
    case DIR_DOWN:
        Quad[0] = (sprite) {0, 0, 10, 0};
        Quad[1] = (sprite) {8, 0, 10, SPR_HORZ_FLAG};
        Quad[2] = (sprite) {0, 8, 11, 0};
        Quad[3] = (sprite) {8, 8, 11, SPR_HORZ_FLAG};
        break;
    case DIR_RIGHT:
        Quad[0] = (sprite) {0, 0, 5, SPR_HORZ_FLAG};
        Quad[1] = (sprite) {8, 0, 4, SPR_HORZ_FLAG};
        Quad[2] = (sprite) {0, 8, 7, SPR_HORZ_FLAG};
        Quad[3] = (sprite) {8, 8, 6, SPR_HORZ_FLAG};
        break;
    }
}

static void SetStepAnimation(sprite *Quad, dir Dir, BOOL IsRight) {
    switch(Dir) {
    case DIR_UP:
        Quad[0] = (sprite) {0, 1, 0, 0};
        Quad[1] = (sprite) {8, 1, 0, SPR_HORZ_FLAG};
        if(IsRight) {
            Quad[2] = (sprite) {0, 8, 2, 0};
            Quad[3] = (sprite) {8, 8, 3, 0};
        } else {
            Quad[2] = (sprite) {0, 8, 3, SPR_HORZ_FLAG};
            Quad[3] = (sprite) {8, 8, 2, SPR_HORZ_FLAG};
        }
        break;
    case DIR_LEFT:
        Quad[0] = (sprite) {0, 0, 14, 0};
        Quad[1] = (sprite) {8, 0, 15, 0};
        Quad[2] = (sprite) {0, 8, 8, 0};
        Quad[3] = (sprite) {8, 8, 9, 0};
        break;
    case DIR_DOWN:
        Quad[0] = (sprite) {0, 1, 10, 0};
        Quad[1] = (sprite) {8, 1, 10, SPR_HORZ_FLAG};
        if(IsRight) {
            Quad[2] = (sprite) {0, 8, 13, SPR_HORZ_FLAG};
            Quad[3] = (sprite) {8, 8, 12, SPR_HORZ_FLAG};
        } else {
            Quad[2] = (sprite) {0, 8, 12, 0};
            Quad[3] = (sprite) {8, 8, 13, 0};
        }
        break;
    case DIR_RIGHT:
        Quad[0] = (sprite) {0, 0, 15, SPR_HORZ_FLAG};
        Quad[1] = (sprite) {8, 0, 14, SPR_HORZ_FLAG};
        Quad[2] = (sprite) {0, 8, 9, SPR_HORZ_FLAG};
        Quad[3] = (sprite) {8, 8, 8, SPR_HORZ_FLAG};
        break;
    }
}

static void UpdateAnimation(const object *Object, sprite *Quad, point QuadPt) {
    uint8_t QuadX = QuadPt.X;
    uint8_t QuadY = QuadPt.Y;
    if(Object->Tick % 16 < 8 || Object->Speed == 0) {
        SetStillAnimation(Quad, Object->Dir);
    } else {
        SetStepAnimation(Quad, Object->Dir, Object->IsRight);
    }
    SetSpriteQuad(Quad, QuadX, QuadY - 4, Object->Tile);
}

int ObjectInUpdateBounds(point QuadPt) {
    return QuadPt.X > -16 && QuadPt.X < 176 && QuadPt.Y > -16 && QuadPt.Y < 160;
}

point GetFacingPoint(point Pos, int Dir) {
    point OldPoint = PtToQuadPt(Pos);
    point DirPoint = DirPoints[Dir];
    return AddPoints(OldPoint, DirPoint);
}

static void UpdateFlowerTiles(int64_t TickCycle) {
    memcpy(g_TileData[3], g_FlowerData[TickCycle % 3], 64);
}

static void UpdateWaterTiles(int64_t TickCycle) {
    int TickMod = TickCycle < 5 ? TickCycle : 9 - TickCycle;
    memcpy(g_TileData[20], g_WaterData[TickMod], 64);
}

static void MulTickUpdate(void) {
    if(g_InOverworld && g_Tick % 32 == 0) {
        int64_t TickCycle = g_Tick / 32 % 9;
        UpdateFlowerTiles(TickCycle);
        UpdateWaterTiles(TickCycle);
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
    if(g_TileSetI != PathI) {
        char TilePath[MAX_PATH];
        char QuadPath[MAX_PATH];
        char PropPath[MAX_PATH];

        g_TileSetI = PathI;
        snprintf(TilePath, _countof(TilePath), "%02d", PathI);
        snprintf(QuadPath, _countof(QuadPath), "Tiles/QuadData%02d", PathI);
        snprintf(PropPath, _countof(PropPath), "Tiles/QuadProps%02d", PathI);

        ReadTileData(TilePath, g_TileData, 96);
        ReadAll(QuadPath, g_QuadData, sizeof(g_QuadData)); 
        ReadAll(PropPath, g_QuadProps, sizeof(g_QuadProps));
    }
}

static void ReadPalleteI(read_buffer *ReadBuffer, map *Map) {
    Map->PalleteI = ReadBufferPopByte(ReadBuffer);
    Map->PalleteI = OUT_OF_RANGE(g_Palletes, Map->PalleteI) ? 0 : Map->PalleteI; 
}

sprite *AllocSpriteQuad(void) {
    sprite *SpriteQuad = NULL;
    if(g_SpriteCount <= _countof(g_Sprites) - 4) {
        SpriteQuad = &g_Sprites[g_SpriteCount];
        g_SpriteCount += 4;
    }
    return SpriteQuad;
}

static sprite *PushQuadSprite(object *Object, point Point) {
    sprite *SpriteQuad = AllocSpriteQuad();
    if(SpriteQuad) {
        UpdateAnimation(Object, SpriteQuad, Point);
    }
    return SpriteQuad;
}

static point GetObjectSpritePos(object *Object, int MapI) {
    point QuadPt = {
        Object->Pos.X - g_Player.Pos.X + 72,
        Object->Pos.Y - g_Player.Pos.Y + 72
    };

    if(g_MapI != MapI) {
        switch(GetMapDir(g_Maps, g_MapI)) {
        case DIR_UP:
            QuadPt.Y -= g_Maps[MapI].Height * 16;
            break;
        case DIR_LEFT:
            QuadPt.X -= g_Maps[MapI].Width * 16;
            break;
        case DIR_DOWN:
            QuadPt.Y += g_Maps[g_MapI].Height * 16;
            break;
        case DIR_RIGHT:
            QuadPt.X += g_Maps[g_MapI].Width * 16;
            break;
        }
    }

    return QuadPt;
}

static void PushNPCSprite(int MapI, object *Object) {
    point QuadPt = GetObjectSpritePos(Object, MapI);
    if(ObjectInUpdateBounds(QuadPt)) {
        PushQuadSprite(Object, QuadPt);
    }
}

static void PushShadowSprite(void) {
    if(g_SpriteCount < _countof(g_Sprites)) {
        sprite *ShadowQuad = &g_Sprites[g_SpriteCount]; 
        ShadowQuad[0] = (sprite) {72, 72, SPR_SHADOW, 0};
        ShadowQuad[1] = (sprite) {80, 72, SPR_SHADOW, SPR_HORZ_FLAG};
        ShadowQuad[2] = (sprite) {72, 80, SPR_SHADOW, SPR_VERT_FLAG};
        ShadowQuad[3] = (sprite) {80, 80, SPR_SHADOW, SPR_HORZ_FLAG | SPR_VERT_FLAG};
        g_SpriteCount += 4;
    }
}

static void SetPlayerLeapAnimation(sprite *SpriteQuad, int DeltaI) {
    const static uint8_t Deltas[] = {0, 4, 6, 8, 9, 10, 11, 12};
    int Delta = Deltas[DeltaI];
    for(int I = 0; I < 4; I++) {
        sprite *Sprite = &SpriteQuad[I];
        Sprite->Y -= Delta;
    }
}

static void PushPlayerSprite(uint32_t Flags) {
    if(Flags & US_LEAPING_UP) {
        PushShadowSprite();
        sprite *SpriteQuad = PushBasicPlayerSprite();
        SetPlayerLeapAnimation(SpriteQuad, 7 - g_Player.Tick / 2);
    } else if(Flags & US_LEAPING_DOWN) {
        PushShadowSprite();
        sprite *SpriteQuad = PushBasicPlayerSprite();
        SetPlayerLeapAnimation(SpriteQuad, g_Player.Tick / 2);
    } else {
        PushBasicPlayerSprite();
    }
}

static void UpdateStaticNPC(object *Object) {
    if(Object->Tick > 0) {
        Object->Tick--; 
    } else {
        Object->Dir = Object->StartingDir;
    }
}

static point GetNewStartPos(point NewPoint) {
    point ReverseDirPoint = DirPoints[ReverseDir(g_Player.Dir)];
    point StartPos = AddPoints(NewPoint, ReverseDirPoint); 
    StartPos.X *= 16;
    StartPos.Y *= 16;
    return StartPos;
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

void ReadMap(int MapI, const char *Path) {
    char TruePath[MAX_PATH];
    read_buffer ReadBuffer;

    snprintf(TruePath, MAX_PATH, "Maps/%s", Path);
    ReadBufferFromFile(&ReadBuffer, TruePath);

    /*SaveMapPath*/
    map *Map = &g_Maps[MapI];
    strcpy_s(Map->Path, MAX_MAP_PATH, Path);

    /*ReadQuadSize*/
    Map->Width = ReadBufferPopByte(&ReadBuffer) + 1;
    Map->Height = ReadBufferPopByte(&ReadBuffer) + 1;
    int Size = Map->Width * Map->Height;

    /*ReadQuads*/
    int QuadIndex = 0;
    int X = 0;
    int Y = 0;
    while(QuadIndex < Size) {
        int QuadRaw = ReadBufferPopByte(&ReadBuffer);
        int Quad = QuadRaw & 127;
        int Repeat = 0;

        if(Quad == QuadRaw) {
            Repeat = 1;
        } else { 
            Repeat = ReadBufferPopByte(&ReadBuffer) + 1;
        }

        Repeat = MIN(Size - QuadIndex, Repeat);

        for(int I = QuadIndex; I < QuadIndex + Repeat; I++) {
            Map->Quads[Y][X] = Quad; 
            X++;
            if (X >= Map->Width) {
                X = 0;
                Y++;
            } 
        }

        QuadIndex += Repeat;
    }

    /*DefaultQuad*/
    Map->DefaultQuad = ReadBufferPopByte(&ReadBuffer) & 127;

    /*ReadText*/
    Map->TextCount = ReadBufferPopByte(&ReadBuffer);
    for(int I = 0; I < Map->TextCount; I++) {
        text *Text = &Map->Texts[I];
        Text->Pos.X = ReadBufferPopByte(&ReadBuffer);
        Text->Pos.Y = ReadBufferPopByte(&ReadBuffer);
        ReadBufferPopString(&ReadBuffer, _countof(Text->Str), Text->Str);
    }
    qsort(Map->Texts, Map->TextCount, sizeof(*Map->Texts), CmpPointsWrapper); 

    /*ReadObjects*/
    Map->ObjectCount = ReadBufferPopByte(&ReadBuffer);
    for(int I = 0; I < Map->ObjectCount; I++) {
        object *Object = &Map->Objects[I];
        Object->Pos.X = ReadBufferPopByte(&ReadBuffer) * 16;
        Object->Pos.Y = ReadBufferPopByte(&ReadBuffer) * 16;
        Object->StartingDir = ReadBufferPopByte(&ReadBuffer) & 3;
        Object->Dir = Object->StartingDir;  
        Object->Speed = ReadBufferPopByte(&ReadBuffer) % 2;
        Object->Tile = ReadBufferPopByte(&ReadBuffer) & 0xF0;
        ReadBufferPopString(&ReadBuffer, _countof(Object->Str), Object->Str);
    }

    /*ReadDoors*/
    Map->DoorCount = ReadBufferPopByte(&ReadBuffer);
    for(int I = 0; I < Map->DoorCount; I++) {
        door *Door = &Map->Doors[I];
        Door->Pos.X = ReadBufferPopByte(&ReadBuffer);
        Door->Pos.Y = ReadBufferPopByte(&ReadBuffer);
        Door->DstPos.X = ReadBufferPopByte(&ReadBuffer);
        Door->DstPos.Y = ReadBufferPopByte(&ReadBuffer);
        ReadBufferPopString(&ReadBuffer, _countof(Door->Path), Door->Path);
    }
    qsort(Map->Doors, Map->DoorCount, sizeof(*Map->Doors), CmpPointsWrapper);

    ReadTileSet(&ReadBuffer);
    ReadPalleteI(&ReadBuffer, Map);
}

void ReadOverworldMap(int MapI, point Load) { 
    const char *MapPath = g_OverworldMapPaths[Load.Y][Load.X]; 
    if(PointInWorld(Load) && MapPath) {
        ReadMap(MapI, MapPath);
        g_InOverworld = 1;

        map *Map = &g_Maps[MapI];
        Map->Loaded = Load; 
    }
}

void GetLoadedPoint(int MapI, const char *MapPath) {
    for(int Y = 0; Y < WORLD_HEIGHT; Y++) {
        for(int X = 0; X < WORLD_WIDTH; X++) {
            if(strcmp(g_OverworldMapPaths[Y][X], MapPath) == 0) {
                g_Maps[MapI].Loaded = (point) {X, Y};
                g_InOverworld = 1;
                return;
            } 
        }
    }
    g_InOverworld = 0;
}

quad_info GetQuadInfo(point Point) {
    quad_info QuadInfo = {.MapI = g_MapI, .Point = Point};

    int OldWidth = g_Maps[QuadInfo.MapI].Width;
    int OldHeight = g_Maps[QuadInfo.MapI].Height;

    int NewWidth = g_Maps[!QuadInfo.MapI].Width;
    int NewHeight = g_Maps[!QuadInfo.MapI].Height;

    switch(GetMapDir(g_Maps, QuadInfo.MapI)) {
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
        if(QuadInfo.Point.Y >= g_Maps[QuadInfo.MapI].Height) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y -= OldHeight; 
            QuadInfo.MapI ^= 1;
        }
        break;
    case DIR_RIGHT:
        if(QuadInfo.Point.X >= g_Maps[QuadInfo.MapI].Width) {
            QuadInfo.Point.X -= OldWidth; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
            QuadInfo.MapI ^= 1;
        }
        break;
    }

    if(PointInMap(&g_Maps[QuadInfo.MapI], QuadInfo.Point)) {
        QuadInfo.Quad = g_Maps[QuadInfo.MapI].Quads[QuadInfo.Point.Y][QuadInfo.Point.X];
        QuadInfo.Prop = g_QuadProps[QuadInfo.Quad];
    } else {
        QuadInfo.Quad = g_Maps[g_MapI].DefaultQuad;
        QuadInfo.Prop = QUAD_PROP_SOLID;
    }

    return QuadInfo;
}

int LoadAdjacentMap(int DeltaX, int DeltaY) {
    int Result = 0;
    point CurMapPt = g_Maps[g_MapI].Loaded;
    if(DeltaX || DeltaY) {
        point NewMapPt = {CurMapPt.X + DeltaX, CurMapPt.Y + DeltaY}; 
        point OldMapPt = g_Maps[!g_MapI].Loaded;
        if(
            PointInWorld(NewMapPt) &&
            g_OverworldMapPaths[NewMapPt.X][NewMapPt.Y] && 
            !EqualPoints(NewMapPt, OldMapPt)
        ) {
            ReadOverworldMap(!g_MapI, NewMapPt);
            Result = 1;
        }
    }
    return Result;
}

void PlaceMap(point TileMin, rect QuadRect) {
    int TileY = CorrectTileCoord(TileMin.Y);
    for(int QuadY = QuadRect.Top; QuadY < QuadRect.Bottom; QuadY++) {
        int TileX = CorrectTileCoord(TileMin.X);
        for(int QuadX = QuadRect.Left; QuadX < QuadRect.Right; QuadX++) {
            int Quad = GetQuadInfo((point) {QuadX, QuadY}).Quad;
            SetToTiles(TileX, TileY, g_QuadData[Quad]); 
            TileX = (TileX + 2) % 32;
        }
        TileY = (TileY + 2) % 32;
    }
}

void PlaceViewMap(void) {
    g_TileMapX = 0;
    g_TileMapY = 0;

    rect QuadRect = {
        .Left = g_Player.Pos.X / 16 - 4,
        .Top = g_Player.Pos.Y / 16 - 4,
        .Right = QuadRect.Left + 10,
        .Bottom = QuadRect.Top + 9
    };
    point TileMin = {0};
    PlaceMap(TileMin, QuadRect);
}

void SetPlayerToDefault(void) {
    g_Player = (object) {
        .Pos = {48, 96},
        .Dir = DIR_DOWN,
        .Speed = 2,
        .Tile = ANIM_PLAYER
    };
}

void ActivateWorld(void) {
    if(!g_IsWorldActive) {
        g_IsWorldActive = TRUE;
        ReadTileData("Flower", g_FlowerData, 3);
        ReadTileData("Water", g_WaterData, 7);
        ReadTileData("MiscSprites", &g_SpriteData[SPR_START], 2);
        ReadTileData("Map", &g_WindowData[MT_MAP], 13);
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

void SwitchMusic(void) {
    g_MusicTick = 60;
}

void SwitchOverworldMusic(void) {
    point Load = g_Maps[g_MapI].Loaded;
    g_MusicI = g_OverworldMusic[Load.Y][Load.X];
    SwitchMusic();
}

void UpdateMusicSwitch(void) {
    if(g_MusicTick > 0) {
        --g_MusicTick;
        MusicSetVolume(g_MusicTick / 60.0F);
        if(g_MusicTick == 0) {
            PlayMusic(g_MusicI); 
        }
    }
} 

void TranslateQuadRectToTiles(point NewPoint) {
    point TileMin = {0};
    rect QuadRect = {0};

    switch(g_Player.Dir) {
    case DIR_UP:
        TileMin.X = (g_TileMapX / 8) & 31;
        TileMin.Y = (g_TileMapY / 8 + 30) & 31;

        QuadRect = (rect) {
            .Left = NewPoint.X - 4,
            .Top = NewPoint.Y - 4, 
            .Right = NewPoint.X + 6,
            .Bottom = NewPoint.Y - 3
        };
        break;
    case DIR_LEFT:
        TileMin.X = (g_TileMapX / 8 + 30) & 31;
        TileMin.Y = (g_TileMapY / 8) & 31;

        QuadRect = (rect) {
            .Left = NewPoint.X - 4,
            .Top = NewPoint.Y - 4,
            .Right = NewPoint.X - 3,
            .Bottom = NewPoint.Y + 5
        };
        break;
    case DIR_DOWN:
        TileMin.X = (g_TileMapX / 8) & 31;
        TileMin.Y = (g_TileMapY / 8 + 18) & 31;

        QuadRect = (rect) {
            .Left = NewPoint.X - 4,
            .Top = NewPoint.Y + 4,
            .Right = NewPoint.X + 6,
            .Bottom = NewPoint.Y + 5
        };
        break;
    case DIR_RIGHT:
        TileMin.X = (g_TileMapX / 8 + 20) & 31;
        TileMin.Y = (g_TileMapY / 8) & 31;

        QuadRect = (rect) {
            .Left = NewPoint.X + 5,
            .Top = NewPoint.Y - 4,
            .Right = NewPoint.X + 6,
            .Bottom = NewPoint.Y + 5
        };
        break;
    }
    PlaceMap(TileMin, QuadRect);
}


void UpdateObjects(uint32_t Flags) {
    g_SpriteCount = 0;
    UpdatePlayerMovement();
    PushPlayerSprite(Flags);
    for(int MapI = 0; MapI < (int) _countof(g_Maps); MapI++) {
        map *Map = &g_Maps[MapI];
        for(int ObjI = 0; ObjI < Map->ObjectCount; ObjI++) {
            object *Object = &Map->Objects[ObjI];
            if(Object->Speed == 0) {
                UpdateStaticNPC(Object);
            } else if(Object->Tick > 0) {
                MoveEntity(Object);
            } else if(Flags & US_RANDOM_MOVE) {
                RandomMove(Map, Object);
            }
            PushNPCSprite(MapI, Object);
        }
    }
}

void MovePlayerAsync(quad_info NewQuadInfo, int Tick) {
    g_Player.IsMoving = TRUE;
    g_Player.Tick = Tick;

    if(g_InOverworld) {
        g_Player.Pos = GetNewStartPos(NewQuadInfo.Point);
        if(g_MapI != NewQuadInfo.MapI) {
            g_MapI = NewQuadInfo.MapI;
            SwitchOverworldMusic();
        }
        point AddTo = GetLoadedDeltaPoint(NewQuadInfo.Point);
        if(AddTo.X == 0 || !LoadAdjacentMap(AddTo.X, 0)) {
            if(AddTo.Y != 0) {
                LoadAdjacentMap(0, AddTo.Y);
            }
        }
    }

    TranslateQuadRectToTiles(NewQuadInfo.Point);
}

void MovePlayerSync(uint32_t Flags) {
    point FacingPoint = GetFacingPoint(g_Player.Pos, g_Player.Dir);
    g_Player.IsMoving = 1;
    g_Player.Tick = 16;
    TranslateQuadRectToTiles(FacingPoint);
    while(g_Player.IsMoving) {
        UpdateObjects(Flags);
        NextFrame();
    }
} 

BOOL IsSolidPropForPlayer(quad_prop Prop) {
    return (
        g_Player.Tile == ANIM_SEAL ? 
            Prop == QUAD_PROP_WATER : 
            Prop == QUAD_PROP_NONE || Prop == QUAD_PROP_EXIT
    ); 
}

void TransitionColors(void) {
    Pause(8);
    g_Bitmap.Colors[0] = g_Palletes[g_Maps[g_MapI].PalleteI][1]; 
    g_Bitmap.Colors[1] = g_Palletes[g_Maps[g_MapI].PalleteI][2]; 
    g_Bitmap.Colors[2] = g_Palletes[g_Maps[g_MapI].PalleteI][3]; 
    Pause(8);
    g_Bitmap.Colors[0] = g_Palletes[g_Maps[g_MapI].PalleteI][2]; 
    g_Bitmap.Colors[1] = g_Palletes[g_Maps[g_MapI].PalleteI][3]; 
    Pause(8);
    g_Bitmap.Colors[0] = g_Palletes[g_Maps[g_MapI].PalleteI][3]; 
    Pause(8);
}

point GetLoadedDeltaPoint(point NewPoint) {
    point DeltaPoint = {};
    map *Map = &g_Maps[g_MapI];
    if(NewPoint.X == 4) {
        DeltaPoint.X = -1;
    } else if(NewPoint.X == Map->Width - 4) {
        DeltaPoint.X = 1;
    }
    if(NewPoint.Y == 4) {
        DeltaPoint.Y = -1;
    } else if(NewPoint.Y == Map->Height - 4) {
        DeltaPoint.Y = 1;
    }
    return DeltaPoint;
}

void PlayerLeap(void) {
    MovePlayerSync(US_LEAPING_UP);
    MovePlayerSync(US_LEAPING_DOWN);
}

sprite *PushBasicPlayerSprite(void) {
    return PushQuadSprite(&g_Player, (point) {72, 72});
}

static const text *GetTextFromPos(size_t TextCount, const text *Texts, point Pos) {
    return bsearch(&Pos, Texts, TextCount, sizeof(*Texts), CmpPointsWrapper);
} 

const char *GetTextStrFromPos(map *Map, point Pos) {
    const text *Text = GetTextFromPos(Map->TextCount, Map->Texts, Pos);
    return Text ? Text->Str : "ERROR: NO TEXT";
}

static const door *GetDoorFromPos(map *Map, point Pos) {
    return bsearch(&Pos, Map->Doors, Map->DoorCount, sizeof(*Map->Doors), CmpPointsWrapper);
} 

static void ResetMap(map *Map) {
    Map->Path[0] = '\0'; 
    Map->Width = 0;
    Map->Height = 0;
    Map->TextCount = 0;
    Map->DoorCount = 0;
    Map->ObjectCount = 0;
}

void LoadDoorMap(point DoorPoint) {
    quad_info QuadInfo = GetQuadInfo(DoorPoint); 
    map *OldMap = &g_Maps[QuadInfo.MapI]; 
    const door *Door = GetDoorFromPos(OldMap, QuadInfo.Point);
    if(Door) {
        /*DoorToDoor*/
        ReadMap(!QuadInfo.MapI, Door->Path);
        g_MapI = !QuadInfo.MapI;
        UpdatePallete();
        g_Player.Pos = QuadPtToPt(Door->DstPos);
        GetLoadedPoint(!QuadInfo.MapI, Door->Path);
        ResetMap(OldMap);
        PlaceViewMap();
    }
}

typedef struct world_map_entry {
    const char *Path;
    const char *Name;
    point Pos;
} world_map_entry;

static const world_map_entry g_WorldMapTable[] = {
    {"VirdianCity", "VIRDIAN CITY", {4, 9}},
    {"Route1", "ROUTE 1", {4, 11}},
    {"PalletTown", "PALLET TOWN", {4, 12}},
};

static const uint8_t g_MapTileMap[17][20] = {
    { 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,  4,  0,  0,  0,  0},
    { 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  2,  2,  2,  2,  4,  0,  0},
    { 2,  2,  1,  2,  2,  2,  2,  3,  8,  3,  3,  3,  1,  3,  3,  3,  3,  2,  2,  0},
    { 2,  2,  3,  2,  1,  3,  3,  3,  2,  2,  2,  2,  3,  2,  2,  2,  8,  2,  7,  0},
    { 2,  2,  8,  2,  8,  2,  2,  2,  2,  2,  2,  2,  3,  2,  2,  2,  3,  7,  0,  0},
    { 2,  2,  3,  2,  3,  2,  3,  3,  3,  1,  3,  3,  1,  3,  3,  3,  1,  0,  0,  0},
    { 2,  2,  3,  2,  3,  2,  3,  2,  2,  2,  2,  2,  3,  2,  2,  2,  3,  0,  0,  0},
    { 2,  2,  3,  2,  3,  2,  3,  7,  6,  2,  2,  2,  3,  2,  2,  2,  3,  0,  0,  0},
    { 2,  2,  3,  3,  1,  2,  3,  0,  0,  0,  6,  2,  3,  2,  2,  2,  3,  0,  0,  0},
    { 2,  7,  6,  2,  3,  2,  3,  2,  0,  0,  0,  2,  1,  3,  3,  3,  3,  5,  0,  0},
    { 7,  0,  0,  2,  3,  2,  3,  7,  0,  0,  0,  6,  2,  2,  2,  2,  3,  7,  0,  0},
    { 0,  0,  0,  2,  1,  7, 11,  0,  0,  0,  0,  0,  2,  3,  3,  3,  3,  0,  0,  0},
    { 0,  0,  0,  6,  3,  0, 11,  0,  0,  5,  2,  2,  2,  3,  2,  2,  0,  0,  0,  0},
    { 0,  0,  0,  0, 11,  0,  9, 10,  3,  3,  1,  3,  3,  3,  2,  7,  0,  0,  0,  0},
    { 0,  0,  0,  5,  3,  4,  0,  0,  6,  2,  3,  7,  0,  0,  0,  0,  0,  0,  0,  0},
    { 0,  0,  0,  2,  1,  3, 10,  8, 10, 10,  9,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}
};

static void PlaceWorldMap(void) {
    for(int Y = 0; Y < 17; Y++) {
        for(int X = 0; X < 20; X++) {
            g_WindowMap[Y + 1][X] = g_MapTileMap[Y][X] + MT_MAP; 
        }
    }
}

static const char *GetOverworldPath(void) {
    return g_InOverworld ? g_Maps[g_MapI].Path : g_RestorePath;
}

static const world_map_entry *PrevWorldMapEntry(const world_map_entry *Entry) {
    return ARY_PREV_CIRCLE(Entry, g_WorldMapTable);
}

static const world_map_entry *NextWorldMapEntry(const world_map_entry *Entry) {
    return ARY_NEXT_CIRCLE(Entry, g_WorldMapTable);
}

static void PlaceMapName(const char *Name) { 
    memset(g_WindowMap, MT_BLANK, 20);
    PlaceText(1, 0, Name);
}

static void SetSpriteQuadByQuad(sprite *Quad, point Pos, int Tile) {
    Pos = MulPoint(Pos, 8);
    SetSpriteQuad(Quad, Pos.X, Pos.Y, Tile);
}

static void PlaceMapCursor(point Pos) {
    sprite *Quad = AllocSpriteQuad();
    if(Quad) {
        Quad[0] = (sprite) { 4,  5, 0, SPR_TOP_FLAG};
        Quad[1] = (sprite) {12,  5, 0, SPR_HORZ_FLAG | SPR_TOP_FLAG};
        Quad[2] = (sprite) { 4, 13, 0, SPR_VERT_FLAG | SPR_TOP_FLAG};
        Quad[3] = (sprite) {12, 13, 0, SPR_HORZ_FLAG | SPR_VERT_FLAG | SPR_TOP_FLAG};
        SetSpriteQuadByQuad(Quad, Pos, SPR_MAP_BRACKET); 
    }
}

static int UpdateTickCylce(int Tick, int Len) {
    Tick++;
    return Tick == Len ? 0 : Tick; 
}

static int FlashMapCursor(int Tick, int InitCount, point Pos) {
    g_SpriteCount = InitCount;
    if(Tick / 24 == 0) {
        PlaceMapCursor(Pos);
    }
    return UpdateTickCylce(Tick, 48);
}

static void UpdateWorldMap(const world_map_entry *Entry) {
    int Tick = 0;
    int InitCount = g_SpriteCount;
    while(VirtButtons[BT_A] != 1 && VirtButtons[BT_B] != 1) {
        if(VirtButtons[BT_DOWN] == 1) {
            Entry = NextWorldMapEntry(Entry);
            PlaceMapName(Entry->Name);
            Tick = 0;
            PlaySoundEffect(SFX_TINK);
        } else if(VirtButtons[BT_UP] == 1){
            Entry = PrevWorldMapEntry(Entry);
            PlaceMapName(Entry->Name);
            Tick = 0;
            PlaySoundEffect(SFX_TINK);
        }
        Tick = FlashMapCursor(Tick, InitCount, Entry->Pos);
        NextFrame();
    }
    PlaySoundEffect(SFX_TINK);
}

static const world_map_entry *FindWorldMapEntry(void) {
    size_t N = _countof(g_WorldMapTable);
    const world_map_entry *Entry = g_WorldMapTable;
    const char *OverworldPath = GetOverworldPath();
    ARY_FOR_EACH(Entry, N) {
        if(strcmp(Entry->Path, OverworldPath) == 0) {
            return Entry; 
        }
    }
    return NULL;
}

static void AddSpriteQuadFlags(sprite *Quad, uint8_t Flags) {
    size_t N = 4;
    sprite *Sprite = Quad;

    ARY_FOR_EACH(Sprite, N) {
       Sprite->Flags |= Flags; 
    }
}

static void PlaceMapPlayer(point Pos) {
    sprite *Quad = AllocSpriteQuad();
    if(Quad) {
        Pos = MulPoint(Pos, 8);
        SetStillAnimation(Quad, DIR_DOWN);
        SetSpriteQuad(Quad, Pos.X + 4, Pos.Y + 4, 0);
        AddSpriteQuadFlags(Quad, SPR_TOP_FLAG);
    }
}

void DisplayWorldMap(void) {
    const world_map_entry *Entry;

    BlankWindow();
    Pause(30);
    Entry = FindWorldMapEntry();
    if(Entry) {
        int SaveCount; 

        SetPallete(PAL_MAP);
        PlaceWorldMap();
        SaveCount = g_SpriteCount;
        PlaceMapName(Entry->Name);
        PlaceMapPlayer(Entry->Pos);
        UpdateWorldMap(Entry);
        g_SpriteCount = SaveCount;
        BlankWindow();
        Pause(30);
        UpdatePallete();
    }
    ClearWindow();
    ExecuteAllWindowTasks();
}

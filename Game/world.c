#include "audio.h"
#include "read_buffer.h"
#include "read.h"
#include "render.h"
#include "scalar.h"
#include "world.h"

static const int8_t OverworldMusic[WORLD_HEIGHT][WORLD_WIDTH] = {
    {-1},
    {9},
    {3},
    {-1}
};

static const char OverworldMapPaths[WORLD_HEIGHT][WORLD_WIDTH][16] = {
    {"VirdianCity"},
    {"Route1"},
    {"PalleteTown"},
    {"Route21"}
};

static const data_path DataPaths[] = {
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

/*TODO: Move in string library*/
static int AreStringsEqual(const char *A, const char *B) {
    return A && B ? strcmp(A, B) == 0 : A != B;
}

static int CorrectTileCoord(int TileCoord) {
    return PosIntMod(TileCoord, 32);
}

static void SetToTiles(int TileX, int TileY, const uint8_t Set[4]) {
    TileMap[TileY][TileX] = Set[0];
    TileMap[TileY][TileX + 1] = Set[1];
    TileMap[TileY + 1][TileX] = Set[2];
    TileMap[TileY + 1][TileX + 1] = Set[3];
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

void ReadTileData(const char *Path, uint8_t TileData[][64], int TileCount) {
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

    /*ReadTileSet*/
    const data_path *NewDataPath = &DataPaths[ReadBufferPopByte(&ReadBuffer) % _countof(DataPaths)]; 
    if(World->DataPath != NewDataPath) {
        World->DataPath = NewDataPath;
        ReadTileData(NewDataPath->Tile, World->TileData, 96);
        ReadAll(NewDataPath->Quad, World->QuadData, sizeof(World->QuadData)); 
        ReadAll(NewDataPath->Prop, World->QuadProps, sizeof(World->QuadProps));
    }

    /*ReadPalleteI*/
    Map->PalleteI = ReadBufferPopByte(&ReadBuffer);
}

void ReadOverworldMap(world *World, int MapI, point Load) { 
    const char *MapPath = OverworldMapPaths[Load.Y][Load.X]; 
    if(PointInWorld(Load) && MapPath) {
        ReadMap(World, MapI, MapPath);
        MusicI[MapI] = OverworldMusic[Load.Y][Load.X];
        World->IsOverworld = 1;

        map *Map = &World->Maps[MapI];
        Map->Loaded = Load; 
    }
}

void GetLoadedPoint(world *World, int MapI, const char *MapPath) {
    for(int Y = 0; Y < WORLD_HEIGHT; Y++) {
        for(int X = 0; X < WORLD_WIDTH; X++) {
            if(AreStringsEqual(OverworldMapPaths[Y][X], MapPath)) {
                World->Maps[MapI].Loaded = (point) {X, Y};
                World->IsOverworld = 1;
                return;
            } 
        }
    }
    World->IsOverworld = 0;
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
        QuadInfo.Prop = World->QuadProps[QuadInfo.Quad];
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
            OverworldMapPaths[NewMapPt.X][NewMapPt.Y] && 
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
            SetToTiles(TileX, TileY, World->QuadData[Quad]); 
            TileX = (TileX + 2) % 32;
        }
        TileY = (TileY + 2) % 32;
    }
}

void PlaceViewMap(const world *World, int IsDown) {
    rect QuadRect = {
        .Left = World->Player.Pos.X / 16 - 4,
        .Top = World->Player.Pos.Y / 16 - 4,
        .Right = QuadRect.Left + 10,
        .Bottom = QuadRect.Top + 9 + !!IsDown
    };
    point TileMin = {0};
    PlaceMap(World, TileMin, QuadRect);
}

void SetPlayerToDefault(world *World) {
    World->Player = (object) {
        .Pos = {80, 96},
        .Dir = DIR_DOWN,
        .Speed = 2,
        .Tile = ANIM_PLAYER
    };
}

#ifndef MAP_H
#define MAP_H

#include "object.h"

#define ANIM_PLAYER 0
#define ANIM_SEAL 128

#define WORLD_WIDTH 1
#define WORLD_HEIGHT 4 

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

typedef struct door {
    point Pos;
    point DstPos;
    char Path[256];
} door;

typedef struct text {
    point Pos;
    char Str[256];
} text;

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

typedef struct world {
    uint8_t TileData[256][64];
    uint8_t QuadProps[128];
    uint8_t QuadData[128][4];

    int MapI;
    map Maps[2];

    object Player;

    int IsOverworld;
} world;

typedef struct quad_info {
    point Point;
    int MapI;
    int Quad;
    int Prop;
} quad_info;

int GetMapDir(const map Maps[2], int Map);
int PointInMap(const map *Map, point Pt);
int PointInWorld(point Pt);
void ReadTileData(const char *Path, uint8_t TileData[][64], int TileCount);
void ReadMap(world *World, int MapI, const char *Path);
void ReadOverworldMap(world *World, int MapI, point Load);
void GetLoadedPoint(world *World, int MapI, const char *MapPath);
quad_info GetQuadInfo(const world *World, point Point);
int LoadAdjacentMap(world *World, int DeltaX, int DeltaY);
int PointInMap(const map *Map, point Pt);
void PlaceViewMap(const world *World, int IsDown);
void PlaceMap(const world *World, point TileMin, rect QuadRect);
void SetPlayerToDefault(world *World);

#endif

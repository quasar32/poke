#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include <windows.h>

#include "coord.h"
#include "render.h"

#define ANIM_PLAYER 0
#define ANIM_SEAL 128

#define WORLD_WIDTH 1
#define WORLD_HEIGHT 4 

#define US_RANDOM_MOVE 1
#define US_LEAPING_UP 2
#define US_LEAPING_DOWN 4

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

typedef struct quad_info {
    point Point;
    int MapI;
    int Quad;
    int Prop;
} quad_info;

extern const point DirPoints[4];
extern const point NextPoints[4];

extern BOOL g_InOverworld;
extern object g_Player;

extern int g_MapI;
extern map g_Maps[2];
extern int g_MusicI;

static inline point PtToQuadPt(point Point) {
    return DivPoint(Point, 16);
}

static inline point QuadPtToPt(point Point) {
    return MulPoint(Point, 16);
}

static inline int ReverseDir(int Dir) {
    return (Dir + 2) % 4;
}

static inline void UpdatePallete(void) {
    SetPallete(g_Maps[g_MapI].PalleteI);
}

void MoveEntity(object *Object);
int WillObjectCollide(const object *OtherObject, point NewPoint);
void UpdateAnimation(const object *Object, sprite *SpriteQuad, point QuadPt);
int ObjectInUpdateBounds(point QuadPt);
point GetFacingPoint(point Pos, int Dir);

int GetMapDir(const map Maps[2], int Map);
int PointInMap(const map *Map, point Pt);
void RandomMove(map *Map, object *Object);
void ReadMap(int MapI, const char *Path);
void ReadOverworldMap(int MapI, point Load);
void GetLoadedPoint(int MapI, const char *MapPath);

int PointInWorld(point Pt);
void ReadTileData(const char *Path, uint8_t TileData[][64], int TileCount);
quad_info GetQuadInfo(point Point);
int LoadAdjacentMap(int DeltaX, int DeltaY);
int PointInMap(const map *Map, point Pt);
void PlaceViewMap(BOOL IsDown);
void PlaceMap(point TileMin, rect QuadRect);
void SetPlayerToDefault(void);
void ActivateWorld(void);
void UpdatePlayerMovement(void);
void UpdateActiveWorld(void);
void UpdateMusicSwitch(void);
void SwitchMusic(void);
void SwitchOverworldMusic(void);
void TranslateQuadRectToTiles(point NewPoint);
void UpdateObjects(uint32_t Flags);
void MovePlayerAsync(quad_info NewQuadInfo, int Tick);
BOOL IsSolidPropForPlayer(quad_prop Prop);
point GetLoadedDeltaPoint(point NewPoint);
void MovePlayerSync(uint32_t Flags);
void TransitionColors(void);
void PlayerLeap(void);
sprite *PushBasicPlayerSprite(void);

#endif

#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>

#include "coord.h"

#define SPR_HORZ_FLAG 1
#define SPR_VERT_FLAG 2

typedef struct sprite {
    uint8_t X;
    uint8_t Y;
    uint8_t Tile;
    uint8_t Flags;
} sprite;

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

void MoveEntity(object *Object);
int WillObjectCollide(const object *OtherObject, point NewPoint);
void UpdateAnimation(const object *Object, sprite *SpriteQuad, point QuadPt);
int ObjectInUpdateBounds(point QuadPt);

extern const point DirPoints[4];
extern const point NextPoints[4];

[[maybe_unused]]
static point PtToQuadPt(point Point) {
    return DivPoint(Point, 16);
}

[[maybe_unused]]
static point QuadPtToPt(point Point) {
    return MulPoint(Point, 16);
}


#endif

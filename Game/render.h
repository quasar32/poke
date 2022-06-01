#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#include "window_map.h"
#include "world.h"

#define BM_WIDTH 160
#define BM_HEIGHT 144

typedef union bitmap {
    BITMAPINFO WinStruct;
    struct {
        BITMAPINFOHEADER Header;
        RGBQUAD Colors[4]; 
    };
} bitmap;

extern uint8_t Pixels[BM_HEIGHT][BM_WIDTH];
extern uint8_t TileMap[32][32];
extern uint8_t SpriteData[256][64];
extern sprite Sprites[40];
extern bitmap Bitmap;

void RenderSprites(int SpriteI);
void RenderTileMap(const world *World, int ScrollX, int ScrollY);
void RenderWindowMap(const world *World);

#endif

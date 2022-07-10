#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

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

extern bitmap g_Bitmap;
extern uint8_t g_Pixels[BM_HEIGHT][BM_WIDTH];

extern uint8_t g_TileMapX;
extern uint8_t g_TileMapY;
extern uint8_t g_TileMap[32][32];
extern uint8_t g_TileData[256][64];

extern int g_SpriteCount;
extern sprite g_Sprites[40];
extern uint8_t g_SpriteData[256][64];

extern uint8_t g_WindowMapX;
extern uint8_t g_WindowMapY;
extern uint8_t g_WindowMap[32][32];
extern uint8_t g_WindowData[256][64];

extern uint8_t g_TrainerWidth;
extern uint8_t g_TrainerHeight;

void RenderSprites(void);
void RenderTileMap(void);
void RenderWindowMap(void);

void ReadTileData(const char *Path, uint8_t TileData[][64], int TileCount);
void ReadTrainerTileData(const char *Path);
void ReadHorzFlipTrainerTileData(const char *Path);

#endif

#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <windows.h>

#define BM_WIDTH 160
#define BM_HEIGHT 144

#define SPR_HORZ_FLAG 1
#define SPR_VERT_FLAG 2
#define SPR_TOP_FLAG  4

typedef uint8_t tile_map[32][32];

typedef enum pallete {
    PAL_DEFAULT,
    PAL_PALLETE,
    PAL_ROUTE_1,
    PAL_OAK,
    PAL_MAP
} pallete;

#define COUNTOF_PALLETES 5

typedef struct sprite {
    uint8_t X;
    uint8_t Y;
    uint8_t Tile;
    uint8_t Flags;
} sprite;

typedef union bitmap {
    BITMAPINFO WinStruct;
    struct {
        BITMAPINFOHEADER Header;
        RGBQUAD Colors[4]; 
    };
} bitmap;

extern const RGBQUAD g_Palletes[COUNTOF_PALLETES][4]; 

extern bitmap g_Bitmap;
extern uint8_t g_Pixels[BM_HEIGHT][BM_WIDTH];

extern uint8_t g_TileMapX;
extern uint8_t g_TileMapY;
extern tile_map g_TileMap;
extern uint8_t g_TileData[256][64];

extern int g_SpriteCount;
extern sprite g_Sprites[40];
extern uint8_t g_SpriteData[256][64];

extern uint8_t g_WindowMapX;
extern uint8_t g_WindowMapY;
extern tile_map g_WindowMap;
extern uint8_t g_WindowData[256][64];

extern uint8_t g_TrainerWidth;
extern uint8_t g_TrainerHeight;

static inline void SetPallete(pallete PalleteI) {
    memcpy(g_Bitmap.Colors, g_Palletes[PalleteI], sizeof(g_Palletes[PalleteI]));
}

static inline void SetPalleteTransition(pallete I, int C1, int C2, int C3) {
    g_Bitmap.Colors[1] = g_Palletes[I][C1];
    g_Bitmap.Colors[2] = g_Palletes[I][C2];
    g_Bitmap.Colors[3] = g_Palletes[I][C3];
}

static inline void ClearPallete(void) {
    for(int I = 0; I < 4; I++) {
        g_Bitmap.Colors[I] = g_Palletes[PAL_DEFAULT][0];
    }
}

void RenderSprites(void);
void RenderTopSprites(void);
void RenderTileMap(void);
void RenderWindowMap(void);

void ReadTileData(const char *Path, uint8_t TileData[][64], int TileCount);
void ReadTrainerTileData(const char *Path);
void ReadHorzFlipTrainerTileData(const char *Path);

#endif

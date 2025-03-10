#ifndef RENDER_H
#define RENDER_H

#include <SDL3/SDL.h>

#define BM_WIDTH 160
#define BM_HEIGHT 144

#define SPR_HORZ_FLAG 1
#define SPR_VERT_FLAG 2
#define SPR_TOP_FLAG  4

typedef Uint8 tile_map[32][32];

typedef enum pallete {
    PAL_DEFAULT,
    PAL_PALLETE,
    PAL_ROUTE_1,
    PAL_OAK,
    PAL_MAP
} pallete;

#define COUNTOF_PALLETES 5

typedef struct sprite {
    Uint8 X;
    Uint8 Y;
    Uint8 Tile;
    Uint8 Flags;
} sprite;

extern SDL_Window *g_Window;
extern SDL_Renderer *g_Renderer;
extern SDL_Texture *g_Texture;

extern SDL_Color g_Pallete[4];
extern SDL_Color g_Palletes[COUNTOF_PALLETES][4]; 

extern Uint8 g_Pixels[BM_HEIGHT][BM_WIDTH];

extern Uint8 g_TileMapX;
extern Uint8 g_TileMapY;
extern tile_map g_TileMap;
extern Uint8 g_TileData[256][64];

extern int g_SpriteCount;
extern sprite g_Sprites[40];
extern Uint8 g_SpriteData[256][64];

extern Uint8 g_WindowMapX;
extern Uint8 g_WindowMapY;
extern tile_map g_WindowMap;
extern Uint8 g_WindowData[256][64];

extern Uint8 g_TrainerWidth;
extern Uint8 g_TrainerHeight;

static inline void SetPallete(pallete PalleteI) {
    memcpy(g_Pallete, g_Palletes[PalleteI], sizeof(g_Palletes[PalleteI]));
}

static inline void SetPalleteTransition(pallete I, int C1, int C2, int C3) {
    g_Pallete[1] = g_Palletes[I][C1];
    g_Pallete[2] = g_Palletes[I][C2];
    g_Pallete[3] = g_Palletes[I][C3];
}

static inline void ClearPallete(void) {
    for(int I = 0; I < 4; I++) {
        g_Pallete[I] = g_Palletes[PAL_DEFAULT][0];
    }
}

void RenderSprites(void);
void RenderTopSprites(void);
void RenderTileMap(void);
void RenderWindowMap(void);

void ReadTileData(const char *Path, Uint8 TileData[][64], int TileCount);
void ReadTrainerTileData(const char *Path);
void ReadHorzFlipTrainerTileData(const char *Path);

#endif

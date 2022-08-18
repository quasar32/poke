#include <string.h>

#include "buffer.h"
#include "render.h"
#include "scalar.h"
#include "text.h"

const RGBQUAD g_Palletes[][4] = {
    [PAL_DEFAULT] = {
        {0xFF, 0xEF, 0xFF},
        {0xA8, 0xA8, 0xA8},
        {0x80, 0x80, 0x80},
        {0x10, 0x10, 0x18}
    },
    [PAL_PALLETE] = {
        {0xFF, 0xEF, 0xFF},
        {0xDE, 0xE7, 0xCE},
        {0xFF, 0xD6, 0xA5},
        {0x10, 0x10, 0x18}
    },
    [PAL_ROUTE_1] = {
        {0xFF, 0xEF, 0xFF},
        {0x5A, 0xE7, 0xAD},
        {0xFF, 0xD6, 0xA5},
        {0x10, 0x10, 0x18}
    },
    [PAL_OAK] = {
        {0xFF, 0xEF, 0xFF},
        {0x8C, 0xB5, 0xF7},
        {0x9C, 0x73, 0x84},
        {0x10, 0x10, 0x18}
    },
    [PAL_MAP] = {
        {0xFF, 0xEF, 0xFF},
        {0xFF, 0xD6, 0xA5},
        {0x52, 0xBD, 0x8C},
        {0x10, 0x10, 0x18}
    }
};

bitmap g_Bitmap = {
    .Header = {
        .biSize = sizeof(g_Bitmap.Header),
        .biWidth = BM_WIDTH,
        .biHeight = -BM_HEIGHT,
        .biPlanes = 1,
        .biBitCount = 8,
        .biCompression = BI_RGB,
        .biClrUsed = 4 
    }
};
uint8_t g_Pixels[BM_HEIGHT][BM_WIDTH];

uint8_t g_TileMapX;
uint8_t g_TileMapY;
uint8_t g_TileMap[32][32];
uint8_t g_TileData[256][64];

int g_SpriteCount;
sprite g_Sprites[40];
uint8_t g_SpriteData[256][64];

uint8_t g_WindowMapX;
uint8_t g_WindowMapY;
uint8_t g_WindowMap[32][32];
uint8_t g_WindowData[256][64];

uint8_t g_TrainerWidth;
uint8_t g_TrainerHeight;

static void InterpertTileData(read_buffer *Buffer, uint8_t TileData[][64], int TileCount) {
    for(int TileI = 0; TileI < TileCount; TileI++) {
        for(int I = 0; I < 16; I++) {
            uint8_t CompByte = ReadBufferPopByte(Buffer);
            uint8_t *HalfRow = &TileData[TileI][I * 4];

            HalfRow[0] = (CompByte >> 6) & 3;
            HalfRow[1] = (CompByte >> 4) & 3;
            HalfRow[2] = (CompByte >> 2) & 3;
            HalfRow[3] = CompByte & 3;
        }
    }
}

static void ReadHorzFlipHalfRow(read_buffer *ReadBuffer, uint8_t *HalfRow) {
    uint8_t CompByte = ReadBufferPopByte(ReadBuffer);

    HalfRow[0] = CompByte & 3;
    HalfRow[1] = (CompByte >> 2) & 3;
    HalfRow[2] = (CompByte >> 4) & 3;
    HalfRow[3] = (CompByte >> 6) & 3;
} 

static void RenderSprite(sprite *Sprite) { 
    int RowsToRender = 8;
    if(Sprite->Y < 8) {
        RowsToRender = Sprite->Y;
    } else if(Sprite->Y > BM_HEIGHT) {
        RowsToRender = MAX(BM_HEIGHT + 8 - Sprite->Y, 0);
    }

    int ColsToRender = 8;
    if(Sprite->X < 8) {
        ColsToRender = Sprite->X;
    } else if(Sprite->X > BM_WIDTH) {
        ColsToRender = MAX(BM_WIDTH + 8 - Sprite->X, 0);
    }

    int DstX = MAX(Sprite->X - 8, 0);
    int DstY = MAX(Sprite->Y - 8, 0);

    int SrcX = MAX(8 - Sprite->X, 0);
    int DispX = 1;
    if(Sprite->Flags & SPR_HORZ_FLAG) {
        SrcX = MIN(Sprite->X, 7);
        DispX = -1;
    }
    int DispY = 8;
    int SrcY = MAX(8 - Sprite->Y, 0);
    if(Sprite->Flags & SPR_VERT_FLAG) {
        SrcY = MIN(Sprite->Y, 7);
        DispY = -8;
    }

    uint8_t *Src = &g_SpriteData[Sprite->Tile][SrcY * 8 + SrcX];

    for(int Y = 0; Y < RowsToRender; Y++) {
        uint8_t *Tile = Src;
        for(int X = 0; X < ColsToRender; X++) {
            if(*Tile != 2) {
                g_Pixels[Y + DstY][X + DstX] = *Tile;
            }
            Tile += DispX;
        }
        Src += DispY;
    }
}

void RenderSprites(void) {
    int N = g_SpriteCount;
    sprite *Sprite = g_Sprites;

    ARY_FOR_EACH(Sprite, N) {
        if(!(Sprite->Flags & SPR_TOP_FLAG)) {
            RenderSprite(Sprite);
        } 
    }
}

void RenderTopSprites(void) {
    int N = g_SpriteCount;
    sprite *Sprite = g_Sprites;

    ARY_FOR_EACH(Sprite, N) {
        if(Sprite->Flags & SPR_TOP_FLAG) {
            RenderSprite(Sprite);
        } 
    }
}


void RenderTileMap(void) {
    uint8_t (*BmRow)[BM_WIDTH] = g_Pixels;

    for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
        int PixelX = 8 - (g_TileMapX & 7);
        int SrcYDsp = ((PixelY + g_TileMapY) & 7) << 3;
        int TileX = g_TileMapX >> 3;
        int TileY = (PixelY + g_TileMapY) / 8 % 32;
        int StartOff = SrcYDsp | (g_TileMapX & 7);
        memcpy(*BmRow, &g_TileData[g_TileMap[TileY][TileX]][StartOff], 8);

        for(int Repeat = 1; Repeat < 20; Repeat++) {
            TileX = (TileX + 1) % 32;
            uint8_t *Pixel = *BmRow + PixelX;
            memcpy(Pixel, &g_TileData[g_TileMap[TileY][TileX]][SrcYDsp], 8);

            PixelX += 8;
        }
        TileX = (TileX + 1) % 32;
        uint8_t *Pixel = *BmRow + PixelX;
        int RemainX = BM_WIDTH - PixelX;
        memcpy(Pixel, &g_TileData[g_TileMap[TileY][TileX]][SrcYDsp], RemainX);
        BmRow++;
    }
}

void RenderWindowMap(void) {
    int PixelDY = 0;
    for(int PixelY = g_WindowMapY; PixelY < BM_HEIGHT; PixelY++) {
        int TileY = PixelDY / 8;
        int SrcY = PixelDY & 7;

        int PixelDX = 0;
        for(int PixelX = g_WindowMapX; PixelX < BM_WIDTH; PixelX++) {
            int TileX = PixelDX / 8; 
            int SrcX = PixelDX & 7;

            int WindowTile = g_WindowMap[TileY][TileX];
            if(WindowTile) {
                int SrcDsp = SrcX + SrcY * 8;
                g_Pixels[PixelY][PixelX] = g_WindowData[WindowTile][SrcDsp];
            }

            PixelDX++;
        }
        PixelDY++;
    }
}

void ReadTileData(const char *Path, uint8_t TileData[][64], int TileCount) {
    char TruePath[MAX_PATH];
    snprintf(TruePath, MAX_PATH, "Tiles/TileData%s", Path);
    read_buffer ReadBuffer;
    ReadBufferFromFile(&ReadBuffer, TruePath);
    InterpertTileData(&ReadBuffer, TileData, TileCount); 
}

void ReadTrainerTileData(const char *Path) {
    char TruePath[MAX_PATH];
    snprintf(TruePath, MAX_PATH, "Trainer/%s", Path);
    read_buffer ReadBuffer;
    ReadBufferFromFile(&ReadBuffer, TruePath);
    uint8_t TrainerWH = ReadBufferPopByte(&ReadBuffer);
    g_TrainerWidth = TrainerWH & 0x0F;
    g_TrainerHeight = TrainerWH >> 4;
    InterpertTileData(&ReadBuffer, &g_WindowData[MT_TRAINER], 49); 
}

void ReadHorzFlipTrainerTileData(const char *Path) {
    char TruePath[MAX_PATH];
    snprintf(TruePath, MAX_PATH, "Trainer/%s", Path);
    read_buffer ReadBuffer;
    ReadBufferFromFile(&ReadBuffer, TruePath);
    uint8_t TrainerWH = ReadBufferPopByte(&ReadBuffer);
    g_TrainerWidth = TrainerWH & 0x0F;
    g_TrainerHeight = TrainerWH >> 4;

    for(int TileY = 0; TileY < g_TrainerHeight; TileY++) {
        for(int TileX = g_TrainerWidth; TileX-- > 0; ) {
            uint8_t *Tile = g_WindowData[MT_TRAINER + TileY * g_TrainerWidth + TileX];
            for(int RowI = 0; RowI < 8; RowI++) {
                uint8_t *Row = &Tile[RowI * 8];
                ReadHorzFlipHalfRow(&ReadBuffer, &Row[4]); 
                ReadHorzFlipHalfRow(&ReadBuffer, &Row[0]); 
            }
        }
    }
}

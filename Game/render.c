#include <string.h>

#include "read_buffer.h"
#include "render.h"
#include "scalar.h"
#include "window_map.h"

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

void RenderSprites(void) {
    for(int I = 0; I < g_SpriteCount; I++) {
        int RowsToRender = 8;
        if(g_Sprites[I].Y < 8) {
            RowsToRender = g_Sprites[I].Y;
        } else if(g_Sprites[I].Y > BM_HEIGHT) {
            RowsToRender = MaxInt(BM_HEIGHT + 8 - g_Sprites[I].Y, 0);
        }

        int ColsToRender = 8;
        if(g_Sprites[I].X < 8) {
            ColsToRender = g_Sprites[I].X;
        } else if(g_Sprites[I].X > BM_WIDTH) {
            ColsToRender = MaxInt(BM_WIDTH + 8 - g_Sprites[I].X, 0);
        }

        int DstX = MaxInt(g_Sprites[I].X - 8, 0);
        int DstY = MaxInt(g_Sprites[I].Y - 8, 0);

        int SrcX = MaxInt(8 - g_Sprites[I].X, 0);
        int DispX = 1;
        if(g_Sprites[I].Flags & SPR_HORZ_FLAG) {
            SrcX = MinInt(g_Sprites[I].X, 7);
            DispX = -1;
        }
        int DispY = 8;
        int SrcY = MaxInt(8 - g_Sprites[I].Y, 0);
        if(g_Sprites[I].Flags & SPR_VERT_FLAG) {
            SrcY = MinInt(g_Sprites[I].Y, 7);
            DispY = -8;
        }

        uint8_t *Src = &g_SpriteData[g_Sprites[I].Tile][SrcY * 8 + SrcX];

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
    read_buffer ReadBuffer;
    ReadBufferFromFile(&ReadBuffer, Path);
    InterpertTileData(&ReadBuffer, TileData, TileCount); 
}

void ReadTrainerTileData(const char *Path) {
    read_buffer ReadBuffer;
    ReadBufferFromFile(&ReadBuffer, Path);
    uint8_t TrainerWH = ReadBufferPopByte(&ReadBuffer);
    g_TrainerWidth = TrainerWH & 0x0F;
    g_TrainerHeight = TrainerWH >> 4;
    InterpertTileData(&ReadBuffer, &g_WindowData[MT_TRAINER], 49); 
}

void ReadHorzFlipTrainerTileData(const char *Path) {
    read_buffer ReadBuffer;
    ReadBufferFromFile(&ReadBuffer, Path);
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


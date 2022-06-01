#include <string.h>

#include "render.h"
#include "scalar.h"

uint8_t Pixels[BM_HEIGHT][BM_WIDTH];
uint8_t TileMap[32][32];
uint8_t SpriteData[256][64];
sprite Sprites[40];

bitmap Bitmap = {
    .Header = {
        .biSize = sizeof(Bitmap.Header),
        .biWidth = BM_WIDTH,
        .biHeight = -BM_HEIGHT,
        .biPlanes = 1,
        .biBitCount = 8,
        .biCompression = BI_RGB,
        .biClrUsed = 4 
    }
};


void RenderSprites(int SpriteI) {
    for(int I = 0; I < SpriteI; I++) {
        int RowsToRender = 8;
        if(Sprites[I].Y < 8) {
            RowsToRender = Sprites[I].Y;
        } else if(Sprites[I].Y > BM_HEIGHT) {
            RowsToRender = MaxInt(BM_HEIGHT + 8 - Sprites[I].Y, 0);
        }

        int ColsToRender = 8;
        if(Sprites[I].X < 8) {
            ColsToRender = Sprites[I].X;
        } else if(Sprites[I].X > BM_WIDTH) {
            ColsToRender = MaxInt(BM_WIDTH + 8 - Sprites[I].X, 0);
        }

        int DstX = MaxInt(Sprites[I].X - 8, 0);
        int DstY = MaxInt(Sprites[I].Y - 8, 0);

        int SrcX = MaxInt(8 - Sprites[I].X, 0);
        int DispX = 1;
        if(Sprites[I].Flags & SPR_HORZ_FLAG) {
            SrcX = MinInt(Sprites[I].X, 7);
            DispX = -1;
        }
        int DispY = 8;
        int SrcY = MaxInt(8 - Sprites[I].Y, 0);
        if(Sprites[I].Flags & SPR_VERT_FLAG) {
            SrcY = MinInt(Sprites[I].Y, 7);
            DispY = -8;
        }

        uint8_t *Src = &SpriteData[Sprites[I].Tile][SrcY * 8 + SrcX];

        for(int Y = 0; Y < RowsToRender; Y++) {
            uint8_t *Tile = Src;
            for(int X = 0; X < ColsToRender; X++) {
                if(*Tile != 2) {
                    Pixels[Y + DstY][X + DstX] = *Tile;
                }
                Tile += DispX;
            }
            Src += DispY;
        }
    }
}

void RenderTileMap(const world *World, int ScrollX, int ScrollY) {
    uint8_t (*BmRow)[BM_WIDTH] = Pixels;

    for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
        int PixelX = 8 - (ScrollX & 7);
        int SrcYDsp = ((PixelY + ScrollY) & 7) << 3;
        int TileX = ScrollX >> 3;
        int TileY = (PixelY + ScrollY) / 8 % 32;
        int StartOff = SrcYDsp | (ScrollX & 7);
        memcpy(*BmRow, &World->TileData[TileMap[TileY][TileX]][StartOff], 8);

        for(int Repeat = 1; Repeat < 20; Repeat++) {
            TileX = (TileX + 1) % 32;
            uint8_t *Pixel = *BmRow + PixelX;
            memcpy(Pixel, &World->TileData[TileMap[TileY][TileX]][SrcYDsp], 8);

            PixelX += 8;
        }
        TileX = (TileX + 1) % 32;
        uint8_t *Pixel = *BmRow + PixelX;
        int RemainX = BM_WIDTH - PixelX;
        memcpy(Pixel, &World->TileData[TileMap[TileY][TileX]][SrcYDsp], RemainX);
        BmRow++;
    }
}

void RenderWindowMap(const world *World) {
    for(int PixelY = 0; PixelY < BM_HEIGHT; PixelY++) {
        int PixelX = 0;
        int SrcYDsp = (PixelY & 7) << 3;
        int TileY = PixelY / 8;

        for(int TileX = 0; TileX < 20; TileX++) {
            if(WindowMap[TileY][TileX]) {
                memcpy(&Pixels[PixelY][PixelX], &World->TileData[WindowMap[TileY][TileX]][SrcYDsp], 8); 
            }
            PixelX += 8;
        }
    }
}

#include <stdio.h>
#include "shared.h"

static int GetPair(int ColorIndex) {
    switch(ColorIndex) {
    case 0:
        return 3;
    case 7:
        return 1;
    case 15:
        return 0;
    }   
    return 2;
}

static int MatchQuad(int TileMapWidth, uint8_t *QuadData, uint8_t *Tile) {
    int Result = 0;
    uint8_t *Quad = QuadData;
    for(int QuadIndex = 0; QuadIndex < 64; QuadIndex++) {
        if(Quad[0] == Tile[0] && 
           Quad[1] == Tile[1] &&
           Quad[2] == Tile[TileMapWidth] &&
           Quad[3] == Tile[TileMapWidth + 1]) {
            Result = QuadIndex; 
            break;
        }
        Quad += 4;
    }
    return Result;
}

static void TranslateTilesToQuads(array_rect *QuadMap, array_rect *TileMap, uint8_t *QuadData) {
    QuadMap->Width = TileMap->Width / 2;
    QuadMap->Height = TileMap->Height / 2;

    uint8_t *Quad = QuadMap->Bytes;
    uint8_t *TileRow = TileMap->Bytes;
    for(int QuadY = 0; QuadY < QuadMap->Height; QuadY++) {
        uint8_t *Tile = TileRow;
        for(int QuadX = 0; QuadX < QuadMap->Width; QuadX++) {
            *Quad++ = MatchQuad(TileMap->Width, QuadData, Tile); 
            Tile += 2;
        }
        TileRow += TileMap->Width * 2;
    }
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        fputs("Source, destination, and type must be supplied\n", stderr);
        return 1;
    }
    SetCurrentDirectory("../../PokeInput");

    HBITMAP BitmapHandle = LoadImage(NULL, argv[1], IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if(!BitmapHandle) {
        fputs("Bitmap failed to load\n", stderr);
        return 1;
    }
    BITMAP Bitmap;
    GetObject(BitmapHandle, sizeof(BITMAP), &Bitmap);
    if(Bitmap.bmWidth & 7 || Bitmap.bmHeight & 7) {
        fputs("Width/height is not tile aligned\n", stderr);
        return 1;
    }
    if(Bitmap.bmBitsPixel != 4) {
        fputs("Pixel depth must be 4 bit\n", stderr);
    }
    int OutputSize = Bitmap.bmWidth * Bitmap.bmHeight / 4;
    if(OutputSize > 4194304) {
        fputs("Bitmap must be at most 16,777,216 pixels\n", stderr);
        return 1;
    }
    
    static uint8_t Output[4194304];
    for(int OutputIndex = 0; OutputIndex < OutputSize; OutputIndex++) {
        int PairIndex = OutputIndex * 4;
        int X = ((PairIndex & 7) + 
            ((PairIndex >> 3) & ~7)) % 
            Bitmap.bmWidth;
        int Y = (Bitmap.bmHeight - 1) - 
            ((PairIndex >> 3) & 7) - 
            ((PairIndex / Bitmap.bmWidth) & ~7);
        WORD *Quads = Bitmap.bmBits;
        WORD Quad = Quads[(X + Y * Bitmap.bmWidth) / 4]; 
        Output[OutputIndex] = (GetPair(Quad & 15) << 4) |
                              (GetPair((Quad >> 4) & 15) << 6) |
                              (GetPair((Quad >> 12) & 15) << 2) |
                              (GetPair((Quad >> 8) & 15));
    }

    SetCurrentDirectory("../Poke/Shared");
    if(strcmp(argv[3], "Entity") == 0) {
        static uint8_t Output2[4194304];
        int EntityCount = OutputSize / SizeOfCompressedTile / 20;
        int Transfer[] = {5, 15, 13, 14, 6, 7, 16, 17, 18, 19, 2, 12, 10, 11};
        uint8_t *OutPtr = Output2;
        uint8_t *InPtr = Output;
        for(int I = 0; I < EntityCount; I++) {
            for(int J = 0; J < _countof(Transfer); J++) {
                memcpy(OutPtr, InPtr + Transfer[J] * SizeOfCompressedTile, SizeOfCompressedTile); 
                OutPtr += SizeOfCompressedTile;
            }
            InPtr += 20 * SizeOfCompressedTile;
        }
        int Output2Size = EntityCount * SizeOfCompressedTile * _countof(Transfer);
        if(WriteAll(argv[2], Output2, Output2Size) != Output2Size) {
            fputs("Could not write output\n", stderr);
            return 1;
        }
    } else if(strcmp(argv[3], "TileData") == 0) {
        if(WriteAll(argv[2], Output, OutputSize) != OutputSize) {
            fputs("Could not write output\n", stderr);
            return 1;
        }
    } else if(strcmp(argv[3], "QuadMap") == 0) {
        /*Convert Tile Data to TileMap*/
        static uint8_t TileData[256 * 16];
        int InTileCount = ReadAll("TileData", TileData, 256 * 16);  

        struct array_rect TileMap = {
            .Width = Bitmap.bmWidth / 8,
            .Height = Bitmap.bmHeight / 8,
            .Bytes = AllocateStatic(262144)
        };
            
        int OutTileCount = OutputSize / SizeOfCompressedTile;

        uint8_t *Tile = TileMap.Bytes;
        uint8_t *OutPtr = Output;
        for(int OutTile = 0; OutTile < OutTileCount; OutTile++) {
            uint8_t *InPtr = TileData;
            for(int InTile = 0; InTile < InTileCount; InTile++) {
                if(memcmp(OutPtr, InPtr, SizeOfCompressedTile) == 0) {
                    *Tile = InTile; 
                    break;
                }
                InPtr += SizeOfCompressedTile;
            }
            Tile++;
            OutPtr += SizeOfCompressedTile;
        }

        /*Convert TileMap to QuadMap*/
        static uint8_t QuadData[SizeOfQuadData];
        ReadQuadData("QuadData", QuadData);

        struct array_rect QuadMap = InitQuadMap;
        TranslateTilesToQuads(&QuadMap, &TileMap, QuadData);
        WriteQuadMap(argv[2], &QuadMap);
    } else {
        fputs("Invalid type option\n", stderr);
        return 1;
    }
}

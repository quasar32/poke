#include <stdio.h>
#include <stdint.h>
#include <windows.h>

#define SizeOfQuadProps 128 
#define SizeOfQuadData 512 
#define SizeOfQuadMapBytes 65536
#define SizeOfCompressedTile 16

#define AllocateStatic(Size) ({\
    static uint8_t StaticArray_[Size];\
    (void *) StaticArray_;\
})

/*Generic Math Functions*/
#define Swap(A, B) do {\
    typeof(A) A_ = (A);\
    typeof(A) B_ = (B);\
    typeof(*A) Tmp = *A_;\
    *A_ = *B_;\
    *B_ = Tmp;\
} while(0)

#define Min(A, B) ({\
    typeof(A) A_ = (A);\
    typeof(A) B_ = (B);\
    A_ < B_ ? A_ : B_;\
})

#define Max(A, B) ({\
    typeof(A) A_ = (A);\
    typeof(A) B_ = (B);\
    A_ > B_ ? A_ : B_;\
})

#define Clamp(Value, Bottom, Top) ({\
    typeof(Value) Value_ = (Value);\
    typeof(Value) Bottom_ = (Bottom);\
    typeof(Value) Top_ = (Top);\
    Value_ = Max(Value_, Bottom_);\
    Value_ = Min(Value_, Top_);\
    Value_;\
})

#define RoundDown(Value, Round) ({\
    typeof(Value) Value_ = (Value);\
    typeof(Value) Round_ = (Round);\
    Value_ -= Value_ % Round_;\
    Value_;\
})

#define Abs(Value) ({\
    typeof(Value) Value_ = (Value);\
    Value_ < 0 ? -Value_ : Value_;\
})

#define InitQuadMap\
    (array_rect) {\
        0,\
        0,\
        AllocateStatic(SizeOfQuadMapBytes)\
    }

/*Conversion Macros*/
#define CoordToQuad(Coord) ((Coord) >> 4)

#define RunLengthPacketFlag 128

#define TileLength 8
#define TileSize 64

typedef struct array_rect {
    int Width;
    int Height;
    uint8_t *Bytes;
} array_rect;

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
    for(int QuadIndex = 0; QuadIndex < 128; QuadIndex++) {
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

static uint32_t WriteAll(const char *Path, void *Bytes, uint32_t BytesToWrite) {
    DWORD BytesWritten = 0;
    HANDLE FileHandle = CreateFile(Path, GENERIC_WRITE, FILE_SHARE_READ, NULL, 
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(FileHandle != INVALID_HANDLE_VALUE) {
        WriteFile(FileHandle, Bytes, BytesToWrite, &BytesWritten, NULL);
        CloseHandle(FileHandle);
    }
    return BytesWritten;
}


static uint8_t *WriteCompressedQuad(uint8_t *RunPtr, int Quad, int Repeat) {
    if(Repeat == 0) {
        *RunPtr++ = Quad;
    } else {
        *RunPtr++ = Quad | RunLengthPacketFlag;
        *RunPtr++ = Repeat;
    }
    return RunPtr;
}

static uint32_t WriteQuadMap(const char *Path, array_rect *QuadMap) {
    int QuadPrev = QuadMap->Bytes[0];
    uint8_t Repeat = 0;
    uint8_t RunData[65536];
    uint8_t *RunPtr = RunData;
    *RunPtr++ = QuadMap->Width - 1;
    *RunPtr++ = QuadMap->Height - 1;

    int QuadSize = QuadMap->Width * QuadMap->Height;
    for(int QuadIndex = 1; QuadIndex < QuadSize; QuadIndex++) {
        int QuadCur = QuadMap->Bytes[QuadIndex];
        if(Repeat != 255 && QuadPrev == QuadCur) {
            Repeat++;
        } else if(RunPtr - RunData >= 65535) { 
            /*Not 65536 in order because of 2-byte copy*/
            return 0;
        } else {
            RunPtr = WriteCompressedQuad(RunPtr, QuadPrev, Repeat);
            Repeat = 0;
        }
        QuadPrev = QuadCur;
    }

    RunPtr = WriteCompressedQuad(RunPtr, QuadPrev, Repeat);

    if(RunPtr - RunData >= 65536) {
        return 0;
    }
    *RunPtr++ = 0;

    if(RunPtr - RunData >= 65536) {
        return 0;
    }
    uint8_t *Count = RunPtr++;
    *Count = 0;

    if(RunPtr - RunData >= 65536) {
        return 0;
    }

    Count = RunPtr++;
    *Count = 0;

    if(RunPtr - RunData >= 65536) {
        return 0;
    }

    Count = RunPtr++;
    *Count = 0;

    if(RunPtr - RunData >= 65536) {
        return 0;
    }

    *RunPtr++ = 0;

    if(RunPtr - RunData >= 65536) {
        return 0;
    }

    *RunPtr++ = 0;

    return WriteAll(Path, RunData, RunPtr - RunData);
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
static uint32_t ReadAll(const char *Path, void *Bytes, uint32_t BytesToRead) {
    DWORD BytesRead = 0;
    HANDLE FileHandle = CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(FileHandle != INVALID_HANDLE_VALUE) {
        ReadFile(FileHandle, Bytes, BytesToRead, &BytesRead, NULL);
        CloseHandle(FileHandle);
    }
    return BytesRead;
}

static int ReadTileData(const char *Path, uint8_t *TileData, int MaxTileCount) {
    int SizeOfCompressedTileData = MaxTileCount * SizeOfCompressedTile;

    uint8_t CompData[SizeOfCompressedTileData];
    int BytesRead = ReadAll(Path, CompData, sizeof(CompData));
    int TilesRead = BytesRead / SizeOfCompressedTile;
    int BytesValid = TilesRead * SizeOfCompressedTile;

    uint8_t *CompPtr = CompData;
    uint8_t *TilePtr = TileData;
    for(int ByteIndex = 0; ByteIndex < BytesValid; ByteIndex++) {
        uint8_t Comp = *CompPtr++;
        *TilePtr++ = (Comp >> 6) & 3;
        *TilePtr++ = (Comp >> 4) & 3;
        *TilePtr++ = (Comp >> 2) & 3;
        *TilePtr++ = (Comp >> 0) & 3;
    }
    return TilesRead;
}

static void ReadQuadData(const char *Path, uint8_t *QuadData) {
    uint32_t TilesRead = ReadAll(Path, QuadData, SizeOfQuadData);
    uint8_t *Tile = QuadData;
    for(int TileIndex = 0; TileIndex < TilesRead; TileIndex++) {
        *Tile++ = Clamp(*Tile, 0, 95);
    }
    memset(QuadData + TilesRead, 0, SizeOfQuadData - TilesRead);
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
        int Transfer[] = {5, 15, 13, 14, 6, 7, 16, 17, 18, 19, 2, 12, 10, 11, 8, 9};
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
    } else if(strcmp(argv[3], "Trainer") == 0) {
        FILE *File = fopen(argv[2], "wb");
        uint8_t TrainerWidth = Bitmap.bmWidth >> 3; 
        uint8_t TrainerHeight = Bitmap.bmHeight >> 3; 
        uint8_t TrainerSize = TrainerWidth | (TrainerHeight << 4); 
        fwrite(&TrainerSize, 1, 1, File); 
        fwrite(Output, OutputSize, 1, File); 
        fclose(File);
    } else {
        fputs("Invalid type option\n", stderr);
        return 1;
    }
int main(}

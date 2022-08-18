#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define QuadPropNone      0
#define QuadPropSolid     1
#define QuadPropEdge      2
#define QuadPropMessage   3
#define QuadPropWater     4
#define QuadPropDoor      5
#define QuadPropExit      6
#define QuadPropTV        7
#define QuadPropShelf     8
#define QuadPropComputer  9
#define QuadPropMap      10
#define CountofQuadProp  11

const char g_PropStr[][6] = {
    "None ",
    "Solid",
    "Edge ",
    "Msg  ",
    "Water",
    "Door ",  
    "Exit ",
    "TV   ",
    "Shelf",
    "Comp ",
    "Map  "
};

#define SizeOfQuadProps 128 
#define SizeOfQuadData 512 
#define SizeOfQuadMapBytes 65536
#define SizeOfCompressedTile 16

#define RunLengthPacketFlag 128

#define TileLength 8
#define TileSize 64

#define SprHorzFlag 1
#define SprVertFlag 2

#define DirUp 0
#define DirLeft 1
#define DirDown 2
#define DirRight 3

#define TEXT_COUNT 256

uint32_t g_Palletes[][4] = {
    {0xF8F8F8, 0xA8A8A8, 0x808080, 0x000000},
    {0xFFEFFF, 0xCEE7DE, 0xA5D6FF, 0x181010},
    {0xFFEFEF, 0xADE75A, 0xA5D6FF, 0x181010}
};

#define EditorPallete {\
    0xF8F8F8, 0xA8A8A8,\
    0x808080, 0x000000,\
    0x0000FF, 0xFF0000\
}


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

/*Conversion Macros*/
#define CoordToQuad(Coord) ((Coord) >> 4)

/*Macro Functions: Static Allocaters*/
#define InitQuadMap\
    (array_rect) {\
        0,\
        0,\
        AllocateStatic(SizeOfQuadMapBytes)\
    }

#define InitBitmap(Width, Height, DefaultColors, Count)\
    (bitmap) {\
        .Pixels = CreateStaticArrayRect((Width), (Height)),\
        .Info = {\
            .Header = {\
                .biSize = sizeof(BITMAPINFOHEADER),\
                .biWidth = (Width),\
                .biHeight = -(Height),\
                .biPlanes = 1,\
                .biBitCount = 8,\
                .biCompression = BI_RGB,\
                .biClrUsed = (Count)\
            },\
            .Colors = DefaultColors\
        }\
    }

#define AllocateStatic(Size) ({\
    static uint8_t StaticArray_[Size];\
    (void *) StaticArray_;\
})

#define CreateStaticArrayRect(Width, Height)\
    (array_rect) {\
        Width,\
        Height,\
        AllocateStatic(Width * Height)\
    }

/*Structures*/
typedef struct rect {
    int X;
    int Y;
    int Width;
    int Height;
} rect;

typedef struct point {
    int X;
    int Y;
} point;

typedef struct array_rect {
    int Width;
    int Height;
    uint8_t *Bytes;
} array_rect;

typedef struct bitmap {
    array_rect Pixels;
    union {
        BITMAPINFO WinStruct;
        struct {
            BITMAPINFOHEADER Header;
            uint32_t Colors[6];
        };
    } Info;
} bitmap;

typedef struct text {
    BOOL HasText;
    BOOL IsDoor; 
    BOOL IsObject;
    int ObjectSpeed;
    point Pos;
    point DstPos;
    char Data[256];
    int Index;
} text;

typedef struct area {
    rect Rect;
    point Cursor;
} area;

typedef struct area_context {
    BOOL LockedMode;
    union {
        struct area Areas[7];
        struct {
            area QuadDataArea;
            area TileDataArea;
            area QuadMapArea;
            area TextArea;
            area DiagArea;
            area DefaultArea;
            area SpriteArea;
        };
    };
    point QuadMapCam;
} area_context;

typedef struct render_context {
    BOOL Fullscreen;
    BOOL ToRender;
    bitmap Bitmap;
} render_context;

typedef struct quad_context {
    array_rect *QuadMap;
} quad_context;

typedef struct command_context {
    char Command[15];
    int Index;
    BOOL CommandMode;
    BOOL InsertMode;
    BOOL Changed;
    enum {
        DD_NEITHER,
        DD_LEFT,
        DD_RIGHT
    } DstDim;
} command_context;

typedef struct shared_context {
    area_context *AreaContext;
    command_context *CommandContext;
    render_context *RenderContext;
    text *Texts;
} shared_context;

typedef struct sprite {
    int16_t X;
    int16_t Y;
    uint8_t Tile;
    uint8_t Flags;
} sprite;

typedef struct object {
    uint8_t Tile; 
    uint8_t Dir;
    uint8_t Speed;
    text Text;
} object;


/*Globals*/
static object g_Objects[256];
static text g_Doors[256];
static int g_DefaultQuad;
static int g_TileSetI;
static uint8_t g_TileData[256 * 64];
static uint8_t g_QuadProps[128];
static uint8_t g_QuadData[512];
static uint8_t g_PalleteNum = 0;
static uint8_t g_MusicI = 0;
static array_rect g_TileMap;

/*Rect Functions*/
static int RectRight(rect *Rect)  {
    return Rect->X + Rect->Width;
}

static int RectBottom(rect *Rect)  {
    return Rect->Y + Rect->Height;
}

static point RectTopLeft(rect *Rect) { 
    point Point = { .X = Rect->X, .Y = Rect->Y }; 
    return Point; 
}  

/*Point Functions*/ 
static point ClampPoint(point Point,  rect *Rect)  {
    point ClampPoint = {
        .X = Clamp(Point.X, Rect->X, RectRight(Rect)),
        .Y = Clamp(Point.Y, Rect->Y, RectBottom(Rect))
    };
    return ClampPoint;
}

static point AddPoints(point A, point B)  {
    point C = {
        .X = A.X + B.X,
        .Y = A.Y + B.Y
    };
    return C;
}

static point SubPoints(point A, point B) {
    point C = {
        .X = A.X - B.X,
        .Y = A.Y - B.Y
    };
    return C;   
}

static BOOL ComparePoints(point A, point B) {
    return A.X == B.X && A.Y == B.Y;
}

static point MultiplyPoint(point Point, int Factor) {
    point Result = {
        .X = Point.X * Factor,
        .Y = Point.Y * Factor
    };
    return Result;
}

/*Array Rect Functions*/
static uint8_t *ArrayRectGet(array_rect *ArrayRect, int X, int Y) {
    return &ArrayRect->Bytes[X + Y * ArrayRect->Width];
}

static uint8_t *ArrayRectPt(array_rect *ArrayRect, point Point) {
    return &ArrayRect->Bytes[Point.X + Point.Y * ArrayRect->Width];
}

static void ArrayRectBoundRect(array_rect *ArrayRect, rect *Rect) {
    int Left = Clamp(Rect->X, 0, ArrayRect->Width);
    int Top = Clamp(Rect->Y, 0, ArrayRect->Height);
    int Right = Clamp(RectRight(Rect), 0, ArrayRect->Width);
    int Bottom = Clamp(RectBottom(Rect), 0, ArrayRect->Height);
    *Rect = (rect) {
        .X = Left,
        .Y = Top,
        .Width = Max(0, Right - Left),
        .Height = Max(0, Bottom - Top)
    };
}

static void ArrayRectChangeSize(array_rect *ArrayRect, int NewWidth, int NewHeight) {
    int OldWidth = ArrayRect->Width;
    int OldHeight = ArrayRect->Height;
    ArrayRect->Width = NewWidth;
    ArrayRect->Height = NewHeight;

    int MinHeight = Min(NewHeight, OldHeight);
    if(NewWidth < OldWidth) {
        uint8_t *Dst = ArrayRect->Bytes;
        uint8_t *Src = ArrayRect->Bytes;

        for(int Count = 1; Count < MinHeight; Count++) {
            Dst += NewWidth;
            Src += OldWidth;
            memmove(Dst, Src, NewWidth);
        }
    } else if(NewWidth > OldWidth) {
        uint8_t *Dst = ArrayRect->Bytes + NewWidth * MinHeight;
        uint8_t *Src = ArrayRect->Bytes + OldWidth * MinHeight;

        for(int Count = 0; Count < MinHeight; Count++) {
            Dst -= NewWidth;
            Src -= OldWidth;
            memmove(Dst, Src, OldWidth);
            memset(Dst + OldWidth, 0, NewWidth - OldWidth);
        }
    }

    if(NewHeight > OldHeight) {
        uint8_t *Dst = ArrayRect->Bytes + NewWidth * OldHeight;
        for(int Count = OldHeight; Count < NewHeight; Count++) {
            memset(Dst, 0, NewWidth);
            Dst += NewWidth;
        }
    }
}

static BOOL ArrayRectColIsZeroed(array_rect *ArrayRect, int Col) {
    for(int Y = 0; Y < ArrayRect->Height; Y++) {
        if(*ArrayRectGet(ArrayRect, Col, Y)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL ArrayRectRowIsZeroed(array_rect *ArrayRect, int Row) {
    for(int X = 0; X < ArrayRect->Width; X++) {
        if(*ArrayRectGet(ArrayRect, X, Row)) {
            return FALSE;
        }
    }
    return TRUE;
}

/*String*/
static BOOL IsUppercaseLetter(int Char) {
    return Char >= 'A' && Char <= 'Z';
}

static BOOL IsLowercaseLetter(int Char) {
    return Char >= 'a' && Char <= 'z';
}

static BOOL IsLetter(int Char) {
    return IsUppercaseLetter(Char) || IsLowercaseLetter(Char);
}

static BOOL IsNumber(int Char) {
    return Char >= '0' && Char <= '9';
}

static BOOL IsAlphaNumeric(int Char) {
    return IsLetter(Char) || IsNumber(Char);
}

/*SetStillAnimation*/
static void SetStillAnimation(sprite SpriteQuad[4], int Tile, int Dir, point Location) {
    switch(Dir) {
    case DirUp:
        SpriteQuad[0] = (sprite) {0, 0, 0, 0};
        SpriteQuad[1] = (sprite) {8, 0, 0, SprHorzFlag};
        SpriteQuad[2] = (sprite) {0, 8, 1, 0};
        SpriteQuad[3] = (sprite) {8, 8, 1, SprHorzFlag};
        break;
    case DirLeft:
        SpriteQuad[0] = (sprite) {0, 0, 4, 0};
        SpriteQuad[1] = (sprite) {8, 0, 5, 0};
        SpriteQuad[2] = (sprite) {0, 8, 6, 0};
        SpriteQuad[3] = (sprite) {8, 8, 7, 0};
        break;
    case DirDown:
        SpriteQuad[0] = (sprite) {0, 0, 10, 0};
        SpriteQuad[1] = (sprite) {8, 0, 10, SprHorzFlag};
        SpriteQuad[2] = (sprite) {0, 8, 11, 0};
        SpriteQuad[3] = (sprite) {8, 8, 11, SprHorzFlag};
        break;
    case DirRight:
        SpriteQuad[0] = (sprite) {0, 0, 5, SprHorzFlag};
        SpriteQuad[1] = (sprite) {8, 0, 4, SprHorzFlag};
        SpriteQuad[2] = (sprite) {0, 8, 7, SprHorzFlag};
        SpriteQuad[3] = (sprite) {8, 8, 6, SprHorzFlag};
        break;
    }

    for(int SpriteI = 0; SpriteI < 4; SpriteI++) {
        SpriteQuad[SpriteI].X += Location.X;
        SpriteQuad[SpriteI].Y += Location.Y;
        SpriteQuad[SpriteI].Tile += Tile;
    } 
}

/*Data loaders*/
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

static void ReadQuadProps(const char *Path, uint8_t *QuadProps) {
    uint32_t PropsRead = ReadAll(Path, QuadProps, SizeOfQuadProps);
    memset(QuadProps + PropsRead, 0, SizeOfQuadProps - PropsRead);
}
/*Translation Functions*/
static void FillQuad(array_rect *TileMap, uint8_t *Tile, uint8_t *Set) {
    Tile[0] = Set[0];
    Tile[1] = Set[1];
    Tile[TileMap->Width] = Set[2];
    Tile[TileMap->Width + 1] = Set[3];
}

static void TranslateQuadsToTiles(uint8_t *QuadData, array_rect *TileMap, rect *TileRect, array_rect *QuadMap, rect *QuadRect) {
    uint8_t *TileRow = ArrayRectGet(TileMap, TileRect->X, TileRect->Y);
    uint8_t *QuadRow = ArrayRectGet(QuadMap, QuadRect->X, QuadRect->Y);
    for(int RelQuadY = 0; RelQuadY < QuadRect->Height; RelQuadY++) {
        uint8_t *Tile = TileRow;
        uint8_t *Quad = QuadRow;
        for(int RelQuadX = 0; RelQuadX < QuadRect->Width; RelQuadX++) {
            uint8_t *Set = QuadData + *Quad * 4;
            FillQuad(TileMap, Tile, Set);
            Tile += 2;
            Quad++;
        }
        TileRow += TileMap->Width * 2;
        QuadRow += QuadMap->Width;
    }
}

static void SetQuadDataArea(void) {
    uint8_t *Set = g_QuadData;
    uint8_t *TileRow = ArrayRectGet(&g_TileMap, 1, 1);
    for(int SetY = 0; SetY < 8; SetY++) {
        uint8_t *Tile = TileRow;
        for(int SetX = 0; SetX < 16; SetX++) {
            FillQuad(&g_TileMap, Tile, Set);
            Set += 4;
            Tile += 2;
        }
        TileRow += g_TileMap.Width * 2;
    }
}

static char g_TilePath[MAX_PATH];
static char g_QuadPath[MAX_PATH];
static char g_PropPath[MAX_PATH];

static void ReadTileSet(int PathI) {
    g_TileSetI = PathI;
    snprintf(g_TilePath, _countof(g_TilePath), "Tiles/TileData%02d", PathI);
    snprintf(g_QuadPath, _countof(g_QuadPath), "Tiles/QuadData%02d", PathI);
    snprintf(g_PropPath, _countof(g_PropPath), "Tiles/QuadProps%02d", PathI);

    ReadTileData(g_TilePath, g_TileData, 96);
    ReadAll(g_QuadPath, g_QuadData, sizeof(g_QuadData)); 
    ReadAll(g_PropPath, g_QuadProps, sizeof(g_QuadProps));
}


static BOOL ReadQuadMap(const char *Path, array_rect *QuadMap, text *Texts) {
    char TruePath[MAX_PATH];
    snprintf(TruePath, MAX_PATH, "Maps/%s", Path); 
    uint8_t RunData[65536];
    uint32_t BytesRead = ReadAll(TruePath, RunData, sizeof(RunData));

    QuadMap->Width = RunData[0] + 1;
    QuadMap->Height = RunData[1] + 1;
    int Size = QuadMap->Width * QuadMap->Height;
    int QuadIndex = 0;
    BOOL EncodeSuccess = TRUE;
    int RunIndex = 2;
    while(EncodeSuccess && QuadIndex < Size && RunIndex < BytesRead) {
        int QuadRaw = RunData[RunIndex++];
        int Quad = QuadRaw & 127;
        int Repeat = 0;

        if(Quad == QuadRaw) {
            Repeat = 1;
        } else if(RunIndex < BytesRead) {
            Repeat = RunData[RunIndex++] + 1;
        }

        if(QuadIndex + Repeat > Size) {
            Repeat = 0;
        }
        if(Repeat == 0) {
            EncodeSuccess = FALSE;
        }

        memset(QuadMap->Bytes + QuadIndex, Quad, Repeat);
        QuadIndex += Repeat;
    }

    if(RunIndex < BytesRead) {
        g_DefaultQuad = RunData[RunIndex++];
    } else {
        EncodeSuccess = FALSE;
    }

    if(RunIndex < BytesRead) {
        int Count = RunData[RunIndex++];

        if(Count <= TEXT_COUNT) {
            for(int I = 0; I < Count; I++) {
                if(RunIndex < BytesRead - 2) {
                    Texts[I].Pos.X = RunData[RunIndex++]; 
                    Texts[I].Pos.Y = RunData[RunIndex++];
                    Texts[I].Index = RunData[RunIndex++];
                    if(Texts[I].Index < _countof(Texts[I].Data)) {
                        memcpy(Texts[I].Data, &RunData[RunIndex], Texts[I].Index);
                        RunIndex += Texts[I].Index;
                        Texts[I].Data[Texts[I].Index] = '\0';
                        Texts[I].HasText = TRUE;
                    } else {
                        Texts[I].Index = 0; 
                        EncodeSuccess = FALSE;
                    }   
                } else {
                    EncodeSuccess = FALSE;
                }
            } 
        } else {
            EncodeSuccess = FALSE;
        }
    } else {
        EncodeSuccess = FALSE;
    }

    if(RunIndex < BytesRead) {
        int Count = RunData[RunIndex++];

        if(Count <= _countof(g_Objects)) {
            for(int I = 0; I < Count; I++) {
                if(RunIndex < BytesRead - 5) {
                    g_Objects[I].Text.Pos.X = RunData[RunIndex++];
                    g_Objects[I].Text.Pos.Y = RunData[RunIndex++];
                    g_Objects[I].Dir = RunData[RunIndex++];
                    g_Objects[I].Text.ObjectSpeed = RunData[RunIndex++];
                    g_Objects[I].Text.IsObject = TRUE;
                    g_Objects[I].Tile = RunData[RunIndex++];
                    g_Objects[I].Text.Index = RunData[RunIndex++];
                    if(g_Objects[I].Text.Index < _countof(g_Objects[I].Text.Data)) {
                        memcpy(g_Objects[I].Text.Data, &RunData[RunIndex], g_Objects[I].Text.Index);
                        RunIndex += g_Objects[I].Text.Index;
                        g_Objects[I].Text.Data[g_Objects[I].Text.Index] = '\0';
                        g_Objects[I].Text.HasText = TRUE;
                    } else {
                        g_Objects[I].Text.Index = 0; 
                        EncodeSuccess = FALSE;
                    }   
                } else {
                    EncodeSuccess = FALSE;
                }
            } 
        } else {
            EncodeSuccess = FALSE;
        }
    } else {
        EncodeSuccess = FALSE;
    }

    if(RunIndex < BytesRead) {
        int Count = RunData[RunIndex++];

        if(Count <= TEXT_COUNT) {
            for(int I = 0; I < Count; I++) {
                if(RunIndex < BytesRead - 4) {
                    g_Doors[I].Pos.X = RunData[RunIndex++]; 
                    g_Doors[I].Pos.Y = RunData[RunIndex++];
                    g_Doors[I].DstPos.X = RunData[RunIndex++]; 
                    g_Doors[I].DstPos.Y = RunData[RunIndex++];
                    g_Doors[I].Index = RunData[RunIndex++];
                    if(g_Doors[I].Index < _countof(g_Doors[I].Data)) {
                        memcpy(g_Doors[I].Data, &RunData[RunIndex], g_Doors[I].Index);
                        RunIndex += g_Doors[I].Index;
                        g_Doors[I].Data[g_Doors[I].Index] = '\0';
                        g_Doors[I].HasText = TRUE;
                        g_Doors[I].IsDoor = TRUE;
                    } else {
                        g_Doors[I].Index = 0; 
                        EncodeSuccess = FALSE;
                    }
                } else {
                    EncodeSuccess = FALSE;
                }
            } 
        } else {
            EncodeSuccess = FALSE;
        }
    } else {
        EncodeSuccess = FALSE;
    }

    if(RunIndex < BytesRead) {
        ReadTileSet(RunData[RunIndex++]);
        SetQuadDataArea();
    } else {
        EncodeSuccess = FALSE;
    }

    if(RunIndex < BytesRead) {
        g_PalleteNum = RunData[RunIndex++];
    } else {
        EncodeSuccess = FALSE;
    }

    BOOL Success = EncodeSuccess && QuadIndex == Size;
    return Success;
}

/*Data Writers*/
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

static uint32_t WriteQuadMap(const char *Path, array_rect *QuadMap, text *Texts) {
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
    *RunPtr++ = g_DefaultQuad;

    if(RunPtr - RunData >= 65536) {
        return 0;
    }
    uint8_t *Count = RunPtr++;
    *Count = 0;
    for(int I = 0; I < TEXT_COUNT; I++) {
        if(Texts[I].HasText) {
            if(RunPtr - RunData + Texts[I].Index >= 65533) {
                return 0;
            } 

            *RunPtr++ = Texts[I].Pos.X;
            *RunPtr++ = Texts[I].Pos.Y;
            *RunPtr++ = Texts[I].Index;

            memcpy(RunPtr, Texts[I].Data, Texts[I].Index);
            RunPtr += Texts[I].Index;
            *Count = *Count + 1;
        }
    }

    if(RunPtr - RunData >= 65536) {
        return 0;
    }

    Count = RunPtr++;
    *Count = 0;
    for(int I = 0; I < _countof(g_Objects); I++) {
        if(g_Objects[I].Text.HasText) {
            if(RunPtr - RunData + g_Objects[I].Text.Index >= 65530) {
                return 0;
            } 

            *RunPtr++ = g_Objects[I].Text.Pos.X;
            *RunPtr++ = g_Objects[I].Text.Pos.Y;
            *RunPtr++ = g_Objects[I].Dir;
            *RunPtr++ = g_Objects[I].Text.ObjectSpeed;
            *RunPtr++ = g_Objects[I].Tile;
            *RunPtr++ = g_Objects[I].Text.Index;
            g_Objects[I].Text.HasText = TRUE;

            memcpy(RunPtr, g_Objects[I].Text.Data, g_Objects[I].Text.Index);
            RunPtr += g_Objects[I].Text.Index;
            *Count = *Count + 1;
        }
    }

    if(RunPtr - RunData >= 65536) {
        return 0;
    }

    Count = RunPtr++;
    *Count = 0;
    for(int I = 0; I < TEXT_COUNT; I++) {
        if(g_Doors[I].HasText) {
            if(RunPtr - RunData + g_Doors[I].Index >= 65531) {
                return 0;
            } 
            printf("%s\n", g_Doors[I].Data);
            if(!g_Doors[I].Data[0]) {
                continue;
            }
            printf("Save: %s\n", g_Doors[I].Data);

            *RunPtr++ = g_Doors[I].Pos.X;
            *RunPtr++ = g_Doors[I].Pos.Y;
            *RunPtr++ = g_Doors[I].DstPos.X;
            *RunPtr++ = g_Doors[I].DstPos.Y;
            *RunPtr++ = g_Doors[I].Index;

            memcpy(RunPtr, g_Doors[I].Data, g_Doors[I].Index);
            RunPtr += g_Doors[I].Index;
            *Count = *Count + 1;
        }
    }

    if(RunPtr - RunData >= 65536) {
        return 0;
    }

    *RunPtr++ = g_TileSetI;

    if(RunPtr - RunData >= 65536) {
        return 0;
    }

    *RunPtr++ = g_PalleteNum;

    return WriteAll(Path, RunData, RunPtr - RunData);
}

/*Rendering*/
static int WriteChar(uint8_t *Tile, int Char, BOOL Format, int IsFirst) {
    int Increase = 0;
    int Output = Char;
    if(Output == '\0' || (!Format && (Output == '\n' || Output == '\f'))) {
        Output = -1;
    } else if(Output >= '0' && Output <= ':') {
        Output += 111 - '0';
    } else if(IsUppercaseLetter(Output)) {
        Output += 122 - 'A';
    } else if(IsLowercaseLetter(Output)) {
        Output += 148 - 'a';
    } else if(Output == '!') {
        Output = 176;
    } else if(Output == 'é') {
        Output = 174;
    } else if(Output == '\'') {
        Output = 177;
    } else if(Output == '-') {
        Output = 178;
    } else if(Output == '~') {
        Output = 180;
    } else if(Output == '-') {
        Output = 177;
    } else if(Output == ',') {
        Output = 181; 
    } else if(Output == '\n') {
        Output = 182;
    } else if(Output == '\f') {
        Output = 179; 
    } else if(Output == '.') {
        Output = 183; 
    } else {
        Output = 0;
    }

    if(Output < 0) {
        *Tile = 0;
    } else {
        *Tile = Output;
        Increase = 1;
    }
    if(Output == 182 || Output == 179) {
        if(!IsFirst) {
            *Tile = 0; 
        }
        Increase = 0;
    }
    return Increase;
}

static void PlaceChars(array_rect *TileMap, rect *Rect, const char *Text, BOOL Format) {
    uint8_t *TileRow = ArrayRectGet(TileMap, Rect->X, Rect->Y);
    for(int RelY = 0; RelY < Rect->Height; RelY++) {
        uint8_t *Tile = TileRow;
        BOOL IsFirst = TRUE;
        for(int RelX = 0; RelX < Rect->Width; RelX++) {
            char Char = *Text;
            Text += WriteChar(Tile, Char, Format, IsFirst);
            if(*Tile == 182 || *Tile == 179) {
                IsFirst = FALSE;
            }
            Tile++;
        }
        if(*Text == '\n' || *Text == '\f') {
            Text++;
        }
        TileRow += TileMap->Width;
    }
}

static void PlaceFormattedText(array_rect *TileMap, rect *Rect, const char *Format, ...) {
    char Text[1024];
    va_list VariadicList;
    va_start(VariadicList, Format);
    if(vsprintf_s(Text, _countof(Text), Format, VariadicList) >= 0) {
        PlaceChars(TileMap, Rect, Text, FALSE);
    }
    va_end(VariadicList);
}

static void RenderTiles(array_rect *Offscreen, array_rect *TileMap, uint8_t *TileData, point Scroll) {
    uint8_t *OffscreenRow = Offscreen->Bytes;
    for(int PixelY = 0; PixelY < Offscreen->Height; PixelY++) {
        int PixelX = 8 - (Scroll.X & 7);
        int SourceYDsp = ((PixelY + Scroll.Y) & 7) << 3;
        int TileX = Scroll.X >> 3;
        int TileY = (PixelY + Scroll.Y) / 8 % TileMap->Height;
        uint8_t *TileRow = ArrayRectGet(TileMap, 0, TileY);
        memcpy(OffscreenRow, TileData + (SourceYDsp | (Scroll.X & 7)) + TileRow[TileX] * TileSize, 8);
        for(int Repeat = 1; Repeat < Offscreen->Width / TileLength; Repeat++) {
            TileX = Clamp(TileX + 1, 0, TileMap->Width);
            memcpy(OffscreenRow + PixelX, TileData + SourceYDsp + TileRow[TileX] * TileSize, 8);
            PixelX += 8;
        }
        TileX = Clamp(TileX + 1, 0, TileMap->Width);
        memcpy(OffscreenRow + PixelX, TileData + SourceYDsp + TileRow[TileX] * TileSize, Offscreen->Width - PixelX);
        OffscreenRow += Offscreen->Width;
    }
}

/*Windows Rect Functions*/
static rect GetMyRect(RECT *WinRect) {
    rect Rect = {
        .X = WinRect->left,
        .Y = WinRect->top,
        .Width = WinRect->right - WinRect->left,
        .Height = WinRect->bottom - WinRect->top,
    };
    return Rect;
}

static rect GetMyWindowRect(HWND Window) {
    RECT WindowWinRect;
    GetWindowRect(Window, &WindowWinRect);
    rect WinRect = GetMyRect(&WindowWinRect);
    return WinRect;
}

static rect GetMyClientRect(HWND Window) {
    RECT ClientWinRect;
    GetClientRect(Window, &ClientWinRect);
    rect ClientRect = GetMyRect(&ClientWinRect);
    return ClientRect;
}

static void SetMyWindowPos(HWND Window, rect *Rect, UINT WindowFlag) {
    SetWindowLongPtr(Window, GWL_STYLE, WindowFlag | WS_VISIBLE);
    SetWindowPos(
        Window, 
        HWND_TOP, 
        Rect->X, Rect->Y, Rect->Width, Rect->Height, 
        SWP_FRAMECHANGED | SWP_NOREPOSITION
    );
}

static void UpdateFullscreen(HWND Window) {
    rect ClientRect = GetMyClientRect(Window);
    HMONITOR Monitor = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO MonitorInfo = {
        .cbSize = sizeof(MonitorInfo)
    };
    GetMonitorInfo(Monitor, &MonitorInfo);

    rect MonitorRect = GetMyRect(&MonitorInfo.rcMonitor);

    if(MonitorRect.Width != ClientRect.Width || MonitorRect.Height != ClientRect.Height) {
        SetMyWindowPos(Window, &MonitorRect, WS_POPUP);
    }
}

static HWND CreatePokeWindow(const char *WindowName, WNDPROC WndProc, int ClientWidth, int ClientHeight) {
    HWND Window = NULL;
    HINSTANCE Instance = GetModuleHandle(NULL);
    WNDCLASS WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = Instance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .hbrBackground = NULL,
        .lpszClassName = "PokeWindowClass"
    };
    if(RegisterClass(&WindowClass)) {
        RECT WinWindowRect = {
            .left = 0,
            .top = 0,
            .right = ClientWidth,
            .bottom = ClientHeight
        };
        AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, FALSE);

        rect WindowRect = GetMyRect(&WinWindowRect);

        Window = CreateWindow(
            WindowClass.lpszClassName, 
            WindowName, 
            WS_OVERLAPPEDWINDOW, 
            CW_USEDEFAULT, CW_USEDEFAULT, WindowRect.Width, WindowRect.Height, 
            NULL, 
            NULL, 
            Instance, 
            NULL
        );
    }
    return Window;
}

static void RenderOnscreen(HWND Window, HDC DeviceContext, 
                           int Width, int Height, 
                           void *Pixels, BITMAPINFO *BitmapInfo) {
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    int ClientWidth = ClientRect.right;
    int ClientHeight = ClientRect.bottom;

    int RenderWidth = RoundDown(ClientWidth, Width);
    int RenderHeight = RoundDown(ClientHeight, Height);

    int RenderColSize = RenderWidth * Height;
    int RenderRowSize = RenderHeight * Width;
    if(RenderColSize > RenderRowSize) {
        RenderWidth = RenderRowSize / Height;
    } else {
        RenderHeight = RenderColSize / Width;
    }
    int RenderX = (ClientWidth - RenderWidth) / 2;
    int RenderY = (ClientHeight - RenderHeight) / 2;

    StretchDIBits(DeviceContext, 
                  RenderX, RenderY, RenderWidth, RenderHeight, 
                  0, 0, Width, Height, 
                  Pixels, BitmapInfo, 
                  DIB_RGB_COLORS, SRCCOPY);
    PatBlt(DeviceContext, 0, 0, RenderX, ClientHeight, BLACKNESS);
    PatBlt(DeviceContext, RenderX + RenderWidth, 0, RenderX, ClientHeight, BLACKNESS);
    PatBlt(DeviceContext, RenderX, 0, RenderWidth, RenderY, BLACKNESS);
    PatBlt(DeviceContext, RenderX, RenderY + RenderHeight, RenderWidth, RenderY + 1, BLACKNESS);
}

/*Section: Text*/
static text *FindTextPt(text *Texts, point Point) {
    text *Text = Texts;
    for(int Index = 0; Index < TEXT_COUNT; Index++) {
        if(Text->HasText && ComparePoints(Text->Pos, Point)) {
            return Text;
        }
        Text++;
    }
    return NULL;
}

static object *FindObjectPt(point Point) {
    object *Object = g_Objects;
    for(int Index = 0; Index < _countof(g_Objects); Index++) {
        if(Object->Text.HasText && ComparePoints(Object->Text.Pos, Point)) {
            return Object;
        }
        Object++;
    }
    return NULL;
}

static object *AllocObject(point Point, int Tile, int Dir) {
    if(!FindObjectPt(Point)) {
        for(int Index = 0; Index < _countof(g_Objects); Index++) {
            object *Object = &g_Objects[Index];
            if(!Object->Text.HasText) {
                *Object = (object) {
                    .Tile = Tile,
                    .Dir = Dir,
                    .Text = {
                        .IsObject = TRUE,
                        .HasText = TRUE,
                        .Pos = Point
                    }
                };
                return Object;
            }
        }
    }
    return NULL;
}

static text *AllocText(text *Texts, point Point) {
    text *Text = Texts;
    for(int Index = 0; Index < TEXT_COUNT; Index++) {
        if(!Text->HasText) {
            *Text = (text) {
                .HasText = TRUE,
                .Pos = Point
            };
            return Text;
        }
        Text++;
    }
    return NULL;
}

static text *AllocDoor(point Point) {
    text *Text = g_Doors;
    for(int Index = 0; Index < TEXT_COUNT; Index++) {
        if(!Text->HasText) {
            *Text = (text) {
                .HasText = TRUE,
                .IsDoor = TRUE, 
                .Pos = Point
            };
            return Text;
        }
        Text++;
    }
    return NULL;
}

static void ClearText(text *Texts) {
    memset(Texts, 0, TEXT_COUNT * sizeof(*Texts)); 
    memset(g_Objects, 0, sizeof(g_Objects)); 
    memset(g_Doors, 0, sizeof(g_Doors));
}


/*Section: Area*/
static point GetRelativeQuadPoint(area *Area) {
    return (point) {
        .X = (Area->Cursor.X - Area->Rect.X) / 2,
        .Y = (Area->Cursor.Y - Area->Rect.Y) / 2
    };
}

static int GetRelativeQuadIndex(area *Area) {
    int QuadX = (Area->Cursor.X - Area->Rect.X) / 2;
    int QuadY = (Area->Cursor.Y - Area->Rect.Y) / 2;
    int QuadWidth = Area->Rect.Width / 2;
    return QuadX + QuadY * QuadWidth;
}

/*Section: Area Context*/
static BOOL MoveSelectTile(area_context *AreaContext, area *Area, point DeltaPoint, array_rect *QuadMap) {
    point AreaCursorOld = Area->Cursor;
    int Transform = AreaContext->LockedMode ? 2 : 1;
    int Width = Area == &AreaContext->QuadMapArea ? Min(Area->Rect.Width, QuadMap->Width * 2) : Area->Rect.Width; 
    int Height = Area == &AreaContext->QuadMapArea ? Min(Area->Rect.Height, QuadMap->Height * 2) : Area->Rect.Height; 
    rect BoundRect = {
        .X = Area->Rect.X,
        .Y = Area->Rect.Y,
        .Width = Width - Transform,
        .Height = Height - Transform
    };
    point DeltaTile = MultiplyPoint(DeltaPoint, Transform);
    point Unbound = AddPoints(Area->Cursor, DeltaTile);
    Area->Cursor = ClampPoint(Unbound, &BoundRect);
    return !ComparePoints(Area->Cursor, AreaCursorOld);
}

static BOOL BoundCam(area_context *AreaContext, array_rect *QuadMap, point DeltaPoint) {
    point QuadMapCamOld = AreaContext->QuadMapCam;
    AreaContext->QuadMapCam = AddPoints(AreaContext->QuadMapCam, DeltaPoint);
    rect ClampRect = {
        .X = 0,
        .Y = 0,
        .Width = Max(0, QuadMap->Width - 16),
        .Height = Max(0, QuadMap->Height - 16)
    };

    AreaContext->QuadMapCam = ClampPoint(AreaContext->QuadMapCam, &ClampRect);
    return !ComparePoints(AreaContext->QuadMapCam, QuadMapCamOld);
}

static void UpdateQuadMapArea(quad_context *QuadContext, area_context *AreaContext) {
    rect QuadRect = {
        .X = AreaContext->QuadMapCam.X,
        .Y = AreaContext->QuadMapCam.Y,
        .Width = Min(Min( AreaContext->QuadMapArea.Rect.Width / 2, 16), QuadContext->QuadMap->Width),
        .Height = Min(Min(AreaContext->QuadMapArea.Rect.Height / 2, 16), QuadContext->QuadMap->Height)
    };
    TranslateQuadsToTiles(g_QuadData, &g_TileMap, &AreaContext->QuadMapArea.Rect, QuadContext->QuadMap, &QuadRect);
}

static BOOL MoveQuadMapCam(quad_context *QuadContext, area_context *AreaContext, point DeltaPoint) {
    BOOL ToRender = BoundCam(AreaContext, QuadContext->QuadMap, DeltaPoint);
    if(ToRender) {
        UpdateQuadMapArea(QuadContext, AreaContext);
    }
    return ToRender;
}

static point GetPlacePoint(area_context *AreaContext) {
    point QuadViewPoint = GetRelativeQuadPoint(&AreaContext->QuadMapArea);
    return (point) {
        .X = QuadViewPoint.X + AreaContext->QuadMapCam.X,
        .Y = QuadViewPoint.Y + AreaContext->QuadMapCam.Y
    };
}

static point GetRelativeCursor(area *Area) {
    return (point) {
        .X = Area->Cursor.X - Area->Rect.X,
        .Y = Area->Cursor.Y - Area->Rect.Y
    };
}

static BOOL ResizeQuadMap(quad_context *QuadContext, area_context *AreaContext, int DeltaQuadWidth, int DeltaQuadHeight) {
    int NewWidth = QuadContext->QuadMap->Width + DeltaQuadWidth;
    int NewHeight = QuadContext->QuadMap->Height + DeltaQuadHeight;
    BOOL ValidWidth = NewWidth > 0 && NewWidth <= 255;
    BOOL ValidHeight = NewWidth > 0 && NewHeight <= 255;
    BOOL ValidDimensions = ValidWidth && ValidHeight;
    if(ValidDimensions) {
        ArrayRectChangeSize(QuadContext->QuadMap, NewWidth, NewHeight);
        BoundCam(AreaContext, QuadContext->QuadMap, (point) {0, 0});
        UpdateQuadMapArea(QuadContext, AreaContext);
    }
    return ValidDimensions;
}

static BOOL IsMessageProp(int QuadProp) {
    return QuadProp == QuadPropMessage || QuadProp == QuadPropTV;
}

static BOOL IsWarpProp(int QuadProp) {
    return QuadProp == QuadPropDoor || QuadProp == QuadPropExit;
}

/**/
static void RenderSelect(array_rect *Offscreen, int ColorIndex, point Cursor, int Width, int Height) {
    int X = Cursor.X * TileLength;
    int Y = Cursor.Y * TileLength;
    memset(ArrayRectGet(Offscreen, X, Y), ColorIndex, Width);
    for(int OffY = 1; OffY < Height - 1; OffY++) {
        *ArrayRectGet(Offscreen, X, Y + OffY) = ColorIndex;
        *ArrayRectGet(Offscreen, X + Width - 1, Y + OffY) = ColorIndex;
    }
    memset(ArrayRectGet(Offscreen, X, Y + Height - 1), ColorIndex, Width);
}

static void UpdateMyWindow(HWND Window, int Width, int Height, void *Pixels, BITMAPINFO *Info) {
    PAINTSTRUCT Paint;
    RenderOnscreen(Window, BeginPaint(Window, &Paint), Width, Height, Pixels, Info);
    EndPaint(Window, &Paint);
}

static BOOL CharIsValid(char Char, int Index, int Size, BOOL SupportSpecial) {
    BOOL CharIsValid = IsAlphaNumeric(Char) || Char == ' ' || Char == '!' || Char == 'é' ;
    if(SupportSpecial) {
        CharIsValid |= Char == '\'' || Char == '\f' || Char == '-' || Char == '\n' || Char == ',' || Char == '.' || Char == '~' || Char == ':';
    }
    BOOL CharCanFit = Index + 1 < Size;
    return CharIsValid && CharCanFit;
}

static BOOL SpecailStringValidated(const char *Text) {
    int Lines = 0;
    int RowLen = 0;
    while(*Text != '\0') {
        if(*Text == '\n' || *Text == '\f') {
            Lines++;
            RowLen = 0;
        } else {
            if(RowLen >= 20) {
               return FALSE; 
            }
            RowLen++;
        }
        Text++;
    }
    return Lines < 15;
}

static int32_t UpdateText(char *Text, int Char, int Index, int Size, BOOL SupportSpecial) {
    if(Char == '/') {
        Char = '\n';
    }
    if(Char == '@') {
        Char = '\f';
    }
    if(Char == VK_BACK) {
        if(Index > 0) {
            Index--;
        }
        Text[Index] = '\0';
    } else if(CharIsValid(Char, Index, Size, SupportSpecial)) {
        if(!SupportSpecial || SpecailStringValidated(Text)) {
            Text[Index] = Char;
            Index++;
            Text[Index] = '\0';
        }
    }
    return Index;
} 

static int32_t GetQuadProp(array_rect *QuadMap, uint8_t *QuadProps, point QuadPoint) {
    int32_t Quad = *ArrayRectPt(QuadMap, QuadPoint);
    int32_t Prop = QuadProps[Quad];
    return Prop;
}

static text *GetTextPlace(area_context *AreaContext, text *Texts) { 
    text *Text = NULL;
    if(AreaContext->LockedMode) {
        point PlacePt = GetPlacePoint(AreaContext);
        Text = FindTextPt(Texts, PlacePt); 
        if(!Text) {
            Text = (text *) FindTextPt(g_Doors, PlacePt);
            if(!Text) {
                object *Object = FindObjectPt(GetPlacePoint(AreaContext)); 
                if(Object) { 
                    Text = &Object->Text;
                }
            }
        }
    }
    return Text;
}

static void UpdateNumber(int *Coord, int Char) {
    switch(Char) {
    case '0'...'9':
        {
            int NewVal = *Coord * 10 + Char - '0';
            if(NewVal < 255) {
                *Coord = NewVal;
            }
            break;
        }
    case VK_BACK:
        *Coord /= 10;
        break;
    } 
} 

static uint8_t *GetCurrentQuadPropPtr(area *Area, uint8_t *QuadProps) {
    int SetNumber = GetRelativeQuadIndex(Area);
    uint8_t *PropPtr = QuadProps + SetNumber;
    return PropPtr;
}

static BOOL UpdateCommand(command_context *Context, area_context *AreaContext, text *Texts, int Char) {
    BOOL ToRender = TRUE;
    text *Text = GetTextPlace(AreaContext, Texts);
    if(Context->CommandMode) {
        Context->Index = UpdateText(Context->Command, Char, Context->Index, _countof(Context->Command), FALSE);
    } else if(Context->InsertMode) { 
        if(Text) {
            if(Text->IsDoor) {
                if(Char == '+') {
                    Context->DstDim = (Context->DstDim + 1) % (DD_RIGHT + 1);
                } else switch(Context->DstDim) {
                case DD_NEITHER:
                    Text->Index = UpdateText(Text->Data, Char, Text->Index, _countof(Text->Data), FALSE);
                    break;
                case DD_LEFT:
                    UpdateNumber(&Text->DstPos.X, Char);
                    break;
                case DD_RIGHT:
                    UpdateNumber(&Text->DstPos.Y, Char);
                    break;
                }
            } else if(Text->IsObject) {
                if(Char == '+') {
                    Context->DstDim = (Context->DstDim + 1) % (DD_LEFT + 1);
                } else switch(Context->DstDim) {
                case DD_NEITHER:
                    Text->Index = UpdateText(Text->Data, Char, Text->Index, _countof(Text->Data), TRUE);
                    break;
                case DD_LEFT:
                    UpdateNumber(&Text->ObjectSpeed, Char);
                    break;
                }
            } else {
                Text->Index = UpdateText(Text->Data, Char, Text->Index, _countof(Text->Data), TRUE);
            }
        }
    } else if(Char == ':') {
        Context->CommandMode = TRUE;
    } else if(Char == 'i' && Text) {
        Context->InsertMode = TRUE;
    } else if(AreaContext->LockedMode) {
        uint8_t *Ptr = GetCurrentQuadPropPtr(&AreaContext->QuadDataArea, g_QuadProps);
        int Val = *Ptr;
        UpdateNumber(&Val, Char);
        *Ptr = Val;
    } else {
        ToRender = FALSE;
    }
    return ToRender;
}

static void *GetUserData(HWND Window) {
    return (void *) GetWindowLongPtr(Window, GWLP_USERDATA);
}

static void CloseCommandContext(command_context *Context, char *Error) {
    *Context = (command_context) {
        .Changed = Context->Changed
    };
    strcpy(Context->Command, Error);
}

static BOOL HandleMessages(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam, shared_context *SharedContext) {
    BOOL WasHandled = TRUE;
    area_context *AreaContext = SharedContext->AreaContext;
    command_context *CommandContext = SharedContext->CommandContext;
    render_context *RenderContext = SharedContext->RenderContext;
    text *Texts = SharedContext->Texts;
    switch(Message) {
    case WM_SIZE:
        if(RenderContext->Fullscreen) {
            UpdateFullscreen(Window);
        }
        break;
    case WM_CLOSE:
        if(CommandContext->Changed) {
            CloseCommandContext(CommandContext, "Error");
        } else {
            PostQuitMessage(0);
        }
        break;
    case WM_PAINT:
        UpdateMyWindow(
            Window, 
            RenderContext->Bitmap.Pixels.Width, 
            RenderContext->Bitmap.Pixels.Height, 
            RenderContext->Bitmap.Pixels.Bytes, 
            &RenderContext->Bitmap.Info.WinStruct
        );
        break;
    case WM_CHAR:
        RenderContext->ToRender = UpdateCommand(CommandContext, AreaContext, Texts, WParam);
        break;
    default:
        WasHandled = FALSE;
    }
    return WasHandled;
}

static LRESULT CALLBACK MyWndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    BOOL Handled = FALSE;
    shared_context *SharedContext = GetUserData(Window);
    if(SharedContext) {
        Handled = HandleMessages(Window, Message, WParam, LParam, SharedContext);
    }
    return Handled ? 0 : DefWindowProc(Window, Message, WParam, LParam);
}

static void DefaultQuadMap(array_rect *QuadMap) {
    QuadMap->Width = 16;
    QuadMap->Height = 16;
    memset(QuadMap->Bytes, 0, QuadMap->Width * QuadMap->Height);
}

static int ConvertUnicodeToAscii(char *AsciiString, const wchar_t *UnicodeString, int UnicodeCount) {
    int CorrectChars = 0;
    for(int Index = 0; Index < UnicodeCount; Index++) {
        if(*UnicodeString < 128) {
            CorrectChars++;
        }
        *AsciiString++ = *UnicodeString++;
    }
    return CorrectChars;
}

static int WideStringCharCount(wchar_t *WideString) {
    return wcslen(WideString) + 1;
}

static void ExtractQuadMapPath(char *Path, int PathCount) {
    Path[0] = '\0';
    int ArgCount;
    wchar_t *Line = GetCommandLineW();
    wchar_t **Args = CommandLineToArgvW(Line, &ArgCount);
    if(Args) {
        if(ArgCount == 2) {
            int Count = WideStringCharCount(Args[1]);
            if(Count <= PathCount) {
                int Converted = ConvertUnicodeToAscii(Path, Args[1], Count);
                if(Converted < Count) {
                    Path[0] = '\0';
                }
            }
        }
        LocalFree(Args);
    }
}

static BOOL CommandWriteQuadMap(command_context *CommandContext, const char *QuadMapPath, array_rect *QuadMap, text *Texts) {
    BOOL Success = *QuadMapPath && WriteQuadMap(QuadMapPath, QuadMap, Texts); 
    if(Success) {
        CommandContext->Changed = FALSE;
    }
    return Success;
}

static BOOL WriteAllButError(const char *Path, void *Data, uint32_t BytesToWrite) {
    uint32_t BytesWritten = WriteAll(Path, Data, BytesToWrite);
    return BytesToWrite == BytesWritten;
}

static int GetRelativeCursorIndex(area *Area) {
    point Point = GetRelativeCursor(Area);
    int Index = Point.X + Point.Y * Area->Rect.Width;
    return Index;
}

static void EditQuadAtCursor(quad_context *QuadContext, area_context *AreaContext, area *CurrentArea, area *NextArea) {
    int SelectedTile = GetRelativeCursorIndex(NextArea);

    *ArrayRectPt(&g_TileMap, CurrentArea->Cursor) = SelectedTile;

    point RelativeCursor = GetRelativeCursor(CurrentArea);
    int SetNumber = GetRelativeQuadIndex(CurrentArea);
    int SetPos = RelativeCursor.X % 2 + RelativeCursor.Y % 2 * 2;
    g_QuadData[SetPos + SetNumber * 4] = SelectedTile;

    UpdateQuadMapArea(QuadContext, AreaContext);
}

static void ProcessComandContext(command_context *CommandContext, quad_context *QuadContext, area_context *AreaContext, UINT32 *Colors, char *QuadMapPath, text *Texts) {
    BOOL Error = FALSE;
    if(strstr(CommandContext->Command, "e ")) {
        strcpy(QuadMapPath, CommandContext->Command + 2);
        ClearText(Texts);
        if(!ReadQuadMap(CommandContext->Command + 2, QuadContext->QuadMap, Texts)) {
            DefaultQuadMap(QuadContext->QuadMap);
            ClearText(Texts);
        }
        AreaContext->QuadMapCam = (point) {0, 0};
        UpdateQuadMapArea(QuadContext, AreaContext);
    } else if(strstr(CommandContext->Command, "w ")) {
        strcpy(QuadMapPath, CommandContext->Command + 2);
        Error |= !CommandWriteQuadMap(CommandContext, QuadMapPath, QuadContext->QuadMap, Texts);
    } else if(strcmp(CommandContext->Command, "w") == 0) {
        Error |= !CommandWriteQuadMap(CommandContext, QuadMapPath, QuadContext->QuadMap, Texts);
    } else if(strcmp(CommandContext->Command, "q!") == 0) {

        PostQuitMessage(0);
    } else if(strcmp(CommandContext->Command, "q") == 0) {
        if(CommandContext->Changed) {
            Error = TRUE;
        } else {
            PostQuitMessage(0);
        }
    } else if(strcmp(CommandContext->Command, "d") == 0) {
        BOOL QuadDataSuccess = WriteAllButError(g_QuadPath, g_QuadData, sizeof(g_QuadData));
        BOOL QuadPropSuccess = WriteAllButError(g_PropPath, g_QuadProps, sizeof(g_QuadProps));
        Error = !QuadDataSuccess || !QuadPropSuccess;
    } else if(strstr(CommandContext->Command, "t ")) {
        int PathI = atoi(&CommandContext->Command[2]);
        ReadTileSet(PathI);
        SetQuadDataArea();
    } else {
        Error = TRUE;
    }
    CloseCommandContext(CommandContext, Error ? "Error" : "");
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE InstancePrev, LPSTR CommandLine, int CommandShow) {
    g_TileMap = CreateStaticArrayRect(67, 44);
    uint8_t SpriteData[256 * TileSize] = {};
    text Texts[TEXT_COUNT] = {};

    render_context RenderContext = {
        .ToRender = TRUE,
        .Bitmap = InitBitmap(67 * TileLength, 44 * TileLength, EditorPallete, 6)
    };
    int TileDataCount = ReadTileData("TileData2", g_TileData, 256);
    ReadTileData("Numbers" , g_TileData + TileDataCount * TileSize, 256 - TileDataCount);
    command_context CommandContext = {};
    SetCurrentDirectory("../Shared");
    ReadTileSet(0);

    ReadTileData("Tiles/TileDataSprites", SpriteData, 256);

    /*Area Setup*/
    area_context AreaContext = {
        .QuadDataArea = {
            .Rect.X = 1,
            .Rect.Y = 1,
            .Rect.Width = 32,
            .Rect.Height = 16,
            .Cursor.X = 1,
            .Cursor.Y = 1,
        },

        .TileDataArea = {
            .Rect.X = 50,
            .Rect.Y = 34,
            .Rect.Width = 16,
            .Rect.Height = 6,
            .Cursor.X = 50,
            .Cursor.Y = 34,
        },

        .QuadMapArea = {
            .Rect.X = 34,
            .Rect.Y = 1,
            .Rect.Width = 32,
            .Rect.Height = 32,
            .Cursor.X = 34,
            .Cursor.Y = 1,
        },

        .TextArea = {
            .Rect.X = 34,
            .Rect.Y = 34,
            .Rect.Width = 15,
            .Rect.Height = 9,
            .Cursor.X = 34,
            .Cursor.Y = 34,
        },

        .DiagArea = {
            .Rect.X = 1,
            .Rect.Y = 18,
            .Rect.Width = 21,
            .Rect.Height = 15,
            .Cursor.X = 1,
            .Cursor.Y = 18,
        },

        .DefaultArea = {
            .Rect.X = 30,
            .Rect.Y = 19,
            .Rect.Width = 1,
            .Rect.Height = 1,
            .Cursor.X = 30,
            .Cursor.Y = 19,
        },
        
        .SpriteArea = {
            .Rect.X = 1,
            .Rect.Y = 34,
            .Rect.Width = 32,
            .Rect.Height = 6,
            .Cursor.X = 1,
            .Cursor.Y = 34,
        }
    };

    area *CurrentArea = &AreaContext.QuadDataArea;
    area *NextArea = &AreaContext.TileDataArea;

    SetQuadDataArea();

    /*QuadContext SetUp*/
    char QuadMapPath[32] = {};
    ExtractQuadMapPath(QuadMapPath, _countof(QuadMapPath));
    uint8_t QuadMapBytes[65536];
    array_rect QuadMap = {0, 0, QuadMapBytes};
    if(*QuadMapPath) {
        if(!ReadQuadMap(QuadMapPath, &QuadMap, Texts)) {
            strcpy(CommandContext.Command, "Error");
            DefaultQuadMap(&QuadMap);
            ClearText(Texts);
        }
    } else {
        DefaultQuadMap(&QuadMap);
    }

    quad_context QuadContext = {
        .QuadMap = &QuadMap,
    };

    UpdateQuadMapArea(&QuadContext, &AreaContext);

    /*Initalize TileDataArea*/
    int TileX = AreaContext.TileDataArea.Rect.X;
    int TileY = AreaContext.TileDataArea.Rect.Y;
    for(int TileID = 0; TileID < 96; TileID++) {
        *ArrayRectGet(&g_TileMap, TileX, TileY) = TileID;
        TileX++;
        if(TileX >= RectRight(&AreaContext.TileDataArea.Rect)) {
            TileX = AreaContext.TileDataArea.Rect.X;
            TileY++;
        }
    }
    
    /*Initalize SpriteArea*/
    sprite Sprites[128] = {};
    int SpriteCount = 64;
    for(int QuadX = 0; QuadX < 8; QuadX++) {
        point QuadPoint = {QuadX * 16 + 16, 280};
        SetStillAnimation(&Sprites[QuadX * 4], QuadX * 16, DirDown, QuadPoint);
    }

    /*Init Area Borders*/
    for(TileX = 1; TileX < 33; TileX++) {
        *ArrayRectGet(&g_TileMap, TileX, 17) = 102;
    }

    for(TileX = 1; TileX < 66; TileX++) {
        *ArrayRectGet(&g_TileMap, TileX,  0) =  97;
        *ArrayRectGet(&g_TileMap, TileX, 33) = 102;
        *ArrayRectGet(&g_TileMap, TileX, 43) = 102;
    }

    for(TileY = 1; TileY < 33; TileY++) {
        *ArrayRectGet(&g_TileMap,  0, TileY) =  99;
        *ArrayRectGet(&g_TileMap, 33, TileY) = 106;
        *ArrayRectGet(&g_TileMap, 66, TileY) = 100;
    }

    for(TileY = 34; TileY < 43; TileY++) {
        *ArrayRectGet(&g_TileMap,  0, TileY) =  99;
        *ArrayRectGet(&g_TileMap, 33, TileY) = 106;
        *ArrayRectGet(&g_TileMap, 49, TileY) = 106;
        *ArrayRectGet(&g_TileMap, 66, TileY) = 100;
    }

    *ArrayRectGet(&g_TileMap,  0,  0) =  96;
    *ArrayRectGet(&g_TileMap,  0, 17) = 104;
    *ArrayRectGet(&g_TileMap,  0, 33) = 104;
    *ArrayRectGet(&g_TileMap,  0, 43) = 101;
    *ArrayRectGet(&g_TileMap, 33,  0) = 107;
    *ArrayRectGet(&g_TileMap, 33, 17) = 110;
    *ArrayRectGet(&g_TileMap, 33, 33) = 109;
    *ArrayRectGet(&g_TileMap, 33, 43) = 108;
    *ArrayRectGet(&g_TileMap, 49, 33) = 107;
    *ArrayRectGet(&g_TileMap, 49, 43) = 108;
    *ArrayRectGet(&g_TileMap, 66,  0) =  98;
    *ArrayRectGet(&g_TileMap, 66, 33) = 105;
    *ArrayRectGet(&g_TileMap, 66, 43) = 103;

    /*Window Creation*/
    HWND Window = CreatePokeWindow("PokéEditor", MyWndProc, RenderContext.Bitmap.Pixels.Width, RenderContext.Bitmap.Pixels.Height);
    if(!Window) {
        return 1;
    }

    shared_context SharedContext = {
        .AreaContext = &AreaContext,
        .CommandContext = &CommandContext,
        .RenderContext = &RenderContext,
        .Texts = Texts,
    };
    SetWindowLongPtr(Window, GWLP_USERDATA, (LONG_PTR) &SharedContext);
    ShowWindow(Window, CommandShow);

    /*Main Loop*/
    rect RestoreWindowRect;
    MSG Message;
    int PropIndexSelected = 0;
    while(GetMessage(&Message, NULL, 0, 0)) {
        if(Message.message == WM_KEYDOWN) {
            BOOL WasCommandMode = CommandContext.CommandMode;
            BOOL WasInsertMode = CommandContext.InsertMode;
            switch(Message.wParam) {
            case VK_ESCAPE:
                CloseCommandContext(&CommandContext, "");
                RenderContext.ToRender = TRUE;
                break;
            case VK_TAB:
                Swap(&CurrentArea, &NextArea);
                RenderContext.ToRender = TRUE;
                break;
            case VK_F11:
                if(RenderContext.Fullscreen) {
                    RenderContext.Fullscreen = FALSE;  /*Needs to be done first!!!*/
                    SetMyWindowPos(Window, &RestoreWindowRect, WS_OVERLAPPEDWINDOW);
                } else {
                    RenderContext.Fullscreen = TRUE;
                    RestoreWindowRect = GetMyWindowRect(Window);
                    UpdateFullscreen(Window);
                }
                break;
            default:
                if(CommandContext.CommandMode) {
                    if(GetAsyncKeyState(VK_RETURN)) {
                        ProcessComandContext(&CommandContext, &QuadContext, &AreaContext, RenderContext.Bitmap.Info.Colors, QuadMapPath, Texts);
                        RenderContext.ToRender = TRUE;
                    }
                } else if(!CommandContext.InsertMode) {
                    if(CurrentArea == &AreaContext.QuadDataArea && AreaContext.LockedMode) {
                    } else if(CurrentArea == &AreaContext.QuadMapArea) {
                        switch(Message.wParam) {
                        /*Move Camera*/
                        case 'K':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext,  &AreaContext, (point) {0, -1});
                            break;
                        case 'H':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext,  &AreaContext, (point) {-1, 0});
                            break;
                        case 'J':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext, &AreaContext, (point) {0, 1});
                            break;
                        case 'L':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext, &AreaContext, (point) {1, 0});
                            break;
                        /*Resize*/
                        case VK_UP:
                            if(ArrayRectRowIsZeroed(&QuadMap, QuadMap.Height - 1)) {
                                RenderContext.ToRender = ResizeQuadMap(&QuadContext, &AreaContext, 0, -1);
                            }
                            break;
                        case VK_LEFT:
                            if(ArrayRectColIsZeroed(&QuadMap, QuadMap.Width - 1)) {
                                RenderContext.ToRender = ResizeQuadMap(&QuadContext, &AreaContext, -1, 0);
                            }
                            break;
                        case VK_DOWN:
                            RenderContext.ToRender = ResizeQuadMap(&QuadContext, &AreaContext, 0, 1);
                            break;
                        case VK_RIGHT:
                            RenderContext.ToRender = ResizeQuadMap(&QuadContext, &AreaContext, 1, 0);
                            break;
                        case VK_INSERT:
                            {
                                object *Object = FindObjectPt(GetPlacePoint(&AreaContext));
                                if(Object) {
                                    Object->Dir = (Object->Dir + 1) % 4;
                                    RenderContext.ToRender = TRUE;
                                }
                            }
                            break;
                        }
                    }
                    switch(Message.wParam) {
                    /*Current select control*/
                    case 'W':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, (point) {0, -1}, &QuadMap);
                        break;
                    case 'A':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, (point) {-1, 0}, &QuadMap);
                        break;
                    case 'S':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, (point) {0, 1}, &QuadMap);
                        break;
                    case 'D':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, (point) {1, 0}, &QuadMap);
                        break;
                    /*Commands*/
                    case 'Q':
                        AreaContext.LockedMode ^= TRUE;
                        CurrentArea->Cursor = RectTopLeft(&CurrentArea->Rect);
                        CurrentArea = &AreaContext.QuadDataArea;
                        NextArea->Cursor = RectTopLeft(&NextArea->Rect);
                        NextArea = AreaContext.LockedMode ? &AreaContext.QuadMapArea : &AreaContext.TileDataArea;
                        RenderContext.ToRender = TRUE;
                        break;
                    case 'E':
                        {
                            area *Area = &AreaContext.QuadDataArea;
                            int SetX = (Area->Cursor.X - Area->Rect.X) / 2;
                            int SetY = (Area->Cursor.Y - Area->Rect.Y) / 2;
                            int SelectedQuadTile = SetX + SetY * 16;
                            g_DefaultQuad = SelectedQuadTile; 
                        }
                        RenderContext.ToRender = TRUE;
                        break;
                    case 'R':
                        if(AreaContext.LockedMode) {
                            if(CurrentArea == &AreaContext.QuadDataArea) {
                                CurrentArea = &AreaContext.SpriteArea; 
                                RenderContext.ToRender = TRUE;
                            } else if(CurrentArea == &AreaContext.SpriteArea) {
                                CurrentArea = &AreaContext.QuadDataArea;
                                RenderContext.ToRender = TRUE;
                            }
                        }
                        break;
                    case 'Y':
                        g_PalleteNum = (g_PalleteNum + 1) % _countof(g_Palletes);
                        RenderContext.ToRender = TRUE;
                        break;
                    }
                }
            }

            if(!WasCommandMode && !WasInsertMode) {
                if(GetAsyncKeyState(VK_BACK) & 1) {
                    if(AreaContext.LockedMode && NextArea == &AreaContext.SpriteArea) {
                        object *Object = FindObjectPt(GetPlacePoint(&AreaContext)); 
                        if(Object) {
                            Object->Text.HasText = FALSE;
                        }
                    }
                    RenderContext.ToRender = TRUE;
                } else if(GetAsyncKeyState(VK_RETURN) & 1) {
                    if(AreaContext.LockedMode) {
                        if(CurrentArea == &AreaContext.QuadMapArea) {
                            if(NextArea == &AreaContext.QuadDataArea) {
                                /*PlaceQuad*/
                                area *Area = &AreaContext.QuadDataArea;
                                int SetX = (Area->Cursor.X - Area->Rect.X) / 2;
                                int SetY = (Area->Cursor.Y - Area->Rect.Y) / 2;
                                int SelectedQuadTile = SetX + SetY * 16;

                                point PlacePt = GetPlacePoint(&AreaContext);
                                uint8_t *PlacePtr = ArrayRectPt(&QuadMap, PlacePt);
                                if(*PlacePtr != SelectedQuadTile) {
                                    if(IsMessageProp(g_QuadProps[*PlacePtr]) & !IsMessageProp(g_QuadProps[SelectedQuadTile])) {
                                        text *Text = FindTextPt(Texts, PlacePt); 
                                        if(Text) {
                                            Text->HasText = FALSE;
                                        }
                                    }
                                    if(IsWarpProp(g_QuadProps[*PlacePtr]) & !IsWarpProp(g_QuadProps[SelectedQuadTile])) {
                                        text *Door = FindTextPt(g_Doors, PlacePt); 
                                        if(Door) {
                                            Door->HasText = FALSE;
                                        }
                                    }

                                    *PlacePtr = SelectedQuadTile;
                                    uint8_t *Tile = ArrayRectGet(&g_TileMap, CurrentArea->Cursor.X, CurrentArea->Cursor.Y);
                                    FillQuad(&g_TileMap, Tile, g_QuadData + SelectedQuadTile * 4);

                                    if(IsMessageProp(g_QuadProps[*PlacePtr])) {
                                        text *Text = AllocText(Texts, PlacePt);
                                        if(!Text) {
                                            CloseCommandContext(&CommandContext, "OOM");
                                        }
                                    }
                                    
                                    if(IsWarpProp(g_QuadProps[*PlacePtr])) {
                                        text *Door = AllocDoor(PlacePt);
                                        if(!Door) {
                                            CloseCommandContext(&CommandContext, "OOM");
                                        }
                                    }
                                    CommandContext.Changed = TRUE;
                                }
                            } else {
                                /*PlaceObject*/
                                int SpriteSetI = (NextArea->Cursor.X - NextArea->Rect.X) / 2;
                                point Place = GetPlacePoint(&AreaContext);
                                if(GetQuadProp(&QuadMap, g_QuadProps, Place) != QuadPropNone) {
                                    CloseCommandContext(&CommandContext, "ERR: Invalid position");
                                } else {
                                    object *Object = AllocObject(Place, SpriteSetI * 16, DirDown);
                                    if(!Object) { 
                                        CloseCommandContext(&CommandContext, "OOM: Objects");
                                    }
                                }
                                
                                CommandContext.Changed = TRUE;
                            }
                        }
                    } else if(CurrentArea == &AreaContext.QuadDataArea) {
                        EditQuadAtCursor(&QuadContext, &AreaContext, CurrentArea, NextArea);
                    }
                    RenderContext.ToRender = TRUE;
                }
            }
        }


        FillQuad(&g_TileMap, ArrayRectPt(&g_TileMap, RectTopLeft(&AreaContext.DefaultArea.Rect)), &g_QuadData[g_DefaultQuad * 4]); 
        rect SrcRect = {AreaContext.DefaultArea.Rect.X - 5, AreaContext.DefaultArea.Rect.Y - 1, 8, 1}; 
        PlaceFormattedText(&g_TileMap, &SrcRect, "Data: %2d", g_TileSetI); 
        TranslateMessage(&Message);
        DispatchMessage(&Message);
        if(RenderContext.ToRender) {
            memcpy(RenderContext.Bitmap.Info.Colors, g_Palletes[g_PalleteNum], sizeof(g_Palletes[g_PalleteNum]));
            if(CurrentArea == &AreaContext.QuadMapArea) {
                point Place = GetPlacePoint(&AreaContext);
                text *Text = GetTextPlace(&AreaContext, Texts);
                if(Text) {
                    const char *Data = Text->Data;
                    if(Text->IsDoor) {
                        rect SrcRect = AreaContext.DiagArea.Rect;
                        rect TopRect = {SrcRect.X, SrcRect.Y, SrcRect.Width, 1};
                        rect BotRect = {SrcRect.X, RectBottom(&SrcRect) - 1, SrcRect.Width, 1};

                        PlaceChars(&g_TileMap, &TopRect, Data, TRUE); 
                        PlaceFormattedText(&g_TileMap, &BotRect, "Dst %3d %3d", Text->DstPos.X, Text->DstPos.Y); 
                    } else if(Text->IsObject) {
                        rect SrcRect = AreaContext.DiagArea.Rect;
                        rect TopRect = {SrcRect.X, SrcRect.Y, SrcRect.Width, 14};
                        rect BotRect = {SrcRect.X, RectBottom(&SrcRect) - 1, SrcRect.Width, 1};

                        PlaceChars(&g_TileMap, &TopRect, Data, TRUE); 
                        PlaceFormattedText(&g_TileMap, &BotRect, "Speed %d", Text->ObjectSpeed); 
                    } else {
                        PlaceChars(&g_TileMap, &AreaContext.DiagArea.Rect, Data, TRUE); 
                    }
                } else {
                    PlaceChars(&g_TileMap, &AreaContext.DiagArea.Rect, "", FALSE); 
                } 
                PlaceFormattedText(&g_TileMap, &AreaContext.TextArea.Rect, 
                        "Dim %3d %3d" "\n" "Pos %3d %3d" "\n", 
                        QuadMap.Width, QuadMap.Height, Place.X, Place.Y);
            } else if(CurrentArea == &AreaContext.QuadDataArea && AreaContext.LockedMode) {
                int Prop = *GetCurrentQuadPropPtr(CurrentArea, g_QuadProps);
                PlaceFormattedText(&g_TileMap, &AreaContext.TextArea.Rect, "%s %d", Prop < _countof(g_PropStr) ? g_PropStr[Prop] : "TBD  ", Prop);
            } else {
                PlaceChars(&g_TileMap, &AreaContext.TextArea.Rect, "", FALSE);
            }
            rect LastRow = {
                .X = AreaContext.TextArea.Rect.X,
                .Y = RectBottom(&AreaContext.TextArea.Rect) - 1,
                .Width = AreaContext.TextArea.Rect.Width,
                .Height = 1,
            };
            PlaceFormattedText(&g_TileMap, &LastRow, ":%s", CommandContext.Command);

            int MaxX = RenderContext.Bitmap.Pixels.Width;
            int MaxY = RenderContext.Bitmap.Pixels.Height;

            int PrevCount = SpriteCount;
            for(int ObjI = 0; ObjI < _countof(g_Objects); ObjI++) {
                point ObjPt = SubPoints(g_Objects[ObjI].Text.Pos, AreaContext.QuadMapCam);
                if(g_Objects[ObjI].Text.HasText && ObjPt.X >= 0 && ObjPt.Y >= 0 && ObjPt.X < 16 && ObjPt.Y < 16) {
                    point TilePoint = AddPoints(AddPoints(MultiplyPoint(ObjPt, 2), RectTopLeft(&AreaContext.QuadMapArea.Rect)), (point) {1, 1});
                    point SpritePoint = MultiplyPoint(TilePoint, 8);
                    SetStillAnimation(&Sprites[SpriteCount], g_Objects[ObjI].Tile, g_Objects[ObjI].Dir, SpritePoint);
                    SpriteCount += 4;
                }
            }

            RenderTiles(&RenderContext.Bitmap.Pixels, &g_TileMap, g_TileData, (point) {0, 0});
            for(size_t I = SpriteCount; I-- > 0; ) {
                int RowsToRender = 8;
                if(Sprites[I].Y < 8) {
                    RowsToRender = Sprites[I].Y;
                } else if(Sprites[I].Y > MaxY) {
                    RowsToRender = Max(MaxY + 8 - Sprites[I].Y, 0);
                }

                int ColsToRender = 8;
                if(Sprites[I].X < 8) {
                    ColsToRender = Sprites[I].X;
                } else if(Sprites[I].X > MaxX) {
                    ColsToRender = Max(MaxX + 8 - Sprites[I].X, 0);
                }

                int DestX = Max(Sprites[I].X - 8, 0);
                int DestY = Max(Sprites[I].Y - 8, 0);
                uint8_t *Dest = RenderContext.Bitmap.Pixels.Bytes + DestY * RenderContext.Bitmap.Pixels.Width + DestX;

                int SrcX = Max(8 - Sprites[I].X, 0);
                int DispX = 1;
                if(Sprites[I].Flags & SprHorzFlag) {
                    SrcX = Min(Sprites[I].X, 7);
                    DispX = -1;
                }
                int DispY = 8;
                int SrcY = Max(8 - Sprites[I].Y, 0);
                if(Sprites[I].Flags & SprVertFlag) {
                    SrcY = Min(Sprites[I].Y, 7);
                    DispY = -8;
                }

                uint8_t *Src = SpriteData + Sprites[I].Tile * TileSize + SrcY * TileLength + SrcX;

                for(int Y = 0; Y < RowsToRender; Y++) {
                    uint8_t *Tile = Src;
                    for(int X = 0; X < ColsToRender; X++) {
                        if(*Tile != 2) {
                            Dest[X] = *Tile;
                        }
                        Tile += DispX;
                    }
                    Src += DispY;
                    Dest += RenderContext.Bitmap.Pixels.Width;
                }
            }

            SpriteCount = PrevCount;

            const int ColorIndexActive = 4;
            const int ColorIndexInactive = 5;
            if(!CommandContext.CommandMode && !CommandContext.InsertMode) {
                int Length = AreaContext.LockedMode ? 16 : 8;
                RenderSelect(&RenderContext.Bitmap.Pixels, ColorIndexActive, CurrentArea->Cursor, Length, Length);
                RenderSelect(&RenderContext.Bitmap.Pixels, ColorIndexInactive, NextArea->Cursor, Length, Length);
            }
            InvalidateRect(Window, NULL, FALSE);
            RenderContext.ToRender = FALSE;
        }
    }
    return Message.wParam;
}

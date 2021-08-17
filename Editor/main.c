#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define QuadPropSolid   1
#define QuadPropEdge    2
#define QuadPropMessage 4
#define QuadPropWater   8
#define CountofQuadProp 4

#define SizeOfQuadProps 128 
#define SizeOfQuadData 512 
#define SizeOfQuadMapBytes 65536
#define SizeOfCompressedTile 16

#define RunLengthPacketFlag 128

#define TileLength 8
#define TileSize 64

#define LastIndex(Array) (_countof(Array) - 1)

#define TEXT_COUNT 32

#define ATTRIBUTE(att) __attribute__((att))
#define UNUSED ATTRIBUTE(unused)

#define DefaultPallete {0xF8F8F8, 0xA8A8A8, 0x808080, 0x000000}
#define EditorPallete {\
    0xF8F8F8, 0xA8A8A8,\
    0x808080, 0x000000,\
    0x0000FF, 0xFF0000\
}

#define FirstLevelPallete {0xFFEFFF, 0xCEE7DE, 0xA5D6FF, 0x181010}

/**/
int g_DefaultQuad = 0;

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
    point Pos;
    char Data[256];
    int Index;
    int LineCount;
} text;

typedef struct area {
    rect Rect;
    point Cursor;
} area;

typedef struct area_context {
    BOOL LockedMode;
    union {
        struct area Areas[6];
        struct {
            area QuadDataArea;
            area TileDataArea;
            area QuadMapArea;
            area TextArea;
            area DiagArea;
            area DefaultArea;
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
    array_rect *TileMap;
    array_rect *QuadMap;
    uint8_t *QuadData;
} quad_context;

typedef struct command_context {
    char Command[15];
    int Index;
    BOOL CommandMode;
    BOOL InsertMode;
    BOOL Changed;
} command_context;

typedef struct shared_context {
    area_context *AreaContext;
    command_context *CommandContext;
    render_context *RenderContext;
    text *Texts;
} shared_context;



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
static point CreatePoint(int X,  int Y)  {
    point Point = {
        .X = X,
        .Y = Y
    };
    return Point;
}

static point ClampPoint(point Point,  rect *Rect)  {
    point ClampPoint = {
        .X = Clamp(Point.X, Rect->X, RectRight(Rect)),
        .Y = Clamp(Point.Y, Rect->Y, RectBottom(Rect))
    };
    return ClampPoint;
}

static point  AddPoints(point A, point B)  {
    point C = {
        .X = A.X + B.X,
        .Y = A.Y + B.Y
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

UNUSED
static void ArrayRectFill(array_rect *ArrayRect, uint8_t Fill, const rect *Rect) {
    uint8_t *Byte = ArrayRectGet(ArrayRect, Rect->X, Rect->Y);
    for(int Y = 0; Y < Rect->Height; Y++) {
        memset(Byte, Fill, Rect->Width);
        Byte += ArrayRect->Width;
    }
}

UNUSED
static void ArrayRectRowFill(array_rect *ArrayRect, uint8_t Fill, int FillY, int FillHeight) {
    uint8_t *Byte = ArrayRectGet(ArrayRect, 0, FillY);
    memset(Byte, Fill, ArrayRect->Width * FillHeight);
}

UNUSED
static void ArrayRectColFill(array_rect *ArrayRect, uint8_t Fill, int FillX, int FillWidth) {
    uint8_t *Byte = ArrayRectGet(ArrayRect, FillX, 0);
    for(int Y = 0; Y < ArrayRect->Height; Y++) {
        memset(Byte, Fill, FillWidth);
        Byte += ArrayRect->Width;
    }
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

UNUSED
static const char *StringifyBool(BOOL Bool) {
    return Bool ? "True" : "False";
}

/*Conversion*/
UNUSED
static point ConvertToQuadPoint(point Point) {
    point QuadPoint = {
        .X = CoordToQuad(Point.X),
        .Y = CoordToQuad(Point.Y)
    };
    return QuadPoint;
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

static BOOL ReadQuadMap(const char *Path, array_rect *QuadMap, text *Texts) {
    uint8_t RunData[65536];
    uint32_t BytesRead = ReadAll(Path, RunData, sizeof(RunData));

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
    }

    if(RunIndex < BytesRead) {
        int Count = RunData[RunIndex++];

        for(int I = 0; I < Count; I++) {
            if(RunIndex < BytesRead - 3) {
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
    }

    BOOL Success = EncodeSuccess && QuadIndex == Size;
    return Success;
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

UNUSED
static void TranslateQuadsToTilesBounded(uint8_t *QuadData, array_rect *TileMap, rect *TileRect, array_rect *QuadMap, rect *QuadRect) {
    rect QuadOutRect = *QuadRect;
    ArrayRectBoundRect(QuadMap, &QuadOutRect);
    rect TileOutRect = {
        .X = TileRect->X + (QuadOutRect.X - QuadRect->X) * 2,
        .Y = TileRect->Y + (QuadOutRect.Y - QuadRect->Y) * 2,
        .Width = QuadOutRect.Width * 2,
        .Height = QuadOutRect.Height * 2
    };
    ArrayRectBoundRect(TileMap, &TileOutRect);
    if(QuadOutRect.Width < TileOutRect.Width) {
        QuadOutRect.Width = TileOutRect.Width / 2;
    }
    if(QuadOutRect.Height < TileOutRect.Height) {
        QuadOutRect.Height = TileOutRect.Height / 2;
    }
    TranslateQuadsToTiles(QuadData, TileMap, &TileOutRect, QuadMap, &QuadOutRect);
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
        Output = 179;
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

static void PlaceText(array_rect *TileMap, rect *Rect, const char *Text, BOOL Format) {
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
        PlaceText(TileMap, Rect, Text, FALSE);
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
    SetWindowPos(Window, HWND_TOP, Rect->X, Rect->Y, Rect->Width, Rect->Height, SWP_FRAMECHANGED | SWP_NOREPOSITION);
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

        Window = CreateWindow(WindowClass.lpszClassName, WindowName, WS_OVERLAPPEDWINDOW, 
                CW_USEDEFAULT, CW_USEDEFAULT, WindowRect.Width, WindowRect.Height, NULL, 
                NULL, Instance, NULL);
    }
    return Window;
}

static void RenderOnscreen(HWND Window, HDC DeviceContext, 
        int Width, int Height, void *Pixels, BITMAPINFO *BitmapInfo) {
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

    StretchDIBits(DeviceContext, RenderX, RenderY, RenderWidth, RenderHeight, 
            0, 0, Width, Height, Pixels, BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
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

static void ClearText(text *Texts) {
   memset(Texts, 0, TEXT_COUNT * sizeof(*Texts)); 
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
static BOOL MoveSelectTile(area_context *AreaContext, area *Area, point DeltaPoint) {
    point AreaCursorOld = Area->Cursor;
    int Transform = AreaContext->LockedMode ? 2 : 1;
    rect BoundRect = {
        .X = Area->Rect.X,
        .Y = Area->Rect.Y,
        .Width = Area->Rect.Width - Transform,
        .Height = Area->Rect.Height - Transform
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
        .Width = QuadMap->Width - 16,
        .Height = QuadMap->Height - 16
    };

    AreaContext->QuadMapCam = ClampPoint(AreaContext->QuadMapCam, &ClampRect);
    return !ComparePoints(AreaContext->QuadMapCam, QuadMapCamOld);
}

static void UpdateQuadMapArea(quad_context *QuadContext, area_context *AreaContext) {
    rect QuadRect = {
        .X = AreaContext->QuadMapCam.X,
        .Y = AreaContext->QuadMapCam.Y,
        .Width = AreaContext->QuadMapArea.Rect.Width / 2,
        .Height = AreaContext->QuadMapArea.Rect.Height / 2
    };
    TranslateQuadsToTiles(QuadContext->QuadData, QuadContext->TileMap, &AreaContext->QuadMapArea.Rect, QuadContext->QuadMap, &QuadRect);
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
    BOOL ValidWidth = NewWidth >= 16 && NewWidth <= 255;
    BOOL ValidHeight = NewHeight >= 16 && NewHeight <= 255;
    BOOL ValidDimensions = ValidWidth && ValidHeight;
    if(ValidDimensions) {
        ArrayRectChangeSize(QuadContext->QuadMap, NewWidth, NewHeight);
        BoundCam(AreaContext, QuadContext->QuadMap, CreatePoint(0, 0));
        UpdateQuadMapArea(QuadContext, AreaContext);
    }
    return ValidDimensions;
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

UNUSED
static void RenderQuadSelect(array_rect *Offscreen, int ColorIndex, int TileX, int TileY) {
    int X = TileX * 8;
    int Y = TileY * 8;
    memset(ArrayRectGet(Offscreen, X, Y), ColorIndex, 16);
    for(int OffY = 1; OffY < 15; OffY++) {
        *ArrayRectGet(Offscreen, X, Y + OffY) = ColorIndex;
        *ArrayRectGet(Offscreen, X + 15, Y + OffY) = ColorIndex;
    }
    memset(ArrayRectGet(Offscreen, X, Y + 15), ColorIndex, 16);
}

static void UpdateMyWindow(HWND Window, render_context *RenderContext) {
    PAINTSTRUCT Paint;
    RenderOnscreen(Window, BeginPaint(Window, &Paint), RenderContext->Bitmap.Pixels.Width, RenderContext->Bitmap.Pixels.Height, RenderContext->Bitmap.Pixels.Bytes, &RenderContext->Bitmap.Info.WinStruct);
    EndPaint(Window, &Paint);
}

static BOOL CharIsValid(char Char, int Index, int Size, BOOL SupportSpecial) {
    BOOL CharIsValid = IsAlphaNumeric(Char) || Char == ' ' || Char == '!' || Char == 'é';
    if(SupportSpecial) {
        CharIsValid |= Char == '\'' || Char == '\f' || Char == '-' || Char == '\n' || Char == ',' || Char == '.';
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
    }
    return Text;
}

static BOOL UpdateCommand(command_context *Context, area_context *AreaContext, text *Texts, int Char) {
    BOOL ToRender = TRUE;
    text *Text = GetTextPlace(AreaContext, Texts);
    if(Context->CommandMode) {
        Context->Index = UpdateText(Context->Command, Char, Context->Index, _countof(Context->Command), FALSE);
    } else if(Context->InsertMode) { 
        if(Text) {
            Text->Index = UpdateText(Text->Data, Char, Text->Index, _countof(Text->Data), TRUE);
        }
    } else if(Char == ':') {
        Context->CommandMode = TRUE;
    } else if(Char == 'i' && Text) {
        Context->InsertMode = TRUE;
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
        UpdateMyWindow(Window, RenderContext);
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

static uint8_t *GetCurrentQuadPropPtr(area *Area, uint8_t *QuadProps) {
    int SetNumber = GetRelativeQuadIndex(Area);
    uint8_t *PropPtr = QuadProps + SetNumber;
    return PropPtr;
}

static void g_DefaultQuadMap(array_rect *QuadMap) {
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

    *ArrayRectPt(QuadContext->TileMap, CurrentArea->Cursor) = SelectedTile;

    point RelativeCursor = GetRelativeCursor(CurrentArea);
    int SetNumber = GetRelativeQuadIndex(CurrentArea);
    int SetPos = RelativeCursor.X % 2 + RelativeCursor.Y % 2 * 2;
    QuadContext->QuadData[SetPos + SetNumber * 4] = SelectedTile;

    UpdateQuadMapArea(QuadContext, AreaContext);
}

static void ProcessComandContext(command_context *CommandContext, quad_context *QuadContext, area_context *AreaContext, UINT32 *Colors, char *QuadMapPath, uint8_t *QuadProps, text *Texts) {
    BOOL Error = FALSE;
    if(strstr(CommandContext->Command, "e ")) {
        strcpy(QuadMapPath, CommandContext->Command + 2);
        ClearText(Texts);
        if(!ReadQuadMap(CommandContext->Command + 2, QuadContext->QuadMap, Texts)) {
            g_DefaultQuadMap(QuadContext->QuadMap);
            ClearText(Texts);
        }
        AreaContext->QuadMapCam = CreatePoint(0, 0);
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
        BOOL QuadDataSuccess = WriteAllButError("QuadData", QuadContext->QuadData, SizeOfQuadData);
        BOOL QuadPropSuccess = WriteAllButError("QuadProps", QuadProps, SizeOfQuadProps);
        Error = !QuadDataSuccess || !QuadPropSuccess;
    } else {
        Error = TRUE;
    }
    CloseCommandContext(CommandContext, Error ? "Error" : "");
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE InstancePrev, LPSTR CommandLine, int CommandShow) {
    uint8_t TileData[256 * 64] = {};
    text Texts[TEXT_COUNT] = {};
    array_rect TileMap = CreateStaticArrayRect(67, 41);

    render_context RenderContext = {
        .ToRender = TRUE,
        .Bitmap = InitBitmap(67 * TileLength, 41 * TileLength, EditorPallete, 6)
    };
    command_context CommandContext = {};
    SetCurrentDirectory("../Shared");

    int TileDataCount    = ReadTileData("TileData2", TileData, 256);
    ReadTileData("Numbers" , TileData + TileDataCount * TileSize, 256 - TileDataCount);

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
            .Rect.X = 1,
            .Rect.Y = 34,
            .Rect.Width = 16,
            .Rect.Height = 6,
            .Cursor.X = 1,
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
            .Rect.Width = 16,
            .Rect.Height = 6,
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
            .Rect.Y = 18,
            .Rect.Width = 1,
            .Rect.Height = 1,
            .Cursor.X = 30,
            .Cursor.Y = 18,
        }
    };

    area *CurrentArea = &AreaContext.QuadDataArea;
    area *NextArea = &AreaContext.TileDataArea;

    uint8_t QuadProps[256];
    ReadQuadProps("QuadProps", QuadProps);

    /*QuadData SetUp*/
    uint8_t QuadData[512];
    ReadQuadData("QuadData", QuadData);

    uint8_t *Set = QuadData;
    uint8_t *TileRow = ArrayRectGet(&TileMap, AreaContext.QuadDataArea.Rect.X, AreaContext.QuadDataArea.Rect.Y);
    for(int SetY = 0; SetY < 8; SetY++) {
        uint8_t *Tile = TileRow;
        for(int SetX = 0; SetX < 16; SetX++) {
            FillQuad(&TileMap, Tile, Set);
            Set += 4;
            Tile += 2;
        }
        TileRow += TileMap.Width * 2;
    }

    /*QuadContext SetUp*/
    char QuadMapPath[32] = {};
    ExtractQuadMapPath(QuadMapPath, _countof(QuadMapPath));
    uint8_t QuadMapBytes[65536];
    array_rect QuadMap = {0, 0, QuadMapBytes};
    if(*QuadMapPath) {
        if(!ReadQuadMap(QuadMapPath, &QuadMap, Texts)) {
            strcpy(CommandContext.Command, "Error");
            g_DefaultQuadMap(&QuadMap);
            ClearText(Texts);
        }
    } else {
        g_DefaultQuadMap(&QuadMap);
    }

    quad_context QuadContext = {
        .TileMap = &TileMap,
        .QuadMap = &QuadMap,
        .QuadData = QuadData
    };

    UpdateQuadMapArea(&QuadContext, &AreaContext);

    /*Initalize TileDataArea*/
    int TileX = AreaContext.TileDataArea.Rect.X;
    int TileY = AreaContext.TileDataArea.Rect.Y;
    for(int TileID = 0; TileID < 96; TileID++) {
        *ArrayRectGet(&TileMap, TileX, TileY) = TileID;
        TileX++;
        if(TileX >= RectRight(&AreaContext.TileDataArea.Rect)) {
            TileX = AreaContext.TileDataArea.Rect.X;
            TileY++;
        }
    }

    /*Init Area Borders*/
    for(TileX = 1; TileX < 33; TileX++) {
        *ArrayRectGet(&TileMap, TileX, 17) = 102;
    }

    for(TileX = 1; TileX < 66; TileX++) {
        *ArrayRectGet(&TileMap, TileX,  0) =  97;
        *ArrayRectGet(&TileMap, TileX, 33) = 102;
        *ArrayRectGet(&TileMap, TileX, 40) = 102;
    }

    for(TileY = 1; TileY < 33; TileY++) {
        *ArrayRectGet(&TileMap,  0, TileY) =  99;
        *ArrayRectGet(&TileMap, 33, TileY) = 106;
        *ArrayRectGet(&TileMap, 66, TileY) = 100;
    }

    *ArrayRectGet(&TileMap,  0,  0) =  96;
    *ArrayRectGet(&TileMap,  0, 17) = 104;
    *ArrayRectGet(&TileMap,  0, 33) = 104;
    *ArrayRectGet(&TileMap,  0, 40) = 101;
    *ArrayRectGet(&TileMap, 33,  0) = 107;
    *ArrayRectGet(&TileMap, 33, 17) = 110;
    *ArrayRectGet(&TileMap, 33, 33) = 109;
    *ArrayRectGet(&TileMap, 33, 40) = 108;
    *ArrayRectGet(&TileMap, 66,  0) =  98;
    *ArrayRectGet(&TileMap, 66, 33) = 105;
    *ArrayRectGet(&TileMap, 66, 40) = 103;
    for(TileY = 34; TileY < 40; TileY++) {
        *ArrayRectGet(&TileMap,  0, TileY) =  99;
        *ArrayRectGet(&TileMap, 33, TileY) = 106;
        *ArrayRectGet(&TileMap, 66, TileY) = 100;
    }

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
                    if(Message.wParam == VK_RETURN) {
                        ProcessComandContext(&CommandContext, &QuadContext, &AreaContext, RenderContext.Bitmap.Info.Colors, QuadMapPath, QuadProps, Texts);
                        RenderContext.ToRender = TRUE;
                    }
                } else if(!CommandContext.InsertMode) {
                    if(CurrentArea == &AreaContext.QuadDataArea && AreaContext.LockedMode) {
                        switch(Message.wParam) {
                        case 'J':
                            PropIndexSelected = (PropIndexSelected + 1) % CountofQuadProp;
                            RenderContext.ToRender = TRUE;
                            break;
                        case 'L':
                            *GetCurrentQuadPropPtr(CurrentArea, QuadProps) ^= (1 << PropIndexSelected);
                            RenderContext.ToRender = TRUE;
                            break;
                        }
                    } else if(CurrentArea == &AreaContext.QuadMapArea) {
                        switch(Message.wParam) {
                        /*Move Camera*/
                        case 'K':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext,  &AreaContext, CreatePoint(0, -1));
                            break;
                        case 'H':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext,  &AreaContext, CreatePoint(-1, 0));
                            break;
                        case 'J':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext, &AreaContext, CreatePoint(0, 1));
                            break;
                        case 'L':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext, &AreaContext, CreatePoint(1, 0));
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
                        }
                    }
                    switch(Message.wParam) {
                    /*Current select control*/
                    case 'W':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, CreatePoint(0, -1));
                        break;
                    case 'A':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, CreatePoint(-1, 0));
                        break;
                    case 'S':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, CreatePoint(0, 1));
                        break;
                    case 'D':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, CreatePoint(1, 0));
                        break;
                    /*Commands*/
                    case '0':
                        AreaContext.LockedMode ^= TRUE;
                        CurrentArea->Cursor = RectTopLeft(&CurrentArea->Rect);
                        CurrentArea = &AreaContext.QuadDataArea;
                        NextArea->Cursor = RectTopLeft(&NextArea->Rect);
                        NextArea = AreaContext.LockedMode ? &AreaContext.QuadMapArea : &AreaContext.TileDataArea;
                        RenderContext.ToRender = TRUE;
                        break;
                    case '1':
                        {
                            area *Area = &AreaContext.QuadDataArea;
                            int SetX = (Area->Cursor.X - Area->Rect.X) / 2;
                            int SetY = (Area->Cursor.Y - Area->Rect.Y) / 2;
                            int SelectedQuadTile = SetX + SetY * 16;
                            g_DefaultQuad = SelectedQuadTile; 
                        }
                        RenderContext.ToRender = TRUE;
                        break;
                    }
                }
            }
            if(GetAsyncKeyState(VK_RETURN) && !WasCommandMode) {
                if(AreaContext.LockedMode) {
                    if(CurrentArea == &AreaContext.QuadMapArea) {
                        area *Area = &AreaContext.QuadDataArea;
                        int SetX = (Area->Cursor.X - Area->Rect.X) / 2;
                        int SetY = (Area->Cursor.Y - Area->Rect.Y) / 2;
                        int SelectedQuadTile = SetX + SetY * 16;

                        point PlacePt = GetPlacePoint(&AreaContext);
                        uint8_t *PlacePtr = ArrayRectPt(&QuadMap, PlacePt);
                        if(*PlacePtr != SelectedQuadTile) {
                            if((*PlacePtr & QuadPropMessage) && !(SelectedQuadTile & QuadPropMessage)) {
                                text *Text = FindTextPt(Texts, PlacePt); 
                                if(Text) {
                                    Text->HasText = FALSE;
                                }
                            }
                            *PlacePtr = SelectedQuadTile;
                            uint8_t *Tile = ArrayRectGet(&TileMap, CurrentArea->Cursor.X, CurrentArea->Cursor.Y);
                            FillQuad(&TileMap, Tile, QuadData + SelectedQuadTile * 4);
                            if(*PlacePtr & QuadPropMessage) {
                                text *Text = AllocText(Texts, PlacePt);
                                if(!Text) {
                                    CloseCommandContext(&CommandContext, "OOM");
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
        FillQuad(&TileMap, ArrayRectPt(&TileMap, RectTopLeft(&AreaContext.DefaultArea.Rect)), &QuadData[g_DefaultQuad * 4]); 
        TranslateMessage(&Message);
        DispatchMessage(&Message);
        if(RenderContext.ToRender) {
            if(CurrentArea == &AreaContext.QuadMapArea) {
                point Place = GetPlacePoint(&AreaContext);
                if(GetQuadProp(&QuadMap, QuadProps, Place) & QuadPropMessage) {
                    const char *Data = "OOM";
                    text *Text = FindTextPt(Texts, Place);
                    if(Text) {
                        Data = Text->Data;
                    } else {
                        Text = AllocText(Texts, Place);
                        if(Text) {
                            Data = "";
                        }
                    }
                    PlaceText(&TileMap, &AreaContext.DiagArea.Rect, Data, TRUE); 
                } else {
                    PlaceText(&TileMap, &AreaContext.DiagArea.Rect, "", FALSE); 
                } 
                PlaceFormattedText(&TileMap, &AreaContext.TextArea.Rect, 
                        "Dim %3d %3d" "\n" "Pos %3d %3d" "\n", 
                        QuadMap.Width, QuadMap.Height, Place.X, Place.Y);
            } else if(CurrentArea == &AreaContext.QuadDataArea && AreaContext.LockedMode) {
                int Prop = *GetCurrentQuadPropPtr(CurrentArea, QuadProps);
                PlaceFormattedText(&TileMap, &AreaContext.TextArea.Rect, 
                        "Solid %d" "\n" "Edge  %d" "\n" "Msg   %d" "\n" "Water %d" "\n", !!(Prop & QuadPropSolid), 
                        !!(Prop & QuadPropEdge), !!(Prop & QuadPropMessage), !!(Prop & QuadPropWater));
            } else {
                PlaceText(&TileMap, &AreaContext.TextArea.Rect, "", FALSE);
            }
            rect LastRow = {
                .X = AreaContext.TextArea.Rect.X,
                .Y = RectBottom(&AreaContext.TextArea.Rect) - 1,
                .Width = AreaContext.TextArea.Rect.Width,
                .Height = 1,
            };
            PlaceFormattedText(&TileMap, &LastRow, ":%s", CommandContext.Command);

            RenderTiles(&RenderContext.Bitmap.Pixels, &TileMap, TileData, CreatePoint(0, 0));

            const int ColorIndexActive = 4;
            const int ColorIndexInactive = 5;
            if(!CommandContext.CommandMode && !CommandContext.InsertMode) {
                int Length = AreaContext.LockedMode ? 16 : 8;
                RenderSelect(&RenderContext.Bitmap.Pixels, ColorIndexActive, CurrentArea->Cursor, Length, Length);
                RenderSelect(&RenderContext.Bitmap.Pixels, ColorIndexInactive, NextArea->Cursor, Length, Length);
            }
            if(CurrentArea == &AreaContext.QuadDataArea && AreaContext.LockedMode) {
                point SelectPoint = CreatePoint(AreaContext.TextArea.Rect.X + 6, AreaContext.TextArea.Rect.Y + PropIndexSelected);
                RenderSelect(&RenderContext.Bitmap.Pixels, ColorIndexActive, SelectPoint, 8, 8);
            }
            InvalidateRect(Window, NULL, FALSE);
            RenderContext.ToRender = FALSE;
        }
    }
    return Message.wParam;
}

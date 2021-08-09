#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define QuadPropSolid   1
#define QuadPropEdge    2
#define QuadPropMessage 4
#define CountofQuadProp 3

#define SizeOfQuadProps 64
#define SizeOfQuadData 256
#define SizeOfQuadMapBytes 65536
#define SizeOfCompressedTile 16

#define RunLengthPacketFlag 128

#define TileLength 8
#define TileSize 64

#define LastIndex(Array) (_countof(Array) - 1)

#define DefaultPallete {0xF8F8F8, 0xA8A8A8, 0x808080, 0x000000}
#define EditorPallete {\
    0xF8F8F8, 0xA8A8A8,\
    0x808080, 0x000000,\
    0x0000FF, 0xFF0000\
}

#define FirstLevelPallete {0xFFEFFF, 0xCEE7DE, 0xA5D6FF, 0x181010}

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

static void ArrayRectFill(array_rect *ArrayRect, uint8_t Fill, const rect *Rect) {
    uint8_t *Byte = ArrayRectGet(ArrayRect, Rect->X, Rect->Y);
    for(int Y = 0; Y < Rect->Height; Y++) {
        memset(Byte, Fill, Rect->Width);
        Byte += ArrayRect->Width;
    }
}

static void ArrayRectRowFill(array_rect *ArrayRect, uint8_t Fill, int FillY, int FillHeight) {
    uint8_t *Byte = ArrayRectGet(ArrayRect, 0, FillY);
    memset(Byte, Fill, ArrayRect->Width * FillHeight);
}

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
    return Char >= '0' || Char <= '9';
}

static BOOL IsAlphaNumeric(int Char) {
    return IsLetter(Char) || IsNumber(Char);
}

static const char * StringifyBool(BOOL Bool) {
    return Bool ? "True" : "False";
}

/*Conversion*/
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
    int CompressionRatio = 4;
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

static BOOL ReadQuadMap(const char *Path, array_rect *QuadMap) {
    uint8_t RunData[65536];
    uint32_t BytesRead = ReadAll(Path, RunData, sizeof(RunData));

    QuadMap->Width = RunData[0] + 1;
    QuadMap->Height = RunData[1] + 1;
    int Size = QuadMap->Width * QuadMap->Height;
    int QuadIndex = 0;
    BOOL EncodeSuccess = TRUE;
    int RunIndex = 2;
    while(EncodeSuccess && RunIndex < BytesRead) {
        int QuadRaw = RunData[RunIndex++];
        int Quad = QuadRaw & 63;
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

static uint8_t * WriteCompressedQuad(uint8_t *RunPtr, int Quad, int Repeat) {
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
    uint8_t RunData[4096];
    uint8_t *RunPtr = RunData;
    *RunPtr++ = QuadMap->Width - 1;
    *RunPtr++ = QuadMap->Height - 1;

    int QuadSize = QuadMap->Width * QuadMap->Height;
    for(int QuadIndex = 1; QuadIndex < QuadSize; QuadIndex++) {
        int QuadCur = QuadMap->Bytes[QuadIndex];
        if(Repeat != 255 && QuadPrev == QuadCur) {
            Repeat++;
        } else {
            RunPtr = WriteCompressedQuad(RunPtr, QuadPrev, Repeat);
            Repeat = 0;
        }
        QuadPrev = QuadCur;
    }

    RunPtr = WriteCompressedQuad(RunPtr, QuadPrev, Repeat);
    return WriteAll(Path, RunData, RunPtr - RunData);
}

/*Rendering*/
static int WriteChar(uint8_t *Tile, char Char) {
    int Increase = 0;
    int Output = Char;
    if(Output == '\0' || Output == '\n') {
        Output = -1;
    } else if(Output >= '0' && Output <= ':') {
        Output += 110 - '0';
    } else if(IsUppercaseLetter(Output)) {
        Output += 121 - 'A';
    } else if(IsLowercaseLetter(Output)) {
        Output += 147 - 'a';
    } else if(Output == '!') {
        Output = 175;
    } else if(Output == 'é') {
        Output = 173;
    } else if(Output == '\'') {
        Output = 176;
    } else if(Output == '-') {
        Output = 177;
    } else {
        Output = 0;
    }

    if(Output < 0) {
        *Tile = 0;
    } else {
        *Tile = Output;
        Increase = 1;
    }
    return Increase;
}

static int PlaceText(array_rect *TileMap, rect *Rect, const char *Text) {
    uint8_t *TileRow = ArrayRectGet(TileMap, Rect->X, Rect->Y);
    for(int RelY = 0; RelY < Rect->Height; RelY++) {
        uint8_t *Tile = TileRow;
        for(int RelX = 0; RelX < Rect->Width; RelX++) {
            char Char = *Text;
            Text += WriteChar(Tile++, Char);
        }
        if(*Text == '\n') {
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
        PlaceText(TileMap, Rect, Text);
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



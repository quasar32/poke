#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>

#define UNUSED __attribute__((unused))

#define AnimPlayer 0
#define AnimSeal 84

#define BmWidth 160
#define BmHeight 144

#define SprHorzFlag 1
#define SprVertFlag 2

#define DirUp 0
#define DirLeft 1
#define DirDown 2
#define DirRight 3

#define StateStop 0
#define StateMoving 1

#define QuadPropSolid   1
#define QuadPropEdge    2
#define QuadPropMessage 4
#define QuadPropWater   8
#define CountofQuadProp 4

#define SizeOfCompressedTile 16

#define TileLength 8
#define TileSize 64

typedef struct point {
    short X;
    short Y;
} point;

typedef struct dim_rect {
    int X;
    int Y;
    int Width;
    int Height;
} dim_rect;

typedef struct text {
    point Pos;
    int Length;
    char Data[256];
} text;

typedef struct quad_map {
    int16_t Width;
    int16_t Height;
    int16_t DefaultQuad;
    int16_t Count;
    uint8_t Quads[256][256];
    point Loaded;
    text Texts[256]; 
} quad_map;

typedef struct sprite {
    uint8_t X;
    uint8_t Y;
    uint8_t Tile;
    uint8_t Flags;
} sprite;

typedef struct quad_info {
    point Point;
    int Map;
    int Dir;
    int Quad;
} quad_info;

/*Global Constants*/
static const point g_DirPoints[] = {
    [DirUp] = {0, -1},
    [DirLeft] = {-1, 0},
    [DirDown] = {0, 1},
    [DirRight] = {1, 0}
};

/*Globals*/
static int g_IsFullscreen = 0;
static int g_HasQuit = 0;
static dim_rect g_RestoreRect = {};
static uint8_t g_Keys[256] = {};
static uint8_t g_BmPixels[160 * 144];
static BITMAPINFO *g_BmInfo = NULL;

/*Misc*/
__attribute__((nonnull))
static void CopyCharsWithNull(char *Dest, const char *Source, size_t Length) {
    memcpy(Dest, Source, Length);
    Dest[Length] = '\0';
} 

static int GetTileFromChar(int Char) {
    int Output = Char;
    if(Output >= '0' && Output <= ':') {
        Output += 110 - '0';
    } else if(Output >= 'A' && Output <= 'Z') {
        Output += 121 - 'A';
    } else if(Output >= 'a' && Output <= 'z') {
        Output += 147 - 'a';
    } else if(Output == '!') {
        Output = 175;
    } else if(Output == 'é') {
        Output = 173;
    } else if(Output == '\'') {
        Output = 176;
    } else if(Output == '~') {
        Output = 179;
    } else if(Char == '-') {
        Output = 177;
    } else if(Char == ',') {
        Output = 180; 
    } else {
        Output = 0;
    }
    return Output;
}

static inline int ReverseDir(int Dir) {
    return (Dir + 2) % 4;
}

/*Math Functions*/
static int Min(int A, int B) {
    return A < B ? A : B;
}

static int Max(int A, int B) {
    return A > B ? A : B;
}

static int Clamp(int Value, int Bottom, int Top) {
    Value = Max(Value, Bottom);
    Value = Min(Value, Top);
    return Value;
}

/*Timing*/
static int64_t GetPerfFreq(void) {
    LARGE_INTEGER PerfFreq;
    QueryPerformanceFrequency(&PerfFreq);
    return PerfFreq.QuadPart;
}

static int64_t GetPerfCounter(void) {
    LARGE_INTEGER PerfCounter;
    QueryPerformanceCounter(&PerfCounter);
    return PerfCounter.QuadPart;
}


static int64_t GetDeltaCounter(int64_t BeginCounter) {
    return GetPerfCounter() - BeginCounter;
}

static void SleepTillNextFrame(int IsGranular, int64_t PerfFreq, 
                               int64_t BeginCounter, int64_t FrameDelta) {
    int64_t InitDeltaCounter = GetDeltaCounter(BeginCounter);
    if(InitDeltaCounter < FrameDelta) {
        if(IsGranular) {
            int64_t RemainCounter = FrameDelta - InitDeltaCounter;
            uint32_t SleepMS = 1000 * RemainCounter / PerfFreq;
            if(SleepMS) {
                Sleep(SleepMS);
            }
        }
        while(GetDeltaCounter(BeginCounter) < FrameDelta);
    }
} 

/*Point Functions*/ 
static point AddPoints(point A, point B)  {
    return (point) {
        .X = A.X + B.X,
        .Y = A.Y + B.Y
    };
}

static point SubPoints(point A, point B)  {
    return (point) {
        .X = A.X - B.X,
        .Y = A.Y - B.Y
    };
}

static int ComparePoints(point A, point B) {
    return A.X == B.X && A.Y == B.Y;
}

/*DimRect Functions*/
static dim_rect WinToDimRect(RECT WinRect) {
    return (dim_rect) {
        .X = WinRect.left,
        .Y = WinRect.top,
        .Width = WinRect.right - WinRect.left,
        .Height = WinRect.bottom - WinRect.top
    };
}

static dim_rect GetDimClientRect(HWND Window) {
    RECT WinClientRect;
    GetClientRect(Window, &WinClientRect);
    return WinToDimRect(WinClientRect);
}

static dim_rect GetDimWindowRect(HWND Window) {
    RECT WinWindowRect;
    GetWindowRect(Window, &WinWindowRect);
    return WinToDimRect(WinWindowRect);
}

/*Point Conversion Functions*/
static point ConvertToQuadPoint(point Point) {
    point QuadPoint = {
        .X = Point.X / 16,
        .Y = Point.Y / 16
    };
    return QuadPoint;
}

static point GetFacingPoint(point Pos, int Dir) {
    point OldPoint = ConvertToQuadPoint(Pos);
    point DirPoint = g_DirPoints[Dir];
    return AddPoints(OldPoint, DirPoint);
}

/*Quad Functions*/
static void SetToTiles(uint8_t TileMap[32][32], int TileX, int TileY, const uint8_t Set[4]) {
    TileMap[TileY][TileX] = Set[0];
    TileMap[TileY][TileX + 1] = Set[1];
    TileMap[TileY + 1][TileX] = Set[2];
    TileMap[TileY + 1][TileX + 1] = Set[3];
}

static int GetQuadMapDir(quad_map QuadMaps[2], int Map) {
    point DirPoint = SubPoints(QuadMaps[!Map].Loaded, QuadMaps[Map].Loaded);
    for(size_t I = 0; I < _countof(g_DirPoints); I++) {
        if(ComparePoints(DirPoint, g_DirPoints[I])) {
            return I;
        }
    }
    return -1; 
} 

static quad_info GetQuad(quad_map QuadMaps[2], quad_info QuadInfo) {
    int OldWidth = QuadMaps[QuadInfo.Map].Width;
    int OldHeight = QuadMaps[QuadInfo.Map].Height;

    int NewWidth = QuadMaps[!QuadInfo.Map].Width;
    int NewHeight = QuadMaps[!QuadInfo.Map].Height;

    switch(QuadInfo.Dir) {
    case DirUp:
        if(QuadInfo.Point.Y < 0) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y += NewHeight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DirLeft:
        if(QuadInfo.Point.X < 0) {
            QuadInfo.Map ^= 1;
            QuadInfo.Point.X += NewHeight; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
        }
        break;
    case DirDown:
        if(QuadInfo.Point.Y >= QuadMaps[QuadInfo.Map].Height) {
            QuadInfo.Point.X += (NewWidth - OldWidth) / 2;
            QuadInfo.Point.Y -= OldHeight; 
            QuadInfo.Map ^= 1;
        }
        break;
    case DirRight:
        if(QuadInfo.Point.X >= QuadMaps[QuadInfo.Map].Width) {
            QuadInfo.Point.X -= OldWidth; 
            QuadInfo.Point.Y += (NewHeight - OldHeight) / 2;
            QuadInfo.Map ^= 1;
        } 
        break;
    }

    if(QuadInfo.Point.X >= 0 && QuadInfo.Point.X < QuadMaps[QuadInfo.Map].Width && 
       QuadInfo.Point.Y >= 0 && QuadInfo.Point.Y < QuadMaps[QuadInfo.Map].Height) {
        QuadInfo.Quad = QuadMaps[QuadInfo.Map].Quads[QuadInfo.Point.Y][QuadInfo.Point.X];
    } 

    return QuadInfo;
}

/*Data loaders*/
static uint32_t ReadAll(const char *Path, void *Bytes, uint32_t BytesToRead) {
    DWORD BytesRead = 0;
    HANDLE FileHandle = CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, 
                                   NULL, OPEN_EXISTING, 0, NULL);
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

static int ReadQuadMap(const char *Path, quad_map *QuadMap) {
    uint8_t RunData[65536];
    uint32_t BytesRead = ReadAll(Path, RunData, sizeof(RunData));

    int EncodeSuccess = 1;
    uint32_t RunIndex = 2;

    /*ReadQuads*/
    int QuadIndex = 0;
    QuadMap->Width = RunData[0] + 1;
    QuadMap->Height = RunData[1] + 1;
    int Size = QuadMap->Width * QuadMap->Height;
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
            EncodeSuccess = 0;
        }

        for(int I = QuadIndex; I < QuadIndex + Repeat; I++) {
            QuadMap->Quads[I / QuadMap->Width][I % QuadMap->Width] = Quad; 
        }

        QuadIndex += Repeat;
    }

    /*DefaultQuad*/
    if(RunIndex < BytesRead) {
        QuadMap->DefaultQuad = RunData[RunIndex++];
    }

    /*ReadText*/
    if(RunIndex < BytesRead) {
        QuadMap->Count = RunData[RunIndex++];

        for(int TextIndex = 0; TextIndex < QuadMap->Count && EncodeSuccess; ) {
            if(RunIndex < BytesRead - 3) {
                int X = RunData[RunIndex++]; 
                int Y = RunData[RunIndex++];
                size_t Length = RunData[RunIndex++];
                if(Length < _countof(QuadMap->Texts[TextIndex].Data)) {
                    QuadMap->Texts[TextIndex].Pos.X = X;
                    QuadMap->Texts[TextIndex].Pos.Y = Y;
                    QuadMap->Texts[TextIndex].Length = Length;
                    CopyCharsWithNull(QuadMap->Texts[TextIndex].Data, (char *) &RunData[RunIndex], Length);

                    RunIndex += Length;
                    TextIndex++;
                } else {
                    QuadMap->Count--;
                    EncodeSuccess = 0;
                }   
            } else {
                QuadMap->Count = TextIndex;
                EncodeSuccess = 0;
            }
        } 
    }

    int Success = EncodeSuccess && QuadIndex == Size;
    return Success;
}

/*Win32 Functions*/
static void RenderFrame(HWND Window, HDC DeviceContext) {
    dim_rect ClientRect = GetDimClientRect(Window);

    int RenderWidth = ClientRect.Width - ClientRect.Width % BmWidth;
    int RenderHeight = ClientRect.Height - ClientRect.Height % BmHeight;

    int RenderColSize = RenderWidth * BmHeight;
    int RenderRowSize = RenderHeight * BmWidth;
    if(RenderColSize > RenderRowSize) {
        RenderWidth = RenderRowSize / BmHeight;
    } else {
        RenderHeight = RenderColSize / BmWidth;
    }
    int RenderX = (ClientRect.Width - RenderWidth) / 2;
    int RenderY = (ClientRect.Height - RenderHeight) / 2;

    StretchDIBits(DeviceContext, RenderX, RenderY, RenderWidth, RenderHeight,
                  0, 0, BmWidth, BmHeight, g_BmPixels, g_BmInfo, DIB_RGB_COLORS, SRCCOPY);
    PatBlt(DeviceContext, 0, 0, RenderX, ClientRect.Height, BLACKNESS);
    PatBlt(DeviceContext, RenderX + RenderWidth, 0, RenderX, ClientRect.Height, BLACKNESS);
    PatBlt(DeviceContext, RenderX, 0, RenderWidth, RenderY, BLACKNESS);
    PatBlt(DeviceContext, RenderX, RenderY + RenderHeight, RenderWidth, RenderY + 1, BLACKNESS);
}

static void SetMyWindowPos(HWND Window, DWORD Style, dim_rect Rect) {
    SetWindowLongPtr(Window, GWL_STYLE, Style | WS_VISIBLE);
    SetWindowPos(Window, 
                 HWND_TOP, 
                 Rect.X, Rect.Y, Rect.Width, Rect.Height, 
                 SWP_FRAMECHANGED | SWP_NOREPOSITION);
}

static void PaintFrame(HWND Window) {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    RenderFrame(Window, DeviceContext);
    EndPaint(Window, &Paint);
}

void UpdateClientSize(HWND Window, int IsFullscreen) {
    if(IsFullscreen) {
        dim_rect ClientRect = GetDimClientRect(Window);

        HMONITOR Monitor = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
        MONITORINFO MonitorInfo = {
            .cbSize = sizeof(MonitorInfo)
        };
        GetMonitorInfo(Monitor, &MonitorInfo);

        dim_rect MonitorRect = WinToDimRect(MonitorInfo.rcMonitor);

        if(MonitorRect.Width != ClientRect.Width || MonitorRect.Height != ClientRect.Height) {
            SetMyWindowPos(Window, WS_POPUP, MonitorRect);
        }
    }
}

static void ToggleFullscreen(HWND Window) {
    ShowCursor(g_IsFullscreen);
    if(g_IsFullscreen) {
        SetMyWindowPos(Window, WS_OVERLAPPEDWINDOW, g_RestoreRect);
    } else {
        g_RestoreRect = GetDimWindowRect(Window);
    }
    g_IsFullscreen ^= 1;
}

static int IsFirstKeyFrame(LPARAM LParam) {
    return LParam >> 30 == 0;
}

static LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    switch(Message) {
    case WM_KILLFOCUS:
        memset(g_Keys, 0, sizeof(g_Keys));
        return 0;
    case WM_CLOSE:
        g_HasQuit = 1;
        return 0;
    case WM_PAINT:
        PaintFrame(Window);
        return 0;
    case WM_KEYDOWN:
        switch(WParam) {
        case VK_F11:
            ToggleFullscreen(Window);
            break;
        default:
            if(IsFirstKeyFrame(LParam)) {
                g_Keys[WParam] = 1;
            }
        }
        return 0;
    case WM_KEYUP:
        g_Keys[WParam] = 0;
        return 0;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

int WINAPI WinMain(HINSTANCE Instance, UNUSED HINSTANCE Prev, UNUSED LPSTR CmdLine, int CmdShow) {
    srand(time(NULL));

    /*InitBitmap*/
    g_BmInfo = alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 4);
    g_BmInfo->bmiHeader = (BITMAPINFOHEADER) {
        .biSize = sizeof(g_BmInfo->bmiHeader),
        .biWidth = BmWidth,
        .biHeight = -BmHeight,
        .biPlanes = 1,
        .biBitCount = 8,
        .biCompression = BI_RGB,
        .biClrUsed = 4
    };
    g_BmInfo->bmiColors[0] = (RGBQUAD) {0xF8, 0xF8, 0xF8, 0x00};
    g_BmInfo->bmiColors[1] = (RGBQUAD) {0xA8, 0xA8, 0xA8, 0x00};
    g_BmInfo->bmiColors[2] = (RGBQUAD) {0x80, 0x80, 0x80, 0x00};
    g_BmInfo->bmiColors[3] = (RGBQUAD) {0x00, 0x00, 0x00, 0x00};

    /*InitWindow*/
    WNDCLASS WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = Instance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = "PokeWindowClass"
    };
    if(!RegisterClass(&WindowClass)) {
        return 1;
    }

    RECT WinWindowRect = {0, 0, BmWidth, BmHeight};
    AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, 0);

    dim_rect WindowRect = WinToDimRect(WinWindowRect);

    HWND Window = CreateWindow(WindowClass.lpszClassName, "PokeGame", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, WindowRect.Width, WindowRect.Height, 
                               NULL, NULL, Instance, NULL);
    if(!Window) {
        return 1;
    }

    SetCurrentDirectory("../Shared");

    /*LoadQuadData*/
    uint8_t QuadData[128][4] = {};
    int SetsRead = ReadAll("QuadData", QuadData, sizeof(QuadData)); 
    for(int Set = 0; Set < SetsRead; Set++) {
        for(int I = 0; I < 4; I++) {
            QuadData[Set][I] = Clamp(QuadData[Set][I], 0, 95);
        }
    }
    
    /*ReadQuadProps*/
    uint8_t QuadProps[128] = {};
    ReadAll("QuadProps", QuadProps, sizeof(QuadProps));

    /*LoadTileData*/
    uint8_t TileData[256 * TileSize];
    uint8_t SpriteData[256 * TileSize];
    uint8_t FlowerData[3 * TileSize];
    uint8_t WaterData[7 * TileSize];

    ReadTileData("TileData", TileData, 110);
    ReadTileData("Numbers", TileData + 110 * TileSize, 146);
    ReadTileData("SpriteData", SpriteData, 256);
    ReadTileData("FlowerData", FlowerData, 3);
    ReadTileData("WaterData", WaterData, 7);
    ReadTileData("ShadowData", SpriteData + 255 * TileSize, 1);

    /*InitQuadMaps*/
    static quad_map QuadMaps[2] = {
        {
            .Loaded = {0, 2} 
        },
        {
            .Loaded = {-1, 0}
        }
    };

    #define WorldWidth 1
    #define WorldHeight 4 

    const char *QuadMapPaths[WorldHeight][WorldWidth] = {
        {"VirdianCity"},
        {"Route1"},
        {"PalleteTown"},
        {"Route21"}
    };

    ReadQuadMap("PalleteTown", &QuadMaps[0]);

    /*InitObjects*/
    struct object {
        point Pos;

        uint8_t Dir;
        uint8_t State;
        uint8_t Speed;
        uint8_t Tile;

        uint8_t Map;
        uint8_t IsRight;
        uint8_t Tick;

        const char *Text;
    } Objs[] = {
        {
            .Pos = {80, 96},
            .Dir = DirDown,
            .Speed = 2,
            .Tile = 0,
            .Map = 0
        }, 
        
        {
            .Pos = {176, 224},
            .Dir = DirDown,
            .Speed = 1,
            .Tile = 14,
            .Map = 0
        },

        {
            .Pos = {48, 128},
            .Dir = DirDown,
            .Speed = 1,
            .Tile = 28,
            .Map = 0,
        },

        {
            .Pos = {80, 384},
            .Dir = DirDown,
            .Speed = 1,
            .Tile = 98,
            .Map = 1
        }
    };
    
    int ObjCount = 4;

    /*GameBoyGraphics*/
    sprite Sprites[40] = {};
    uint8_t TileMap[32][32];

    uint8_t ScrollX = 0;
    uint8_t ScrollY = 0;

    /*TranslateFullQuadMapToTiles*/
    void PlaceQuadMap(int QuadMinX, int QuadMinY, 
                      int QuadMaxX, int QuadMaxY,
                      int TileMinX, int TileMinY) {
        int TileY = TileMinY;
        for(int QuadY = QuadMinY; QuadY < QuadMaxY; QuadY++) {
            int TileX = TileMinX;
            for(int QuadX = QuadMinX; QuadX < QuadMaxX; QuadX++) {
                quad_info QuadInfo = {
                    .Point = (point) {QuadX, QuadY},
                    .Map = Objs[0].Map,
                    .Dir = GetQuadMapDir(QuadMaps, QuadInfo.Map),
                    .Quad = -1
                };
                int Quad = GetQuad(QuadMaps, QuadInfo).Quad;
                if(Quad < 0) {
                    Quad = QuadMaps[QuadInfo.Map].DefaultQuad;
                }
                SetToTiles(TileMap, TileX, TileY, QuadData[Quad]); 
                TileX = (TileX + 2) % 32;
            }
            TileY = (TileY + 2) % 32;
        } 
    }

    int QuadMinX = Objs[0].Pos.X / 16 - 4;
    int QuadMinY = Objs[0].Pos.Y / 16 - 4;
    int QuadMaxX = QuadMinX + 10;
    int QuadMaxY = QuadMinY + 9;
    PlaceQuadMap(QuadMinX, QuadMinY, QuadMaxX, QuadMaxY, 0, 0);

    /*WindowMap*/
    uint8_t WindowMap[32][32];
    uint8_t WindowY = 144;

    /*Misc*/
    uint64_t Tick = 0;

    int IsLeaping = 0;
    int IsPaused = 0;

    /*Text*/
    const char *ActiveText = "Text not init";
    point TextTilePt = {1, 14};
    uint64_t TextTick = 0;

    /*Timing*/
    int64_t BeginCounter = 0;
    int64_t PerfFreq = GetPerfFreq();
    int64_t FrameDelta = PerfFreq / 30;

    /*LoadWinmm*/
    int IsGranular = 0;

    typedef MMRESULT WINAPI(winmm_func)(UINT);

    winmm_func *TimeEndPeriod = NULL;
    HMODULE WinmmLib = LoadLibrary("winmm.dll");
    if(WinmmLib) {
        winmm_func *TimeBeginPeriod = (winmm_func *) GetProcAddress(WinmmLib, "timeBeginPeriod"); 
        if(TimeBeginPeriod) {
            TimeEndPeriod = (winmm_func *) GetProcAddress(WinmmLib, "timeEndPeriod"); 
            if(TimeEndPeriod) {
                IsGranular = TimeBeginPeriod(1) == TIMERR_NOERROR;
            }
        }
        if(!IsGranular) {
            FreeLibrary(WinmmLib);
        }
    }

    /*MainLoop*/
    ShowWindow(Window, CmdShow);
    while(!g_HasQuit) {
        BeginCounter = GetPerfCounter();

        int TickCycle = Tick / 16;

        /*ProcessMessage*/
        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        /*InitSharedUpdate*/
        point NextPoints[] = {
            [DirUp] = {0, 1},
            [DirLeft] = {1, 0},
            [DirDown] = {0, 1},
            [DirRight] = {1, 0}
        };
        sprite *PlayerQuad = Sprites;

        /*UpdatePlayer*/
        if(IsPaused) {
            if(ActiveText[0]) {
                switch(TextTilePt.Y) {
                case 17:
                    /*MoreTextDisplay*/
                    if(TextTick > 0) {
                        TextTick--;
                    } else switch(ActiveText[-1]) {
                    case '\n':
                        memcpy(&WindowMap[14][1], &WindowMap[15][1], 17);
                        memset(&WindowMap[13][1], 0, 17);
                        memset(&WindowMap[15][1], 0, 17);
                        TextTilePt.Y = 16;
                        break;
                    case '\f':
                        TextTilePt.Y = 14;
                        break;
                    }
                    break;
                case 18:
                    /*MoreTextWait*/
                    WindowMap[16][18] = TextTick / 16 % 2 ? 0 : 178;
                    if(g_Keys['X']) {
                        if(ActiveText[-1] == '\n') {
                            memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                            memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                        }
                        memset(&WindowMap[14][1], 0, 17);
                        memset(&WindowMap[16][1], 0, 18);
                        TextTilePt.Y = 17;
                        g_Keys['X'] = 0;
                        TextTick = 4;
                    } else {
                        TextTick++;
                    }
                    break;
                default:
                    switch(ActiveText[0]) {
                    case '\n':
                        TextTilePt.X = 1;
                        TextTilePt.Y += 2;
                        break;
                    case '\f':
                        TextTilePt.X = 1;
                        TextTilePt.Y = 18;
                        break;
                    default:
                        WindowMap[TextTilePt.Y][TextTilePt.X] = GetTileFromChar(ActiveText[0]);
                        TextTilePt.X++;
                    }
                    ActiveText++;
                }
            } else if(g_Keys['X']) {
                IsPaused = 0;
                WindowY = 144;
                g_Keys['X'] = 0;
            }
        } else {
            sprite *ShadowQuad = Sprites + 36;

            /*PlayerUpdate*/
            if(Objs[0].Tick == 0) {
                memset(ShadowQuad, 0, sizeof(*ShadowQuad) * 4);
                int AttemptLeap = 0;

                /*PlayerKeyboard*/
                int HasMoveKey = 1;
                if(g_Keys['W']) {
                    Objs[0].Dir = DirUp;
                } else if(g_Keys['A']) {
                    Objs[0].Dir = DirLeft;
                } else if(g_Keys['S']) {
                    Objs[0].Dir = DirDown;
                    AttemptLeap = 1;
                } else if(g_Keys['D']) {
                    Objs[0].Dir = DirRight;
                } else {
                    HasMoveKey = 0;
                }

                point NewPoint = GetFacingPoint(Objs[0].Pos, Objs[0].Dir);

                /*FetchQuadProp*/
                quad_info QuadInfo = {
                    .Point = NewPoint,
                    .Map = Objs[0].Map,
                    .Dir = GetQuadMapDir(QuadMaps, QuadInfo.Map),
                    .Quad = -1,
                };
                QuadInfo = GetQuad(QuadMaps, QuadInfo);

                NewPoint = QuadInfo.Point;
                int Quad = QuadInfo.Quad;
                int NewQuadMap = QuadInfo.Map;

                point StartPos = AddPoints(NewPoint, g_DirPoints[ReverseDir(Objs[0].Dir)]); 
                StartPos.X *= 16;
                StartPos.Y *= 16;

                int Prop = Quad < 0 ? QuadPropSolid : QuadProps[Quad];

                /*FetchObject*/
                AttemptLeap &= !!(Prop & QuadPropEdge);
                point TestPoint = NewPoint; 
                if(AttemptLeap) {
                    TestPoint.Y++;
                }

                int ObjI = 1;
                for(; ObjI < ObjCount; ObjI++) {
                    if(Objs[ObjI].Map == Objs[0].Map) {
                        point OtherOldPoint = ConvertToQuadPoint(Objs[ObjI].Pos);
                        if(Objs[ObjI].Tick > 0) {
                            point OtherDirPoint = NextPoints[Objs[ObjI].Dir];
                            point OtherNewPoint = AddPoints(OtherOldPoint, OtherDirPoint);
                            if(ComparePoints(TestPoint, OtherOldPoint) ||
                               ComparePoints(TestPoint, OtherNewPoint)) {
                                Prop = QuadPropSolid;
                                break;
                            }
                        } else if(ComparePoints(TestPoint, OtherOldPoint)) {
                            Prop = QuadPropSolid;
                            if(!AttemptLeap) {
                                Prop |= QuadPropMessage;
                            }
                            break;
                        }
                    }
                }

                /*PlayerTestProp*/
                if(g_Keys['X']) {
                    if(Prop & QuadPropMessage) {
                        IsPaused = 1;
                        WindowY = 96;
                        g_Keys['X'] = 0;

                        /*PlaceTextBox*/
                        uint8_t TextBoxHeadRow[] = {96, [1 ... 18] = 97, 98};
                        uint8_t TextBoxBodyRow[] = {99, [1 ... 18] = 0, 100};
                        uint8_t TextBoxFooterRow[] = {101, [1 ... 18] = 97, 103};
                        memcpy(WindowMap[12], TextBoxHeadRow, sizeof(TextBoxHeadRow));
                        for(int Y = 13; Y < 17; Y++) {
                            memcpy(WindowMap[Y], TextBoxBodyRow, sizeof(TextBoxBodyRow));
                        }
                        memcpy(WindowMap[17], TextBoxFooterRow, sizeof(TextBoxFooterRow));

                        /*GetActiveText*/
                        const char *ObjectText[] = {
                            "Technology is\nincredible!\f"
                            "You can now store\nand recall items\nand POKéMON as\ndata via PC!",
                            
                            "I~ raising\nPOKéMON too!\f"
                            "When they get\nstrong, they can\nprotect me!",

                            "We also carry\nPOKé BALLS for\ncatching POKéMON!"
                        };

                        if(ObjI < ObjCount) {
                            ActiveText = ObjectText[ObjI - 1];
                            Objs[ObjI].Dir = ReverseDir(Objs[0].Dir);
                        } else {
                            ActiveText = "No text found";
                            for(int I = 0; I < QuadMaps[Objs[0].Map].Count; I++) {
                                if(ComparePoints(QuadMaps[Objs[0].Map].Texts[I].Pos, NewPoint)) {
                                    ActiveText = QuadMaps[Objs[0].Map].Texts[I].Data;
                                    break;
                                }
                            }
                        }

                        TextTilePt = (point) {1, 14};
                        TextTick = 0;

                        HasMoveKey = 0;
                    } else if(Prop & QuadPropWater) {
                        if(Objs[0].Tile == AnimPlayer) {
                            Objs[0].Tile = AnimSeal;
                            HasMoveKey = 1;
                        } 
                    } else if(Objs[0].Tile == AnimSeal && !(Prop & QuadPropSolid)) {
                        Objs[0].Tile = AnimPlayer;
                        HasMoveKey = 1;
                    }
                } 

                if(Objs[0].Tile != 0) {
                    if(Prop & QuadPropWater) {
                        Prop &= ~QuadPropSolid;
                    } else {
                        Prop |= QuadPropSolid;
                    }
                } else if(Prop & QuadPropWater) {
                    Prop |= QuadPropSolid;
                }

                if(HasMoveKey) {
                    /*MovePlayer*/
                    IsLeaping = 0;
                    if(Objs[0].Dir == DirDown && Prop & QuadPropEdge) {
                        Objs[0].State = StateMoving;
                        IsLeaping = 1;

                        /*CreateShadowQuad*/
                        ShadowQuad[0] = (sprite) {72, 72, 255, 0};
                        ShadowQuad[1] = (sprite) {80, 72, 255, SprHorzFlag};
                        ShadowQuad[2] = (sprite) {72, 80, 255, SprVertFlag};
                        ShadowQuad[3] = (sprite) {80, 80, 255, SprHorzFlag | SprVertFlag};
                    } else if(Prop & QuadPropSolid) {
                        Objs[0].State = StateStop;
                    } else {
                        Objs[0].State = StateMoving;
                    }
                    Objs[0].Tick = IsLeaping ? 16 : 8;
                } else {
                    Objs[0].State = StateStop;
                }

                if(Objs[0].State == StateMoving) {
                    Objs[0].Pos = StartPos;
                    Objs[0].Map = NewQuadMap;

                    /*UpdateLoadedQuadMaps*/
                    int AddToX = 0;
                    int AddToY = 0;
                    if(NewPoint.X == 4) {
                        AddToX = -1;
                    } else if(NewPoint.X == QuadMaps[Objs[0].Map].Width - 4) {
                        AddToX = 1;
                    } 
                    if(NewPoint.Y == 4) {
                        AddToY = -1;
                    } else if(NewPoint.Y == QuadMaps[Objs[0].Map].Height - 4) {
                        AddToY = 1;
                    }

                    const char *LoadNextQuadMap(int DeltaX, int DeltaY) {
                        const char *Path = NULL;
                        point AddTo = {DeltaX, DeltaY};
                        point CurQuadMapPt = QuadMaps[Objs[0].Map].Loaded;
                        point NewQuadMapPt = AddPoints(CurQuadMapPt, AddTo);
                        if(!ComparePoints(QuadMaps[!Objs[0].Map].Loaded, NewQuadMapPt)) {
                            if(NewQuadMapPt.X >= 0 && NewQuadMapPt.X < WorldWidth &&
                               NewQuadMapPt.Y >= 0 && NewQuadMapPt.Y < WorldHeight) {
                                Path = QuadMapPaths[NewQuadMapPt.Y][NewQuadMapPt.X];
                                if(Path) {
                                    ReadQuadMap(Path, &QuadMaps[!Objs[0].Map]); 
                                    QuadMaps[!Objs[0].Map].Loaded = NewQuadMapPt;
                                }
                            }
                        }
                        return Path;
                    }

                    const char *Path = NULL;
                    if(AddToX) {
                        Path = LoadNextQuadMap(AddToX, 0);
                    } 

                    if(AddToY && !Path) {
                        LoadNextQuadMap(0, AddToY);
                    }

                    /*TranslateQuadRectToTiles*/
                    int TileMinX = 0;
                    int TileMinY = 0;
                    int QuadMinX = 0;
                    int QuadMinY = 0; 
                    int QuadMaxX = 0;
                    int QuadMaxY = 0;

                    switch(Objs[0].Dir) {
                    case DirUp:
                        TileMinX = (ScrollX / 8) & 31;
                        TileMinY = (ScrollY / 8 + 30) & 31;

                        QuadMinX = NewPoint.X - 4;
                        QuadMaxX = NewPoint.X + 6;
                        QuadMinY = NewPoint.Y - 4; 
                        QuadMaxY = NewPoint.Y - 3;
                        break;
                    case DirLeft:
                        TileMinX = (ScrollX / 8 + 30) & 31;
                        TileMinY = (ScrollY / 8) & 31;

                        QuadMinX = NewPoint.X - 4;
                        QuadMaxX = NewPoint.X - 3;
                        QuadMinY = NewPoint.Y - 4; 
                        QuadMaxY = NewPoint.Y + 5;
                        break;
                    case DirDown:
                        TileMinX = (ScrollX / 8) & 31;
                        TileMinY = (ScrollY / 8 + 18) & 31;

                        QuadMinX = NewPoint.X - 4;
                        QuadMaxX = NewPoint.X + 6;
                        QuadMinY = NewPoint.Y + 4;
                        QuadMaxY = NewPoint.Y + (IsLeaping ? 6 : 5);
                        break;
                    case DirRight:
                        TileMinX = (ScrollX / 8 + 20) & 31;
                        TileMinY = (ScrollY / 8) & 31;

                        QuadMinX = NewPoint.X + 5;
                        QuadMaxX = NewPoint.X + 6;
                        QuadMinY = NewPoint.Y - 4;
                        QuadMaxY = NewPoint.Y + 5;
                        break;
                    }
                    PlaceQuadMap(QuadMinX, QuadMinY, QuadMaxX, QuadMaxY, TileMinX, TileMinY);
                }
            }
        }

        int SprI = 0;
        for(int I = 0; I < ObjCount; I++) {
            sprite *SpriteQuad = &Sprites[SprI];
            int TmpQuadX = Objs[I].Pos.X - Objs[0].Pos.X + 72;
            int TmpQuadY = Objs[I].Pos.Y - Objs[0].Pos.Y + 72;

            if(Objs[0].Map != Objs[I].Map) {
                /*
                switch(QuadMapDir) {
                case DirUp:
                    TmpQuadY -= QuadMaps[Objs[0].Map].Height * 16;
                    break;
                case DirDown:
                    TmpQuadY += QuadMaps[Objs[0].Map].Height * 16;
                    break;
                }
                */
            }

            if(TmpQuadX > -16 && TmpQuadX < 176 && TmpQuadY > -16 && TmpQuadY < BmWidth) {
                uint8_t QuadX = TmpQuadX;
                uint8_t QuadY = TmpQuadY;
                if(Objs[I].Tick % 8 < 4) {
                    /*SetStillAnimation*/
                    switch(Objs[I].Dir) {
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
                } else {
                    /*ChangeFootAnimation*/
                    switch(Objs[I].Dir) {
                    case DirUp:
                        SpriteQuad[0] = (sprite) {0, 1, 0, 0};
                        SpriteQuad[1] = (sprite) {8, 1, 0, SprHorzFlag};
                        if(Objs[I].IsRight) {
                            SpriteQuad[2] = (sprite) {0, 8, 2, 0};
                            SpriteQuad[3] = (sprite) {8, 8, 3, 0};
                        } else {
                            SpriteQuad[2] = (sprite) {0, 8, 3, SprHorzFlag};
                            SpriteQuad[3] = (sprite) {8, 8, 2, SprHorzFlag};
                        }
                        break;
                    case DirLeft:
                        SpriteQuad[0] = (sprite) {0, 1, 4, 0};
                        SpriteQuad[1] = (sprite) {8, 1, 5, 0};
                        SpriteQuad[2] = (sprite) {0, 8, 8, 0};
                        SpriteQuad[3] = (sprite) {8, 8, 9, 0};
                        break;
                    case DirDown:
                        SpriteQuad[0] = (sprite) {0, 1, 10, 0};
                        SpriteQuad[1] = (sprite) {8, 1, 10, SprHorzFlag};
                        if(Objs[I].IsRight) {
                            SpriteQuad[2] = (sprite) {0, 8, 13, SprHorzFlag};
                            SpriteQuad[3] = (sprite) {8, 8, 12, SprHorzFlag};
                        } else {
                            SpriteQuad[2] = (sprite) {0, 8, 12, 0};
                            SpriteQuad[3] = (sprite) {8, 8, 13, 0};
                        }
                        break;
                    case DirRight:
                        SpriteQuad[0] = (sprite) {0, 1, 5, SprHorzFlag};
                        SpriteQuad[1] = (sprite) {8, 1, 4, SprHorzFlag};
                        SpriteQuad[2] = (sprite) {0, 8, 9, SprHorzFlag};
                        SpriteQuad[3] = (sprite) {8, 8, 8, SprHorzFlag};
                        break;
                    }
                    if(Objs[I].Tick % 8 == 4) {
                        Objs[I].IsRight ^= 1;
                    }
                }
                for(int InI = 0; InI < 4; InI++) {
                    SpriteQuad[InI].X += QuadX;
                    SpriteQuad[InI].Y += QuadY;
                    SpriteQuad[InI].Tile += Objs[I].Tile;
                }

                /*TickFrame*/
                if(!IsPaused) {
                    if(Objs[I].Tick > 0) {
                        if(Objs[I].State == StateMoving) {
                            point DeltaPoint = g_DirPoints[Objs[I].Dir];
                            DeltaPoint.X *= Objs[I].Speed;
                            DeltaPoint.Y *= Objs[I].Speed;
                            Objs[I].Pos = AddPoints(Objs[I].Pos, DeltaPoint);
                            if(I == 0) {
                                switch(Objs[0].Dir) {
                                case DirLeft:
                                    ScrollX -= Objs[I].Speed;
                                    break;
                                case DirRight:
                                    ScrollX += Objs[I].Speed;
                                    break;
                                case DirDown:
                                    ScrollY += Objs[I].Speed;
                                    break;
                                case DirUp:
                                    ScrollY -= Objs[I].Speed;
                                    break;
                                }
                            } 
                        }
                        Objs[I].Tick--;
                    } else if(I != 0) {
                        /*RandomMove*/
                        int Seed = rand();
                        if(Seed % 64 == 0) {
                            Objs[I].Dir = Seed / 64 % 4;

                            point NewPoint = GetFacingPoint(Objs[I].Pos, Objs[I].Dir);

                            /*FetchQuadProp*/
                            int Quad = -1;
                            int Prop = QuadPropSolid;
                            int InHorzBounds = NewPoint.X >= 0 && NewPoint.X < QuadMaps[Objs[I].Map].Width;
                            int InVertBounds = NewPoint.Y >= 0 && NewPoint.Y < QuadMaps[Objs[I].Map].Height;
                            if(InHorzBounds && InVertBounds) {
                                Quad = QuadMaps[Objs[I].Map].Quads[NewPoint.Y][NewPoint.X];
                                Prop = QuadProps[Quad];
                            }

                            point PlayerCurPoint = ConvertToQuadPoint(Objs[0].Pos);

                            if(ComparePoints(PlayerCurPoint, NewPoint)) {
                                Prop = QuadPropSolid;
                            } else if(Objs[0].State != StateStop) {
                                point NextPoint = NextPoints[Objs[0].Dir];
                                point PlayerNextPoint = AddPoints(PlayerCurPoint, NextPoint);
                                if(ComparePoints(PlayerNextPoint, NewPoint)) {
                                    Prop = QuadPropSolid;
                                }
                            }

                            if(!(Prop & QuadPropSolid)) {
                                Objs[I].Tick = 16;
                                Objs[I].State = StateMoving;
                            }
                        }
                    } 
                }
                SprI += 4;
            }
        }

        for(int ClearI = SprI; ClearI < 36; ClearI++) {
            Sprites[ClearI].Y = 240;
        }

        /*PlayerUpdateJumpingAnimation*/
        if(IsLeaping) {
            uint8_t PlayerDeltas[] = {0, 4, 6, 8, 9, 10, 11, 12};
            uint8_t DeltaI = Objs[0].Tick < 8 ? Objs[0].Tick: 15 - Objs[0].Tick;
            PlayerQuad[0].Y -= PlayerDeltas[DeltaI];
            PlayerQuad[1].Y -= PlayerDeltas[DeltaI];
            PlayerQuad[2].Y -= PlayerDeltas[DeltaI];
            PlayerQuad[3].Y -= PlayerDeltas[DeltaI];
        }

        /*MutTileUpdate*/
        if(Tick % 16 == 0) {
            /*FlowersUpdate*/
            uint8_t *FlowerDst = TileData + 3 * TileSize;
            uint8_t *FlowerSrc = FlowerData + TickCycle % 3 * TileSize;
            memcpy(FlowerDst, FlowerSrc, TileSize);

            /*WaterUpdate*/
            int TickMod = TickCycle % 9 < 5 ? TickCycle % 9 : 9 - TickCycle % 9;
            uint8_t *WaterDst = TileData + 20 * TileSize;
            uint8_t *WaterSrc = WaterData + TickMod * TileSize;
            memcpy(WaterDst, WaterSrc, TileSize);
        }

        /*RenderTiles*/
        uint8_t *BmRow = g_BmPixels;

        int MaxPixelY = WindowY < BmHeight ? WindowY : BmHeight; 

        for(int PixelY = 0; PixelY < MaxPixelY; PixelY++) {
            int PixelX = 8 - (ScrollX & 7);
            int SrcYDsp = ((PixelY + ScrollY) & 7) << 3;
            int TileX = ScrollX >> 3;
            int TileY = (PixelY + ScrollY) / 8 % 32;
            memcpy(BmRow, TileData + (SrcYDsp | (ScrollX & 7)) + TileMap[TileY][TileX] * TileSize, 8);

            for(int Repeat = 1; Repeat < 20; Repeat++) {
                TileX = (TileX + 1) % 32;
                uint8_t *Pixel = BmRow + PixelX;
                memcpy(Pixel, TileData + SrcYDsp + TileMap[TileY][TileX] * TileSize, 8);
                PixelX += 8;
            }
            TileX = (TileX + 1) % 32;
            uint8_t *Pixel = BmRow + PixelX;
            memcpy(Pixel, TileData + SrcYDsp + TileMap[TileY][TileX] * TileSize, BmWidth - PixelX);
            BmRow += BmWidth;
        }

        for(int PixelY = MaxPixelY; PixelY < BmHeight; PixelY++) {
            int PixelX = 0;
            int SrcYDsp = (PixelY & 7) << 3;
            int TileX = 0;
            int TileY = PixelY / 8;

            for(int Repeat = 0; Repeat < 20; Repeat++) {
                memcpy(BmRow + PixelX, TileData + SrcYDsp + WindowMap[TileY][TileX] * TileSize, 8);
                TileX++;
                PixelX += 8;
            }
            BmRow += BmWidth;
        }

        /*RenderSprites*/
        int MaxX = BmWidth;
        int MaxY = WindowY < BmHeight ? WindowY : BmHeight;

        for(size_t I = _countof(Sprites); I-- > 0; ) {
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
            uint8_t *Dest = g_BmPixels + DestY * BmWidth + DestX;

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
                Dest += BmWidth;
            }
        }

        /*NextFrame*/
        UpdateClientSize(Window, g_IsFullscreen);
        SleepTillNextFrame(IsGranular, PerfFreq, BeginCounter, FrameDelta); 
        InvalidateRect(Window, NULL, FALSE);
        UpdateWindow(Window);

        Tick++;
    }

    /*CleanUp*/
    if(IsGranular) {
        TimeEndPeriod(1);
    }
    return 0;
}


#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>

#define UNUSED __attribute__((unused))

#define SprHorzFlag 1
#define SprVertFlag 2

#define DirInBounds -2
#define DirOutOfBounds -1

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

typedef struct quad_map {
    int Width;
    int Height;
    uint8_t Quads[256][256];
} quad_map;

typedef struct sprite {
    uint8_t X;
    uint8_t Y;
    uint8_t Tile;
    uint8_t Flags;
} sprite;

/*Globals*/
int g_IsFullscreen = 0;
int g_HasQuit = 0;
RECT g_RestoreRect = {};
uint8_t g_Keys[256] = {};

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

/*Point Functions*/ 
static point AddPoints(point A, point B)  {
    point C = {
        .X = A.X + B.X,
        .Y = A.Y + B.Y
    };
    return C;
}

static int ComparePoints(point A, point B) {
    return A.X == B.X && A.Y == B.Y;
}

/*Inverse Dir*/
static inline int ReverseDir(int Dir) {
    return (Dir + 2) % 4;
}


/*Conversion Functions*/
static point ConvertToQuadPoint(point Point) {
    point QuadPoint = {
        .X = Point.X / 16,
        .Y = Point.Y / 16
    };
    return QuadPoint;
}

static void SetToTiles(uint8_t TileMap[32][32], int TileX, int TileY, const uint8_t Set[4]) {
    TileMap[TileY][TileX] = Set[0];
    TileMap[TileY][TileX + 1] = Set[1];
    TileMap[TileY + 1][TileX] = Set[2];
    TileMap[TileY + 1][TileX + 1] = Set[3];
}

typedef struct quad_info {
    point Point;
    int Map;
    int Dir; 
    int Quad;
} quad_info;

static quad_info GetQuad(quad_map QuadMaps[2], quad_info QuadInfo) {
    switch(QuadInfo.Dir) {
    case DirUp:
        if(QuadInfo.Point.Y < 0) {
            QuadInfo.Map ^= 1;
            QuadInfo.Point.X += (QuadMaps[QuadInfo.Map].Width - QuadMaps[!QuadInfo.Map].Width) / 2;
            QuadInfo.Point.Y += QuadMaps[QuadInfo.Map].Height; 
            QuadInfo.Dir = ReverseDir(QuadInfo.Dir);
        }
        break;
    case DirLeft:
        if(QuadInfo.Point.X < 0) {
            QuadInfo.Map ^= 1;
            QuadInfo.Point.X += QuadMaps[QuadInfo.Map].Height; 
            QuadInfo.Point.Y += (QuadMaps[QuadInfo.Map].Height- QuadMaps[!QuadInfo.Map].Height) / 2;
            QuadInfo.Dir = ReverseDir(QuadInfo.Dir);
        }
        break;
    case DirDown:
        if(QuadInfo.Point.Y >= QuadMaps[QuadInfo.Map].Height) {
            QuadInfo.Point.Y -= QuadMaps[QuadInfo.Map].Height; 
            QuadInfo.Map ^= 1;
            QuadInfo.Point.X += (QuadMaps[QuadInfo.Map].Width - QuadMaps[!QuadInfo.Map].Width) / 2;
            QuadInfo.Dir = ReverseDir(QuadInfo.Dir);
        }
        break;
    case DirRight:
        if(QuadInfo.Point.X >= QuadMaps[QuadInfo.Map].Width) {
            QuadInfo.Point.X -= QuadMaps[QuadInfo.Map].Width; 
            QuadInfo.Map ^= 1;
            QuadInfo.Point.Y += (QuadMaps[QuadInfo.Map].Height- QuadMaps[!QuadInfo.Map].Height) / 2;
            QuadInfo.Dir = ReverseDir(QuadInfo.Dir);
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

    QuadMap->Width = RunData[0] + 1;
    QuadMap->Height = RunData[1] + 1;
    int Size = QuadMap->Width * QuadMap->Height;
    int QuadIndex = 0;
    int EncodeSuccess = 1;
    uint32_t RunIndex = 2;
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
            EncodeSuccess = 0;
        }

        for(int I = QuadIndex; I < QuadIndex + Repeat; I++) {
            QuadMap->Quads[I / QuadMap->Width][I % QuadMap->Width] = Quad; 
        }

        QuadIndex += Repeat;
    }

    int Success = EncodeSuccess && QuadIndex == Size;
    return Success;
}

/*Win32 Functions*/
LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    switch(Message) {
    case WM_KILLFOCUS:
        memset(g_Keys, 0, sizeof(g_Keys));
        return 0;
    case WM_CLOSE:
        g_HasQuit = 1;
        return 0;
    case WM_KEYDOWN:
        switch(WParam) {
        case VK_F11:
            /*ToggleFullscreen*/
            ShowCursor(g_IsFullscreen);
            if(g_IsFullscreen) {
                SetWindowLongPtr(Window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
                int RestoreX = g_RestoreRect.left;
                int RestoreY = g_RestoreRect.top;
                int RestoreWidth = g_RestoreRect.right - g_RestoreRect.left;
                int RestoreHeight = g_RestoreRect.bottom - g_RestoreRect.top;
                SetWindowPos(Window, HWND_TOP, RestoreX, RestoreY, RestoreWidth,
                        RestoreHeight, SWP_FRAMECHANGED | SWP_NOREPOSITION);
            } else {
                GetWindowRect(Window, &g_RestoreRect);
            }
            g_IsFullscreen ^= 1;
            break;
        default:
            if(LParam >> 30 == 0) {
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
    uint8_t BmPixels[160 * 144];

    BITMAPINFO *BmInfo = alloca(sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 4);
    BmInfo->bmiHeader = (BITMAPINFOHEADER) {
        .biSize = sizeof(BmInfo->bmiHeader),
        .biWidth = 160,
        .biHeight = -144,
        .biPlanes = 1,
        .biBitCount = 8,
        .biCompression = BI_RGB,
        .biClrUsed = 4
    };
    BmInfo->bmiColors[0] = (RGBQUAD) {0xF8, 0xF8, 0xF8, 0x00};
    BmInfo->bmiColors[1] = (RGBQUAD) {0xA8, 0xA8, 0xA8, 0x00};
    BmInfo->bmiColors[2] = (RGBQUAD) {0x80, 0x80, 0x80, 0x00};
    BmInfo->bmiColors[3] = (RGBQUAD) {0x00, 0x00, 0x00, 0x00};

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

    RECT WinWindowRect = {0, 0, 160, 144};
    AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, 0);

    int WindowWidth = WinWindowRect.right - WinWindowRect.left;
    int WindowHeight = WinWindowRect.bottom - WinWindowRect.top;

    HWND Window = CreateWindow(WindowClass.lpszClassName, "PokeGame", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight, 
                               NULL, NULL, Instance, NULL);
    if(!Window) {
        return 1;
    }

    SetCurrentDirectory("../Shared");

    /*LoadQuadData*/
    uint8_t QuadData[64][4] = {};
    int SetsRead = ReadAll("QuadData", QuadData, sizeof(QuadData)); 
    for(int Set = 0; Set < SetsRead; Set++) {
        for(int I = 0; I < 4; I++) {
            QuadData[Set][I] = Clamp(QuadData[Set][I], 0, 95);
        }
    }
    
    /*ReadQuadProps*/
    uint8_t QuadProps[64] = {};
    ReadAll("QuadProps", QuadProps, sizeof(QuadProps));

    /*LoadTileData*/
    uint8_t TileData[256 * TileSize];
    uint8_t SpriteData[256 * TileSize];
    uint8_t FlowerData[3 * TileSize];
    uint8_t WaterData[7 * TileSize];

    ReadTileData("TileData", TileData, 110);
    ReadTileData("Numbers", TileData + 110 * TileSize, 256);
    ReadTileData("SpriteData", SpriteData, 256);
    ReadTileData("FlowerData", FlowerData, 3);
    ReadTileData("WaterData", WaterData, 7);
    ReadTileData("ShadowData", SpriteData + 255 * TileSize, 1);

    uint8_t TileMap[32][32];
    static quad_map QuadMaps[2] = {};

    #define WorldWidth 2
    #define WorldHeight 4 

    const char *QuadMapPaths[WorldHeight][WorldWidth] = {
        {"ViridianCity", "ViridianCity"},
        {"Route1"},
        {"PalleteTown"},
        {"Route21"}
    };

    point LoadedQuadMaps[] = {{0, 2}, {-1, 0}};
    int QuadMapDir = -1;
    ReadQuadMap("PalleteTown", &QuadMaps[0]);

    sprite Sprites[40] = {};

    int IsLeaping = 0;
    int IsPaused = 0;

    struct object {
        point Pos;

        uint8_t Dir;
        uint8_t State;
        uint8_t Speed;
        uint8_t Tile;

        uint8_t Map;
        uint8_t IsRight;
        uint8_t Tick;
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
        /*

        {
            .Pos = {80, 384},
            .Dir = DirDown,
            .Speed = 1,
            .Tile = 98,
            .Map = 0
        }
        */
    };
    
    int ObjCount = 3;

    /*TranslateFullQuadMapToTiles*/
    void PlaceQuadMap(int QuadMinX, int QuadMinY, 
                      int QuadMaxX, int QuadMaxY,
                      int TileMinX, int TileMinY) {
        uint8_t DefaultQuads[WorldHeight][WorldWidth] = {
            {14},
            {14},
            {14},
            {39}
        }; 
        
        int TileY = TileMinY;
        for(int QuadY = QuadMinY; QuadY < QuadMaxY; QuadY++) {
            int TileX = TileMinX;
            for(int QuadX = QuadMinX; QuadX < QuadMaxX; QuadX++) {
                quad_info QuadInfo = {
                    .Point = (point) {QuadX, QuadY},
                    .Map = Objs[0].Map,
                    .Dir = QuadMapDir,
                    .Quad = -1
                };
                int Quad = GetQuad(QuadMaps, QuadInfo).Quad;
                if(Quad < 0) {
                    point QuadPoint = LoadedQuadMaps[QuadInfo.Map];
                    Quad = DefaultQuads[QuadPoint.Y][QuadPoint.X];
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

    int TileY = 0;
    for(int QuadY = QuadMinY; QuadY < QuadMaxY; QuadY++) {
        int TileX = 0;
        for(int QuadX = QuadMinX; QuadX < QuadMaxX; QuadX++) {
            uint8_t *Set = QuadData[QuadMaps[Objs[0].Map].Quads[QuadY][QuadX]];
            TileMap[TileY][TileX] = Set[0];
            TileMap[TileY][TileX + 1] = Set[1];
            TileMap[TileY + 1][TileX] = Set[2];
            TileMap[TileY + 1][TileX + 1] = Set[3];
            TileX += 2;
        }
        TileY += 2;
    }

    /*WindowMap*/
    uint8_t WindowMap[32][32];
    UNUSED uint8_t WindowX = 0;
    uint8_t WindowY = 160;

    /*Other*/
    uint64_t Tick = 0;

    const char *ActiveText = "Text not init";
    point TextTilePt = {1, 14};
    uint64_t TextTick = 0;

    uint8_t ScrollX = 0;
    uint8_t ScrollY = 0;

    /*Timing*/
    LARGE_INTEGER PerfFreq;
    QueryPerformanceFrequency(&PerfFreq);

    LARGE_INTEGER BeginCounter;
    int64_t FrameDelta = PerfFreq.QuadPart / 10;

    /*LoadWinmm*/
    int IsGranular = 0;
    MMRESULT WINAPI(*TimeEndPeriod)(UINT) = NULL;
    HMODULE WinmmLib = LoadLibrary("winmm.dll");
    if(WinmmLib) {
        MMRESULT WINAPI(*TimeBeginPeriod)(UINT);
        TimeBeginPeriod = (void *) GetProcAddress(WinmmLib, "timeBeginPeriod"); 
        if(TimeBeginPeriod) {
            TimeEndPeriod = (void *) GetProcAddress(WinmmLib, "timeEndPeriod"); 
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
        QueryPerformanceCounter(&BeginCounter);

        int TickCycle = Tick / 16;

        /*ProcessMessage*/
        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        /*InitSharedUpdate*/
        point DirPoints[] = {
            [DirUp] = {0, -1},
            [DirLeft] = {-1, 0},
            [DirDown] = {0, 1},
            [DirRight] = {1, 0}
        };

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
                        {
                            int Output = ActiveText[0];
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
                            } else if(Output == '-') {
                                Output = 177;
                            } else if(Output == ',') {
                                Output = 180; 
                            } else {
                                Output = 0;
                            }

                            WindowMap[TextTilePt.Y][TextTilePt.X] = Output;
                            TextTilePt.X++;
                        }
                    }
                    ActiveText++;
                }
            } else if(g_Keys['X']) {
                IsPaused = 0;
                WindowY = 160;
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

                /*GetFacingPoint*/
                point OldPoint = ConvertToQuadPoint(Objs[0].Pos);
                point DirPoint = DirPoints[Objs[0].Dir];
                point NewPoint = AddPoints(OldPoint, DirPoint);

                /*FetchQuadProp*/
                quad_info QuadInfo = {
                    .Point = NewPoint,
                    .Map = Objs[0].Map,
                    .Dir = QuadMapDir,
                    .Quad = -1,
                };
                QuadInfo = GetQuad(QuadMaps, QuadInfo);

                NewPoint = QuadInfo.Point;
                int Quad = QuadInfo.Quad;
                int NewQuadMap = QuadInfo.Map;
                int NewQuadMapDir = QuadInfo.Dir; 

                point StartPos = AddPoints(NewPoint, DirPoints[ReverseDir(Objs[0].Dir)]); 
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
                        typedef struct text_ref {
                            point Pos;
                            const char *Text;
                        } text_ref;


                        const char *ObjectText[] = {
                            "Technology is\nincredible!\f"
                            "You can now store\nand recall items\nand POKéMON as\ndata via PC!",
                            
                            "I~ raising\nPOKéMON too!\f"
                            "When they get\nstrong, they can\nprotect me!",

                            "We also carry\nPOKé BALLS for\ncatching POKéMON!"
                        };

                        text_ref PalleteTextRefs[] = {
                            {{3, 5}, "RED' house"},
                            {{11, 5}, "BLUE' house"},
                            {{7, 9}, "PALLETE TOWN\nShades of your\njourney awaits!"},
                            {{13, 13}, "OAK POKéMON\nRESEARCH LAB"}
                        };

                        text_ref Route1Refs[] = {
                            {{9, 27}, "ROUTE 1\nPALLETE TOWN -\nVIRIDIAN CITY"}
                        };

                        typedef struct text_ref_array {
                            text_ref *Refs;
                            int Count;
                        } text_ref_array;

                        text_ref_array TextRefs[WorldHeight][WorldWidth] = {
                            {},
                            {
                                {Route1Refs, _countof(Route1Refs)}
                            },
                            { 
                                {PalleteTextRefs, _countof(PalleteTextRefs)},
                            }
                        };

                        point ActiveRefPoint = LoadedQuadMaps[NewQuadMap];
                        text_ref_array ActiveRefs = TextRefs[ActiveRefPoint.Y][ActiveRefPoint.X];

                        if(ObjI < ObjCount) {
                            ActiveText = ObjectText[ObjI - 1];
                            Objs[ObjI].Dir = ReverseDir(Objs[0].Dir);
                        } else {
                            ActiveText = "No text found";
                            for(int I = 0; I < ActiveRefs.Count; I++) {
                                if(ComparePoints(ActiveRefs.Refs[I].Pos, NewPoint)) {
                                    ActiveText = ActiveRefs.Refs[I].Text;
                                    break;
                                }
                            }
                        }

                        TextTilePt = (point) {1, 14};
                        TextTick = 0;

                        HasMoveKey = 0;
                    } else if(Prop & QuadPropWater) {
                        if(Objs[0].Tile == 0) {
                            Objs[0].Tile = 84;
                            HasMoveKey = 1;
                        } 
                    } else if(Objs[0].Tile == 84 && !(Prop & QuadPropSolid)) {
                        Objs[0].Tile = 0;
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
                    QuadMapDir = NewQuadMapDir;

                    /*UpdateLoadedQuadMaps*/
                    point AddTo = {};
                    int NewQuadMapDir = -1;
                    if(NewPoint.X == 4) {
                        AddTo.X = -1;
                        NewQuadMapDir = DirLeft;
                    } else if(NewPoint.X == QuadMaps[Objs[0].Map].Width - 4) {
                        AddTo.X = 1;
                        NewQuadMapDir = DirRight;
                    } else if(NewPoint.Y == 4) {
                        AddTo.Y = -1;
                        NewQuadMapDir = DirUp;
                    } else if(NewPoint.Y == QuadMaps[Objs[0].Map].Height - 4) {
                        AddTo.Y = 1;
                        NewQuadMapDir = DirDown;
                    }


                    if(AddTo.X || AddTo.Y) {
                        point CurQuadMapPt = LoadedQuadMaps[Objs[0].Map];
                        point NewQuadMapPt = AddPoints(CurQuadMapPt, AddTo);
                        if(!ComparePoints(LoadedQuadMaps[!Objs[0].Map], NewQuadMapPt)) {
                            if(NewQuadMapPt.X >= 0 && NewQuadMapPt.X < WorldWidth &&
                               NewQuadMapPt.Y >= 0 && NewQuadMapPt.Y < WorldHeight) {
                                const char *Path = QuadMapPaths[NewQuadMapPt.Y][NewQuadMapPt.X];
                                if(Path) {
                                    ReadQuadMap(Path, &QuadMaps[!Objs[0].Map]); 
                                    LoadedQuadMaps[!Objs[0].Map] = NewQuadMapPt;
                                    QuadMapDir = NewQuadMapDir;
                                }
                            }
                        }
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
                switch(QuadMapDir) {
                case DirUp:
                    TmpQuadY -= QuadMaps[Objs[0].Map].Height * 16;
                    break;
                case DirDown:
                    TmpQuadY += QuadMaps[Objs[0].Map].Height * 16;
                    break;
                }
            }

            if(TmpQuadX > -16 && TmpQuadX < 176 && TmpQuadY > -16 && TmpQuadY < 160) {
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
                            point DeltaPoint = DirPoints[Objs[I].Dir];
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

                            /*GetFacingPoint*/
                            point OldPoint = ConvertToQuadPoint(Objs[I].Pos);
                            point DirPoint = DirPoints[Objs[I].Dir];
                            point NewPoint = AddPoints(OldPoint, DirPoint);

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
        uint8_t *BmRow = BmPixels;

        int MaxPixelY = WindowY < 144 ? WindowY : 144; 

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
            memcpy(Pixel, TileData + SrcYDsp + TileMap[TileY][TileX] * TileSize, 160 - PixelX);
            BmRow += 160;
        }

        for(int PixelY = MaxPixelY; PixelY < 144; PixelY++) {
            int PixelX = 0;
            int SrcYDsp = (PixelY & 7) << 3;
            int TileX = 0;
            int TileY = PixelY / 8;

            for(int Repeat = 0; Repeat < 20; Repeat++) {
                memcpy(BmRow + PixelX, TileData + SrcYDsp + WindowMap[TileY][TileX] * TileSize, 8);
                TileX++;
                PixelX += 8;
            }
            BmRow += 160;
        }

        /*RenderSprites*/
        int MaxX = 160;
        int MaxY = IsPaused ? 96 : 144;

        for(int I = _countof(Sprites); I-- > 0; ) {
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
            uint8_t *Dest = BmPixels + DestY * 160 + DestX;

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
                Dest += 160;
            }
        }

        /*UpdateFullscreen*/
        if(g_IsFullscreen) {
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            HMONITOR Monitor = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
            MONITORINFO MonitorInfo = {
                .cbSize = sizeof(MonitorInfo)
            };
            GetMonitorInfo(Monitor, &MonitorInfo);

            int MonitorX = MonitorInfo.rcMonitor.left;
            int MonitorY = MonitorInfo.rcMonitor.top;
            int MonitorWidth = MonitorInfo.rcMonitor.right - MonitorX;
            int MonitorHeight = MonitorInfo.rcMonitor.bottom - MonitorY;

            if(MonitorWidth != ClientRect.right || MonitorHeight != ClientRect.bottom) {
                SetWindowLongPtr(Window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                SetWindowPos(Window, HWND_TOP, MonitorX, MonitorY,
                             MonitorWidth, MonitorHeight, SWP_FRAMECHANGED | SWP_NOREPOSITION);
            }
        }

        /*SleepTillNextFrame*/
        if(g_Keys['E']) {
            FrameDelta = PerfFreq.QuadPart / 300;
        } else {
            FrameDelta = PerfFreq.QuadPart / 30;
        }
        LARGE_INTEGER PerfCounter;
        QueryPerformanceCounter(&PerfCounter);
        int64_t DeltaCounter = PerfCounter.QuadPart - BeginCounter.QuadPart;
        if(DeltaCounter < FrameDelta) {
            if(IsGranular) {
                int64_t RemainCounter = FrameDelta - DeltaCounter;
                uint32_t SleepMS = 1000 * RemainCounter / PerfFreq.QuadPart;
                if(SleepMS) {
                    Sleep(SleepMS);
                }
            }
            do {
                QueryPerformanceCounter(&PerfCounter);
                DeltaCounter = PerfCounter.QuadPart - BeginCounter.QuadPart;
            } while(DeltaCounter < FrameDelta);
        }

        /*RenderFrame*/
        HDC DeviceContext = GetDC(Window);
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        int ClientWidth = ClientRect.right;
        int ClientHeight = ClientRect.bottom;

        int RenderWidth = ClientWidth - ClientWidth % 160;
        int RenderHeight = ClientHeight - ClientHeight % 144;

        int RenderColSize = RenderWidth * 144;
        int RenderRowSize = RenderHeight * 160;
        if(RenderColSize > RenderRowSize) {
            RenderWidth = RenderRowSize / 144;
        } else {
            RenderHeight = RenderColSize / 160;
        }
        int RenderX = (ClientWidth - RenderWidth) / 2;
        int RenderY = (ClientHeight - RenderHeight) / 2;

        StretchDIBits(DeviceContext, RenderX, RenderY, RenderWidth, RenderHeight,
                      0, 0, 160, 144, BmPixels, BmInfo, DIB_RGB_COLORS, SRCCOPY);
        PatBlt(DeviceContext, 0, 0, RenderX, ClientHeight, BLACKNESS);
        PatBlt(DeviceContext, RenderX + RenderWidth, 0, RenderX, ClientHeight, BLACKNESS);
        PatBlt(DeviceContext, RenderX, 0, RenderWidth, RenderY, BLACKNESS);
        PatBlt(DeviceContext, RenderX, RenderY + RenderHeight, RenderWidth, RenderY + 1, BLACKNESS);
        ReleaseDC(Window, DeviceContext);

        Tick++;
    }

    /*CleanUp*/
    if(IsGranular) {
        TimeEndPeriod(1);
    }
    return 0;
}


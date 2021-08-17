#include <windows.h>
#include <stdint.h>

/*Macros*/
#define BITMAP_WIDTH 128 
#define BITMAP_HEIGHT 128 

/*Structures*/
typedef struct point {
    int X;
    int Y;
} point;

typedef struct rect {
    int X;
    int Y;
    int Width;
    int Height;
} rect;

/*Globals*/
static uint32_t g_TileData[256][8][8];
static uint32_t g_TileCount = 0;
static uint32_t g_BitmapPixels[BITMAP_HEIGHT][BITMAP_WIDTH];
static int g_TileSelected = 0;
static int g_PixelSelectedX = 0;
static int g_PixelSelectedY = 0;
static int g_ClientWidth = BITMAP_WIDTH;
static int g_ClientHeight = BITMAP_HEIGHT;
static uint32_t g_Colors[] = {0xF8F8F8, 0xA8A8A8, 0x808080, 0x000000};

const static BITMAPINFO g_BitmapInfo = {
    .bmiHeader = {
        .biSize = sizeof(BITMAPINFOHEADER),
        .biWidth = BITMAP_WIDTH,
        .biHeight = -BITMAP_HEIGHT,
        .biPlanes = 1,
        .biBitCount = 32,
        .biCompression = BI_RGB,
    }
}; 

/*Rect Functions*/
static rect GetMyRect(RECT WinRect) {
    rect Rect = {
        .X = WinRect.left,
        .Y = WinRect.top,
        .Width = WinRect.right - WinRect.left,
        .Height = WinRect.bottom - WinRect.top,
    };
    return Rect;
}

/*Data loaders*/
__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
static uint32_t ReadAll(const char Path[], void *Bytes, uint32_t BytesToRead) {
    DWORD BytesRead = 0;
    HANDLE FileHandle = CreateFile(Path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if(FileHandle != INVALID_HANDLE_VALUE) {
        ReadFile(FileHandle, Bytes, BytesToRead, &BytesRead, NULL);
        CloseHandle(FileHandle);
    }
    return BytesRead;
}

__attribute__((nonnull, access(read_only, 1), access(write_only, 2)))
static int ReadTileData(const char Path[], uint32_t TileData[][8][8], int MaxTileCount) {
    int SizeOfCompressedTile = 16;
    int CompressionRatio = 16;
    int SizeOfCompressedTileData = MaxTileCount * SizeOfCompressedTile;

    uint8_t CompData[SizeOfCompressedTileData];
    int BytesRead = ReadAll(Path, CompData, sizeof(CompData));
    int TilesRead = BytesRead / SizeOfCompressedTile;

    const uint8_t *CompPtr = CompData;
    for(int TileIndex = 0; TileIndex < TilesRead; TileIndex++) {
        for(int Y = 0; Y < 8; Y++) {
            for(int X = 0; X < 8; X += 4) {
                uint8_t Comp = *CompPtr++;
                TileData[TileIndex][Y][X + 0] = g_Colors[(Comp >> 6) & 3];
                TileData[TileIndex][Y][X + 1] = g_Colors[(Comp >> 4) & 3];
                TileData[TileIndex][Y][X + 2] = g_Colors[(Comp >> 2) & 3];
                TileData[TileIndex][Y][X + 3] = g_Colors[(Comp >> 0) & 3];
            }
        }
    }
    return TilesRead;
}

/*DataWriters*/
static uint8_t MatchColor(int Tile, int X, int Y) {
    uint32_t Color = g_TileData[Tile][Y][X];
    for(int I = 0; I < _countof(g_Colors); I++) {
        if(g_Colors[I] == Color) {
            return I;
        }
    }
    exit(1);
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

static void WriteTileData(void) {
    uint8_t OutTileData[4096];
    uint8_t *OutPtr = OutTileData;
    for(int Tile = 0; Tile < g_TileCount; Tile++) {
        for(int Y = 0; Y < 8; Y++) {
            for(int X = 0; X < 8; X += 4) {
                uint8_t CR0 = MatchColor(Tile, X, Y);
                uint8_t CR1 = MatchColor(Tile, X + 1, Y);
                uint8_t CR2 = MatchColor(Tile, X + 2, Y);
                uint8_t CR3 = MatchColor(Tile, X + 3, Y);

                *OutPtr++ = (CR0 << 6) | (CR1 << 4) | (CR2 << 2) | (CR3 << 0); 
            }
        }
    }
    WriteAll("../Shared/TileData2", OutTileData, g_TileCount * 16); 
}

/*Drawer*/
static void DrawTileStretched(int Tile, int TileX, int TileY, int Ratio) {
    int DstX = TileX * 8;
    int DstY = TileY * 8;
    for(int Y = 0; Y < 8 * Ratio; Y++) {
        for(int X = 0; X < 8 * Ratio; X++) {
            g_BitmapPixels[DstY + Y][DstX + X] = g_TileData[Tile][Y / Ratio][X / Ratio]; 
        }
    }
}

static void DrawPixelSelected(int TilePixelX, int TilePixelY) {
    int PixelX = TilePixelX * 16;
    int PixelY = TilePixelY * 16;
    for(int X = 0; X < 16; X++) {
        g_BitmapPixels[PixelY][PixelX + X] = 0x0000FF;
    }

    for(int Y = 1; Y < 15; Y++) {
        g_BitmapPixels[PixelY + Y][PixelX] = 0x0000FF;
        g_BitmapPixels[PixelY + Y][PixelX + 15] = 0x0000FF;
    }

    for(int X = 0; X < 16; X++) {
        g_BitmapPixels[PixelY + 15][PixelX + X] = 0x0000FF;
    }
}

static void DrawSelected(void) {
    DrawTileStretched(g_TileSelected, 0, 0, 16);
    DrawPixelSelected(g_PixelSelectedX, g_PixelSelectedY);
}

/*
static void DrawTile(int Tile, int TileX, int TileY) {
    int DstX = TileX * 8;
    int DstY = TileY * 8;
    for(int Y = 0; Y < 8; Y++) {
        for(int X = 0; X < 8; X++) {
           g_BitmapPixels[DstY + Y][DstX + X] = g_TileData[Tile][Y][X]; 
        }
    }
}

static void DrawTileData(void) {
    int Tile = 0;
    for(int Y = 0; Y < 6; Y++) {
        for(int X = 0; X < 16; X++) {
            DrawTile(Tile, X, Y);
            Tile++;
        }
    }
}
*/

/*WinAPI*/
__attribute__((nonnull))
static LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    switch(Message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        g_ClientWidth = LOWORD(LParam);
        g_ClientHeight = HIWORD(LParam);
        return 0;
    case WM_CLOSE:
        DestroyWindow(Window);
        return 0;
    case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            StretchDIBits(
                DeviceContext, /*DeviceContext*/ 
                0, /*DestY*/
                0, /*DestX*/ 
                g_ClientWidth, /*DestWidth*/
                g_ClientHeight, /*DestHeight*/ 
                0, /*SourceX*/
                0, /*SourceY*/
                BITMAP_WIDTH, /*SourceWidth*/ 
                BITMAP_HEIGHT, /*SourceHeight*/
                g_BitmapPixels, /*Pixels*/ 
                &g_BitmapInfo, /*BitmapInfo*/
                DIB_RGB_COLORS, /*ColorMode*/
                SRCCOPY /*RasterCode*/
            ); 
            EndPaint(Window, &Paint); 
        }
        return 0;
    case WM_KEYDOWN:
        switch(WParam) {
        case '0':
        case '1':
        case '2':
        case '3':
            g_TileData[g_TileSelected][g_PixelSelectedY][g_PixelSelectedX] = g_Colors[WParam - '0'];
            DrawSelected();
            InvalidateRect(Window, NULL, FALSE);
            break;
        case 'W':
            if(g_PixelSelectedY > 0) {
                g_PixelSelectedY--;
                DrawSelected();
                InvalidateRect(Window, NULL, FALSE);
            }
            break;
        case 'A':
            if(g_PixelSelectedX > 0) {
                g_PixelSelectedX--;
                DrawSelected();
                InvalidateRect(Window, NULL, FALSE);
            }
            break;
        case 'S':
            if(g_PixelSelectedY < 7) {
                g_PixelSelectedY++;
                DrawSelected();
                InvalidateRect(Window, NULL, FALSE);
            }
            break;
        case 'D':
            if(g_PixelSelectedX < 7) {
                g_PixelSelectedX++;
                DrawSelected();
                InvalidateRect(Window, NULL, FALSE);
            }
            break;
        case VK_LEFT:
            if(g_TileSelected > 0) {
                g_TileSelected--;
                DrawSelected();
                InvalidateRect(Window, NULL, FALSE);
            }
            break;
        case VK_RIGHT:
            if(g_TileSelected < 255) {
                g_TileSelected++;
                if(g_TileSelected >= g_TileCount) {
                    g_TileCount++;
                }
                DrawSelected();
                InvalidateRect(Window, NULL, FALSE);
            }
            break;
        case VK_DOWN:
            WriteTileData();
            break;
        }
        return 0;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
} 

__attribute__((nonnull))
static HWND CreatePokeWindow(const char *WindowName, int ClientWidth, int ClientHeight) {
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
        DWORD Style = WS_OVERLAPPEDWINDOW; 

        RECT WinWindowRect = {
            .left = 0,
            .top = 0,
            .right = ClientWidth,
            .bottom = ClientHeight
        };
        AdjustWindowRect(&WinWindowRect, Style, FALSE);

        rect WindowRect = GetMyRect(WinWindowRect);

        Window = CreateWindow(
            WindowClass.lpszClassName, /*Class Name*/ 
            WindowName, /*Window Name*/ 
            Style, /*Style*/
            CW_USEDEFAULT, /*X*/ 
            CW_USEDEFAULT, /*Y*/
            WindowRect.Width, /*Width*/ 
            WindowRect.Height, /*Height*/
            NULL, /*Parent*/ 
            NULL, /*Menu*/
            Instance, /*Instance*/
            NULL /*Extra*/
        );
    }
    return Window;
}

__attribute__((nonnull(1, 3)))
int WINAPI WinMain(HINSTANCE Instance, HINSTANCE Prev, LPSTR CmdLine, int CmdShow) {
    g_TileCount = ReadTileData("../Shared/TileData", g_TileData, _countof(g_TileData));
    DrawSelected();

    HWND Window = CreatePokeWindow("PokeWindow", BITMAP_WIDTH, BITMAP_HEIGHT);
    if(!Window) {
        MessageBox(NULL, "Failed to create window", "Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(Window, CmdShow);

    MSG Message;
    while(GetMessage(&Message, NULL, 0, 0)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    return 0;
} 

#include "render.h"
#include "win32.h"

static HWND g_Window;
static BOOL g_IsFullscreen;

static void SetMyWindowPos(HWND Window, DWORD Style, dim_rect Rect) {
    SetWindowLongPtr(Window, GWL_STYLE, Style | WS_VISIBLE);
    SetWindowPos(Window, 
                 HWND_TOP, 
                 Rect.X, Rect.Y, Rect.Width, Rect.Height, 
                 SWP_FRAMECHANGED | SWP_NOREPOSITION);
}

static void PaintFrame(void) {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(g_Window, &Paint);
    dim_rect ClientRect = GetDimClientRect(g_Window);

    int RenderWidth = ClientRect.Width - ClientRect.Width % BM_WIDTH;
    int RenderHeight = ClientRect.Height - ClientRect.Height % BM_HEIGHT;

    int RenderColSize = RenderWidth * BM_HEIGHT;
    int RenderRowSize = RenderHeight * BM_WIDTH;
    if(RenderColSize > RenderRowSize) {
        RenderWidth = RenderRowSize / BM_HEIGHT;
    } else {
        RenderHeight = RenderColSize / BM_WIDTH;
    }
    int RenderX = (ClientRect.Width - RenderWidth) / 2;
    int RenderY = (ClientRect.Height - RenderHeight) / 2;

    StretchDIBits(DeviceContext, 
                  RenderX, RenderY, RenderWidth, RenderHeight,
                  0, 0, BM_WIDTH, BM_HEIGHT, 
                  g_Pixels, &g_Bitmap.WinStruct, 
                  DIB_RGB_COLORS, SRCCOPY);

    PatBlt(DeviceContext, 0, 0, RenderX, ClientRect.Height, BLACKNESS);
    int RenderRight = RenderX + RenderWidth;
    PatBlt(DeviceContext, RenderRight, 0, RenderX, ClientRect.Height, BLACKNESS);
    PatBlt(DeviceContext, RenderX, 0, RenderWidth, RenderY, BLACKNESS);
    int RenderBottom = RenderY + RenderHeight;
    PatBlt(DeviceContext, RenderX, RenderBottom, RenderWidth, RenderY + 1, BLACKNESS);
    EndPaint(g_Window, &Paint);
}

static LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    switch(Message) {
    case WM_DESTROY:
        ExitProcess(0);
        return 0;
    case WM_CLOSE:
        DestroyWindow(Window);
        return 0;
    case WM_PAINT:
        PaintFrame();
        return 0;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

void CreateGameWindow(HINSTANCE Instance) {
    WNDCLASS WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = Instance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = "PokeWindowClass"
    };
    if(!RegisterClass(&WindowClass)) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR); 
        ExitProcess(1);
    }

    RECT WinWindowRect = {0, 0, BM_WIDTH, BM_HEIGHT};
    AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, 0);

    dim_rect WindowRect = WinToDimRect(WinWindowRect);

    g_Window = CreateWindow(
        WindowClass.lpszClassName, 
        "PokeGame", 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        WindowRect.Width, 
        WindowRect.Height, 
        NULL,
        NULL, 
        Instance, 
        NULL
    );
    if(!g_Window) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR); 
        ExitProcess(1);
    }
    ShowWindow(g_Window, SW_SHOW);
}

void ProcessMessages(void) {
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }
}

BOOL ToggleFullscreen(void) {
    static dim_rect RestoreRect;

    ShowCursor(g_IsFullscreen);
    if(g_IsFullscreen) {
        SetMyWindowPos(g_Window, WS_OVERLAPPEDWINDOW, RestoreRect);
    } else {
        RECT WinWindowRect;
        GetWindowRect(g_Window, &WinWindowRect);
        RestoreRect = WinToDimRect(WinWindowRect);
    }
    g_IsFullscreen ^= TRUE;
    return g_IsFullscreen;
}

void UpdateFullscreen(void) {
    if(g_IsFullscreen) {
        dim_rect ClientRect = GetDimClientRect(g_Window);

        HMONITOR Monitor = MonitorFromWindow(g_Window, MONITOR_DEFAULTTONEAREST);
        MONITORINFO MonitorInfo = {
            .cbSize = sizeof(MonitorInfo)
        };
        GetMonitorInfo(Monitor, &MonitorInfo);

        dim_rect MonitorRect = WinToDimRect(MonitorInfo.rcMonitor);

        if(MonitorRect.Width != ClientRect.Width || MonitorRect.Height != ClientRect.Height) {
            SetMyWindowPos(g_Window, WS_POPUP, MonitorRect);
        }
    }
}

BOOL IsInputActive(void) {
    return GetActiveWindow() == g_Window;
} 

void RenderWindow(void) {
    InvalidateRect(g_Window, NULL, 0);
}


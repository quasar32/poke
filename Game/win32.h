#ifndef WIN32_H
#define WIN32_H

#include <windows.h>

typedef struct dim_rect {
    int X;
    int Y;
    int Width;
    int Height;
} dim_rect;

void CreateGameWindow(HINSTANCE Instance);
void ProcessMessages(void);
void RenderWindow(void);
BOOL IsInputActive(void);

BOOL ToggleFullscreen(void);
void UpdateFullscreen(void);

dim_rect WinToDimRect(RECT WinRect);
dim_rect GetDimClientRect(HWND Window);

HMODULE LoadProcs(
    const char *Path, 
    size_t ProcCount, 
    const char *ProcNames[static ProcCount],
    FARPROC Procs[static ProcCount]
);

HMODULE LoadProcsVersioned(
    size_t PathCount,
    const char *Paths[static PathCount], 
    size_t ProcCount, 
    const char *ProcNames[static ProcCount],
    FARPROC Procs[static ProcCount]
);

#endif

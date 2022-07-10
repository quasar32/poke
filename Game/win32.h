#ifndef WIN32_H
#define WIN32_H

#include <windows.h>

void CreateGameWindow(HINSTANCE Instance);
void ProcessMessages(void);
BOOL ToggleFullscreen(void);
void UpdateFullscreen(void);
BOOL IsInputActive(void);
void RenderWindow(void);

#endif

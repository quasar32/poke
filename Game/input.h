#ifndef INPUT_H
#define INPUT_H

#include <windows.h>

typedef enum buttons {
    BT_UP,
    BT_LEFT,
    BT_DOWN,
    BT_RIGHT,
    BT_A,
    BT_B,
    BT_START,
    BT_FULLSCREEN,
} buttons;

#define COUNTOF_BT 8

extern int VirtButtons[COUNTOF_BT];

__attribute__((constructor))
BOOL InitXInput(void);

void UpdateInput(BOOL IsActive);

#endif

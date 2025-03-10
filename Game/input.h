#ifndef INPUT_H
#define INPUT_H

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

void UpdateInput(void);

#endif

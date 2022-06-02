#ifndef WINDOW_MAP_H
#define WINDOW_MAP_H

#include <stdint.h>
#include <stdlib.h>

#include "coord.h"
#include "inventory.h"
#include "window_task.h"

#define AT_MANUAL_RESTORE 0x01 
#define AT_WAIT_FLASH     0x02
#define AT_AUTO_CLEAR     0x04

#define MF_AUTO_RESET 1

typedef enum menu_tile {
    MT_FULL_VERT_ARROW = 171,
    MT_FULL_HORZ_ARROW = 174,
    MT_BLANK = 176,
    MT_EMPTY_HORZ_ARROW = 177,
    MT_TIMES = 178,
    MT_QUESTION = 179
} menu_tile;

typedef enum option_names {
    OPT_TEXT_SPEED,
    OPT_BATTLE_ANIMATION, 
    OPT_BATTLE_STYLE,
    OPT_CANCEL
} option_names;

typedef struct menu {
    window_task WindowTask;
    const char *const Text;
    const rect Rect;
    const int TextY;
    const int EndI;
    const int Flags;
    int SelectI;
} menu;

typedef struct active_text {
    const char *Str;
    const char *Restore;
    uint64_t Tick;
    point TilePt; 
    int BoxDelay;
    int Props;
} active_text;

extern const rect BottomTextRect;

extern menu MainMenu; 
extern menu ContinueMenu;
extern menu StartMenu;
extern menu RedPCMenu;
extern menu YesNoMenu;
extern menu UseTossMenu;
extern menu ConfirmTossMenu;

extern uint8_t WindowMap[18][20];

extern active_text ActiveText;

extern window_task *DeferedTask;
extern const char *DeferedMessage;

int CharToTile(int Char);
void PlaceTextBox(rect Rect);
void PlaceText(point TileMin, const char *Text);
void PlaceTextF(point TileMin, const char *Format, ...);
void PlaceStaticText(rect Rect, const char *Text);

void PlaceInventory(const inventory *Inventory);

void PlaceSave(rect Rect);

void PlaceMenuCursor(const menu *Menu, int MenuTile);
void PlaceMenu(const menu *Menu);
void MoveMenuCursor(menu *Menu);
void MoveMenuCursorWrap(menu *Menu);

void ClearWindow(void);
void ClearWindowRect(rect Rect);
void ClearBottomWindow(void);

void FlashTextCursor(active_text *ActiveText);

#endif

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
#define AT_NIDORINA       0x08

#define MF_AUTO_RESET 1

typedef enum menu_tile {
    MT_EMPTY = 0,
    MT_BLANK = 1,
    MT_TOP_LEFT = 2,
    MT_MIDDLE = 3,
    MT_TOP_RIGHT = 4,
    MT_CENTER_LEFT = 5,
    MT_BOTTOM_LEFT = 6,
    MT_CENTER_RIGHT = 7,
    MT_BOTTOM_RIGHT = 8,
    MT_ZERO = 9,
    MT_NINE = 18,
    MT_COLON = 19,
    MT_CAPITAL_A = 20,
    MT_CAPITAL_U = 40,
    MT_CAPITAL_Z = 45,
    MT_LOWERCASE_A = 46,
    MT_LOWERCASE_L = 57,
    MT_LOWERCASE_Z = 71,
    MT_ACCENTED_E = 72,
    MT_EXCLAMATION_POINT = 74,
    MT_QUOTE_S = 75,
    MT_DASH = 76,
    MT_FULL_VERT_ARROW = 77,
    MT_QUOTE_M = 78,
    MT_COMMA = 79,
    MT_FULL_HORZ_ARROW = 80,
    MT_PERIOD = 81,
    MT_EMPTY_HORZ_ARROW = 82,
    MT_SEMI_COLON = 83,
    MT_LEFT_BRACKET = 84,
    MT_RIGHT_BRACKET = 85,
    MT_LEFT_PARENTHESIS = 86,
    MT_RIGHT_PARENTHESIS = 87,
    MT_MALE_SYMBOL = 88,
    MT_FEMALE_SYMBOL = 89,
    MT_SLASH = 90,
    MT_PK = 91,
    MT_MN = 92,
    MT_MIDDLE_SCORE = 93,
    MT_UPPER_SCORE = 94,
    MT_END = 95,
    MT_TIMES = 96,
    MT_QUESTION = 97,
    MT_TRAINER = 98,
    MT_TRANSITION = 147,
    MT_MAP = 171,
    MT_MAP_BRACKET = 183,
} menu_tile;

typedef struct menu {
    window_task WindowTask;
    const char *const Text;
    const rect Rect;
    const int TextY;
    const int EndI;
    const int Flags;
    int SelectI;
} menu;

extern const rect BottomTextRect;

extern menu MainMenu; 
extern menu ContinueMenu;
extern menu StartMenu;
extern menu RedPCMenu;
extern menu YesNoMenu;
extern menu UseTossMenu;
extern menu ConfirmTossMenu;

extern window_task *DeferedTask;
extern const char *DeferedMessage;

int CharToTile(int Ch);
int TileToChar(int Tile);

void PlaceBox(rect Rect);
void PlaceText(int TileX, int TileY, const char *Text);
void PlaceTextF(int Tile, int TileY, const char *Format, ...);

void PlaceInventory(const inventory *Inventory);

void PlaceSave(rect Rect);

void PlaceMenuCursor(const menu *Menu, int MenuTile);
void PlaceMenu(const menu *Menu);
void MoveMenuCursor(menu *Menu);
void MoveMenuCursorWrap(menu *Menu);
int GetMenuOptionSelected(menu *Menu, int CancelI);

void BlankWindow(void);
void ClearWindow(void);
void ClearWindowRect(rect Rect);
void ClearBottomWindow(void);

void DisplayTextBox(const char *Str, int Delay);
void PlaceTextBox(const char *Str, int Delay);
int FlashCursor(int Tick);
void WaitOnFlash(void);

#endif

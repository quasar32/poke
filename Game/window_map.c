#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "menu.h"
#include "window_map.h"
#include "scalar.h"
#include "input.h"

const rect BottomTextRect = {0, 12, 20, 18};

save_rect ContinueSaveRect = {
    .WindowTask.Type = TT_SAVE, 
    .Rect = {4, 7, 20, 17}
};

save_rect StartSaveRect = {
    .WindowTask.Type = TT_SAVE, 
    .Rect = {4, 0, 20, 10} 
};

uint8_t WindowMap[18][20];

options_menu Options = {
    .WindowTask.Type = TT_OPTIONS,
    .E = {
        [OPT_TEXT_SPEED] = {
            .Y = 3,
            .Xs = {1, 7, 14},
            .Count = 3
        },
        [OPT_BATTLE_ANIMATION] = {
            .Y = 8,
            .Xs = {1, 10},
            .Count = 2
        },
        [OPT_BATTLE_STYLE] = {
            .Y = 13,
            .Xs = {1, 10},
            .Count = 2
        },
        [OPT_CANCEL] = {
            .Y = 16,
            .Xs = {1},
            .Count = 1
        }
    }
};

menu MainMenu = { 
    .WindowTask.Type = TT_MENU,
    .Text = "NEW GAME\nOPTION",
    .Rect = {0, 0, 15, 6},
    .EndI = 2,
    .TextY = 2,
    .Flags = MF_AUTO_RESET
};

menu ContinueMenu = {
    .WindowTask.Type = TT_MENU,
    .Rect = {0, 0, 15, 8},
    .Text = "CONTINUE\nNEW GAME\nOPTION",
    .EndI = 3,
    .TextY = 2,
    .Flags = MF_AUTO_RESET
}; 

menu StartMenu = {
    .WindowTask.Type = TT_MENU,
    .Text = "POK" "\xE9" "MON\nITEM\nRED\nSAVE\nOPTION\nEXIT", 
    .Rect = {10, 0, 20, 14},
    .TextY = 2,
    .EndI = 6,
};

menu RedPCMenu = {
    .WindowTask.Type = TT_MENU,
    .Text = "WITHDRAW ITEM\nDEPOSIT ITEM\nTOSS ITEM\nLOG OFF",
    .Rect = {0, 0, 16, 10},
    .TextY = 2,
    .EndI = 4,
    .Flags = MF_AUTO_RESET
};

menu YesNoMenu = {
    .WindowTask.Type = TT_MENU,
    .Text = "YES\nNO",
    .Rect = {0, 7, 6, 12},
    .TextY = 8,
    .EndI = 2,
    .Flags = MF_AUTO_RESET
};

menu UseTossMenu = {
    .WindowTask.Type = TT_MENU,
    .Text = "USE\nTOSS",
    .Rect = {13, 10, 20, 15},
    .TextY = 11,
    .EndI = 2,
    .Flags = MF_AUTO_RESET
};

menu ConfirmTossMenu = {
    .WindowTask.Type = TT_MENU,
    .Text = "YES\nNO",
    .Rect = {14, 7, 20, 12},
    .TextY = 8,
    .EndI = 2,
    .Flags = MF_AUTO_RESET
};

int SaveSec;
int StartSaveSec;

active_text ActiveText;

void PlaceTextBox(rect Rect) {
    /*HeadRow*/
    WindowMap[Rect.Top][Rect.Left] = 96;
    memset(&WindowMap[Rect.Top][Rect.Left + 1], 97, Rect.Right - Rect.Left - 2);
    WindowMap[Rect.Top][Rect.Right - 1] = 98;

    /*BodyRows*/
    for(int Y = Rect.Top + 1; Y < Rect.Bottom - 1; Y++) {
        WindowMap[Y][Rect.Left] = 99;
        memset(&WindowMap[Y][Rect.Left + 1], MT_BLANK, Rect.Right - Rect.Left - 2);
        WindowMap[Y][Rect.Right - 1] = 101;
    }

    /*FootRow*/
    WindowMap[Rect.Bottom - 1][Rect.Left] = 100;
    memset(&WindowMap[Rect.Bottom - 1][Rect.Left + 1], 97, Rect.Right - Rect.Left - 2);
    WindowMap[Rect.Bottom - 1][Rect.Right - 1] = 102;
}

int CharToTile(int Char) {
    int Output = Char;
    if(Output == '*') {
        Output = MT_TIMES;
    } else if(Output == '.') {
        Output = 175;
    } else if(Output >= '0' && Output <= ':') {
        Output += 103 - '0';
    } else if(Output >= 'A' && Output <= 'Z') {
        Output += 114 - 'A';
    } else if(Output >= 'a' && Output <= 'z') {
        Output += 140 - 'a';
    } else if(Output == '!') {
        Output = 168;
    } else if(Output == '\xE9') {
        Output = 166;
    } else if(Output == '\'') {
        Output = 169;
    } else if(Output == '~') {
        Output = 172;
    } else if(Output == '-') {
        Output = 170;
    } else if(Output == ',') {
        Output = 173; 
    } else if(Output == '?') {
        Output = MT_QUESTION; 
    } else {
        Output = MT_BLANK;
    }
    return Output;
}

void PlaceText(point TileMin, const char *Text) {
    int X = TileMin.X;
    int Y = TileMin.Y;
    for(int I = 0; Text[I] != '\0'; I++) {
        switch(Text[I]) {
        case '\n':
            X = TileMin.X;
            Y += 2;
            break;
        case '\r':
            X = TileMin.X;
            Y += 3; 
            break;
        default:
            WindowMap[Y][X] = CharToTile(Text[I]);
            X++;
        }
    }
}

void PlaceTextF(point TileMin, const char *Format, ...) {
    char Text[256];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Text, sizeof(Text), Format, Args);
    va_end(Args);
    PlaceText(TileMin, Text);
}

void PlaceStaticText(rect Rect, const char *Text) {
    PlaceTextBox(Rect);
    PlaceText((point) {1, 14}, Text);
}

void PlaceSave(rect Rect) {
    int SaveMin = MinInt(SaveSec / 60, 99 * 59 + 60); 
    PlaceTextBox(Rect);
    PlaceTextF(
        (point) {Rect.Left + 1, Rect.Top + 2},
        "PLAYER %s\nBADGES       %d\nPOK" "\xE9" "DEX    %3d\nTIME     %2d:%02d",
        "RED",
        0,
        0,
        SaveMin / 60,
        SaveMin % 60
    );
}

void PlaceMenuCursor(const menu *Menu, int MenuTile) {
    WindowMap[Menu->SelectI * 2 + Menu->TextY][Menu->Rect.Left + 1] = MenuTile; 
}

void PlaceMenu(const menu *Menu) {
    PlaceTextBox(Menu->Rect); 
    PlaceText((point) {Menu->Rect.Left + 2, Menu->TextY}, Menu->Text); 
    PlaceMenuCursor(Menu, MT_FULL_HORZ_ARROW);
}

void PlaceOptionCursor(const option *Option, int Tile) {
    WindowMap[Option->Y][Option->Xs[Option->I]] = Tile;
}

void ChangeOptionX(option *Option, int NewOptionI) {
    PlaceOptionCursor(Option, MT_BLANK);
    Option->I = NewOptionI;
    PlaceOptionCursor(Option, MT_FULL_HORZ_ARROW);
}

void PlaceInventory(const inventory *Inventory) {
    PlaceTextBox((rect) {4, 2, 20, 13});
    int MinItem = Inventory->ItemSelect - Inventory->Y;
    for(int I = 0; I < 4; I++) {
        int ItemI = MinItem + I;
        int Y = I * 2 + 4;

        /*PlaceItem*/
        if(ItemI < Inventory->ItemCount) {
            PlaceText((point) {6, Y}, ItemNames[Inventory->Items[ItemI].ID]);
            PlaceTextF((point) {14, Y + 1}, "*%2d", Inventory->Items[ItemI].Count);
        } else if(ItemI == Inventory->ItemCount) {
            PlaceText((point) {6, Y}, "CANCEL");
        }
    }
    
    /*PlaceInventoryCursor*/
    WindowMap[Inventory->Y * 2 + 4][5] = MT_FULL_HORZ_ARROW;
}

void ClearWindow(void) {
    memset(WindowMap, 0, sizeof(WindowMap));
}

void ClearWindowRect(rect Rect) {
    for(int Y = Rect.Top; Y < Rect.Bottom; Y++) {
        for(int X = Rect.Left; X < Rect.Right; X++) {
            WindowMap[Y][X] = 0;
        }
    }
}

void MoveMenuCursor(menu *Menu) {
    if(VirtButtons[BT_UP] == 1 && Menu->SelectI > 0) {
        PlaceMenuCursor(Menu, MT_BLANK);
        Menu->SelectI--; 
        PlaceMenuCursor(Menu, MT_FULL_HORZ_ARROW);
    } else if(VirtButtons[BT_DOWN] == 1 && Menu->SelectI < Menu->EndI - 1) {
        PlaceMenuCursor(Menu, MT_BLANK);
        Menu->SelectI++; 
        PlaceMenuCursor(Menu, MT_FULL_HORZ_ARROW);
    }
}

void MoveMenuCursorWrap(menu *Menu) { 
    if(VirtButtons[BT_UP] == 1) {
        PlaceMenuCursor(Menu, MT_BLANK);
        Menu->SelectI = PosIntMod(Menu->SelectI - 1, Menu->EndI); 
        PlaceMenuCursor(Menu, MT_FULL_HORZ_ARROW);
    } else if(VirtButtons[BT_DOWN] == 1) {
        PlaceMenuCursor(Menu, MT_BLANK);
        Menu->SelectI = PosIntMod(Menu->SelectI + 1, Menu->EndI); 
        PlaceMenuCursor(Menu, MT_FULL_HORZ_ARROW);
    }
}

void PlaceOptionsMenu(options_menu *Options) {
    memset(WindowMap, MT_BLANK, sizeof(WindowMap));
    PlaceTextBox((rect) {0, 0, 20, 5});
    PlaceTextBox((rect) {0, 5, 20, 10});
    PlaceTextBox((rect) {0, 10, 20, 15});
    PlaceText(
        (point) {1, 1}, 
        "TEXT SPEED\n FAST  MEDIUM SLOW\r" 
        "BATTLE ANIMATION\n ON       OFF\r" 
        "BATTLE STYLE\n SHIFT    SET\r"
        " CANCEL"
    ); 
    Options->I = 0;
    PlaceOptionCursor(&Options->E[0], MT_FULL_HORZ_ARROW); 
    for(size_t I = 1; I < _countof(Options->E); I++) {
        PlaceOptionCursor(&Options->E[I], MT_EMPTY_HORZ_ARROW);
    }
}

void FlashTextCursor(active_text *ActiveText) {
    WindowMap[16][18] = ActiveText->Tick++ / 30 % 2 ? MT_BLANK : MT_FULL_VERT_ARROW;
}

void ClearBottomWindow(void) { 
    ClearWindowRect(BottomTextRect);
}

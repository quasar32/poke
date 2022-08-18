#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "frame.h"
#include "options.h"
#include "input.h"
#include "save.h"
#include "scalar.h"
#include "render.h"
#include "text.h"

const rect BottomTextRect = {0, 12, 20, 18};

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
    .Text = "POKÈMON\nITEM\n@RED\nSAVE\nOPTION\nEXIT", 
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

window_task *DeferedTask;
const char *DeferedMessage;

static const char *StrMovePastSpan(const char *Str, const char *Span) {
    while(*Span != '\0') {
        if(*Str != *Span) {
            return NULL;
        }
        Str++;
        Span++;
    }
    return Str;
}

static const char *GetNewRestoreText(const char **Str) {
    const char *Restore = NULL;
    Restore = StrMovePastSpan(*Str, "@ITEM");
    if(Restore) {
        *Str = ItemNames[DisplayItem.ID];
        goto out;
    }

    Restore = StrMovePastSpan(*Str, "@RED");
    if(Restore) {
        *Str = g_Name;
        goto out;
    }
    
    Restore = StrMovePastSpan(*Str, "@BLUE");
    if(Restore) {
        *Str = g_Rival;
        goto out;
    }
out:
    if(Restore && !**Str) {
        *Str = Restore;
        Restore = NULL;
    }
    return Restore;
}

static size_t GetTokenLength(const char *Str) {
    size_t I;
    for(I = 0; Str[I] && !isspace(Str[I]) && Str[I] != '-'; I++); 
    return I;
}

static void GoToNewLine(int *TileX, int *TileY) {
    *TileX = 1;
    *TileY += 2;
    if(*TileY == 18) {
        WaitOnFlash();
        memcpy(&g_WindowMap[13][1], &g_WindowMap[14][1], 17);
        memcpy(&g_WindowMap[15][1], &g_WindowMap[16][1], 17);
        memset(&g_WindowMap[14][1], MT_BLANK, 17);
        memset(&g_WindowMap[16][1], MT_BLANK, 18);
        Pause(8);
        memcpy(&g_WindowMap[14][1], &g_WindowMap[15][1], 17);
        memset(&g_WindowMap[13][1], MT_BLANK, 17);
        memset(&g_WindowMap[15][1], MT_BLANK, 17);
        *TileY = 16;
    }
} 

static void GoToNewSentence(int *TileX, int *TileY) {
    *TileX = 1;
    *TileY = 18;
    WaitOnFlash();
    memset(&g_WindowMap[14][1], MT_BLANK, 17);
    memset(&g_WindowMap[16][1], MT_BLANK, 18);
    Pause(8);
    *TileY = 14;
} 

static BOOL GetCurText(LPCSTR *Str, LPCSTR *Restore) {
    if(!**Str) {
        if(!*Restore) {
            return FALSE;
        } 
        if(**Restore) {
            *Str = *Restore;
        }
        *Restore = NULL;
    }
    if(!*Restore) {
        *Restore = GetNewRestoreText(Str);
    }
    return TRUE;
}

void PlaceBox(rect Rect) {
    /*HeadRow*/
    g_WindowMap[Rect.Top][Rect.Left] = MT_TOP_LEFT;
    memset(&g_WindowMap[Rect.Top][Rect.Left + 1], MT_MIDDLE, Rect.Right - Rect.Left - 2);
    g_WindowMap[Rect.Top][Rect.Right - 1] = MT_TOP_RIGHT;

    /*BodyRows*/
    for(int Y = Rect.Top + 1; Y < Rect.Bottom - 1; Y++) {
        g_WindowMap[Y][Rect.Left] = MT_CENTER_LEFT;
        memset(&g_WindowMap[Y][Rect.Left + 1], MT_BLANK, Rect.Right - Rect.Left - 2);
        g_WindowMap[Y][Rect.Right - 1] = MT_CENTER_RIGHT;
    }

    /*FootRow*/
    g_WindowMap[Rect.Bottom - 1][Rect.Left] = MT_BOTTOM_LEFT;
    memset(&g_WindowMap[Rect.Bottom - 1][Rect.Left + 1], MT_MIDDLE, Rect.Right - Rect.Left - 2);
    g_WindowMap[Rect.Bottom - 1][Rect.Right - 1] = MT_BOTTOM_RIGHT;
}

int CharToTile(int Ch) {
    switch(Ch) {
    case ' ':
        return MT_BLANK;
    case '0' ... '9':
        return MT_ZERO + (Ch - '0');
    case ':':
        return MT_COLON;
    case 'A' ... 'Z':
        return MT_CAPITAL_A + (Ch - 'A');
    case 'a' ... 'z':
        return MT_LOWERCASE_A + (Ch - 'a');
    case 'È':
        return MT_ACCENTED_E;
    case '!':
        return MT_EXCLAMATION_POINT;
    case '\'':
        return MT_QUOTE_S;
    case '-':
        return MT_DASH;
    case '~':
        return MT_QUOTE_M;
    case ',':
        return MT_COMMA;
    case '.':
        return MT_PERIOD;
    case ';':
        return MT_SEMI_COLON;
    case '[':
        return MT_LEFT_BRACKET;
    case ']':
        return MT_RIGHT_BRACKET; 
    case '(':
        return MT_LEFT_PARENTHESIS;
    case ')':
        return MT_RIGHT_PARENTHESIS;
    case '^':
        return MT_MALE_SYMBOL;
    case '|':
        return MT_FEMALE_SYMBOL;
    case '/':
        return MT_SLASH;
    case '\1':
        return MT_PK;
    case '\2':
        return MT_MN;
    case '=':
        return MT_MIDDLE_SCORE;
    case '_':
        return MT_UPPER_SCORE;
    case '\3':
        return MT_END;
    case '*': 
        return MT_TIMES;
    case '?':
        return MT_QUESTION;
    }
    return MT_EMPTY;
}

int TileToChar(int Tile) {
    switch(Tile) {
    case MT_BLANK:
        return ' ';
    case MT_ZERO ... MT_NINE:
        return Tile - MT_ZERO + '0';
    case MT_COLON:
        return ':';
    case MT_CAPITAL_A ... MT_CAPITAL_Z:
        return Tile - MT_CAPITAL_A + 'A';
    case MT_LOWERCASE_A ... MT_LOWERCASE_Z:
        return Tile - MT_LOWERCASE_A + 'a';
    case MT_ACCENTED_E:
        return 'È';
    case MT_EXCLAMATION_POINT:
        return '!';
    case MT_QUOTE_S:
        return '\'';
    case MT_DASH:
        return '-';
    case MT_QUOTE_M:
        return '~';
    case MT_COMMA:
        return ',';
    case MT_PERIOD:
        return '.';
    case MT_SEMI_COLON:
        return ';';
    case MT_LEFT_BRACKET:
        return '[';
    case MT_RIGHT_BRACKET: 
        return ']';
    case MT_LEFT_PARENTHESIS:
        return '(';
    case MT_RIGHT_PARENTHESIS:
        return ')';
    case MT_MALE_SYMBOL:
        return '^';
    case MT_FEMALE_SYMBOL:
        return '|';
    case MT_SLASH:
        return '/';
    case MT_PK:
        return '\1';
    case MT_MN:
        return '\2';
    case MT_MIDDLE_SCORE:
        return '=';
    case MT_UPPER_SCORE:
        return '_';
    case MT_END:
        return '\3';
    case MT_TIMES:
        return '*'; 
    case MT_QUESTION:
        return '?';
    }
    return '\0';
}


void PlaceText(int X, int Y, const char *Text) {
    const char *Restore = NULL;
    int StartX = X;

    while(GetCurText(&Text, &Restore)) {
        switch(*Text) {
        case '\n':
            X = StartX;
            Y += 2;
            break;
        default:
            g_WindowMap[Y][X] = CharToTile(*Text);
            X++;
        }
        Text++;
    }
}

void PlaceTextF(int X, int Y, const char *Format, ...) {
    char Text[256];
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Text, sizeof(Text), Format, Args);
    va_end(Args);
    PlaceText(X, Y, Text);
}

void PlaceMenuCursor(const menu *Menu, int MenuTile) {
    g_WindowMap[Menu->SelectI * 2 + Menu->TextY][Menu->Rect.Left + 1] = MenuTile; 
}

void PlaceMenu(const menu *Menu) {
    PlaceBox(Menu->Rect); 
    PlaceText(Menu->Rect.Left + 2, Menu->TextY, Menu->Text); 
    PlaceMenuCursor(Menu, MT_FULL_HORZ_ARROW);
}

void PlaceInventory(const inventory *Inventory) {
    PlaceBox((rect) {4, 2, 20, 13});
    int MinItem = Inventory->ItemSelect - Inventory->Y;
    for(int I = 0; I < 4; I++) {
        int ItemI = MinItem + I;
        int Y = I * 2 + 4;

        /*PlaceItem*/
        if(ItemI < Inventory->ItemCount) {
            PlaceText(6, Y, ItemNames[Inventory->Items[ItemI].ID]);
            PlaceTextF(14, Y + 1, "*%2d", Inventory->Items[ItemI].Count);
        } else if(ItemI == Inventory->ItemCount) {
            PlaceText(6, Y, "CANCEL");
        }
    }
    
    /*PlaceInventoryCursor*/
    g_WindowMap[Inventory->Y * 2 + 4][5] = MT_FULL_HORZ_ARROW;
}

void ClearWindow(void) {
    memset(g_WindowMap, MT_EMPTY, sizeof(g_WindowMap));
}

void BlankWindow(void) {
    memset(g_WindowMap, MT_BLANK, sizeof(g_WindowMap));
}

void ClearWindowRect(rect Rect) {
    for(int Y = Rect.Top; Y < Rect.Bottom; Y++) {
        for(int X = Rect.Left; X < Rect.Right; X++) {
            g_WindowMap[Y][X] = 0;
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

int GetMenuOptionSelected(menu *Menu, int CancelI) {
    if(VirtButtons[BT_A] == 1) {
        return Menu->SelectI; 
    }
    if(VirtButtons[BT_B] == 1) {
        return CancelI;
    }
    return -1;
}

void ClearBottomWindow(void) { 
    ClearWindowRect(BottomTextRect);
}

void DisplayTextBox(const char *Str, int Delay) {
    const char *Restore = NULL;
    int TileX = 1;
    int TileY = 14;

    PlaceBox(BottomTextRect);
    Pause(Delay + 1);
    while(GetCurText(&Str, &Restore)) {
        switch(*Str) {
        case '\f':
            GoToNewSentence(&TileX, &TileY);
            Str++;
            break;
        case '\n':
            GoToNewLine(&TileX, &TileY);
            Str++;
            break;
        case '\0':
            break;
        default:
            size_t TokLen = GetTokenLength(Str);
            if(TokLen > 16) {
                Str = "ERROR: TOKEN TOO LONG";
            } else if(TileX + TokLen < 19) {
                int Tile = CharToTile(*Str);
                g_WindowMap[TileY][TileX] = Tile; 
                TileX++;
                Str++;
            } else {
                GoToNewLine(&TileX, &TileY);
            }
        }
        if(!VirtButtons[BT_A] && !VirtButtons[BT_B]) {
            Pause((int[]) {0, 2, 3}[g_Options[OPT_TEXT_SPEED].I]);
        }
        NextFrame();
    }
}

void PlaceTextBox(const char *Str, int Delay) {
    const char *Restore = NULL;
    int TileX = 1;
    int TileY = 14;

    PlaceBox(BottomTextRect);
    Pause(Delay); 

    while(GetCurText(&Str, &Restore)) {
        size_t TokLen = GetTokenLength(Str);
        if(TokLen > 16) {
            DisplayTextBox("ERROR: TOKEN TOO LONG", 0);
            return;
        } else if(TileX + TokLen < 19) {
            int Tile = CharToTile(*Str);
            g_WindowMap[TileY][TileX] = Tile; 
            TileX++;
            Str++;
        } else if(TileY < 18) {
            TileX = 1;
            TileY += 2;
        } else {
            DisplayTextBox("ERROR: TEXT TOO LONG", 0);
            return;
        }
    }
}

int FlashCursor(int Tick) {
    g_WindowMap[16][18] = Tick / 30 % 2 ? MT_BLANK : MT_FULL_VERT_ARROW;
    return Tick + 1;
}

void WaitOnFlash(void) {
    int Tick = 0;
    while(VirtButtons[BT_A] != 1 && VirtButtons[BT_B] != 1) {
        Tick = FlashCursor(Tick);
        NextFrame();
    }
    PlaySoundEffect(SFX_PRESS_AB);
    g_WindowMap[16][18] = MT_BLANK;
}


#include <windows.h>

#include "audio.h"
#include "frame.h"
#include "input.h"
#include "options.h"
#include "render.h"
#include "scalar.h"
#include "text.h"

option g_Options[4] = {
    [OPT_TEXT_SPEED] = {
        .Y = 3,
        .Xs = {1, 7, 14},
        .Count = 3,
        .I = 1
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
};

static void PlaceOptionCursor(const option *Option, int Tile) {
    g_WindowMap[Option->Y][Option->Xs[Option->I]] = Tile;
}

static void PlaceOptionsMenu(void) {
    memset(g_WindowMap, MT_BLANK, sizeof(g_WindowMap));
    PlaceBox((rect) {0, 0, 20, 5});
    PlaceBox((rect) {0, 5, 20, 10});
    PlaceBox((rect) {0, 10, 20, 15});
    PlaceText(1, 1, "TEXT SPEED\n FAST  MEDIUM SLOW"); 
    PlaceText(1, 6, "BATTLE ANIMATION\n ON       OFF"); 
    PlaceText(1, 11, "BATTLE STYLE\n SHIFT    SET");
    PlaceText(1, 16, " CANCEL");
    g_Options->I = 0;
    PlaceOptionCursor(&g_Options[0], MT_FULL_HORZ_ARROW); 
    for(size_t I = 1; I < _countof(g_Options); I++) {
        PlaceOptionCursor(&g_Options[I], MT_EMPTY_HORZ_ARROW);
    }
}

static BOOL HasClosed(int I) {
    BOOL Request = (
        VirtButtons[BT_START] == 1 || 
        VirtButtons[BT_B] == 1 ||
        (VirtButtons[BT_A] == 1 && I == OPT_CANCEL)
    );
    if(Request) {
        PlaySoundEffect(SFX_PRESS_AB); 
    }
    return Request;
}

static void ChangeOptionX(option *Option, int NewOptionI) {
    PlaceOptionCursor(Option, MT_BLANK);
    Option->I = NewOptionI;
    PlaceOptionCursor(Option, MT_FULL_HORZ_ARROW);
}

static void ChangeOptionY(int *I, int NewI) {
    PlaceOptionCursor(&g_Options[*I], MT_EMPTY_HORZ_ARROW);
    *I = PosIntMod(NewI, _countof(g_Options)); 
    PlaceOptionCursor(&g_Options[*I], MT_FULL_HORZ_ARROW);
}

static void MoveOptionUp(int *I) {
    ChangeOptionY(I, *I - 1);
}

static void MoveOptionLeft(int I) {
    switch(g_Options[I].Count) {
    case 2:
        /*SwapOptionSelected*/
        ChangeOptionX(&g_Options[I], g_Options[I].I ^ 1); 
        break;
    case 3:
        /*NextOptionLeft*/
        if(g_Options[I].I > 0) {
            ChangeOptionX(&g_Options[I], g_Options[I].I - 1); 
        }
        break;
    }
}

static void MoveOptionDown(int *I) {
    ChangeOptionY(I, *I + 1);
}

static void MoveOptionRight(int I) {
    switch(g_Options[I].Count) {
    case 2:
        /*SwapOptionSelected*/
        ChangeOptionX(&g_Options[I], g_Options[I].I ^ 1); 
        break;
    case 3:
        /*NextOptionRight*/
        if(g_Options[I].I < 2) {
            ChangeOptionX(&g_Options[I], g_Options[I].I + 1); 
        }
        break;
    }
}

static void UpdateOptions(void) {
    int I = 0;
    while(!HasClosed(I)) {
        if(VirtButtons[BT_UP] == 1) {
            MoveOptionUp(&I);
        } else if(VirtButtons[BT_LEFT] == 1) {
            MoveOptionLeft(I);
        } else if(VirtButtons[BT_DOWN] == 1) {
            MoveOptionDown(&I);
        } else if(VirtButtons[BT_RIGHT] == 1) {
            MoveOptionRight(I);
        }
        NextFrame();
    }
}

void OpenOptions(void) {
    PlaceOptionsMenu();
    NextFrame();
    UpdateOptions();
    ClearWindow();
    ExecuteAllWindowTasks();
}


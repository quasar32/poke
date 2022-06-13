#include "audio.h"
#include "input.h"
#include "options.h"
#include "state.h"
#include "scalar.h"

options_menu Options = {
    .WindowTask.Type = TT_OPTIONS,
    .E = {
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
    }
};

static int OptionI; 

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

void PlaceOptionCursor(const option *Option, int Tile) {
    WindowMap[Option->Y][Option->Xs[Option->I]] = Tile;
}

void ChangeOptionX(option *Option, int NewOptionI) {
    PlaceOptionCursor(Option, MT_BLANK);
    Option->I = NewOptionI;
    PlaceOptionCursor(Option, MT_FULL_HORZ_ARROW);
}

void GS_OPTIONS(void) {
    if(
        VirtButtons[BT_START] == 1 || 
        VirtButtons[BT_B] == 1 ||
        (VirtButtons[BT_A] == 1 && OptionI == OPT_CANCEL)
    ) {
        /*RemoveSubMenu*/
        RemoveWindowTask();
        PopState();
        PlayWave(Sound.Voice, &Sound.PressAB); 
    } else {
        /*MoveOptionsCursor*/
        if(VirtButtons[BT_UP] == 1) {
            PlaceOptionCursor(&Options.E[OptionI], MT_EMPTY_HORZ_ARROW);
            OptionI = PosIntMod(OptionI - 1, _countof(Options.E)); 
            PlaceOptionCursor(&Options.E[OptionI], MT_FULL_HORZ_ARROW);
        } else if(VirtButtons[BT_LEFT] == 1) {
            switch(Options.E[OptionI].Count) {
            case 2:
                /*SwapOptionSelected*/
                ChangeOptionX(&Options.E[OptionI], Options.E[OptionI].I ^ 1); 
                break;
            case 3:
                /*NextOptionLeft*/
                if(Options.E[OptionI].I > 0) {
                    ChangeOptionX(&Options.E[OptionI], Options.E[OptionI].I - 1); 
                }
                break;
            }
        } else if(VirtButtons[BT_DOWN] == 1) {
            PlaceOptionCursor(&Options.E[OptionI], MT_EMPTY_HORZ_ARROW);
            OptionI = PosIntMod(OptionI + 1, _countof(Options.E)); 
            PlaceOptionCursor(&Options.E[OptionI], MT_FULL_HORZ_ARROW);
        } else if(VirtButtons[BT_RIGHT] == 1) {
            switch(Options.E[OptionI].Count) {
            case 2:
                /*SwapOptionSelected*/
                ChangeOptionX(&Options.E[OptionI], Options.E[OptionI].I ^ 1); 
                break;
            case 3:
                /*NextOptionRight*/
                if(Options.E[OptionI].I < 2) {
                    ChangeOptionX(&Options.E[OptionI], Options.E[OptionI].I + 1); 
                }
                break;
            }
        }
    }
}

#include "audio.h"
#include "input.h"
#include "save.h"
#include "state.h"
#include "str.h"
#include "options.h"
#include "window_map.h"

void GS_TEXT(void) {
    if(ActiveText.BoxDelay >= 0) {
        if(ActiveText.BoxDelay == 0) {
            if(IsSaveComplete) {
                PlayWave(Sound.Voice, &Sound.Save);
                IsSaveComplete = false;
            }
            PlaceTextBox(BottomTextRect);
            ActiveText.TilePt = (point) {1, 14};
        }
        ActiveText.BoxDelay--;
    } else { 
        if(!ActiveText.Str[0]) {
            if(ActiveText.Restore) {
                ActiveText.Str = ActiveText.Restore;
                ActiveText.Restore = NULL;
            } 
        }
        if(ActiveText.Str[0]) {
            switch(ActiveText.TilePt.Y) {
            case 17:
                /*MoreTextDisplay*/
                if(ActiveText.Tick > 0) {
                    ActiveText.Tick--;
                } else switch(ActiveText.Str[-1]) {
                case '\n':
                    memcpy(&WindowMap[14][1], &WindowMap[15][1], 17);
                    memset(&WindowMap[13][1], MT_BLANK, 17);
                    memset(&WindowMap[15][1], MT_BLANK, 17);
                    ActiveText.TilePt.Y = 16;
                    break;
                case '\f':
                    ActiveText.TilePt.Y = 14;
                    break;
                }
                break;
            case 18:
                /*MoreTextWait*/
                if(VirtButtons[BT_A] == 1 || VirtButtons[BT_B] == 1) {
                    if(ActiveText.Str[-1] == '\n') {
                        memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                        memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                    }
                    memset(&WindowMap[14][1], MT_BLANK, 17);
                    memset(&WindowMap[16][1], MT_BLANK, 18);
                    ActiveText.TilePt.Y = 17;
                    ActiveText.Tick = 8;
                    PlayWave(Sound.Voice, &Sound.PressAB);
                } else {
                   FlashTextCursor(&ActiveText);
                }
                break;
            default:
                /*UpdateTextForNextChar*/
                if(ActiveText.Tick > 0) {
                    ActiveText.Tick--;
                } else {
                    if(ActiveText.Str[0] == '[') {
                        ActiveText.Restore = ActiveText.Str;
                        if(DoesStartStringMatch(ActiveText.Str, "[ITEM]")) {
                            ActiveText.Restore += sizeof("[ITEM]") - 1;
                            ActiveText.Str = ItemNames[DisplayItem.ID];
                        } else {
                            ActiveText.Restore++;
                            ActiveText.Str = "???";
                        }
                    }
                    switch(ActiveText.Str[0]) {
                    case '\n':
                        ActiveText.TilePt.X = 1;
                        ActiveText.TilePt.Y += 2;
                        ActiveText.Str++;
                        break;
                    case '\f':
                        ActiveText.TilePt.X = 1;
                        ActiveText.TilePt.Y = 18;
                        ActiveText.Str++;
                        break;
                    default:
                        WindowMap[ActiveText.TilePt.Y][ActiveText.TilePt.X] = CharToTile(&ActiveText.Str);
                        if(ActiveText.TilePt.X < 20) {
                            ActiveText.TilePt.X++;
                        } else {
                            ActiveText.Str = "";
                        }
                    }
                    if(ActiveText.TilePt.Y != 18) {
                        /*UseTextSpeed*/
                        if(!VirtButtons[BT_A] && !VirtButtons[BT_B]) {
                            ActiveText.Tick = (uint64_t[]) {0, 2, 3}[Options.E[OPT_TEXT_SPEED].I];
                        }
                    }
                }
            }
        } else {
            /*RestoreFromText*/
            if(ActiveText.Props & AT_WAIT_FLASH) {
                FlashTextCursor(&ActiveText);
            }
            if(VirtButtons[BT_A] || !(ActiveText.Props & AT_MANUAL_RESTORE)) {
                WindowMap[16][18] = MT_BLANK;
                if(ActiveText.Props & AT_WAIT_FLASH) {
                    PlayWave(Sound.Voice, &Sound.PressAB);
                }
                if(ActiveText.Props & AT_AUTO_CLEAR) {
                    ClearBottomWindow();
                }
                PopState();
            }
        }
    }
}

/*Includes*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "audio.h"
#include "frame.h"
#include "inventory.h"
#include "input.h"
#include "intro.h"
#include "options.h"
#include "procs.h"
#include "scalar.h"
#include "text.h"
#include "world.h"
#include "save.h"
#include "state.h"
#include "win32.h"

/*Macro Strings*/
#define RPC_INTRO "What do you want to do?"

/*Globals*/
static const rect g_SaveRect = {4, 0, 20, 10};

static BOOL g_IsPauseAttempt;

void GS_MAIN_MENU(void);

void GS_NORMAL(void);

void GS_START_MENU(void);
void GS_INVENTORY(void);
void GS_SAVE(void);

void GS_USE_TOSS(void);
void GS_TOSS(void);
void GS_CONFIRM_TOSS(void);

void GS_RED_PC_MAIN(void);
void GS_RED_PC_ITEM(void);

void GS_RED_PC_REPEAT_INTRO(void);

/*StartGame*/
static void StartWorld(void) {
    strcpy(g_RestorePath, "PalletTown");
    ReadMap(0, "PlayerHouse2");
    PlaceViewMap();
    UpdatePallete();
    PushBasicPlayerSprite();
    Pause(15);
    g_MusicI = MUS_PALLET_TOWN;
    SwitchMusic();
    g_Player.Dir = DIR_UP;
    ActivateWorld();
    StartSaveCounter();
    ReplaceState(GS_NORMAL);
}

static void StartGame(void) {
    PlaySoundEffect(SFX_PRESS_AB); 
    ClearWindowStack();
    
    PresentOak();
    PresentNidorino();
    NameRed();
    NameBlue();
    Outro();

    StartWorld();
}

/**/
static void ContinueGame(void) {
    ClearWindowStack();
    memset(g_WindowMap, MT_BLANK, sizeof(g_WindowMap));
    ReadSave();
    SwitchMusic();
    Pause(30);
    ClearWindow();
    PlaceViewMap();
    UpdatePallete();
    ActivateWorld();
    StartSaveCounter();
    ReplaceState(GS_NORMAL);
} 

static void ClearContinuePrompt(void) {
    ResetSaveSec();
    ClearWindow();
    Pause(30);
    ExecuteAllWindowTasks();
}

static void OpenContinuePrompt(void) {
    BOOL ContinuePromptOpen = TRUE;
    PlaySoundEffect(SFX_PRESS_AB); 
    PlaceSave(g_ContinueSaveRect.Rect);
    while(ContinuePromptOpen) {
        NextFrame();
        if(VirtButtons[BT_A] == 1) {
            ContinueGame();
            ContinuePromptOpen = FALSE;
        } else if(VirtButtons[BT_B] == 1) {
            ClearContinuePrompt();
            ContinuePromptOpen = FALSE;
        }
    }
}


void GS_MAIN_MENU(void) {
    MoveMenuCursor(&MainMenu);
    switch(GetMenuOptionSelected(&MainMenu, -1)) {
    case 0: /*NEW GAME*/
        StartGame();
        break;
    case 1: /*OPTIONS*/
        OpenOptions();
        break;
    }
}

void GS_CONTINUE_MENU(void) {
    MoveMenuCursor(&ContinueMenu);
    switch(GetMenuOptionSelected(&ContinueMenu, -1)) {
    case 0: /*CONTINUE*/
        OpenContinuePrompt();
        break;
    case 1: /*NEW GAME*/
        StartGame();
        break;
    case 2: /*OPTIONS*/ 
        OpenOptions();
        break;
    } 
}

static void GoThroughDoor(point DoorPoint) {
    MovePlayerSync(0);
    TransitionColors();
    LoadDoorMap(DoorPoint);
}

void MovePlayer(quad_info NewQuadInfo, quad_info OldQuadInfo) {
    if(NewQuadInfo.Prop == QUAD_PROP_DOOR) { 
        if(!g_InOverworld) {
            PlaySoundEffect(SFX_GO_OUTSIDE);
            GoThroughDoor(NewQuadInfo.Point);
        } else if(g_Player.Dir == DIR_UP) {
            PlaySoundEffect(SFX_GO_INSIDE);
            strcpy(g_RestorePath, g_Maps[g_MapI].Path);
            GoThroughDoor(NewQuadInfo.Point);
        }
    } else if(g_Player.Dir == DIR_DOWN && NewQuadInfo.Prop == QUAD_PROP_EDGE) {
        PlaySoundEffect(SFX_LEDGE);
        PlayerLeap();
    } else if(IsSolidPropForPlayer(NewQuadInfo.Prop)) {
        MovePlayerAsync(NewQuadInfo, 16);
    } else if(g_Player.Dir == DIR_DOWN && OldQuadInfo.Prop == QUAD_PROP_EXIT) {
        PlaySoundEffect(SFX_GO_OUTSIDE);
        UpdateObjects(0);
        TransitionColors();
        LoadDoorMap(OldQuadInfo.Point);
        MovePlayerSync(0);
    } else {
        PlaySoundEffect(SFX_COLLISION);
        g_Player.Tick = 16;
    }
}

static void OpenText(map *Map, point Point) {
    const char *Str = GetTextStrFromPos(Map, Point);
    DisplayTextBox(Str, 16);
    WaitOnFlash();
    ClearWindow();
}

static BOOL UpdateKeyboard(void) {
    if(VirtButtons[BT_UP]) {
        g_Player.Dir = DIR_UP;
        return TRUE;
    } 
    if(VirtButtons[BT_LEFT]) {
        g_Player.Dir = DIR_LEFT;
        return TRUE;
    } 
    if(VirtButtons[BT_DOWN]) {
        g_Player.Dir = DIR_DOWN;
        return TRUE;
    } 
    if(VirtButtons[BT_RIGHT]) {
        g_Player.Dir = DIR_RIGHT;
        return TRUE;
    }
    return FALSE;
}

object *GetFacingObject(quad_info NewQuadInfo) {
    map *Map = &g_Maps[g_MapI];
    point TestPoint = NewQuadInfo.Point;
    for(int ObjI = 0; ObjI < Map->ObjectCount; ObjI++) {
        object *Object = &Map->Objects[ObjI];
        point ObjOldPt = PtToQuadPt(Object->Pos);
        if(EqualPoints(TestPoint, ObjOldPt)) {
            return Object;
        }
        if(Object->Tick > 0) {
            point ObjDirPt = NextPoints[Object->Dir];
            point ObjNewPt = AddPoints(ObjOldPt, ObjDirPt);
            if(EqualPoints(TestPoint, ObjNewPt)) {
                return Object;
            }
        }
    }
    return NULL;
}

static BOOL IsLeapAttempted(object *Obj, BOOL AttemptMove, const quad_info *QuadInfo) {
    return Obj->Dir == DIR_DOWN && AttemptMove && QuadInfo->Prop == QUAD_PROP_EDGE;
}

static quad_info FetchNewQuadInfo(object *Obj, BOOL AttemptMove) {
    point FacingPoint = GetFacingPoint(Obj->Pos, Obj->Dir);
    quad_info QuadInfo = GetQuadInfo(FacingPoint);

    return QuadInfo;
}

static quad_info FetchTestQuadInfo(object *Obj, BOOL AttemptMove, quad_info NewQuadInfo) {
    quad_info TestQuadInfo = NewQuadInfo;
    if(IsLeapAttempted(Obj, AttemptMove, &TestQuadInfo)) {
        TestQuadInfo.Point.Y++;
        TestQuadInfo = GetQuadInfo(TestQuadInfo.Point);
    }
    return TestQuadInfo;
}

void GS_NORMAL(void) {
    /*PlayerUpdate*/
    g_IsPauseAttempt |= (VirtButtons[BT_START] == 1);
    if(g_Player.Tick == 0) {
        UpdatePallete();
        if(g_IsPauseAttempt) {
            g_IsPauseAttempt = FALSE;
            PlaySoundEffect(SFX_START_MENU); 
            PushState(GS_START_MENU);
            PushWindowTask(&StartMenu.WindowTask);
        } else { 
            /*PlayerKeyboard*/
            int HasMoveKey = UpdateKeyboard();
            quad_info OldQuadInfo = GetQuadInfo(PtToQuadPt(g_Player.Pos));
            quad_info NewQuadInfo = FetchNewQuadInfo(&g_Player, HasMoveKey);
            quad_info TestQuadInfo = FetchTestQuadInfo(&g_Player, HasMoveKey, NewQuadInfo); 

            object *FacingObject = GetFacingObject(TestQuadInfo); 
            if(FacingObject) {
                NewQuadInfo.Prop = QUAD_PROP_SOLID;
            }

            /*PlayerTestProp*/
            if(VirtButtons[BT_A] == 1) {
                switch(NewQuadInfo.Prop) {
                case QUAD_PROP_NONE:
                    if(g_Player.Tile == ANIM_SEAL) {
                        g_Player.Tile = ANIM_PLAYER;
                        HasMoveKey = 1;
                    }
                    break;
                case QUAD_PROP_MESSAGE: 
                    OpenText(&g_Maps[NewQuadInfo.MapI], NewQuadInfo.Point); 
                    HasMoveKey = 0;
                    break;
                case QUAD_PROP_WATER:
                    if(g_Player.Tile == ANIM_PLAYER) {
                        g_Player.Tile = ANIM_SEAL;
                        HasMoveKey = 1;
                    }
                    break;
                case QUAD_PROP_RED_PC:
                    PlaySoundEffect(SFX_TURN_ON_PC);
                    PlaceTextBox("@RED turned on the PC.", 16);
                    HasTextBox = true;
                    WaitOnFlash();
                    PushState(GS_RED_PC_MAIN);
                    PushWindowTask(&RedPCMenu.WindowTask);
                    PlaceTextBox(RPC_INTRO, 0);
                    HasMoveKey = 0;
                    break;
                case QUAD_PROP_SHELF:
                    if(g_Player.Dir == DIR_UP) {
                        DisplayTextBox("Crammed full of\nPOKéMON books!", 16); 
                        WaitOnFlash();
                        ClearWindow();
                        HasMoveKey = 0;
                    }
                    break;
                case QUAD_PROP_TV:
                    if(g_Player.Dir == DIR_UP) {
                        OpenText(&g_Maps[NewQuadInfo.MapI], NewQuadInfo.Point); 
                    } else {
                        DisplayTextBox("Oops, wrong side.", 16); 
                        WaitOnFlash();
                        ClearWindow();
                    }
                    HasMoveKey = 0;
                    break;
                case QUAD_PROP_MAP:
                    DisplayTextBox("A TOWN MAP.", 16);
                    WaitOnFlash();
                    DisplayWorldMap();
                    break;
                default:
                    if(FacingObject && !FacingObject->IsMoving) {
                        if(FacingObject->Speed == 0) {
                            FacingObject->Tick = 120;
                        }
                        FacingObject->Dir = ReverseDir(g_Player.Dir);
                        UpdateObjects(0);
                        DisplayTextBox(FacingObject->Str, 16);
                        WaitOnFlash();
                        ClearWindow();
                        HasMoveKey = 0;
                    }
                }
            } 
            if(HasMoveKey) {
                MovePlayer(NewQuadInfo, OldQuadInfo);
            }
        }
    }
    UpdateObjects(g_IsPauseAttempt ? 0 : US_RANDOM_MOVE);
}

void GS_START_MENU(void) {
    if(VirtButtons[BT_START] == 1) {
        RemoveWindowTask();
        PopState();
    } else { 
        MoveMenuCursorWrap(&StartMenu);
        switch(GetMenuOptionSelected(&StartMenu, 5)) {
        case 0: /*POKéMON*/
            break;
        case 1: /*ITEM*/
            PlaySoundEffect(SFX_PRESS_AB); 
            g_Inventory = &g_Bag;
            g_InventoryState = IS_ITEM; 
            PushWindowTask(&g_Inventory->WindowTask);
            PushState(GS_INVENTORY);
            break;
        case 2: /*RED*/
            break;
        case 3: /*SAVE*/
            PlaySoundEffect(SFX_PRESS_AB); 
            UpdateSaveSec();
            PlaceSave(g_SaveRect);

            Pause(16);
            DisplayTextBox("Would you like to\nSAVE the game?", 4);
            PushWindowTask(&YesNoMenu.WindowTask);

            ReplaceState(GS_SAVE);
            break;
        case 4: /*OPTIONS*/
            OpenOptions();
            break;
        case 5: /*EXIT*/
            PlaySoundEffect(SFX_PRESS_AB); 
            RemoveWindowTask();
            PopState();
            break;
        }
    }
}

static void GS_RED_PC_UNSELECT(void) {
    DisplayTextBox(DeferedMessage, 0);
    RemoveWindowTask();
    PopState();
}

static void SelectRedPCItem(void) {
    DeferedMessage = g_RedPCSelects[RedPCMenu.SelectI].Normal;
    PushState(GS_RED_PC_UNSELECT);
    PushState(GS_RED_PC_ITEM);
    DisplayTextBox("How many?", 0);
    PushWindowTask(&DisplayWindowTask);
}

void GS_INVENTORY(void) {
    if(VirtButtons[BT_B] == 1 || 
       (VirtButtons[BT_A] == 1 && g_Inventory->ItemSelect == g_Inventory->ItemCount)) {
        /*RemoveSubMenu*/
        RemoveWindowTask();
        PopState();
        PlaySoundEffect(SFX_PRESS_AB); 
    } else {
        /*MoveTextCursor*/
        if(VirtButtons[BT_A] == 1) {
            PlaySoundEffect(SFX_PRESS_AB); 
            g_WindowMap[g_Inventory->Y * 2 + 4][5] = MT_EMPTY_HORZ_ARROW;
            switch(g_InventoryState) {
            case IS_ITEM:
                PushWindowTask(&UseTossMenu.WindowTask);
                PushState(GS_USE_TOSS);
                break;
            default:
                SelectRedPCItem();
            } 
        } else if(VirtButtons[BT_UP] % 10 == 1 && g_Inventory->ItemSelect > 0) {
            g_Inventory->ItemSelect--; 
            if(g_Inventory->Y > 0) {
                g_Inventory->Y--;
            }
            PlaceInventory(g_Inventory); 
        } else if(VirtButtons[BT_DOWN] % 10 == 1 && g_Inventory->ItemSelect < g_Inventory->ItemCount) {
            g_Inventory->ItemSelect++; 
            if(g_Inventory->Y < 2) {
                g_Inventory->Y++;
            }
            PlaceInventory(g_Inventory); 
        } 

        /*MoreItemsWait*/
        static int Tick;
        int InventoryLast = g_Inventory->ItemSelect - g_Inventory->Y + 3; 
        if(InventoryLast < g_Inventory->ItemCount) {
            FlashCursor(Tick++);
        } else {
            Tick = 0;
        }
    }
}

void GS_SAVE(void) {
    MoveMenuCursor(&YesNoMenu);
    switch(GetMenuOptionSelected(&YesNoMenu, 1)) {
    case 0: /*YES*/
        PlaySoundEffect(SFX_PRESS_AB); 
        /*RemoveYesNoSavePrompt*/
        RemoveWindowTask();
        PlaceSave(g_SaveRect);
        PlaceTextBox("Now saving...", 0);

        if(WriteSaveHeader() && WriteSave()) {
            Pause(120);
            PlaySoundEffect(SFX_SAVE);
            DisplayTextBox("@RED saved\nthe game!", 0);
            Pause(10);
        } else {
            Pause(60);
            DisplayTextBox("ERROR:\nWriteSave failed", 0);
            Pause(10);
        } 
        ClearWindowStack();
        PopState();
        break;
    case 1: /*NO*/
        PlaySoundEffect(SFX_PRESS_AB); 
        ClearWindowStack();
        PopState();
        break;
    }
}

void GS_USE_TOSS(void) {
    MoveMenuCursor(&UseTossMenu);
    switch(GetMenuOptionSelected(&UseTossMenu, 2)) {
    case 0: /*USE*/
        PlaySoundEffect(SFX_PRESS_AB);
        item *SelectedItem = GetSelectedItem(g_Inventory);
        switch(SelectedItem->ID) {
        case ITEM_MAP:
            DisplayWorldMap();
            break;
        default:
            DisplayTextBox("ERROR: TODO", 0);
            WaitOnFlash();
        }
        RemoveWindowTask();
        PopState();
        break;
    case 1: /*TOSS*/
        PlaySoundEffect(SFX_PRESS_AB);
        PlaceMenuCursor(&UseTossMenu, MT_EMPTY_HORZ_ARROW);
        PopWindowTask();
        PushWindowTask(&DisplayWindowTask);
        ReplaceState(GS_TOSS);
        break;
    case 2: /*CANCEL*/
        PlaySoundEffect(SFX_PRESS_AB);
        RemoveWindowTask();
        PopState();
        break;
    }
}

void GS_TOSS(void) {
    UpdateDisplayItem(g_Inventory);
    if(VirtButtons[BT_A] == 1) {
        PlaySoundEffect(SFX_PRESS_AB);
        DisplayTextBox("Is it OK to toss\n@ITEM?", 0);
        PushWindowTask(&ConfirmTossMenu.WindowTask);
        ReplaceState(GS_CONFIRM_TOSS);
    } else if(VirtButtons[BT_B] == 1) {
        PlaySoundEffect(SFX_PRESS_AB);
        RemoveWindowTask();
        PopState();
    }
}

void GS_CONFIRM_TOSS(void) {
    MoveMenuCursor(&ConfirmTossMenu);
    switch(GetMenuOptionSelected(&ConfirmTossMenu, 1)) {
    case 0: /*YES*/
        PlaySoundEffect(SFX_PRESS_AB);
        RemoveWindowTask();
        RemoveItem(g_Inventory, DisplayItem.Count); 
        DisplayTextBox("Threw away\n@ITEM.", 0);
        WaitOnFlash();
        RemoveWindowTask();
        PopState();
        break;
    case 1: /*NO*/
        PlaySoundEffect(SFX_PRESS_AB);
        RemoveWindowTask();
        RemoveWindowTask();
        PopState();
        break;
    }
}

static void OpenInventoryInPC(red_pc_select_i SelectI) {
    const red_pc_select *Select = &g_RedPCSelects[SelectI];
    PlaySoundEffect(SFX_PRESS_AB); 
    DeferedMessage = RPC_INTRO;
    PushState(GS_RED_PC_REPEAT_INTRO);
    g_InventoryState = Select->State;
    g_Inventory = Select->Inventory;
    if(g_Inventory->ItemCount == 0) {
        DisplayTextBox(Select->Empty, 0);
        WaitOnFlash();
    } else {
        g_Inventory->ItemSelect = 0;
        g_Inventory->Y = 0;
        DisplayTextBox(Select->Normal, 0);
        WaitOnFlash();
        PushWindowTask(&g_Inventory->WindowTask);
        PushState(GS_INVENTORY);
    } 
}

void GS_RED_PC_MAIN(void) {
    MoveMenuCursor(&RedPCMenu);
    switch(GetMenuOptionSelected(&RedPCMenu, 3)) {
    case 0 ... 2:
        OpenInventoryInPC(RedPCMenu.SelectI);
        break;
    case 3:
        PlaySoundEffect(SFX_TURN_OFF_PC);
        HasTextBox = false;
        RemoveWindowTask();
        PopState();
        break;
    }
}

void GS_RED_PC_TOSS(void) {
    MoveMenuCursor(&ConfirmTossMenu);
    switch(GetMenuOptionSelected(&ConfirmTossMenu, 1)) {
    case 0: /*YES*/
        PlaySoundEffect(SFX_PRESS_AB);
        RemoveWindowTask();
        RemoveItem(&g_RedPC, DisplayItem.Count);
        DisplayTextBox("Threw away\n@ITEM.", 0);
        WaitOnFlash();
        PopState();
        break;
    case 1: /*NO*/
        PlaySoundEffect(SFX_PRESS_AB);
        RemoveWindowTask();
        PopState();
        break;
    }
}

void GS_RED_PC_ITEM(void) { 
    UpdateDisplayItem(g_Inventory);
    if(VirtButtons[BT_A] == 1) {
        /*RedPCDoItemOp*/
        switch(g_InventoryState) {
        case IS_WITHDRAW:
            PlaySoundEffect(SFX_WITHDRAW_DEPOSIT);
            MoveItem(&g_Bag, &g_RedPC, DisplayItem.Count);
            DisplayTextBox("Withdrew\n@ITEM", 0); 
            WaitOnFlash();
            PopState();
            break;
        case IS_DEPOSIT:
            PlaySoundEffect(SFX_WITHDRAW_DEPOSIT);
            MoveItem(&g_RedPC, &g_Bag, DisplayItem.Count);
            DisplayTextBox("@ITEM was\nstored via PC.", 0); 
            WaitOnFlash();
            PopState();
            break;
        case IS_TOSS:
            PlaySoundEffect(SFX_PRESS_AB);
            DisplayTextBox("Is it OK to toss\n@ITEM?", 0);
            PushWindowTask(&ConfirmTossMenu.WindowTask);
            ReplaceState(GS_RED_PC_TOSS);
            break;
        default:
            assert(!"ERROR: Invalid inventory state");
        }
    } else if(VirtButtons[BT_B] == 1) {
        /*RedPCCancelItemOp*/
        PlaySoundEffect(SFX_PRESS_AB);
        PopState();
        ExecuteState();
    }
}

void GS_RED_PC_REPEAT_INTRO(void) {
    DisplayTextBox(DeferedMessage, 0);
    PopState();
}

static BOOL DoesSaveExist(void) {
    return GetFileAttributes("Save") != INVALID_FILE_ATTRIBUTES;
}

static void PlayGame(void) {
    SetPlayerToDefault();

    ReadTileData("Menu", &g_WindowData[2], 200);
    ReadTileData("Sprites", g_SpriteData, 224);

    /*InitMainMenu*/
    if(DoesSaveExist()) {
        PushWindowTask(&ContinueMenu.WindowTask);
        PlaceMenu(&ContinueMenu);
        ReadSaveHeader();
        PushState(GS_CONTINUE_MENU);
    } else {
        PushWindowTask(&MainMenu.WindowTask);
        PushState(GS_MAIN_MENU);
    }

    SetPallete(PAL_DEFAULT);
    PlayMusic(MUS_TITLE);
    
    while(1) {
        BeginFrame();
        ExecuteState();
        EndFrame();
    }
}


void SetDirectoryToShared(void) {
    char Path[MAX_PATH];
    char *Find;

    GetModuleFileName(NULL, Path, MAX_PATH - 16);
    Find = strrchr(Path, '\\');
    if(!Find) return;
    *Find = '\0';
    Find = strrchr(Path, '\\');
    if(!Find) return;
    strcpy(Find, "\\Shared"); 
    SetCurrentDirectory(Path);
}

int WINAPI WinMain(
    HINSTANCE Instance, 
    HINSTANCE Prev, 
    LPSTR CmdLine, 
    int CmdShow
) {
    UNREFERENCED_PARAMETER(Prev);
    UNREFERENCED_PARAMETER(CmdLine);
    UNREFERENCED_PARAMETER(CmdShow);

    srand(time(NULL));
    SetDirectoryToShared();
    CreateGameWindow(Instance);
    PlayGame();
    return 0;
}

/*Includes*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "audio.h"
#include "container.h"
#include "frame.h"
#include "inventory.h"
#include "input.h"
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

static uint8_t g_TrainerX;
static uint8_t g_TrainerY;

/*Math Functions*/
void GS_MAIN_MENU(void);

void GS_NORMAL(void);

void GS_START_MENU(void);
void GS_INVENTORY(void);
void GS_SAVE(void);
void GS_OPTIONS(void);

void GS_USE_TOSS(void);
void GS_TOSS(void);
void GS_CONFIRM_TOSS(void);

void GS_RED_PC_MAIN(void);
void GS_RED_PC_ITEM(void);

void GS_RED_PC_REPEAT_INTRO(void);

void GS_REMOVE_WINDOW_TASK(void);

int GetMenuOptionSelected(menu *Menu, int CancelI) {
    if(VirtButtons[BT_A] == 1) {
        return Menu->SelectI; 
    }
    if(VirtButtons[BT_B] == 1) {
        return CancelI;
    }
    return -1;
}

static void FadeOutMusic(void) {
    int Tick = 60;
    while(Tick-- > 0) {
        MusicSetVolume(Tick / 60.0F);
        NextFrame();
    }
}

static inline int TrainerGetLeft(void) {
    return (7 - g_TrainerWidth) / 2; 
}

static inline int TrainerGetTop(void) {
    return 7 - g_TrainerHeight;
}

static void PlaceTrainer(int BoundX, int BoundY) {
    int TileI = MT_TRAINER;

    g_TrainerX = BoundX;
    g_TrainerY = BoundY;

    int StartX = TrainerGetLeft() + BoundX;
    int StartY = TrainerGetTop() + BoundY; 

    int EndX = StartX + g_TrainerWidth;
    int EndY = StartY + g_TrainerHeight;

    for(int Y = StartY; Y < EndY; Y++) {
        for(int X = StartX; X < EndX; X++) {
            g_WindowMap[Y][X] = TileI++;
        }
    }
}

static void FadeInTrainer(void) {
    SetPalleteTransition(PAL_OAK, 1, 1, 1);
    Pause(8);
    SetPalleteTransition(PAL_OAK, 2, 2, 2);
    Pause(8);
    SetPalleteTransition(PAL_OAK, 3, 3, 3);
    Pause(8);
    g_Bitmap.Colors[1] = g_Palletes[PAL_OAK][2];
    Pause(8);
    g_Bitmap.Colors[1] = g_Palletes[PAL_OAK][1];
    Pause(8);
    g_Bitmap.Colors[2] = g_Palletes[PAL_OAK][2];
    Pause(24);
}

static void FadeOutTrainer(void) {
    SetPalleteTransition(PAL_OAK, 0, 1, 2); 
    Pause(8);
    SetPalleteTransition(PAL_OAK, 0, 0, 1); 
    Pause(8);
    ClearWindow();
    SetPallete(PAL_OAK);
    Pause(48);
}

static void SlideInTrainer(void) {
    g_WindowMapX = BM_WIDTH;
    while(g_WindowMapX) {
        g_WindowMapX -= 8;
        NextFrame();
    }
}

static void PlaySyncCry(cry_i Cry) {
    PlayCry(Cry);
    while(IsSoundPlaying()) {
        NextFrame();
    }
}

static void ShiftTrainerRowsRight(int StartX, int StartY, int EndY) {
    int EndX = StartX + 7;
    for(int Y = StartY; Y < EndY; Y++) {
        for(int X = EndX; X-- > StartX; ) {
            g_WindowMap[Y][X + 1] = g_WindowMap[Y][X];
        }
        g_WindowMap[Y][StartX] = MT_BLANK;
    } 
    Pause(1);
}
static void ShiftTrainerRowsLeft(int StartX, int StartY, int EndY) {
    int EndX = StartX + 7;
    for(int Y = StartY; Y < EndY; Y++) {
        for(int X = StartX; X < EndX; X++) {
            g_WindowMap[Y][X - 1] = g_WindowMap[Y][X];
        }
        g_WindowMap[Y][EndX - 1] = MT_BLANK;
    } 
    Pause(1);
}

static void ShiftTrainerRight(void) {
    for(int X = 6; X < 12; X++) {
        ShiftTrainerRowsRight(X, 4, 6);
        ShiftTrainerRowsRight(X, 6, 11); 
    }
}

static void ShiftTrainerLeft(void) {
    for(int X = 12; X-- > 6; ) {
        ShiftTrainerRowsLeft(X, 6, 11); 
        ShiftTrainerRowsLeft(X, 4, 6);
    }
}

static void PresentOak(void) {
    FadeOutMusic();
    PlayMusic(MUS_OAK_SPEECH);
    Pause(30);
    ClearPallete();
    ReadTrainerTileData("Oak");
    PlaceTrainer(6, 4);
    FadeInTrainer();
    DisplayTextBox(
        "Hello there! Welcome to the world of POKÈMON!\f"
        "My name is OAK! People call me the POKÈMON PROF!",
        0
    );
    WaitOnFlash();
    FadeOutTrainer();
}

static void PresentNidorino(void) {
    ReadHorzFlipTrainerTileData("Nidorino");
    PlaceTrainer(6, 4);
    SlideInTrainer();
    DisplayTextBox(
        "This world is inhabited by creatures called POKÈMON!",
        0
    );
    PlaySyncCry(CRY_NIDORINA);
    WaitOnFlash();
    DisplayTextBox(
        "For some people, POKÈMON are pets. Others use them for fights.\f"
        "Myself...\f"
        "I study POKÈMON as a profession.",
        0
    );
    WaitOnFlash();
    FadeOutTrainer();
}

static void PlaceNameLine(void) {
    g_WindowMap[3][10] = MT_UPPER_SCORE;
    for(int X = 11; X < 17; X++) {
        g_WindowMap[3][X] = MT_MIDDLE_SCORE;
    }
}

static void PlaceLowercaseLetters(void) {
    PlaceText(2, 5, "a b c d e f g h i"); 
    PlaceText(2, 7, "j k l m n o p q r");
    PlaceText(2, 9, "s t u v w x y z  ");
}

static void PlaceCapitalLetters(void) {
    PlaceText(2, 5, "A B C D E F G H I"); 
    PlaceText(2, 7, "J K L M N O P Q R");
    PlaceText(2, 9, "S T U V W X Y Z  ");
}

static void TileToStr(char *Str, int StartX, int EndX, int TileY) {
    for(int TileX = StartX; TileX < EndX; TileX++) {
        *Str++ = TileToChar(g_WindowMap[TileY][TileX]);
    }
    *Str = '\0';
}

static void SwitchCase(int CursorX, int CursorY) {
    if(g_WindowMap[CursorY][CursorX + 1] == MT_LOWERCASE_L) {
        PlaceText(2, 15, "UPPER CASE");
        PlaceLowercaseLetters();
    } else {
        PlaceText(2, 15, "lower case");
        PlaceCapitalLetters();
    }
}

static void PromptName(char *Name) {
start_prompt:
    int CursorX = 1;
    int CursorY = 5;
    int NameX = 10;

    ClearWindow();
    Pause(30);

    PlaceBox((rect) {0, 4, 20, 15});
    PlaceNameLine();
    PlaceText(0, 1, "YOUR NAME?");
    PlaceCapitalLetters();
    PlaceText(2, 11,  "* ( ) : ; [ ] \1 \2");
    PlaceText(2, 13, "- ? ! ^ | / . , \3");
    PlaceText(2, 15, "lower case");

    while(1) {
        g_WindowMap[CursorY][CursorX] = MT_BLANK;
        if(VirtButtons[BT_UP] == 1) {
            if(CursorY == 5) {
                CursorX = 1;
                CursorY = 15;
            } else {
                CursorY -= 2;
            }
        } else if(VirtButtons[BT_LEFT] == 1) {
            if(CursorY != 15) {
                if(CursorX == 1) {
                    CursorX = 17;
                } else {
                    CursorX -= 2;
                }
            }
        } else if(VirtButtons[BT_DOWN] == 1) {
            if(CursorY == 15) {
                CursorY = 5;
            } else {
                if(CursorY == 13) {
                    CursorX = 1;
                }
                CursorY += 2;
            }
        } else if(VirtButtons[BT_RIGHT] == 1) {
            if(CursorY != 15) {
                if(CursorX == 17) {
                    CursorX = 1;
                } else {
                    CursorX += 2;
                }
            }
        } else if(VirtButtons[BT_A] == 1) {
            if(CursorY == 15) {
                SwitchCase(CursorX, CursorY);
            } else if(CursorX == 17 && CursorY == 13) {
                if(NameX == 10) {
                    goto start_prompt;
                } 
                TileToStr(Name, 10, NameX, 2);
                ClearWindow();
                Pause(30);
                return;
            } else {
                PlaySoundEffect(SFX_PRESS_AB);
                g_WindowMap[2][NameX] = g_WindowMap[CursorY][CursorX + 1];
                if(NameX != 16) {
                    g_WindowMap[3][NameX] = MT_MIDDLE_SCORE;
                    g_WindowMap[3][NameX + 1] = MT_UPPER_SCORE;
                }
                NameX++;
            }
            if(NameX == 17) {
                CursorX = 17;
                CursorY = 13;
            }
        } else if(VirtButtons[BT_B] == 1) {
            if(NameX != 10) {
                if(NameX != 17) {
                    g_WindowMap[3][NameX] = MT_MIDDLE_SCORE; 
                    g_WindowMap[3][NameX - 1] = MT_UPPER_SCORE; 
                }
                g_WindowMap[2][NameX - 1] = MT_BLANK;
                NameX--;
            }
        }
        g_WindowMap[CursorY][CursorX] = MT_FULL_HORZ_ARROW;
        NextFrame();
    }
}

typedef struct name_select {
    char *Name;
    const char *WhoseName;
    const char *PresetNames[3];
} name_select;

static void UsePresetName(name_select *Select, menu *Menu) {
    strcpy(Select->Name, Select->PresetNames[Menu->SelectI - 1]);
    ClearWindowRect(Menu->Rect);
    ShiftTrainerLeft();
}

static void PresentNameSelect(name_select *Select) {
    ShiftTrainerRight();

    char MenuText[64];
    snprintf(
        MenuText, 
        _countof(MenuText), 
        "NEW NAME\n%s\n%s\n%s", 
        Select->PresetNames[0], 
        Select->PresetNames[1],
        Select->PresetNames[2] 
    ); 

    menu Menu = {
        .WindowTask.Type = TT_MENU,
        .Rect = {0, 0, 11, 12},
        .Text = MenuText,
        .TextY = 2,
        .EndI = 4
    };

    PlaceMenu(&Menu);
    PlaceText(3, 0, "NAME"); 
    NextFrame();

    Select->Name[0] = '\0';
    while(!Select->Name[0]) {
        MoveMenuCursor(&Menu);
        switch(GetMenuOptionSelected(&Menu, -1)) {
        case 0:
            PlaySoundEffect(SFX_PRESS_AB);
            PromptName(Select->Name);
            PlaceTrainer(6, 4);
            break;
        case 1 ... 2:
            PlaySoundEffect(SFX_PRESS_AB);
            UsePresetName(Select, &Menu);
            break;
        }
        NextFrame();
    }
}

static void NameRed(void) {
    ReadTrainerTileData("Red");
    PlaceTrainer(6, 4);
    SlideInTrainer();
    DisplayTextBox(
        "First, what is your name?",
        0
    );
    WaitOnFlash();

    name_select Select = {
        .Name = g_Name,
        .WhoseName = "YOUR NAME", 
        .PresetNames = {"RED", "ASH", "JACK"}
    };
    PresentNameSelect(&Select);

    DisplayTextBox(
        "Right! So your name is @RED!",
        0
    );
    WaitOnFlash();
    FadeOutTrainer();
}

static void NameBlue(void) {
    ReadTrainerTileData("Blue");
    PlaceTrainer(6, 4);
    FadeInTrainer();
    DisplayTextBox(
        "This is my grand-son. He' been your rival since you were a baby.\f"
        "...Erm, what is his name again?",
        0
    );
    WaitOnFlash();

    name_select Select = {
        .Name = g_Rival,
        .WhoseName = "RIVAL' NAME", 
        .PresetNames = {"BLUE", "GARY", "JOHN"}
    };
    PresentNameSelect(&Select);
    DisplayTextBox(
        "That' right! I remember now! His name is @BLUE!",
        0
    );
    WaitOnFlash();
    FadeOutTrainer();
}

static void ToWorldTransition(void) {
    PlaySoundEffect(SFX_SHRINK);
    ReadTileData("Transition", &g_WindowData[MT_TRANSITION], 32);

    g_WindowMap[4][7] = MT_BLANK;
    Pause(1);

    g_WindowMap[4][8] = MT_BLANK;
    g_WindowMap[5][7] = MT_BLANK;
    g_WindowMap[6][7] = MT_BLANK;
    g_WindowMap[5][8] = MT_TRANSITION;
    g_WindowMap[7][7] = MT_TRANSITION + 1;
    g_WindowMap[8][7] = MT_TRANSITION + 2;
    g_WindowMap[10][7] = MT_TRANSITION + 3;
    Pause(1);

    g_WindowMap[4][9] = MT_BLANK;
    g_WindowMap[5][9] = MT_TRANSITION + 4;
    g_WindowMap[6][8] = MT_TRANSITION + 5;
    g_WindowMap[6][9] = MT_TRANSITION + 7;
    g_WindowMap[7][8] = MT_TRANSITION + 6;
    g_WindowMap[8][7] = MT_TRANSITION + 2;
    g_WindowMap[8][8] = MT_TRANSITION + 8;
    g_WindowMap[9][8] = MT_TRANSITION + 9;
    g_WindowMap[10][8] = MT_TRANSITION + 10;
    Pause(1);

    g_WindowMap[5][10] = MT_BLANK;
    g_WindowMap[6][10] = MT_BLANK;
    g_WindowMap[7][9] = MT_TRANSITION + 11;
    g_WindowMap[7][10] = MT_TRANSITION + 16;
    g_WindowMap[8][9] = MT_TRANSITION + 12;
    g_WindowMap[9][9] = MT_TRANSITION + 14;
    g_WindowMap[10][9] = MT_TRANSITION + 15;
    Pause(1);

    g_WindowMap[9][10] = MT_BLANK;
    g_WindowMap[8][10] = MT_TRANSITION + 13;
    g_WindowMap[10][10] = MT_TRANSITION + 17;
    Pause(30);

    g_WindowMap[5][8] = MT_BLANK;
    g_WindowMap[7][7] = MT_BLANK;
    g_WindowMap[8][7] = MT_BLANK;
    g_WindowMap[10][7] = MT_BLANK;
    Pause(1);
    
    g_WindowMap[5][8] = MT_BLANK;
    g_WindowMap[5][9] = MT_BLANK;
    g_WindowMap[10][8] = MT_BLANK;
    g_WindowMap[6][8] = MT_TRANSITION;
    g_WindowMap[6][9] = MT_TRANSITION + 4;
    g_WindowMap[7][8] = MT_TRANSITION + 18;
    g_WindowMap[8][8] = MT_TRANSITION + 19;
    g_WindowMap[9][8] = MT_TRANSITION + 20;
    Pause(1);

    g_WindowMap[7][10] = MT_BLANK;
    g_WindowMap[10][9] = MT_BLANK;
    g_WindowMap[7][9] = MT_TRANSITION + 21;
    g_WindowMap[8][9] = MT_TRANSITION + 22;
    g_WindowMap[9][9] = MT_TRANSITION + 23;
    Pause(1);

    g_WindowMap[8][10] = MT_BLANK;
    g_WindowMap[10][10] = MT_BLANK;
    Pause(30);
    
    ClearWindowRect((rect) {0, 0, 20, 12});
    PushBasicPlayerSprite();
    SetPalleteTransition(PAL_OAK, 1, 1, 1);
    Pause(8);
    SetPalleteTransition(PAL_OAK, 2, 2, 2);
    Pause(8);
    g_SpriteCount = 0;
    g_MusicI = MUS_INVALID;
    SwitchMusic();
    ClearWindow();
    Pause(60);
}

static void Outro(void) {
    ReadTrainerTileData("Red");
    PlaceTrainer(6, 4);
    FadeInTrainer();
    DisplayTextBox(
        "@RED!\f"
        "Your very own POKÈMON legend is about to unfold\f"
        "A world of dreams and adventures with POKÈMON awaits! Let' go!",
        0
    );
    ToWorldTransition();
}

static void StartWorld(void) {
    ReadMap(0, "PlayerHouse2");
    PlaceViewMap(0);
    UpdatePallete();
    PushBasicPlayerSprite();
    Pause(15);
    g_MusicI = MUS_PALLETE_TOWN;
    SwitchMusic();
    g_Player.Dir = DIR_UP;
    g_TileMapX = 0;
    g_TileMapY = 0;
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

static void ContinueGame(void) {
    ClearWindowStack();
    memset(g_WindowMap, MT_BLANK, sizeof(g_WindowMap));
    ReadSave();
    SwitchMusic();
    Pause(30);
    ClearWindow();
    PlaceViewMap(0);
    UpdatePallete();
    ActivateWorld();
    StartSaveCounter();
    ReplaceState(GS_NORMAL);
} 

static void ClearContinuePrompt(void) {
    ResetSaveSec();
    PopWindowTask();
    ClearWindow();
    Pause(30);
    ExecuteAllWindowTasks();
}

static void OpenContinuePrompt(void) {
    BOOL ContinuePromptOpen = TRUE;
    PlaySoundEffect(SFX_PRESS_AB); 
    PushWindowTask(&g_ContinueSaveRect.WindowTask);
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

static void LoadDoorMap(point DoorPoint) {
    quad_info QuadInfo = GetQuadInfo(DoorPoint); 
    map *OldMap = &g_Maps[QuadInfo.MapI]; 

    for(int I = 0; I < OldMap->DoorCount; I++) {
        const door *Door = &OldMap->Doors[I];
        if(EqualPoints(Door->Pos, QuadInfo.Point)) {
            /*DoorToDoor*/
            ReadMap(!QuadInfo.MapI, Door->Path);
            g_MapI = !QuadInfo.MapI;
            UpdatePallete();
            g_Player.Pos = QuadPtToPt(Door->DstPos);
            GetLoadedPoint(!QuadInfo.MapI, Door->Path);

            /*CompleteTransition*/
            g_TileMapX = 0;
            g_TileMapY = 0;

            memset(OldMap, 0, sizeof(*OldMap));
            PlaceViewMap(0);
            break;
        } 
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

static const char *GetTextFromPos(map *Map, point NewPoint) {
    for(int I = 0; I < Map->TextCount; I++) {
        text *Text = &Map->Texts[I];
        if(EqualPoints(Text->Pos, NewPoint)) {
            return Text->Str;
        } 
    } 
    return "ERROR: NO TEXT";
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
            int AttemptLeap = 0;

            /*PlayerKeyboard*/
            int HasMoveKey = 1;
            if(VirtButtons[BT_UP]) {
                g_Player.Dir = DIR_UP;
            } else if(VirtButtons[BT_LEFT]) {
                g_Player.Dir = DIR_LEFT;
            } else if(VirtButtons[BT_DOWN]) {
                g_Player.Dir = DIR_DOWN;
                AttemptLeap = 1;
            } else if(VirtButtons[BT_RIGHT]) {
                g_Player.Dir = DIR_RIGHT;
            } else {
                HasMoveKey = 0;
            }

            point NewPoint = GetFacingPoint(g_Player.Pos, g_Player.Dir);

            /*FetchQuadProp*/
            quad_info OldQuadInfo = GetQuadInfo(PtToQuadPt(g_Player.Pos));
            quad_info NewQuadInfo = GetQuadInfo(NewPoint);
            NewPoint = NewQuadInfo.Point;

            /*FetchObject*/
            AttemptLeap &= !!(NewQuadInfo.Prop == QUAD_PROP_EDGE);
            point TestPoint = NewPoint; 
            if(AttemptLeap) {
                TestPoint.Y++;
            }

            object *FacingObject = NULL; 
            for(int ObjI = 0; ObjI < g_Maps[g_MapI].ObjectCount; ObjI++) {
                object *Object = &g_Maps[g_MapI].Objects[ObjI];
                point ObjOldPt = PtToQuadPt(Object->Pos);
                if(Object->Tick > 0) {
                    point ObjDirPt = NextPoints[Object->Dir];
                    point ObjNewPt = AddPoints(ObjOldPt, ObjDirPt);
                    if(EqualPoints(TestPoint, ObjOldPt) || 
                       EqualPoints(TestPoint, ObjNewPt)) {
                        NewQuadInfo.Prop = QUAD_PROP_SOLID;
                        FacingObject = Object;
                        break;
                    }
                } else if(EqualPoints(TestPoint, ObjOldPt)) {
                    NewQuadInfo.Prop = AttemptLeap ? QUAD_PROP_SOLID : QUAD_PROP_MESSAGE;
                    FacingObject = Object;
                    break;
                }
            }

            /*PlayerTestProp*/
            if(VirtButtons[BT_A] == 1) {
                int IsTV = NewQuadInfo.Prop == QUAD_PROP_TV;
                int IsShelf = NewQuadInfo.Prop == QUAD_PROP_SHELF && g_Player.Dir == DIR_UP;
                int IsComputer = NewQuadInfo.Prop == QUAD_PROP_RED_PC && g_Player.Dir == DIR_UP;
                if(NewQuadInfo.Prop == QUAD_PROP_MESSAGE || IsTV || IsShelf || IsComputer) {
                    /*GetActiveText*/
                    if(IsShelf) {
                        DisplayTextBox("Crammed full of\nPOKÈMON books!", 16); 
                        WaitOnFlash();
                        ClearWindow();
                    } else if(IsTV && g_Player.Dir != DIR_UP) {
                        DisplayTextBox("Oops, wrong side.", 16); 
                        WaitOnFlash();
                        ClearWindow();
                    } else if(FacingObject) {
                        if(FacingObject->Speed == 0) {
                            FacingObject->Tick = 120;
                        }
                        FacingObject->Dir = ReverseDir(g_Player.Dir);
                        UpdateObjects(0);
                        DisplayTextBox(FacingObject->Str, 16);
                        WaitOnFlash();
                        ClearWindow();
                    } else if(IsComputer) {
                        PlaySoundEffect(SFX_TURN_ON_PC);
                        PlaceTextBox("@RED turned on the PC.", 16);
                        HasTextBox = true;
                        WaitOnFlash();
                        PushState(GS_RED_PC_MAIN);
                        PushWindowTask(&RedPCMenu.WindowTask);
                        PlaceTextBox(RPC_INTRO, 0);
                    } else {
                        map *Map = &g_Maps[NewQuadInfo.MapI];
                        const char *Str = GetTextFromPos(Map, NewPoint);
                        DisplayTextBox(Str, 16);
                        WaitOnFlash();
                        ClearWindow();
                    }
                } else if(NewQuadInfo.Prop == QUAD_PROP_WATER) {
                    if(g_Player.Tile == ANIM_PLAYER) {
                        g_Player.Tile = ANIM_SEAL;
                        MovePlayer(NewQuadInfo, OldQuadInfo);
                    }
                } else if(g_Player.Tile == ANIM_SEAL && 
                          NewQuadInfo.Prop == QUAD_PROP_NONE) {
                    g_Player.Tile = ANIM_PLAYER;
                    MovePlayer(NewQuadInfo, OldQuadInfo);
                }
            } else if(HasMoveKey) {
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
        case 0: /*POKÈMON*/
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

static void SelectRedPCItem(void) {
    DeferedMessage = g_RedPCSelects[RedPCMenu.SelectI].Normal;
    PushState(GS_REMOVE_WINDOW_TASK);
    PushState(GS_RED_PC_REPEAT_INTRO);
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
        ReplaceState(GS_NORMAL);
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
        DisplayTextBox("ERROR: TODO", 0);
        WaitOnFlash();
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

void GS_REMOVE_WINDOW_TASK(void) {
    RemoveWindowTask();
    PopState();
    ExecuteState();
}

static BOOL DoesSaveExist(void) {
    return GetFileAttributes("Save") != INVALID_FILE_ATTRIBUTES;
}

static void PlayGame(void) {
    SetPlayerToDefault();

    ReadTileData("Menu", &g_WindowData[2], 200);
    ReadTileData("SpriteData", g_SpriteData, 255);

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

    memcpy(g_Bitmap.Colors, g_Palletes[0], sizeof(g_Palletes[0]));
    PlayMusic(MUS_TITLE);
    
    while(1) {
        BeginFrame();
        ExecuteState();
        EndFrame();
    }
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
    SetCurrentDirectory("../Shared");
    CreateGameWindow(Instance);
    PlayGame();
    return 0;
}

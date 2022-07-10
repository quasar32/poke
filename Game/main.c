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
#include "read_buffer.h"
#include "pallete.h"
#include "window_map.h"
#include "world.h"
#include "write_buffer.h"
#include "read.h"
#include "save.h"
#include "state.h"
#include "str.h"
#include "win32.h"

/*Macro Strings*/
#define RPC_INTRO "What do you want to do?"

/*Globals*/
static const rect SaveRect = {4, 0, 20, 10};

static BOOL g_IsPauseAttempt;

static int MusicTick;

static world World;

static char g_Name[8];

static int64_t StartCounter;

void PopWindowState(void) {
    RemoveWindowTask();
    PopState();
}

/*Math Functions*/
static int ReverseDir(int Dir) {
    return (Dir + 2) % 4;
}

static void BeginFrame(void);
static void EndFrame(void);

static void NextFrame(void);

static void NextFrame(void) {
    EndFrame();
    BeginFrame();
}

void SwitchMusic(void) {
    MusicTick = 60;
}

void GS_MAIN_MENU(void);

void GS_NORMAL(void);
void GS_LEAPING(void);

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

static int IsPressABWithSound(void) {
    int ABIsPressed = VirtButtons[BT_A] == 1 || VirtButtons[BT_B] == 1;
    if(ABIsPressed) {
        PlaySoundEffect(SFX_PRESS_AB);
    }
    return ABIsPressed;
}


void Pause(int Delay) {
    while(Delay-- > 0) {
        NextFrame();
    }
}

int FlashCursor(int Tick) {
    g_WindowMap[16][18] = Tick / 30 % 2 ? MT_BLANK : MT_FULL_VERT_ARROW;
    return Tick + 1;
}

void WaitOnFlash(void) {
    int Tick = 0;
    while(!IsPressABWithSound()) {
        Tick = FlashCursor(Tick);
        NextFrame();
    }
    g_WindowMap[16][18] = MT_BLANK;
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

static const char *GetNewRestoreText(const char **Str) {
    const char *Restore = NULL;
    Restore = StrMovePastSpan(*Str, "%ITEM");
    if(Restore) {
        *Str = ItemNames[DisplayItem.ID];
        goto out;
    }
    Restore = StrMovePastSpan(*Str, "%NAME");
    if(Restore) {
        *Str = g_Name;
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
    for(I = 0; Str[I] && !isspace(Str[I]); I++); 
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

BOOL GetCurText(LPCSTR *Str, LPCSTR *Restore) {
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
            Pause((int[]) {0, 2, 3}[Options.E[OPT_TEXT_SPEED].I]);
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

static void UpdateLeaping(void) {
    if(GetState() == GS_LEAPING) {
        /*PlayerUpdateJumpingAnimation*/
        const static uint8_t PlayerDeltas[] = {0, 4, 6, 8, 9, 10, 11, 12};
        uint8_t HalfTick = g_Player.Tick / 2;
        uint8_t DeltaI = HalfTick < 8 ? HalfTick: 15 - HalfTick;
        for(int I = 0; I < 4; I++) {
            g_Sprites[I].Y -= PlayerDeltas[DeltaI];
        }

        /*CreateShadowQuad*/
        if(HalfTick) {
            sprite *ShadowQuad  = &g_Sprites[g_SpriteCount]; 
            ShadowQuad[0] = (sprite) {72, 72, 255, 0};
            ShadowQuad[1] = (sprite) {80, 72, 255, SPR_HORZ_FLAG};
            ShadowQuad[2] = (sprite) {72, 80, 255, SPR_VERT_FLAG};
            ShadowQuad[3] = (sprite) {80, 80, 255, SPR_HORZ_FLAG | SPR_VERT_FLAG};
            g_SpriteCount += 4;
        } else {
            ReplaceState(GS_NORMAL);
        }
    }
}

static point GetObjectSpritePos(object *Object, int MapI) {
    point QuadPt = {
        Object->Pos.X - g_Player.Pos.X + 72,
        Object->Pos.Y - g_Player.Pos.Y + 72
    };

    if(World.MapI != MapI) {
        switch(GetMapDir(World.Maps, World.MapI)) {
        case DIR_UP:
            QuadPt.Y -= World.Maps[MapI].Height * 16;
            break;
        case DIR_LEFT:
            QuadPt.X -= World.Maps[MapI].Width * 16;
            break;
        case DIR_DOWN:
            QuadPt.Y += World.Maps[World.MapI].Height * 16;
            break;
        case DIR_RIGHT:
            QuadPt.X += World.Maps[World.MapI].Width * 16;
            break;
        }
    }

    return QuadPt;
}

static void PushQuadSprite(object *Object, point Point) {
    if(g_SpriteCount < _countof(g_Sprites)) {
        sprite *SpriteQuad = &g_Sprites[g_SpriteCount];
        UpdateAnimation(Object, SpriteQuad, Point);
        g_SpriteCount += 4;
    }
}

static void PushNPCSprite(int MapI, object *Object) {
    point QuadPt = GetObjectSpritePos(Object, MapI);
    if(ObjectInUpdateBounds(QuadPt)) {
        PushQuadSprite(Object, QuadPt);
    }
}

static void PushPlayerSprite(void) {
    PushQuadSprite(&g_Player, (point) {72, 72});
}

static void UpdateStaticNPC(object *Object) {
    if(Object->Tick > 0) {
        Object->Tick--; 
    } else {
        Object->Dir = Object->StartingDir;
    }
}

static void UpdateObjects(BOOL IsObjectActive) {
    g_SpriteCount = 0;
    UpdatePlayerMovement();
    PushPlayerSprite();
    UpdateLeaping(); 
    for(int MapI = 0; MapI < (int) _countof(World.Maps); MapI++) {
        for(int ObjI = 0; ObjI < World.Maps[MapI].ObjectCount; ObjI++) {
            object *Object = &World.Maps[MapI].Objects[ObjI];

            if(IsObjectActive) {
                if(Object->Speed == 0) {
                    UpdateStaticNPC(Object);
                } else if(Object->Tick > 0) {
                    MoveEntity(Object);
                } else {
                    RandomMove(&World.Maps[MapI], Object);
                }
            }

            PushNPCSprite(MapI, Object);
        }
    }
}

static void UpdatePallete(void) {
    SetPallete(World.Maps[World.MapI].PalleteI);
}

void FadeOutMusic(void) {
    int Tick = 60;
    while(Tick-- > 0) {
        MusicSetVolume(Tick / 60.0F);
        NextFrame();
    }
}

static inline int TrainerGetLeft(void) {
    return (8 - g_TrainerWidth) / 2; 
}

static inline int TrainerGetTop(void) {
    return 7 - g_TrainerHeight;
}

static void PlaceTrainer(int BoundX, int BoundY) {
    int TileI = MT_TRAINER;

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

static void FadeInOak(void) {
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
    } 
    Pause(1);
}
static void ShiftTrainerRowsLeft(int StartX, int StartY, int EndY) {
    int EndX = StartX + 7;
    for(int Y = StartY; Y < EndY; Y++) {
        for(int X = StartX; X < EndX; X++) {
            g_WindowMap[Y][X - 1] = g_WindowMap[Y][X];
        }
    } 
    Pause(1);
}

static void ShiftRedRight(void) {
    for(int X = 6; X < 12; X++) {
        ShiftTrainerRowsRight(X, 4, 6);
        ShiftTrainerRowsRight(X, 6, 11); 
    }
}

static void ShiftRedLeft(void) {
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
    FadeInOak();
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
    PlaceTrainer(5, 4);
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
    PlaceText((point) {2,  5}, "a b c d e f g h i"); 
    PlaceText((point) {2,  7}, "j k l m n o p q r");
    PlaceText((point) {2,  9}, "s t u v w x y z  ");
}

static void PlaceCapitalLetters(void) {
    PlaceText((point) {2,  5}, "A B C D E F G H I"); 
    PlaceText((point) {2,  7}, "J K L M N O P Q R");
    PlaceText((point) {2,  9}, "S T U V W X Y Z  ");
}

static void TileToStr(char *Str, int StartX, int EndX, int TileY) {
    for(int TileX = StartX; TileX < EndX; TileX++) {
        *Str++ = TileToChar(g_WindowMap[TileY][TileX]);
    }
    *Str = '\0';
}

static void SwitchCase(int CursorX, int CursorY) {
    if(g_WindowMap[CursorY][CursorX + 1] == MT_LOWERCASE_L) {
        PlaceText((point) {2, 15}, "UPPER CASE");
        PlaceLowercaseLetters();
    } else {
        PlaceText((point) {2, 15}, "lower case");
        PlaceCapitalLetters();
    }
}

static void PromptName(void) {
start_prompt:
    int CursorX = 1;
    int CursorY = 5;
    int NameX = 10;

    ClearWindow();
    Pause(30);

    PlaceBox((rect) {0, 4, 20, 15});
    PlaceNameLine();
    PlaceText((point) {0,  1}, "YOUR NAME?");
    PlaceCapitalLetters();
    PlaceText((point) {2,  11}, "* ( ) : ; [ ] \1 \2");
    PlaceText((point) {2,  13}, "- ? ! ^ | / . , \3");
    PlaceText((point) {2,  15}, "lower case");

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
                TileToStr(g_Name, 10, NameX, 2);
                ClearWindow();
                Pause(30);
                return;
            } else {
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

static void PresentRed(void) {
    ReadTrainerTileData("Red");
    PlaceTrainer(5, 4);
    SlideInTrainer();
    DisplayTextBox(
        "First, what is your name?",
        0
    );
    WaitOnFlash();
    ShiftRedRight();

    menu RedMenu = {
        .WindowTask.Type = TT_MENU,
        .Rect = {0, 0, 11, 12},
        .Text = "NEW NAME\nRED\nASH\nJACK",
        .TextY = 2,
        .EndI = 4
    };
    PlaceMenu(&RedMenu);
    PlaceText((point) {3, 0}, "NAME"); 
    NextFrame();

    g_Name[0] = '\0';
    while(!g_Name[0]) {
        MoveMenuCursor(&RedMenu);
        switch(GetMenuOptionSelected(&RedMenu, -1)) {
        case 0:
            PromptName();
            PlaceTrainer(5, 4);
            break;
        case 1:
            memcpy(g_Name, "RED", sizeof("RED"));
            break;
        case 2:
            memcpy(g_Name, "ASH", sizeof("ASH"));
            break;
        case 3:
            memcpy(g_Name, "JACK", sizeof("JACK"));
            break;
        }
        NextFrame();
    }
    if(RedMenu.SelectI != 0) {
        ClearWindowRect(RedMenu.Rect);
        ShiftRedLeft();
    }
    DisplayTextBox(
        "Right! So your name is %NAME!",
        0
    );
    WaitOnFlash();
}

void StartWorld(void) {
    StartSaveSec = 0;
    StartCounter = QueryPerfCounter();
    ClearWindow();
    ReadOverworldMap(&World, 0, (point) {0, 2});
    point Load = World.Maps[World.MapI].Loaded;
    World.MusicI = OverworldMusic[Load.Y][Load.X];
    SwitchMusic();
    PlaceViewMap(&World, 0);
    UpdatePallete();
    g_TileMapX = 0;
    g_TileMapY = 0;
    ActivateWorld();
    ReplaceState(GS_NORMAL);
}

static void StartGame(void) {
    PlaySoundEffect(SFX_PRESS_AB); 
    ClearWindowStack();
    
    PresentOak();
    PresentNidorino();
    PresentRed();

    StartWorld();
}

static void ContinueGame(void) {
    ClearWindowStack();
    ReadSave(&World);
    ReplaceState(GS_NORMAL);
    SwitchMusic();
    Pause(30);
    PlaceViewMap(&World, 0);
    UpdatePallete();
    ActivateWorld();
    StartCounter = QueryPerfCounter();
} 

static void ClearContinuePrompt(void) {
    PopWindowTask();
    ClearWindow();
    Pause(30);
    ExecuteAllWindowTasks();
}

static void OpenContinuePrompt(void) {
    BOOL ContinuePromptOpen = TRUE;
    PlaySoundEffect(SFX_PRESS_AB); 
    PushWindowTask(&ContinueSaveRect.WindowTask);
    memset(g_TileMap, 255, sizeof(g_TileMap));
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

void PushOptionsMenu(void) {
    PlaySoundEffect(SFX_PRESS_AB); 
    PushWindowTask(&Options.WindowTask);
    PushState(GS_OPTIONS);
}

void GS_MAIN_MENU(void) {
    MoveMenuCursor(&MainMenu);
    switch(GetMenuOptionSelected(&MainMenu, -1)) {
    case 0: /*NEW GAME*/
        StartGame();
        break;
    case 1: /*OPTIONS*/
        PushOptionsMenu();
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
        PushOptionsMenu();
        break;
    } 
}

static void LoadDoorMap(point DoorPoint) {
    quad_info QuadInfo = GetQuadInfo(&World, DoorPoint); 
    map *OldMap = &World.Maps[QuadInfo.MapI]; 

    for(int I = 0; I < OldMap->DoorCount; I++) {
        const door *Door = &OldMap->Doors[I];
        if(EqualPoints(Door->Pos, QuadInfo.Point)) {
            /*DoorToDoor*/
            ReadMap(&World, !QuadInfo.MapI, Door->Path);
            World.MapI = !QuadInfo.MapI;
            UpdatePallete();
            g_Player.Pos = QuadPtToPt(Door->DstPos);
            GetLoadedPoint(&World, !QuadInfo.MapI, Door->Path);

            /*CompleteTransition*/
            g_TileMapX = 0;
            g_TileMapY = 0;

            memset(OldMap, 0, sizeof(*OldMap));
            PlaceViewMap(&World, 0);
            break;
        } 
    }
}

void TranslateQuadRectToTiles(point NewPoint) {
    point TileMin = {0};
    rect QuadRect = {0};

    switch(g_Player.Dir) {
    case DIR_UP:
        TileMin.X = (g_TileMapX / 8) & 31;
        TileMin.Y = (g_TileMapY / 8 + 30) & 31;

        QuadRect = (rect) {
            .Left = NewPoint.X - 4,
            .Top = NewPoint.Y - 4, 
            .Right = NewPoint.X + 6,
            .Bottom = NewPoint.Y - 3
        };
        break;
    case DIR_LEFT:
        TileMin.X = (g_TileMapX / 8 + 30) & 31;
        TileMin.Y = (g_TileMapY / 8) & 31;

        QuadRect = (rect) {
            .Left = NewPoint.X - 4,
            .Top = NewPoint.Y - 4,
            .Right = NewPoint.X - 3,
            .Bottom = NewPoint.Y + 5
        };
        break;
    case DIR_DOWN:
        TileMin.X = (g_TileMapX / 8) & 31;
        TileMin.Y = (g_TileMapY / 8 + 18) & 31;

        QuadRect = (rect) {
            .Left = NewPoint.X - 4,
            .Top = NewPoint.Y + 4,
            .Right = NewPoint.X + 6,
            .Bottom = NewPoint.Y + (GetState() == GS_LEAPING ? 6 : 5)
        };
        break;
    case DIR_RIGHT:
        TileMin.X = (g_TileMapX / 8 + 20) & 31;
        TileMin.Y = (g_TileMapY / 8) & 31;

        QuadRect = (rect) {
            .Left = NewPoint.X + 5,
            .Top = NewPoint.Y - 4,
            .Right = NewPoint.X + 6,
            .Bottom = NewPoint.Y + 5
        };
        break;
    }
    PlaceMap(&World, TileMin, QuadRect);
}

static void MovePlayerSync(void) {
    point FacingPoint = GetFacingPoint(g_Player.Pos, g_Player.Dir);
    g_Player.IsMoving = 1;
    g_Player.Tick = 16;
    TranslateQuadRectToTiles(FacingPoint);
    while(g_Player.IsMoving) {
        UpdateObjects(FALSE);
        NextFrame();
    }
} 

static void TransitionColors(void) {
    Pause(8);
    g_Bitmap.Colors[0] = g_Palletes[World.Maps[World.MapI].PalleteI][1]; 
    g_Bitmap.Colors[1] = g_Palletes[World.Maps[World.MapI].PalleteI][2]; 
    g_Bitmap.Colors[2] = g_Palletes[World.Maps[World.MapI].PalleteI][3]; 
    Pause(8);
    g_Bitmap.Colors[0] = g_Palletes[World.Maps[World.MapI].PalleteI][2]; 
    g_Bitmap.Colors[1] = g_Palletes[World.Maps[World.MapI].PalleteI][3]; 
    Pause(8);
    g_Bitmap.Colors[0] = g_Palletes[World.Maps[World.MapI].PalleteI][3]; 
    Pause(8);
}

static point GetLoadedDeltaPoint(point NewPoint) {
    point DeltaPoint = {};
    map *Map = &World.Maps[World.MapI];
    if(NewPoint.X == 4) {
        DeltaPoint.X = -1;
    } else if(NewPoint.X == Map->Width - 4) {
        DeltaPoint.X = 1;
    }
    if(NewPoint.Y == 4) {
        DeltaPoint.Y = -1;
    } else if(NewPoint.Y == Map->Height - 4) {
        DeltaPoint.Y = 1;
    }
    return DeltaPoint;
}

static point GetNewStartPos(point NewPoint) {
    point ReverseDirPoint = DirPoints[ReverseDir(g_Player.Dir)];
    point StartPos = AddPoints(NewPoint, ReverseDirPoint); 
    StartPos.X *= 16;
    StartPos.Y *= 16;
    return StartPos;
} 

static void MovePlayerAsync(quad_info NewQuadInfo, int Tick) {
    g_Player.IsMoving = TRUE;
    g_Player.Tick = Tick;

    if(g_InOverworld) {
        g_Player.Pos = GetNewStartPos(NewQuadInfo.Point);
        if(World.MapI != NewQuadInfo.MapI) {
            World.MapI = NewQuadInfo.MapI;
            point Load = World.Maps[World.MapI].Loaded;
            World.MusicI = OverworldMusic[Load.Y][Load.X];
            SwitchMusic();
        }
        point AddTo = GetLoadedDeltaPoint(NewQuadInfo.Point);
        if(AddTo.X == 0 || !LoadAdjacentMap(&World, AddTo.X, 0)) {
            if(AddTo.Y != 0) {
                LoadAdjacentMap(&World, 0, AddTo.Y);
            }
        }
    }

    TranslateQuadRectToTiles(NewQuadInfo.Point);
}

static BOOL IsSolidPropForPlayer(quad_prop Prop) {
    return (
        g_Player.Tile == ANIM_SEAL ? 
            Prop == QUAD_PROP_WATER : 
            Prop == QUAD_PROP_NONE || Prop == QUAD_PROP_EXIT
    ); 
}

static void GoThroughDoor(point DoorPoint) {
    MovePlayerSync();
    TransitionColors();
    LoadDoorMap(DoorPoint);
}

void MovePlayer(quad_info NewQuadInfo, quad_info OldQuadInfo) {
    ReplaceState(GS_NORMAL);
    /*MovePlayer*/
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
        MovePlayerAsync(NewQuadInfo, 32);
        ReplaceState(GS_LEAPING);
    } else if(IsSolidPropForPlayer(NewQuadInfo.Prop)) {
        MovePlayerAsync(NewQuadInfo, 16);
    } else if(g_Player.Dir == DIR_DOWN && OldQuadInfo.Prop == QUAD_PROP_EXIT) {
        PlaySoundEffect(SFX_GO_OUTSIDE);
        UpdateObjects(FALSE);
        TransitionColors();
        LoadDoorMap(OldQuadInfo.Point);
        MovePlayerSync();
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
            quad_info OldQuadInfo = GetQuadInfo(&World, PtToQuadPt(g_Player.Pos));
            quad_info NewQuadInfo = GetQuadInfo(&World, NewPoint);
            NewPoint = NewQuadInfo.Point;

            /*FetchObject*/
            AttemptLeap &= !!(NewQuadInfo.Prop == QUAD_PROP_EDGE);
            point TestPoint = NewPoint; 
            if(AttemptLeap) {
                TestPoint.Y++;
            }

            object *FacingObject = NULL; 
            for(int ObjI = 0; ObjI < World.Maps[World.MapI].ObjectCount; ObjI++) {
                object *Object = &World.Maps[World.MapI].Objects[ObjI];
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
                        UpdateObjects(TRUE);
                        DisplayTextBox(FacingObject->Str, 16);
                        WaitOnFlash();
                        ClearWindow();
                    } else if(IsComputer) {
                        PlaySoundEffect(SFX_TURN_ON_PC);
                        PlaceTextBox("%NAME turned on the PC.", 16);
                        HasTextBox = true;
                        WaitOnFlash();
                        PushState(GS_RED_PC_MAIN);
                        PushWindowTask(&RedPCMenu.WindowTask);
                        PlaceTextBox(RPC_INTRO, 0);
                    } else {
                        map *Map = &World.Maps[NewQuadInfo.MapI];
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
    UpdateObjects(!g_IsPauseAttempt);
}

void GS_LEAPING(void) {
    g_IsPauseAttempt |= (VirtButtons[BT_START] == 1);
    UpdateObjects(FALSE);
}

void UpdateSaveSec(void) {
    SaveSec = (int) MinInt64(INT_MAX, StartSaveSec + GetSecondsElapsed(StartCounter));
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
            PlaceSave(SaveRect);

            Pause(16);
            DisplayTextBox("Would you like to\nSAVE the game?", 4);
            PushWindowTask(&YesNoMenu.WindowTask);

            ReplaceState(GS_SAVE);
            break;
        case 4: /*OPTIONS*/
            PlaySoundEffect(SFX_PRESS_AB); 
            PushWindowTask(&Options.WindowTask);
            PushState(GS_OPTIONS);
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
        PlaceSave(SaveRect);
        PlaceTextBox("Now saving...", 0);

        if(WriteSaveHeader() && WriteSave(&World)) {
            Pause(120);
            PlaySoundEffect(SFX_SAVE);
            DisplayTextBox("%NAME saved\nthe game!", 0);
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
        DisplayTextBox("Is it OK to toss\n%ITEM?", 0);
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
        DisplayTextBox("Threw away\n%ITEM.", 0);
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
    case 0:
    case 1:
    case 2:
        OpenInventoryInPC(RedPCMenu.SelectI);
        break;
    case 3:
        PlaySoundEffect(SFX_TURN_OFF_PC);
        HasTextBox = false;
        PopWindowState();
        break;
    }
}

void GS_RED_PC_TOSS(void) {
    MoveMenuCursor(&ConfirmTossMenu);
    bool Toss = VirtButtons[BT_A] == 1 && ConfirmTossMenu.SelectI == 0;
    bool Cancel = (VirtButtons[BT_A] == 1 && ConfirmTossMenu.SelectI == 1) || VirtButtons[BT_B]; 
    if(Toss) {
        PlaySoundEffect(SFX_PRESS_AB);
        RemoveWindowTask();
        RemoveItem(&g_RedPC, DisplayItem.Count);
        DisplayTextBox("Threw away\n%ITEM.", 0);
        WaitOnFlash();
    } else if(Cancel) {
        PlaySoundEffect(SFX_PRESS_AB);
        RemoveWindowTask();
        PopState();
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
            DisplayTextBox("Withdrew\n%ITEM", 0); 
            WaitOnFlash();
            PopState();
            break;
        case IS_DEPOSIT:
            PlaySoundEffect(SFX_WITHDRAW_DEPOSIT);
            MoveItem(&g_RedPC, &g_Bag, DisplayItem.Count);
            DisplayTextBox("%ITEM was\nstored via PC.", 0); 
            WaitOnFlash();
            PopState();
            break;
        case IS_TOSS:
            PlaySoundEffect(SFX_PRESS_AB);
            DisplayTextBox("Is it OK to toss\n%ITEM?", 0);
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

    /*InitMainMenu*/
    if(DoesSaveExist()) {
        PushWindowTask(&ContinueMenu.WindowTask);
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

static void BeginFrame(void) {
    StartFrameTimer();

    ProcessMessages();
    UpdateInput();

    if(VirtButtons[BT_FULLSCREEN] == 1) {
        ToggleFullscreen();
    }
}

static void UpdateMusicSwitch(void) {
    if(MusicTick > 0) {
        MusicTick--;
        MusicSetVolume(MusicTick / 60.0F);
        if(MusicTick == 0) {
            PlayMusic(World.MusicI);
        }
    }
}

static void EndFrame(void) {
    UpdateMusicSwitch();
    UpdateActiveWorld();

    RenderTileMap(); 
    RenderSprites();
    RenderWindowMap();

    UpdateFullscreen();
    EndFrameTimer();
    RenderWindow();
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

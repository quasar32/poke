#include "audio.h"
#include "input.h"
#include "frame.h"
#include "intro.h"
#include "text.h"
#include "render.h"
#include "world.h"
#include "save.h"
#include <stdio.h>

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
    g_Pallete[1] = g_Palletes[PAL_OAK][2];
    Pause(8);
    g_Pallete[1] = g_Palletes[PAL_OAK][1];
    Pause(8);
    g_Pallete[2] = g_Palletes[PAL_OAK][2];
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

void PresentOak(void) {
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

void PresentNidorino(void) {
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
        countof(MenuText), 
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

void NameRed(void) {
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

void NameBlue(void) {
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
    ReadTileData("Transition", &g_WindowData[MT_TRANSITION], 31);

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

void Outro(void) {
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


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
#include "procs.h"
#include "scalar.h"
#include "menu.h"
#include "read_buffer.h"
#include "window_map.h"
#include "world.h"
#include "write_buffer.h"
#include "read.h"
#include "render.h"
#include "state.h"

/*Macro Strings*/
#define RPC_WANT "What do you want\nto "
#define RPC_INTRO RPC_WANT "do?"
#define RPC_WITHDRAW RPC_WANT "withdraw?"
#define RPC_DEPOSIT RPC_WANT "deposit?"
#define RPC_TOSS RPC_WANT "toss away?"

/*Globals*/
static int SpriteI;

static BOOL HasQuit;
static dim_rect RestoreRect;
static int IsFullscreen;

static const rect SaveRect = {4, 0, 20, 10};

static int IsSaveComplete;

static int IsPauseAttempt;
static int OptionI; 

static world World;
static uint8_t ScrollX;
static uint8_t ScrollY;

static int TransitionTick;
static point DoorPoint = {0};

static uint8_t FlowerData[3][64];
static uint8_t WaterData[7][64];

static int64_t Tick;
static int64_t StartCounter;

static window_task *DeferedTask;
static const char *DeferedMessage;

static const RGBQUAD Palletes[256][4] = {
    {
        {0xFF, 0xEF, 0xFF, 0x00},
        {0xA8, 0xA8, 0xA8, 0x00},
        {0x80, 0x80, 0x80, 0x00},
        {0x10, 0x10, 0x18, 0x00}
    },
    {
        {0xFF, 0xEF, 0xFF, 0x00},
        {0xDE, 0xE7, 0xCE, 0x00},
        {0xFF, 0xD6, 0xA5, 0x00},
        {0x10, 0x10, 0x18, 0x00}
    },
    {
        {0xFF, 0xEF, 0xFF, 0x00},
        {0x5A, 0xE7, 0xAD, 0x00},
        {0xFF, 0xD6, 0xA5, 0x00},
        {0x10, 0x10, 0x18, 0x00}
    }
};

static const char *const RPCStrings[] = {
    [IS_WITHDRAW] = RPC_WITHDRAW,
    [IS_DEPOSIT] = RPC_DEPOSIT,
    [IS_TOSS] = RPC_TOSS
};

void PopWindowState(void) {
    RemoveWindowTask();
    PopState();
}

/*Math Functions*/
static int ReverseDir(int Dir) {
    return (Dir + 2) % 4;
}

/*String Functions*/
static int DoesStartStringMatch(const char *Dst, const char *Src) {
    for(int I = 0; Src[I] != '\0'; I++) {
        if(Dst[I] != Src[I]) {
            return 0;
        }
    }
    return 1; 
}

/*Point Conversion unctions*/
static point GetFacingPoint(point Pos, int Dir) {
    point OldPoint = PtToQuadPt(Pos);
    point DirPoint = DirPoints[Dir];
    return AddPoints(OldPoint, DirPoint);
}

/*Quad Functions*/
static BOOL WriteAll(LPCSTR Path, LPCVOID Data, DWORD Size) {
    BOOL Success = FALSE;
    HANDLE FileHandle = CreateFile(
        Path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    ); 
    if(FileHandle != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten;
        WriteFile(FileHandle, Data, Size, &BytesWritten, NULL);
        if(BytesWritten == Size) {
            Success = TRUE;
        }
        CloseHandle(FileHandle);
    }
    return Success;
}

static BOOL WriteSaveHeader(void) {
    write_buffer WriteBuffer = {0};
    WriteBufferPushObject(&WriteBuffer, &SaveSec, sizeof(SaveSec));
    return WriteAll("SaveHeader", WriteBuffer.Data, WriteBuffer.Index);
}

static void ReadSaveHeader(void) {
    read_buffer ReadBuffer = {
        .Size = ReadAll("SaveHeader", ReadBuffer.Data, sizeof(ReadBuffer.Data))
    };
    ReadBufferPopObject(&ReadBuffer, &SaveSec, sizeof(SaveSec)); 
    StartSaveSec = SaveSec;
}

static BOOL WriteSave(const world *World) {
    write_buffer WriteBuffer = {0};
    WriteInventory(&WriteBuffer, &Bag);
    WriteInventory(&WriteBuffer, &RedPC);

    const map *CurMap = &World->Maps[World->MapI];
    const map *OthMap = &World->Maps[!World->MapI];

    WriteBufferPushString(&WriteBuffer, CurMap->Path, CurMap->PathLen);
    WriteBufferPushString(&WriteBuffer, OthMap->Path, OthMap->PathLen);

    WriteBufferPushByte(&WriteBuffer, World->Player.Tile);
    WriteBufferPushSaveObject(&WriteBuffer, &World->Player);
    WriteBufferPushSaveObjects(&WriteBuffer, CurMap);
    WriteBufferPushSaveObjects(&WriteBuffer, OthMap);
    WriteBufferPushByte(&WriteBuffer, MusicI[World->MapI]);
    WriteBufferPushByte(&WriteBuffer, MusicI[!World->MapI]);

    return WriteAll("Save", WriteBuffer.Data, WriteBuffer.Index);
}

/*Read Functions*/
static void ReadSave(world *World) {
    read_buffer ReadBuffer = {
        .Size = ReadAll("Save", ReadBuffer.Data, sizeof(ReadBuffer.Data))
    };
    ReadBufferPopInventory(&ReadBuffer, &Bag);
    ReadBufferPopInventory(&ReadBuffer, &RedPC);

    map *CurMap = &World->Maps[World->MapI];
    map *OthMap = &World->Maps[!World->MapI];

    ReadBufferPopString(&ReadBuffer, CurMap->Path);
    ReadBufferPopString(&ReadBuffer, OthMap->Path);

    if(CurMap->Path[0]) {
        ReadMap(World, World->MapI, CurMap->Path);
        GetLoadedPoint(World, World->MapI, CurMap->Path); 
    } else {
        memset(CurMap, 0, sizeof(*CurMap));
    }

    if(OthMap->Path[0]) {
        ReadMap(World, !World->MapI, OthMap->Path);
        GetLoadedPoint(World, !World->MapI, OthMap->Path); 
    } else {
        memset(OthMap, 0, sizeof(*OthMap));
    }

    World->Player.Tile = ReadBufferPopByte(&ReadBuffer) & 0xF0;
    ReadBufferPopSaveObject(&ReadBuffer, &World->Player);
    ReadBufferPopSaveObjects(&ReadBuffer, CurMap);
    ReadBufferPopSaveObjects(&ReadBuffer, OthMap);
    MusicI[0] = ReadBufferPopByte(&ReadBuffer); 
    MusicI[1] = ReadBufferPopByte(&ReadBuffer); 
}

/*Misc Functions*/
static int IsPressABWithSound(void) {
    int ABIsPressed = VirtButtons[BT_A] == 1 || VirtButtons[BT_B] == 1;
    if(ABIsPressed) {
        PlayWave(Sound.Voice, &Sound.PressAB);
    }
    return ABIsPressed;
}

void GS_MAIN_MENU(void);
void GS_CONTINUE(void);

void GS_NORMAL(void);
void GS_LEAPING(void);
void GS_TEXT(void);
void GS_TRANSITION(void);

void GS_START_MENU(void);
void GS_INVENTORY(void);
void GS_SAVE(void);
void GS_OPTIONS(void);

void GS_USE_TOSS(void);
void GS_TOSS(void);
void GS_CONFIRM_TOSS(void);

void GS_RED_PC_TEXT(void);
void GS_RED_PC_MAIN(void);
void GS_RED_PC_ITEM(void);

void GS_RED_PC_REPEAT_INTRO(void);

void GS_REMOVE_WINDOW_TASK(void);
void GS_PUSH_WINDOW_TASK(void);

void GS_MAIN_MENU(void) {
    menu *CurrentMenu = GET_TOP_WINDOW_TASK(menu); 
    MoveMenuCursor(CurrentMenu);
    if(VirtButtons[BT_A] == 1) {
        switch(CurrentMenu->EndI - CurrentMenu->SelectI) {
        case 3: /*CONTINUE*/
            ReadSaveHeader();
            PushWindowTask(&ContinueSaveRect.WindowTask);
            memset(TileMap, MT_BLANK, sizeof(TileMap));
            PushState(GS_CONTINUE);
            break;
        case 2: /*NEW GAME*/
            StartCounter = QueryPerfCounter();
            ClearWindowStack();
            ReadOverworldMap(&World, 0, (point) {0, 2});
            SetPlayerToDefault(&World);
            MusicTick = 60;
            PlaceViewMap(&World, 0);
            ScrollX = 0;
            ScrollY = 0;
            ReplaceState(GS_NORMAL);
            break;
        case 1: /*OPTIONS*/
            PushWindowTask(&Options.WindowTask);
            PushState(GS_OPTIONS);
            break;
        }
        PlayWave(Sound.Voice, &Sound.PressAB); 
    }
}

void GS_CONTINUE(void) {
    static int ShowTick = 0;
    if(MusicTick <= 0) {
        if(ShowTick > 0) {
            ShowTick--;
            if(ShowTick == 0) {
                PopState();
                ExecuteAllWindowTasks();
            }
        } else if(VirtButtons[BT_A] == 1) {
            MusicTick = 60;
            ClearWindowStack();
            ReadSave(&World);
        } else if(VirtButtons[BT_B] == 1) {
            ShowTick = 30;
            PopWindowTask();
            ClearWindow();
        }
    }
}

void GS_NORMAL(void) {
    /*PlayerUpdate*/
    IsPauseAttempt |= (VirtButtons[BT_START] == 1);
    if(World.Player.Tick == 0) {
        if(IsPauseAttempt) {
            if(GetQueueCount(Sound.Voice) < 1) {
                IsPauseAttempt = 0;
                PlayWave(Sound.Voice, &Sound.StartMenu); 
                PushState(GS_START_MENU);
                PushWindowTask(&StartMenu.WindowTask);
            }
        } else { 
            int AttemptLeap = 0;

            /*PlayerKeyboard*/
            int HasMoveKey = 1;
            if(VirtButtons[BT_UP]) {
                World.Player.Dir = DIR_UP;
            } else if(VirtButtons[BT_LEFT]) {
                World.Player.Dir = DIR_LEFT;
            } else if(VirtButtons[BT_DOWN]) {
                World.Player.Dir = DIR_DOWN;
                AttemptLeap = 1;
            } else if(VirtButtons[BT_RIGHT]) {
                World.Player.Dir = DIR_RIGHT;
            } else {
                HasMoveKey = 0;
            }

            point NewPoint = GetFacingPoint(World.Player.Pos, World.Player.Dir);

            /*FetchQuadProp*/
            quad_info OldQuadInfo = GetQuadInfo(&World, PtToQuadPt(World.Player.Pos));
            quad_info NewQuadInfo = GetQuadInfo(&World, NewPoint);
            NewPoint = NewQuadInfo.Point;

            point ReverseDirPoint = DirPoints[ReverseDir(World.Player.Dir)];
            point StartPos = AddPoints(NewPoint, ReverseDirPoint); 
            StartPos.X *= 16;
            StartPos.Y *= 16;

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
                int IsShelf = NewQuadInfo.Prop == QUAD_PROP_SHELF && World.Player.Dir == DIR_UP;
                int IsComputer = NewQuadInfo.Prop == QUAD_PROP_RED_PC && World.Player.Dir == DIR_UP;
                if(NewQuadInfo.Prop == QUAD_PROP_MESSAGE || IsTV || IsShelf || IsComputer) {
                    ActiveText = (active_text) {
                        .Tick = 16,
                        .Props = AT_MANUAL_RESTORE | AT_AUTO_CLEAR 
                    };
                    /*GetActiveText*/
                    if(IsShelf) {
                        PushState(GS_TEXT);
                        ActiveText.Str = "Crammed full of\nPOKéMON books!"; 
                    } else if(IsTV && World.Player.Dir != DIR_UP) {
                        PushState(GS_REMOVE_WINDOW_TASK);
                        PushState(GS_TEXT);
                        ActiveText.Str = "Oops, wrong side."; 
                    } else if(FacingObject) {
                        PushState(GS_REMOVE_WINDOW_TASK);
                        PushState(GS_TEXT);
                        ActiveText.Str = FacingObject->Str;
                        if(FacingObject->Speed == 0) {
                            FacingObject->Tick = 120;
                        }
                        FacingObject->Dir = ReverseDir(World.Player.Dir);
                    } else if(IsComputer) {
                        PushState(GS_RED_PC_TEXT);
                        ActiveText.Str = "RED turned on\nthe PC.";
                        PlaceTextBox(BottomTextRect);
                        PlayWave(Sound.Voice, &Sound.TurnOnPC);
                    } else {
                        PushState(GS_REMOVE_WINDOW_TASK);
                        PushState(GS_TEXT);
                        /*GetTextFromPos*/
                        map *Map = &World.Maps[NewQuadInfo.MapI];
                        ActiveText.Str = "ERROR: NO TEXT";
                        for(int I = 0; I < Map->TextCount; I++) {
                            text *Text = &Map->Texts[I];
                            if(EqualPoints(Map->Texts[I].Pos, NewPoint)) {
                                ActiveText.Str = Text->Str;
                                break;
                            } 
                        } 
                    }

                    HasMoveKey = 0;
                } else if(NewQuadInfo.Prop == QUAD_PROP_WATER) {
                    if(World.Player.Tile == ANIM_PLAYER) {
                        World.Player.Tile = ANIM_SEAL;
                        HasMoveKey = 1;
                    }
                } else if(World.Player.Tile == ANIM_SEAL && 
                          NewQuadInfo.Prop == QUAD_PROP_NONE) {
                    World.Player.Tile = ANIM_PLAYER;
                    HasMoveKey = 1;
                }
            }

            if(HasMoveKey) {
                ReplaceState(GS_NORMAL);
                /*MovePlayer*/
                if(NewQuadInfo.Prop == QUAD_PROP_DOOR && 
                   (!World.IsOverworld || World.Player.Dir == DIR_UP)) {
                    World.Player.IsMoving = 1;
                    World.Player.Tick = 16; 
                    DoorPoint = NewQuadInfo.Point;
                    ReplaceState(GS_TRANSITION);
                    if(World.IsOverworld) {
                        PlayWave(Sound.Voice, &Sound.GoInside);
                    } else {
                        PlayWave(Sound.Voice, &Sound.GoOutside);
                    }
                    NewPoint.Y--;
                } else if(World.Player.Dir == DIR_DOWN && 
                          NewQuadInfo.Prop == QUAD_PROP_EDGE) {
                    World.Player.IsMoving = 1;
                    World.Player.Tick = 32;
                    ReplaceState(GS_LEAPING);
                    PlayWave(Sound.Voice, &Sound.Ledge);
                } else if(
                    World.Player.Tile == ANIM_SEAL ? 
                          NewQuadInfo.Prop == QUAD_PROP_WATER : 
                          NewQuadInfo.Prop == QUAD_PROP_NONE || NewQuadInfo.Prop == QUAD_PROP_EXIT
                ) { 
                    World.Player.IsMoving = 1;
                    World.Player.Tick = 16;
                } else {
                    World.Player.IsMoving = 0;
                    World.Player.Tick = 16;
                    if(World.Player.Dir == DIR_DOWN && OldQuadInfo.Prop == QUAD_PROP_EXIT) {
                        World.Player.Tick = 0;
                        TransitionTick = 32;
                        DoorPoint = OldQuadInfo.Point;
                        ReplaceState(GS_TRANSITION);
                        PlayWave(Sound.Voice, &Sound.GoOutside);
                    } else if(GetQueueCount(Sound.Voice) < 2) {
                        PlayWave(Sound.Voice, &Sound.Collision);
                    }
                }
            } else {
                World.Player.IsMoving = 0;
            }

            if(World.Player.IsMoving) {
                World.Player.Pos = StartPos;
                if(World.MapI != NewQuadInfo.MapI) {
                    MusicTick = 60;
                    World.MapI = NewQuadInfo.MapI;
                }

                /*UpdateLoadedMaps*/
                if(World.IsOverworld) {
                    int AddToX = 0;
                    int AddToY = 0;
                    if(NewPoint.X == 4) {
                        AddToX = -1;
                    } else if(NewPoint.X == World.Maps[World.MapI].Width - 4) {
                        AddToX = 1;
                    }
                    if(NewPoint.Y == 4) {
                        AddToY = -1;
                    } else if(NewPoint.Y == World.Maps[World.MapI].Height - 4) {
                        AddToY = 1;
                    }

                    if(AddToX || AddToY) {
                        if(!LoadAdjacentMap(&World, AddToX, 0)) {
                            LoadAdjacentMap(&World, 0, AddToY);
                        }
                    }
                }

                /*TranslateQuadRectToTiles*/
                point TileMin = {0};
                rect QuadRect = {0};

                switch(World.Player.Dir) {
                case DIR_UP:
                    TileMin.X = (ScrollX / 8) & 31;
                    TileMin.Y = (ScrollY / 8 + 30) & 31;

                    QuadRect = (rect) {
                        .Left = NewPoint.X - 4,
                        .Top = NewPoint.Y - 4, 
                        .Right = NewPoint.X + 6,
                        .Bottom = NewPoint.Y - 3
                    };
                    if(World.IsOverworld && GetState() == GS_TRANSITION) {
                        QuadRect.Bottom += 4;
                        TileMin.Y -= 2;
                    }
                    break;
                case DIR_LEFT:
                    TileMin.X = (ScrollX / 8 + 30) & 31;
                    TileMin.Y = (ScrollY / 8) & 31;

                    QuadRect = (rect) {
                        .Left = NewPoint.X - 4,
                        .Top = NewPoint.Y - 4,
                        .Right = NewPoint.X - 3,
                        .Bottom = NewPoint.Y + 5
                    };
                    break;
                case DIR_DOWN:
                    TileMin.X = (ScrollX / 8) & 31;
                    TileMin.Y = (ScrollY / 8 + 18) & 31;

                    QuadRect = (rect) {
                        .Left = NewPoint.X - 4,
                        .Top = NewPoint.Y + 4,
                        .Right = NewPoint.X + 6,
                        .Bottom = NewPoint.Y + (GetState() == GS_LEAPING ? 6 : 5)
                    };
                    break;
                case DIR_RIGHT:
                    TileMin.X = (ScrollX / 8 + 20) & 31;
                    TileMin.Y = (ScrollY / 8) & 31;

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
        }
    }
}

void GS_LEAPING(void) {
    IsPauseAttempt |= (VirtButtons[BT_START] == 1);
}

void GS_TEXT(void) {
    if(ActiveText.BoxDelay >= 0) {
        if(ActiveText.BoxDelay == 0) {
            if(IsSaveComplete) {
                PlayWave(Sound.Voice, &Sound.Save);
                IsSaveComplete = 0;
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
                if(IsPressABWithSound()) {
                    if(ActiveText.Str[-1] == '\n') {
                        memcpy(&WindowMap[13][1], &WindowMap[14][1], 17);
                        memcpy(&WindowMap[15][1], &WindowMap[16][1], 17);
                    }
                    memset(&WindowMap[14][1], MT_BLANK, 17);
                    memset(&WindowMap[16][1], MT_BLANK, 18);
                    ActiveText.TilePt.Y = 17;
                    ActiveText.Tick = 8;
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
                        WindowMap[ActiveText.TilePt.Y][ActiveText.TilePt.X] = CharToTile(*ActiveText.Str);
                        if(ActiveText.TilePt.X < 20) {
                            ActiveText.TilePt.X++;
                            ActiveText.Str++;
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

void GS_TRANSITION(void) {
    /*EnterTransition*/
    if(World.Player.IsMoving) {
        if(World.Player.Tick <= 0) {
            World.Player.IsMoving = 0;
            TransitionTick = 32;
        }
    }

    /*ProgressTransition*/
    if(!World.Player.IsMoving) {
        if(TransitionTick-- > 0) {
            switch(TransitionTick) {
            case 24:
                Bitmap.Colors[0] = Palletes[World.Maps[World.MapI].PalleteI][1]; 
                Bitmap.Colors[1] = Palletes[World.Maps[World.MapI].PalleteI][2]; 
                Bitmap.Colors[2] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                break;
            case 16:
                Bitmap.Colors[0] = Palletes[World.Maps[World.MapI].PalleteI][2]; 
                Bitmap.Colors[1] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                break;
            case 8:

                Bitmap.Colors[0] = Palletes[World.Maps[World.MapI].PalleteI][3]; 
                break;
            case 0:
                {
                    /*LoadDoorMap*/
                    quad_info QuadInfo = GetQuadInfo(&World, DoorPoint); 
                    map *OldMap = &World.Maps[QuadInfo.MapI]; 

                    for(int I = 0; I < OldMap->DoorCount; I++) {
                        const door *Door = &OldMap->Doors[I];
                        if(EqualPoints(Door->Pos, QuadInfo.Point)) {
                            /*DoorToDoor*/
                            ReadMap(&World, !QuadInfo.MapI, Door->Path);
                            World.MapI = !QuadInfo.MapI;
                            World.Player.Pos = QuadPtToPt(Door->DstPos);
                            GetLoadedPoint(&World, !QuadInfo.MapI, Door->Path);

                            /*CompleteTransition*/
                            ScrollX = 0;
                            ScrollY = 0;

                            memset(OldMap, 0, sizeof(*OldMap));
                            if(QuadInfo.Prop == QUAD_PROP_EXIT) {
                                World.Player.IsMoving = 1;
                                World.Player.Tick = 16;
                                PlaceViewMap(&World, 1);
                            } else {
                                PlaceViewMap(&World, 0);
                            }
                            break;
                        } 
                    }

                    ReplaceState(GS_NORMAL);
                    break;
                }
            }
        }
    }
}

void GS_START_MENU(void) {
    if(VirtButtons[BT_START] == 1 || VirtButtons[BT_B] == 1) {
        PopWindowState();
        if(VirtButtons[BT_B] == 1) {
            PlayWave(Sound.Voice, &Sound.PressAB); 
        }
    } else { 
        MoveMenuCursorWrap(&StartMenu);

        /*SelectMenuOption*/
        if(VirtButtons[BT_A] == 1) {
            switch(StartMenu.SelectI) {
            case 0: /*POKéMON*/
                break;
            case 1: /*ITEM*/
                Inventory = &Bag;
                InventoryState = IS_ITEM; 
                PushWindowTask(&Inventory->WindowTask);
                PushState(GS_INVENTORY);
                break;
            case 2: /*RED*/
                break;
            case 3: /*SAVE*/
                SaveSec = (int) MinInt64(INT_MAX, StartSaveSec + GetSecondsElapsed(StartCounter));
                PlaceSave(SaveRect);

                ActiveText = (active_text) {
                    .Tick = 4,
                    .BoxDelay = 16, 
                    .Str = "Would you like to\nSAVE the game?"
                };

                DeferedTask = &YesNoMenu.WindowTask;
                ReplaceState(GS_SAVE);
                PushState(GS_PUSH_WINDOW_TASK);
                PushState(GS_TEXT);
                break;
            case 4: /*OPTIONS*/
                PushWindowTask(&Options.WindowTask);
                PushState(GS_OPTIONS);
                break;
            case 5: /*EXIT*/
                RemoveWindowTask();
                PopState();
                break;
            }
            PlayWave(Sound.Voice, &Sound.PressAB); 
        }
    }
}

void GS_INVENTORY(void) {
    if(VirtButtons[BT_B] == 1 || 
       (VirtButtons[BT_A] == 1 && Inventory->ItemSelect == Inventory->ItemCount)) {
        /*RemoveSubMenu*/
        RemoveWindowTask();
        PopState();
        PlayWave(Sound.Voice, &Sound.PressAB); 
    } else {
        /*MoveTextCursor*/
        if(VirtButtons[BT_A] == 1) {
            PlayWave(Sound.Voice, &Sound.PressAB); 
            WindowMap[Inventory->Y * 2 + 4][5] = MT_EMPTY_HORZ_ARROW;
            switch(InventoryState) {
            case IS_ITEM:
                PushWindowTask(&UseTossMenu.WindowTask);
                PushState(GS_USE_TOSS);
                break;
            default:
                ActiveText = (active_text) {
                    .Str = "How many?", 
                };
                DeferedTask = &DisplayWindowTask;
                DeferedMessage = RPCStrings[InventoryState];
                PushState(GS_REMOVE_WINDOW_TASK);
                PushState(GS_RED_PC_REPEAT_INTRO);
                PushState(GS_RED_PC_ITEM);
                PushState(GS_PUSH_WINDOW_TASK);
                PushState(GS_TEXT);
            } 
        } else if(VirtButtons[BT_UP] % 10 == 1 && Inventory->ItemSelect > 0) {
            Inventory->ItemSelect--; 
            if(Inventory->Y > 0) {
                Inventory->Y--;
            }
            PlaceInventory(Inventory); 
        } else if(VirtButtons[BT_DOWN] % 10 == 1 && Inventory->ItemSelect < Inventory->ItemCount) {
            Inventory->ItemSelect++; 
            if(Inventory->Y < 2) {
                Inventory->Y++;
            }
            PlaceInventory(Inventory); 
        } 

        /*MoreItemsWait*/
        int InventoryLast = Inventory->ItemSelect - Inventory->Y + 3; 
        if(InventoryLast < Inventory->ItemCount) {
            FlashTextCursor(&ActiveText);
        } else {
            ActiveText.Tick = 0;
        }
    }
}

void GS_SAVE(void) {
    if(VirtButtons[BT_A] == 1 && YesNoMenu.SelectI == 0) {
        PlayWave(Sound.Voice, &Sound.PressAB); 

        /*RemoveYesNoSavePrompt*/
        RemoveWindowTask();
        PlaceSave(SaveRect);
        PlaceStaticText(BottomTextRect, "Now saving...");

        if(WriteSaveHeader() && WriteSave(&World)) {
            /*ArtificalSaveDelay*/
            ActiveText = (active_text) {
                .Str = "RED saved\nthe game!",
                .BoxDelay = 120
            };
            PopState();
            ReplaceState(GS_NORMAL);
            PushState(GS_REMOVE_WINDOW_TASK);
            PushState(GS_TEXT);
            IsSaveComplete = 1;
        } else {
            /*Error*/
            ActiveText = (active_text) {
                .Str = "ERROR:\nWriteSave failed",
                .BoxDelay = 60
            };
            PopState();
            ReplaceState(GS_NORMAL);
            PushState(GS_REMOVE_WINDOW_TASK);
            PushState(GS_TEXT);
        } 
    } else if(IsPressABWithSound()) {
        ClearWindowStack();
        PopState();
    } else {
        MoveMenuCursor(&YesNoMenu);
    }
}

void GS_OPTIONS(void) {
    if(
        VirtButtons[BT_START] == 1 || 
        VirtButtons[BT_B] == 1 ||
        (VirtButtons[BT_A] == 1 && OptionI == OPT_CANCEL)
    ) {
        /*RemoveSubMenu*/
        PopWindowState();
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

void GS_USE_TOSS(void) {
    MoveMenuCursor(&UseTossMenu);
    if(VirtButtons[BT_A] == 1) {
        if(UseTossMenu.SelectI == 0) {
            ActiveText = (active_text) {
                .Str = "ERROR: TODO", 
                .Props = AT_MANUAL_RESTORE | AT_WAIT_FLASH
            };
            
            ReplaceState(GS_REMOVE_WINDOW_TASK);
            PushState(GS_TEXT);
        } else {
            PlaceMenuCursor(&UseTossMenu, MT_EMPTY_HORZ_ARROW);
            PopWindowTask();
            PushWindowTask(&DisplayWindowTask);
            ReplaceState(GS_TOSS);
        }
        PlayWave(Sound.Voice, &Sound.PressAB);
    } else if(VirtButtons[BT_B] == 1) {
        RemoveWindowTask();
        PlayWave(Sound.Voice, &Sound.PressAB);
        PopState();
    }
}

void GS_TOSS(void) {
    UpdateDisplayItem(Inventory);
    if(VirtButtons[BT_A] == 1) {
        ActiveText = (active_text) {
            .Str = "Is it OK to toss\n[ITEM]?"
        };
        DeferedTask = &ConfirmTossMenu.WindowTask;
        PlayWave(Sound.Voice, &Sound.PressAB);
        ReplaceState(GS_CONFIRM_TOSS);
        PushState(GS_PUSH_WINDOW_TASK);
        PushState(GS_TEXT);
    } else if(VirtButtons[BT_B] == 1) {
        RemoveWindowTask();
        PlayWave(Sound.Voice, &Sound.PressAB);
        PopState();
    }
}

void GS_CONFIRM_TOSS(void) {
    MoveMenuCursor(&ConfirmTossMenu);
    if(VirtButtons[BT_A] == 1 && ConfirmTossMenu.SelectI == 0) {
        RemoveWindowTask();
        ReplaceState(GS_REMOVE_WINDOW_TASK);
        PushState(GS_TEXT);
        RemoveItem(Inventory, DisplayItem.Count); 
        ActiveText = (active_text) { 
            .Str = "Threw away\n[ITEM].",
            .Props = AT_MANUAL_RESTORE | AT_WAIT_FLASH
        };
        PlayWave(Sound.Voice, &Sound.PressAB);
        ExecuteState();
    } else if((VirtButtons[BT_A] == 1 && ConfirmTossMenu.SelectI == 1) || VirtButtons[BT_B] == 1) {
        RemoveWindowTask();
        RemoveWindowTask();
        PopState();
        PlayWave(Sound.Voice, &Sound.PressAB);
    }
}

void GS_RED_PC_TEXT(void) {
    if(ActiveText.Str) {
        if(ActiveText.Tick > 0) {
            ActiveText.Tick--;
        } else {
            PlaceText((point) {1, 14}, ActiveText.Str);
            ActiveText.Str = NULL;
        } 
    } else if(IsPressABWithSound()) {
        ReplaceState(GS_RED_PC_MAIN);
        HasTextBox = true;
        PushWindowTask(&RedPCMenu.WindowTask);
        PlaceStaticText(BottomTextRect, RPC_INTRO);
    } else {
        FlashTextCursor(&ActiveText);
    }
}

void GS_RED_PC_MAIN(void) {
    MoveMenuCursor(&RedPCMenu); 
    if((VirtButtons[BT_A] == 1 && RedPCMenu.SelectI == 3) || VirtButtons[BT_B] == 1) {
        HasTextBox = false;
        PopWindowState();
        PlayWave(Sound.Voice, &Sound.TurnOffPC);
    } else if(VirtButtons[BT_A] == 1) {
        /*FindRedPCOption*/
        typedef struct {
            inventory_state InventoryState;
            inventory *Inventory;
            const char *Empty;
            const char *Normal;
        } red_pc_select; 
        static const red_pc_select RedPCSelection[] = {
            {
                .InventoryState = IS_WITHDRAW,
                .Inventory = &RedPC,
                .Empty = "There is\nnothing stored.",
                .Normal = RPC_WITHDRAW
            }, 
            {
                .InventoryState = IS_DEPOSIT,
                .Inventory = &Bag,
                .Empty = "You have nothing\nto deposit.",
                .Normal = RPC_DEPOSIT
            }, 
            {
                .InventoryState = IS_TOSS,
                .Inventory = &RedPC,
                .Empty = "There is\nnothing stored.",
                .Normal = RPC_TOSS
            } 
        };

        const red_pc_select *PCSelect = &RedPCSelection[RedPCMenu.SelectI];

        /*OpenInventory*/
        DeferedMessage = RPC_INTRO;
        PushState(GS_RED_PC_REPEAT_INTRO);
        InventoryState = PCSelect->InventoryState;
        Inventory = PCSelect->Inventory;
        if(Inventory->ItemCount == 0) {
            ActiveText = (active_text) {
                .Str = PCSelect->Empty,
                .Props = AT_MANUAL_RESTORE | AT_WAIT_FLASH
            };
        } else {
            DeferedTask = &Inventory->WindowTask;
            PushState(GS_INVENTORY);
            PushState(GS_PUSH_WINDOW_TASK);
            Inventory->ItemSelect = 0;
            Inventory->Y = 0;
            ActiveText = (active_text) {
                .Str = PCSelect->Normal
            };
        } 
        PushState(GS_TEXT);
        PlayWave(Sound.Voice, &Sound.PressAB); 
    }
}

void GS_RED_PC_TOSS(void) {
    MoveMenuCursor(&ConfirmTossMenu);
    bool Toss = VirtButtons[BT_A] == 1 && ConfirmTossMenu.SelectI == 0;
    bool Cancel = (VirtButtons[BT_A] == 1 && ConfirmTossMenu.SelectI == 1) || VirtButtons[BT_B]; 
    if(Toss) {
        PlayWave(Sound.Voice, &Sound.PressAB);
        ActiveText = (active_text) {
            .Str = "Threw away\n[ITEM].",
            .Props = AT_MANUAL_RESTORE | AT_WAIT_FLASH
        };
        RemoveWindowTask();
        RemoveItem(&RedPC, DisplayItem.Count);
        ReplaceState(GS_TEXT);
    } else if(Cancel) {
        PlayWave(Sound.Voice, &Sound.PressAB);
        RemoveWindowTask();
        PopState();
    }
}

void GS_RED_PC_ITEM(void) { 
    UpdateDisplayItem(Inventory);
    if(VirtButtons[BT_A] == 1) {
        /*RedPCDoItemOp*/
        switch(InventoryState) {
        case IS_WITHDRAW:
            MoveItem(&Bag, &RedPC, DisplayItem.Count);
            ActiveText = (active_text) {
                .Str = "Withdrew\n[ITEM]\f", 
                .Props = AT_MANUAL_RESTORE | AT_WAIT_FLASH
            };
            PlayWave(Sound.Voice, &Sound.WithdrawDeposit);
            ReplaceState(GS_TEXT);
            ExecuteState();
            break;
        case IS_DEPOSIT:
            MoveItem(&RedPC, &Bag, DisplayItem.Count);
            ActiveText = (active_text) {
                .Str = "[ITEM] was\nstored via PC.\f", 
                .Props = AT_MANUAL_RESTORE | AT_WAIT_FLASH
            };
            PlayWave(Sound.Voice, &Sound.WithdrawDeposit);
            ReplaceState(GS_TEXT);
            ExecuteState();
            break;
        case IS_TOSS:
            ActiveText = (active_text) {
                .Str = "Is it OK to toss\n[ITEM]?"
            };
            DeferedTask = &ConfirmTossMenu.WindowTask;
            PlayWave(Sound.Voice, &Sound.PressAB);
            ReplaceState(GS_RED_PC_TOSS);
            PushState(GS_PUSH_WINDOW_TASK);
            PushState(GS_TEXT);
            break;
        default:
            assert(!"ERROR: Invalid inventory state");
        }
    } else if(VirtButtons[BT_B] == 1) {
        /*RedPCCancelItemOp*/
        PopState();
        ExecuteState();
        PlayWave(Sound.Voice, &Sound.PressAB);
    }
}

void GS_RED_PC_REPEAT_INTRO(void) {
    PlaceTextBox(BottomTextRect);
    ActiveText = (active_text) {
        .Str = DeferedMessage 
    }; 
    DeferedMessage = RPC_INTRO;
    ReplaceState(GS_TEXT);
    ExecuteState();
}

void GS_REMOVE_WINDOW_TASK(void) {
    RemoveWindowTask();
    PopState();
    ExecuteState();
}

void GS_PUSH_WINDOW_TASK(void) {
    PushWindowTask(DeferedTask);
    PopState();
    ExecuteState();
}

/*Win32 Functions*/
static void SetMyWindowPos(HWND Window, DWORD Style, dim_rect Rect) {
    SetWindowLongPtr(Window, GWL_STYLE, Style | WS_VISIBLE);
    SetWindowPos(Window, 
                 HWND_TOP, 
                 Rect.X, Rect.Y, Rect.Width, Rect.Height, 
                 SWP_FRAMECHANGED | SWP_NOREPOSITION);
}

static void UpdateFullscreen(HWND Window) {
    dim_rect ClientRect = GetDimClientRect(Window);

    HMONITOR Monitor = MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO MonitorInfo = {
        .cbSize = sizeof(MonitorInfo)
    };
    GetMonitorInfo(Monitor, &MonitorInfo);

    dim_rect MonitorRect = WinToDimRect(MonitorInfo.rcMonitor);

    if(MonitorRect.Width != ClientRect.Width || MonitorRect.Height != ClientRect.Height) {
        SetMyWindowPos(Window, WS_POPUP, MonitorRect);
    }
}

static BOOL ToggleFullscreen(HWND Window) {
    ShowCursor(IsFullscreen);
    if(IsFullscreen) {
        SetMyWindowPos(Window, WS_OVERLAPPEDWINDOW, RestoreRect);
    } else {
        RECT WinWindowRect;
        GetWindowRect(Window, &WinWindowRect);
        RestoreRect = WinToDimRect(WinWindowRect);
    }
    IsFullscreen ^= 1;
    return IsFullscreen;
}

static void ProcessMessages(HWND Window) {
    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
        switch(Message.message) {
        case WM_QUIT:
            HasQuit = TRUE;
            break;
        default:
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }
}

static void PaintFrame(HWND Window) {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    dim_rect ClientRect = GetDimClientRect(Window);

    int RenderWidth = ClientRect.Width - ClientRect.Width % BM_WIDTH;
    int RenderHeight = ClientRect.Height - ClientRect.Height % BM_HEIGHT;

    int RenderColSize = RenderWidth * BM_HEIGHT;
    int RenderRowSize = RenderHeight * BM_WIDTH;
    if(RenderColSize > RenderRowSize) {
        RenderWidth = RenderRowSize / BM_HEIGHT;
    } else {
        RenderHeight = RenderColSize / BM_WIDTH;
    }
    int RenderX = (ClientRect.Width - RenderWidth) / 2;
    int RenderY = (ClientRect.Height - RenderHeight) / 2;

    StretchDIBits(DeviceContext, 
                  RenderX, RenderY, RenderWidth, RenderHeight,
                  0, 0, BM_WIDTH, BM_HEIGHT, 
                  Pixels, &Bitmap.WinStruct, 
                  DIB_RGB_COLORS, SRCCOPY);

    PatBlt(DeviceContext, 0, 0, RenderX, ClientRect.Height, BLACKNESS);
    int RenderRight = RenderX + RenderWidth;
    PatBlt(DeviceContext, RenderRight, 0, RenderX, ClientRect.Height, BLACKNESS);
    PatBlt(DeviceContext, RenderX, 0, RenderWidth, RenderY, BLACKNESS);
    int RenderBottom = RenderY + RenderHeight;
    PatBlt(DeviceContext, RenderX, RenderBottom, RenderWidth, RenderY + 1, BLACKNESS);
    EndPaint(Window, &Paint);
}

static LRESULT CALLBACK WndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    switch(Message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        DestroyWindow(Window);
        return 0;
    case WM_PAINT:
        PaintFrame(Window);
        return 0;
    }
    return DefWindowProc(Window, Message, WParam, LParam);
}

int WINAPI WinMain(HINSTANCE Instance, [[maybe_unused]] HINSTANCE Prev, LPSTR CmdLine, int CmdShow) {
    srand(time(NULL));
    SetCurrentDirectory("../Shared");

    /*MuteModeLaunch*/
    int IsMute = strcmp(CmdLine, "mute") == 0;

    /*InitPallete*/
    /*InitWindow*/
    WNDCLASS WindowClass = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WndProc,
        .hInstance = Instance,
        .hCursor = LoadCursor(NULL, IDC_ARROW),
        .lpszClassName = "PokeWindowClass"
    };
    if(!RegisterClass(&WindowClass)) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR); 
        return 1;
    }

    RECT WinWindowRect = {0, 0, BM_WIDTH, BM_HEIGHT};
    AdjustWindowRect(&WinWindowRect, WS_OVERLAPPEDWINDOW, 0);

    dim_rect WindowRect = WinToDimRect(WinWindowRect);

    HWND Window = CreateWindow(
        WindowClass.lpszClassName, 
        "PokeGame", 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        WindowRect.Width, 
        WindowRect.Height, 
        NULL, 
        NULL, 
        Instance, 
        NULL
    );
    if(!Window) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_ICONERROR); 
        return 1;
    }

    /*InitAudio*/
    com Com = {0};
    xaudio2 XAudio2 = {0};

    if(!IsMute) { 
        if(InitCom(&Com)) {
            if(InitXAudio2(&XAudio2)) {
                HRESULT SoundResult = CreateGenericVoice(XAudio2.Engine, &Sound.Voice);
                if(SUCCEEDED(SoundResult)) {
                    ReadSound("SFX_COLLISION.wav", &Sound.Collision);
                    ReadSound("SFX_LEDGE.wav", &Sound.Ledge);
                    ReadSound("SFX_START_MENU.wav", &Sound.StartMenu);
                    ReadSound("SFX_PRESS_AB.wav", &Sound.PressAB);
                    ReadSound("SFX_GO_OUTSIDE.wav", &Sound.GoOutside);
                    ReadSound("SFX_GO_INSIDE.wav", &Sound.GoInside);
                    ReadSound("SFX_SAVE.wav", &Sound.Save);
                    ReadSound("SFX_TURN_ON_PC.wav", &Sound.TurnOnPC);
                    ReadSound("SFX_TURN_OFF_PC.wav", &Sound.TurnOffPC);
                    ReadSound("SFX_WITHDRAW_DEPOSIT.wav", &Sound.WithdrawDeposit);
                }
         
                HRESULT MusicResult = CreateGenericVoice(XAudio2.Engine, &MusicVoice);
                if(SUCCEEDED(MusicResult)) {
                    ReadMusic(1, 1);
                }
            } else { 
                DestroyCom(&Com);
                MessageBox(Window, "XAudio2 could not be initalized", "Error", MB_ICONERROR);
            }
        } else {
            MessageBox(Window, "COM could not be initalized", "Error", MB_ICONERROR);
        }
    }

    SetPlayerToDefault(&World);

    ReadTileData("Menu", &World.TileData[96], 152);
    ReadTileData("SpriteData", SpriteData, 256);
    ReadTileData("FlowerData", FlowerData, 3);
    ReadTileData("WaterData", WaterData, 7);
    ReadTileData("ShadowData", &SpriteData[255], 1);

    /*InitMainMenu*/
    if(GetFileAttributes("Save") == INVALID_FILE_ATTRIBUTES) {
        PushWindowTask(&MainMenu.WindowTask);
    } else {
        PushWindowTask(&ContinueMenu.WindowTask);
    }

    memcpy(Bitmap.Colors, Palletes[0], sizeof(Palletes[0]));
    PlayMusic(1); 
    
    PushState(GS_MAIN_MENU);

    /*MainLoop*/
    ShowWindow(Window, CmdShow);
    while(!HasQuit) {
        StartFrame();

        ProcessMessages(Window);
        UpdateInput(GetActiveWindow() == Window);

        if(VirtButtons[BT_FULLSCREEN] == 1) ToggleFullscreen(Window);
        ExecuteState();

        SpriteI = 0;

        if(MusicTick > 0) {
            MusicTick--;
            SetVolume(MusicVoice, (float) MusicTick / 60.0f);
            if(MusicTick == 0) {
                StartCounter = QueryPerfCounter();
                SetVolume(MusicVoice, 1.0f);
                if(GetState() == GS_CONTINUE) {
                    if(MusicI[0]) {
                        ReadMusic(MusicI[0], 0);
                    }
                    if(MusicI[1]) {
                        ReadMusic(MusicI[1], 1);
                    }
                    PlaceViewMap(&World, 0);
                    PopState();
                    ReplaceState(GS_NORMAL);
                }
                PlayMusic(World.MapI);
            }
        }

        if(GetState() != GS_MAIN_MENU && GetState() != GS_CONTINUE && GetState() != GS_OPTIONS) {
            /*UpdatePlayer*/
            int CanPlayerMove = GetState() == GS_NORMAL || 
                                GetState() == GS_LEAPING || 
                                GetState() == GS_TRANSITION;
            if(CanPlayerMove && World.Player.Tick > 0) {
                if(World.Player.Tick % 16 == 8) {
                    World.Player.IsRight ^= 1;
                }
                if(World.Player.IsMoving && World.Player.Tick % 2 == 0) {
                    switch(World.Player.Dir) {
                    case DIR_UP:
                        ScrollY -= World.Player.Speed;
                        break;
                    case DIR_LEFT:
                        ScrollX -= World.Player.Speed;
                        break;
                    case DIR_RIGHT:
                        ScrollX += World.Player.Speed;
                        break;
                    case DIR_DOWN:
                        ScrollY += World.Player.Speed;
                        break;
                    }
                }
                MoveEntity(&World.Player);
            }
            sprite *SpriteQuad = &Sprites[SpriteI];
            UpdateAnimation(&World.Player, SpriteQuad, (point) {72, 72});
            SpriteI += 4;

            /*UpdateLeaping*/
            if(GetState() == GS_LEAPING) {
                /*PlayerUpdateJumpingAnimation*/
                const static uint8_t PlayerDeltas[] = {0, 4, 6, 8, 9, 10, 11, 12};
                uint8_t HalfTick = World.Player.Tick / 2;
                uint8_t DeltaI = HalfTick < 8 ? HalfTick: 15 - HalfTick;
                for(int I = 0; I < 4; I++) {
                    Sprites[I].Y -= PlayerDeltas[DeltaI];
                }

                /*CreateShadowQuad*/
                if(HalfTick) {
                    sprite *ShadowQuad  = &Sprites[SpriteI]; 
                    ShadowQuad[0] = (sprite) {72, 72, 255, 0};
                    ShadowQuad[1] = (sprite) {80, 72, 255, SPR_HORZ_FLAG};
                    ShadowQuad[2] = (sprite) {72, 80, 255, SPR_VERT_FLAG};
                    ShadowQuad[3] = (sprite) {80, 80, 255, SPR_HORZ_FLAG | SPR_VERT_FLAG};
                    SpriteI += 4;
                } else {
                    ReplaceState(GS_NORMAL);
                }
            }
       
            /*UpdateObjects*/
            for(int MapI = 0; MapI < (int) _countof(World.Maps); MapI++) {
                for(int ObjI = 0; ObjI < World.Maps[MapI].ObjectCount; ObjI++) {
                    object *Object = &World.Maps[MapI].Objects[ObjI];

                    /*TickFrame*/
                    if(GetState() == GS_NORMAL) {
                        if(Object->Speed == 0) {
                            if(Object->Tick > 0) {
                                Object->Tick--; 
                            } else {
                                Object->Dir = Object->StartingDir;
                            }
                            if(Object->Tick % 16 == 8) {
                                Object->IsRight ^= 1;
                            }
                        } else if(Object->Tick > 0) {
                            MoveEntity(Object);
                        } else if(!IsPauseAttempt) {
                            /*RandomMove*/
                            const map *Map = &World.Maps[MapI];
                            int Seed = rand();
                            if(Seed % 64 == 0) {
                                Object->Dir = Seed / 64 % 4;

                                point NewPoint = GetFacingPoint(Object->Pos, Object->Dir);

                                /*FetchQuadPropSingleMap*/
                                int Quad = -1;
                                int Prop = QUAD_PROP_SOLID;
                                if(PointInMap(Map, NewPoint)) {
                                    Quad = Map->Quads[NewPoint.Y][NewPoint.X];
                                    Prop = World.QuadProps[Quad];
                                }
                                
                                /*CheckForCollisions*/
                                int WillCollide = WillObjectCollide(&World.Player, NewPoint);
                                for(int I = 0; I < World.Maps[MapI].ObjectCount && !WillCollide; I++) {
                                    const object *OtherObject = &World.Maps[MapI].Objects[I]; 
                                    if(Object != OtherObject) {
                                        WillCollide = WillObjectCollide(OtherObject, NewPoint); 
                                    }
                                } 

                                /*StartMovingEntity*/
                                if(Prop == QUAD_PROP_NONE && !WillCollide) {
                                    Object->Tick = 32;
                                    Object->IsMoving = 1;
                                }
                            }
                        }
                    }

                    /*GetObjectRenderPos*/
                    point QuadPt = {
                        Object->Pos.X - World.Player.Pos.X + 72,
                        Object->Pos.Y - World.Player.Pos.Y + 72
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

                    /*RenderObject*/
                    if(ObjectInUpdateBounds(QuadPt) && SpriteI < 40) {
                        sprite *SpriteQuad = &Sprites[SpriteI];
                        UpdateAnimation(Object, SpriteQuad, QuadPt);
                        SpriteI += 4;
                    }
                }
            }

            /*MutTileUpdate*/
            int TickCycle = Tick / 32 % 9;
            if(Tick % 16 == 0 && World.IsOverworld) {
                /*FlowersUpdate*/
                memcpy(World.TileData[3], FlowerData[TickCycle % 3], 64);

                /*WaterUpdate*/
                int TickMod = TickCycle < 5 ? TickCycle : 9 - TickCycle;
                memcpy(World.TileData[20], WaterData[TickMod], 64);
            }

            /*UpdatePallete*/
            if(GetState() != GS_TRANSITION && World.Player.Tick <= 0) {
                int PalleteI = World.Maps[World.MapI].PalleteI;
                memcpy(Bitmap.Colors, Palletes[PalleteI], sizeof(Palletes[PalleteI]));
            }
        }

        RenderTileMap(&World, ScrollX, ScrollY);
        RenderSprites(SpriteI);
        RenderWindowMap(&World);
        if(IsFullscreen) UpdateFullscreen(Window);

        EndFrame();

        /*NextFrame*/
        InvalidateRect(Window, NULL, 0);
        Tick++;
    }

    DestroyXAudio2(&XAudio2);
    DestroyCom(&Com);
    return 0;
}

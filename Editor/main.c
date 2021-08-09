#include "shared.h"

typedef struct text {
    BOOL HasText;
    point Pos;
    char Data[16];
    int Index;
} text;

typedef struct area {
    rect Rect;
    point Cursor;
} area;

typedef struct area_context {
    BOOL LockedMode;
    union {
        struct area Areas[4];
        struct {
            area QuadDataArea;
            area TileDataArea;
            area QuadMapArea;
            area TextArea;
        };
    };
    point QuadMapCam;
} area_context;

typedef struct render_context {
    BOOL Fullscreen;
    BOOL ToRender;
    bitmap Bitmap;
} render_context;

typedef struct quad_context {
    array_rect *TileMap;
    array_rect *QuadMap;
    uint8_t *QuadData;
} quad_context;

typedef struct command_context {
    char Command[16];
    int Index;
    BOOL CommandMode;
    BOOL InsertMode;
    BOOL Changed;
} command_context;

typedef struct shared_context {
    area_context *AreaContext;
    command_context *CommandContext;
    render_context *RenderContext;
    text *Texts;
} shared_context;

/*Section: Text*/
static text *FindTextPt(text *Texts, point Point) {
    text *Text = Texts;
    for(int Index = 0; Index < 16; Index++) {
        if(ComparePoints(Text->Pos, Point)) {
            return Text;
        }
        Text++;
    }
    return NULL;
}
static text *AllocText(text *Texts, point Point) {
    text *Text = Texts;
    for(int Index = 0; Index < 16; Index++) {
        if(!Text->HasText) {
            *Text = (text) {
                .HasText = TRUE,
                .Pos = Point
            };
            return Text;
        }
        Text++;
    }
    return NULL;
}

/*Section: Area*/
static point GetRelativeQuadPoint(area *Area) {
    return (point) {
        .X = (Area->Cursor.X - Area->Rect.X) / 2,
        .Y = (Area->Cursor.Y - Area->Rect.Y) / 2
    };
}

static int GetRelativeQuadIndex(area *Area) {
    int QuadX = (Area->Cursor.X - Area->Rect.X) / 2;
    int QuadY = (Area->Cursor.Y - Area->Rect.Y) / 2;
    int QuadWidth = Area->Rect.Width / 2;
    return QuadX + QuadY * QuadWidth;
}

/*Section: Area Context*/
static BOOL MoveSelectTile(area_context *AreaContext, area *Area, point DeltaPoint) {
    point AreaCursorOld = Area->Cursor;
    int Transform = AreaContext->LockedMode ? 2 : 1;
    rect BoundRect = {
        .X = Area->Rect.X,
        .Y = Area->Rect.Y,
        .Width = Area->Rect.Width - Transform,
        .Height = Area->Rect.Height - Transform
    };
    point DeltaTile = MultiplyPoint(DeltaPoint, Transform);
    point Unbound = AddPoints(Area->Cursor, DeltaTile);
    Area->Cursor = ClampPoint(Unbound, &BoundRect);
    return !ComparePoints(Area->Cursor, AreaCursorOld);
}

static BOOL BoundCam(area_context *AreaContext, array_rect *QuadMap, point DeltaPoint) {
    point QuadMapCamOld = AreaContext->QuadMapCam;
    AreaContext->QuadMapCam = AddPoints(AreaContext->QuadMapCam, DeltaPoint);
    rect ClampRect = {
        .X = 0,
        .Y = 0,
        .Width = QuadMap->Width - 8,
        .Height = QuadMap->Height - 8
    };

    AreaContext->QuadMapCam = ClampPoint(AreaContext->QuadMapCam, &ClampRect);
    return !ComparePoints(AreaContext->QuadMapCam, QuadMapCamOld);
}

static void UpdateQuadMapArea(quad_context *QuadContext, area_context *AreaContext) {
    rect QuadRect = {
        .X = AreaContext->QuadMapCam.X,
        .Y = AreaContext->QuadMapCam.Y,
        .Width = AreaContext->QuadMapArea.Rect.Width / 2,
        .Height = AreaContext->QuadMapArea.Rect.Height / 2
    };
    TranslateQuadsToTiles(QuadContext->QuadData, QuadContext->TileMap, &AreaContext->QuadMapArea.Rect, QuadContext->QuadMap, &QuadRect);
}

static BOOL MoveQuadMapCam(quad_context *QuadContext, area_context *AreaContext, point DeltaPoint) {
    BOOL ToRender = BoundCam(AreaContext, QuadContext->QuadMap, DeltaPoint);
    if(ToRender) {
        UpdateQuadMapArea(QuadContext, AreaContext);
    }
    return ToRender;
}

static point GetPlacePoint(area_context *AreaContext) {
    point QuadViewPoint = GetRelativeQuadPoint(&AreaContext->QuadMapArea);
    return (point) {
        .X = QuadViewPoint.X + AreaContext->QuadMapCam.X,
        .Y = QuadViewPoint.Y + AreaContext->QuadMapCam.Y
    };
}

static point GetRelativeCursor(area *Area) {
    return (point) {
        .X = Area->Cursor.X - Area->Rect.X,
        .Y = Area->Cursor.Y - Area->Rect.Y
    };
}

static BOOL ResizeQuadMap(quad_context *QuadContext, area_context *AreaContext, int DeltaQuadWidth, int DeltaQuadHeight) {
    int NewWidth = QuadContext->QuadMap->Width + DeltaQuadWidth;
    int NewHeight = QuadContext->QuadMap->Height + DeltaQuadHeight;
    BOOL ValidWidth = NewWidth >= 8 && NewWidth <= 255;
    BOOL ValidHeight = NewHeight >= 8 && NewHeight <= 255;
    BOOL ValidDimensions = ValidWidth && ValidHeight;
    if(ValidDimensions) {
        ArrayRectChangeSize(QuadContext->QuadMap, NewWidth, NewHeight);
        BoundCam(AreaContext, QuadContext->QuadMap, CreatePoint(0, 0));
        UpdateQuadMapArea(QuadContext, AreaContext);
    }
    return ValidDimensions;
}

/**/
static void RenderSelect(array_rect *Offscreen, int ColorIndex, point Cursor, int Width, int Height) {
    int X = Cursor.X * TileLength;
    int Y = Cursor.Y * TileLength;
    memset(ArrayRectGet(Offscreen, X, Y), ColorIndex, Width);
    for(int OffY = 1; OffY < Height - 1; OffY++) {
        *ArrayRectGet(Offscreen, X, Y + OffY) = ColorIndex;
        *ArrayRectGet(Offscreen, X + Width - 1, Y + OffY) = ColorIndex;
    }
    memset(ArrayRectGet(Offscreen, X, Y + Height - 1), ColorIndex, Width);
}

static void RenderQuadSelect(array_rect *Offscreen, int ColorIndex, int TileX, int TileY) {
    int X = TileX * 8;
    int Y = TileY * 8;
    memset(ArrayRectGet(Offscreen, X, Y), ColorIndex, 16);
    for(int OffY = 1; OffY < 15; OffY++) {
        *ArrayRectGet(Offscreen, X, Y + OffY) = ColorIndex;
        *ArrayRectGet(Offscreen, X + 15, Y + OffY) = ColorIndex;
    }
    memset(ArrayRectGet(Offscreen, X, Y + 15), ColorIndex, 16);
}


static void UpdateMyWindow(HWND Window, render_context *RenderContext) {
    PAINTSTRUCT Paint;
    RenderOnscreen(Window, BeginPaint(Window, &Paint), RenderContext->Bitmap.Pixels.Width, RenderContext->Bitmap.Pixels.Height, RenderContext->Bitmap.Pixels.Bytes, &RenderContext->Bitmap.Info.WinStruct);
    EndPaint(Window, &Paint);
}

static BOOL CharIsValid(int Char, int Index, int Size) {
    BOOL CharIsValid = IsAlphaNumeric(Char) || Char == ' ' || Char == '!';
    BOOL CharCanFit = Index + 1 < Size;
    return CharIsValid && CharCanFit;
}

static int32_t UpdateText(char *Text, int Char, int Index, int Size) {
    if(Char == VK_BACK) {
        if(Index > 0) {
            Index--;
        }
        Text[Index] = '\0';
    } else if(CharIsValid(Char, Index, Size)) {
        Text[Index] = Char;
        Index++;
        Text[Index] = '\0';
    }
    return Index;
} 

static int32_t GetQuadProp(array_rect *QuadMap, uint8_t *QuadProps, point QuadPoint) {
    int32_t Quad = *ArrayRectPt(QuadMap, QuadPoint);
    int32_t Prop = QuadProps[Quad];
    return Prop;
}

static text *GetTextPlace(area_context *AreaContext, text *Texts) { 
    text *Text = NULL;
    if(AreaContext->LockedMode) {
        point PlacePt = GetPlacePoint(AreaContext);
        Text = FindTextPt(Texts, PlacePt); 
    }
    return Text;
}

static BOOL UpdateCommand(command_context *Context, area_context *AreaContext, text *Texts, int Char) {
    BOOL ToRender = TRUE;
    text *Text = GetTextPlace(AreaContext, Texts);
    if(Context->CommandMode) {
        Context->Index = UpdateText(Context->Command, Char, Context->Index, _countof(Context->Command));
    } else if(Context->InsertMode) { 
        if(Text) {
            Text->Index = UpdateText(Text->Data, Char, Text->Index, _countof(Text->Data));
        }
    } else if(Char == ':') {
        Context->CommandMode = TRUE;
    } else if(Char == 'i' && Text) {
        Context->InsertMode = TRUE;
    } else {
        ToRender = FALSE;
    }
    return ToRender;
}

static void *GetUserData(HWND Window) {
    return (void *) GetWindowLongPtr(Window, GWLP_USERDATA);
}

static void CloseCommandContext(command_context *Context, char *Error) {
    *Context = (command_context) {
        .Changed = Context->Changed
    };
    strcpy(Context->Command, Error);
}

static BOOL HandleMessages(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam, shared_context *SharedContext) {
    BOOL WasHandled = TRUE;
    area_context *AreaContext = SharedContext->AreaContext;
    command_context *CommandContext = SharedContext->CommandContext;
    render_context *RenderContext = SharedContext->RenderContext;
    text *Texts = SharedContext->Texts;
    switch(Message) {
    case WM_SIZE:
        if(RenderContext->Fullscreen) {
            UpdateFullscreen(Window);
        }
        break;
    case WM_CLOSE:
        if(CommandContext->Changed) {
            CloseCommandContext(CommandContext, "Error");
        } else {
            PostQuitMessage(0);
        }
        break;
    case WM_PAINT:
        UpdateMyWindow(Window, RenderContext);
        break;
    case WM_CHAR:
        RenderContext->ToRender = UpdateCommand(CommandContext, AreaContext, Texts, WParam);
        break;
    default:
        WasHandled = FALSE;
    }
    return WasHandled;
}

static LRESULT CALLBACK MyWndProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam) {
    BOOL Handled = FALSE;
    shared_context *SharedContext = GetUserData(Window);
    if(SharedContext) {
        Handled = HandleMessages(Window, Message, WParam, LParam, SharedContext);
    }
    return Handled ? 0 : DefWindowProc(Window, Message, WParam, LParam);
}

static uint8_t *GetCurrentQuadPropPtr(area *Area, uint8_t *QuadProps) {
    int SetNumber = GetRelativeQuadIndex(Area);
    uint8_t *PropPtr = QuadProps + SetNumber;
    return PropPtr;
}

static void DefaultQuadMap(array_rect *QuadMap) {
    QuadMap->Width = 8;
    QuadMap->Height = 8;
    memset(QuadMap->Bytes, 0, QuadMap->Width * QuadMap->Height);
}

static int ConvertUnicodeToAscii(char *AsciiString, const wchar_t *UnicodeString, int UnicodeCount) {
    int CorrectChars = 0;
    for(int Index = 0; Index < UnicodeCount; Index++) {
        if(*UnicodeString < 128) {
            CorrectChars++;
        }
        *AsciiString++ = *UnicodeString++;
    }
    return CorrectChars;
}

static int WideStringCharCount(wchar_t *WideString) {
    return wcslen(WideString) + 1;
}

static void ExtractQuadMapPath(char *Path, int PathCount) {
    Path[0] = '\0';
    int ArgCount;
    wchar_t *Line = GetCommandLineW();
    wchar_t **Args = CommandLineToArgvW(Line, &ArgCount);
    if(Args) {
        if(ArgCount == 2) {
            int Count = WideStringCharCount(Args[1]);
            if(Count <= PathCount) {
                int Converted = ConvertUnicodeToAscii(Path, Args[1], Count);
                if(Converted < Count) {
                    Path[0] = '\0';
                }
            }
        }
        LocalFree(Args);
    }
}

static BOOL CommandWriteQuadMap(command_context *CommandContext, const char *QuadMapPath, array_rect *QuadMap) {
    BOOL Success = *QuadMapPath && WriteQuadMap(QuadMapPath, QuadMap);
    if(Success) {
        CommandContext->Changed = FALSE;
    }
    return Success;
}

static BOOL WriteAllButError(const char *Path, void *Data, uint32_t BytesToWrite) {
    uint32_t BytesWritten = WriteAll(Path, Data, BytesToWrite);
    return BytesToWrite == BytesWritten;
}

static int GetRelativeCursorIndex(area *Area) {
    point Point = GetRelativeCursor(Area);
    int Index = Point.X + Point.Y * Area->Rect.Width;
    return Index;
}

static void EditQuadAtCursor(quad_context *QuadContext, area_context *AreaContext, area *CurrentArea, area *NextArea) {
    int SelectedTile = GetRelativeCursorIndex(NextArea);

    *ArrayRectPt(QuadContext->TileMap, CurrentArea->Cursor) = SelectedTile;

    point RelativeCursor = GetRelativeCursor(CurrentArea);
    int SetNumber = GetRelativeQuadIndex(CurrentArea);
    int SetPos = RelativeCursor.X % 2 + RelativeCursor.Y % 2 * 2;
    QuadContext->QuadData[SetPos + SetNumber * 4] = SelectedTile;

    UpdateQuadMapArea(QuadContext, AreaContext);
}

static void ProcessComandContext(command_context *CommandContext, quad_context *QuadContext, area_context *AreaContext, UINT32 *Colors, char *QuadMapPath, uint8_t *QuadProps) {
    BOOL Error = FALSE;
    if(strstr(CommandContext->Command, "e ")) {
        strcpy(QuadMapPath, CommandContext->Command + 2);
        if(!ReadQuadMap(CommandContext->Command + 2, QuadContext->QuadMap)) {
            DefaultQuadMap(QuadContext->QuadMap);
            AreaContext->QuadMapCam = CreatePoint(0, 0);
        }
        UpdateQuadMapArea(QuadContext, AreaContext);
    } else if(strstr(CommandContext->Command, "w ")) {
        strcpy(QuadMapPath, CommandContext->Command + 2);
        Error |= !CommandWriteQuadMap(CommandContext, QuadMapPath, QuadContext->QuadMap);
    } else if(strcmp(CommandContext->Command, "w") == 0) {
        Error |= !CommandWriteQuadMap(CommandContext, QuadMapPath, QuadContext->QuadMap);
    } else if(strcmp(CommandContext->Command, "q!") == 0) {

        PostQuitMessage(0);
    } else if(strcmp(CommandContext->Command, "q") == 0) {
        if(CommandContext->Changed) {
            Error = TRUE;
        } else {
            PostQuitMessage(0);
        }
    } else if(strcmp(CommandContext->Command, "c") == 0) {
        UINT32 NewColors[] = {
            0xF8F8F8, 0xA8E058,
            0xA0D0F8, 0x000000
        };
        memcpy(Colors, NewColors, sizeof(NewColors));
    } else if(strcmp(CommandContext->Command, "d") == 0) {
        BOOL QuadDataSuccess = WriteAllButError("QuadData", QuadContext->QuadData, SizeOfQuadData);
        BOOL QuadPropSuccess = WriteAllButError("QuadProps", QuadProps, SizeOfQuadProps);
        Error = !QuadDataSuccess || !QuadPropSuccess;
    } else if(strcmp(CommandContext->Command, "wt") == 0) { 
        
    } else {
        Error = TRUE;
    }
    CloseCommandContext(CommandContext, Error ? "Error" : "");
}


int WINAPI WinMain(HINSTANCE Instance, HINSTANCE InstancePrev, LPSTR CommandLine, int CommandShow) {
    uint8_t TileData[256 * 64] = {};
    text Texts[16] = {};
    array_rect TileMap = CreateStaticArrayRect(35, 25);

    render_context RenderContext = {
        .ToRender = TRUE,
        .Bitmap = InitBitmap(35 * TileLength, 25 * TileLength, EditorPallete, 6)
    };
    command_context CommandContext = {};
    SetCurrentDirectory("../Shared");

    int TileDataCount    = ReadTileData("TileData", TileData, 256);
    int TileNumbersCount = ReadTileData("Numbers" , TileData + TileDataCount *TileSize, 256 - TileDataCount);

    /*Area Setup*/
    area_context AreaContext = {
        .Areas = {
            {
                .Rect.X = 1,
                .Rect.Y = 1,
                .Rect.Width = 16,
                .Rect.Height = 16,
                .Cursor.X = 1,
                .Cursor.Y = 1,
            },
            {
                .Rect.X = 1,
                .Rect.Y = 18,
                .Rect.Width = 16,
                .Rect.Height = 6,
                .Cursor.X = 1,
                .Cursor.Y = 18,
            },
            {
                .Rect.X = 18,
                .Rect.Y = 1,
                .Rect.Width = 16,
                .Rect.Height = 16,
                .Cursor.X = 18,
                .Cursor.Y = 1,
            },
            {
                .Rect.X = 18,
                .Rect.Y = 18,
                .Rect.Width = 16,
                .Rect.Height = 6,
                .Cursor.X = 18,
                .Cursor.Y = 18,
            }
        }
    };

    area *CurrentArea = &AreaContext.QuadDataArea;
    area *NextArea = &AreaContext.TileDataArea;

    uint8_t QuadProps[64];
    ReadQuadProps("QuadProps", QuadProps);

    /*QuadData SetUp*/
    uint8_t QuadData[256];
    ReadQuadData("QuadData", QuadData);

    uint8_t *Set = QuadData;
    uint8_t *TileRow = ArrayRectGet(&TileMap, AreaContext.QuadDataArea.Rect.X, AreaContext.QuadDataArea.Rect.Y);
    for(int SetY = 0; SetY < 8; SetY++) {
        uint8_t *Tile = TileRow;
        for(int SetX = 0; SetX < 8; SetX++) {
            FillQuad(&TileMap, Tile, Set);
            Set += 4;
            Tile += 2;
        }
        TileRow += TileMap.Width * 2;
    }

    /*QuadContext SetUp*/
    char QuadMapPath[16] = {};
    ExtractQuadMapPath(QuadMapPath, _countof(QuadMapPath));
    array_rect QuadMap = InitQuadMap;
    if(*QuadMapPath) {
        if(!ReadQuadMap(QuadMapPath, &QuadMap)) {
            strcpy(CommandContext.Command, "Error");
            DefaultQuadMap(&QuadMap);
        }
    } else {
        DefaultQuadMap(&QuadMap);
    }

    quad_context QuadContext = {
        .TileMap = &TileMap,
        .QuadMap = &QuadMap,
        .QuadData = QuadData
    };

    UpdateQuadMapArea(&QuadContext, &AreaContext);

    /*Initalize TileDataArea*/
    int TileX = AreaContext.TileDataArea.Rect.X;
    int TileY = AreaContext.TileDataArea.Rect.Y;
    for(int TileID = 0; TileID < 96; TileID++) {
        *ArrayRectGet(&TileMap, TileX, TileY) = TileID;
        TileX++;
        if(TileX >= RectRight(&AreaContext.TileDataArea.Rect)) {
            TileX = AreaContext.TileDataArea.Rect.X;
            TileY++;
        }
    }

    /*Init Area Borders*/
    *ArrayRectGet(&TileMap,  0,  0) =  96;
    *ArrayRectGet(&TileMap, 17,  0) = 107;
    *ArrayRectGet(&TileMap, 34,  0) =  98;
    *ArrayRectGet(&TileMap,  0, 17) = 104;
    *ArrayRectGet(&TileMap, 17, 17) = 109;
    *ArrayRectGet(&TileMap, 34, 17) = 105;
    *ArrayRectGet(&TileMap,  0, 24) = 101;
    *ArrayRectGet(&TileMap, 17, 24) = 108;
    *ArrayRectGet(&TileMap, 34, 24) = 103;

    for(TileX = 1; TileX < 17; TileX++) {
        *ArrayRectGet(&TileMap, TileX,  0) =  97;
        *ArrayRectGet(&TileMap, TileX, 17) = 102;
        *ArrayRectGet(&TileMap, TileX, 24) = 102;
    }
    for(TileX = 18; TileX < 34; TileX++) {
        *ArrayRectGet(&TileMap, TileX,  0) =  97;
        *ArrayRectGet(&TileMap, TileX, 17) = 102;
        *ArrayRectGet(&TileMap, TileX, 24) = 102;
    }
    for(TileY = 1; TileY < 17; TileY++) {
        *ArrayRectGet(&TileMap,  0, TileY) =  99;
        *ArrayRectGet(&TileMap, 17, TileY) = 106;
        *ArrayRectGet(&TileMap, 34, TileY) = 100;
    }
    for(TileY = 18; TileY < 24; TileY++) {
        *ArrayRectGet(&TileMap,  0, TileY) =  99;
        *ArrayRectGet(&TileMap, 17, TileY) = 106;
        *ArrayRectGet(&TileMap, 34, TileY) = 100;
    }


    /*Window Creation*/
    HWND Window = CreatePokeWindow("PokeEditor", MyWndProc, RenderContext.Bitmap.Pixels.Width, RenderContext.Bitmap.Pixels.Height);
    if(!Window) {
        return 1;
    }

    shared_context SharedContext = {
        .AreaContext = &AreaContext,
        .CommandContext = &CommandContext,
        .RenderContext = &RenderContext,
        .Texts = Texts,
    };
    SetWindowLongPtr(Window, GWLP_USERDATA, (LONG_PTR) &SharedContext);
    ShowWindow(Window, CommandShow);

    /*Main Loop*/
    rect RestoreWindowRect;
    MSG Message;
    int PropIndexSelected = 0;
    while(GetMessage(&Message, NULL, 0, 0)) {
        if(Message.message == WM_KEYDOWN) {
            BOOL WasCommandMode = CommandContext.CommandMode;
            switch(Message.wParam) {
            case VK_ESCAPE:
                CloseCommandContext(&CommandContext, "");
                RenderContext.ToRender = TRUE;
                break;
            case VK_TAB:
                Swap(&CurrentArea, &NextArea);
                RenderContext.ToRender = TRUE;
                break;
            case VK_F11:
                if(RenderContext.Fullscreen) {
                    RenderContext.Fullscreen = FALSE;  /*Needs to be done first!!!*/
                    SetMyWindowPos(Window, &RestoreWindowRect, WS_OVERLAPPEDWINDOW);
                } else {
                    RenderContext.Fullscreen = TRUE;
                    RestoreWindowRect = GetMyWindowRect(Window);
                    UpdateFullscreen(Window);
                }
                break;
            default:
                if(CommandContext.CommandMode) {
                    if(Message.wParam == VK_RETURN) {
                        ProcessComandContext(&CommandContext, &QuadContext, &AreaContext, RenderContext.Bitmap.Info.Colors, QuadMapPath, QuadProps);
                        RenderContext.ToRender = TRUE;
                    }
                } else if(!CommandContext.InsertMode) {
                    if(CurrentArea == &AreaContext.QuadDataArea && AreaContext.LockedMode) {
                        switch(Message.wParam) {
                        case 'J':
                            PropIndexSelected = (PropIndexSelected + 1) % CountofQuadProp;
                            RenderContext.ToRender = TRUE;
                            break;
                        case 'L':
                            *GetCurrentQuadPropPtr(CurrentArea, QuadProps) ^= (1 << PropIndexSelected);
                            RenderContext.ToRender = TRUE;
                            break;
                        }
                    } else if(CurrentArea == &AreaContext.QuadMapArea) {
                        switch(Message.wParam) {
                        /*Move Camera*/
                        case 'K':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext,  &AreaContext, CreatePoint(0, -1));
                            break;
                        case 'H':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext,  &AreaContext, CreatePoint(-1, 0));
                            break;
                        case 'J':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext, &AreaContext, CreatePoint(0, 1));
                            break;
                        case 'L':
                            RenderContext.ToRender = MoveQuadMapCam(&QuadContext, &AreaContext, CreatePoint(1, 0));
                            break;
                        /*Resize*/
                        case VK_UP:
                            if(ArrayRectRowIsZeroed(&QuadMap, QuadMap.Height - 1)) {
                                RenderContext.ToRender = ResizeQuadMap(&QuadContext, &AreaContext, 0, -1);
                            }
                            break;
                        case VK_LEFT:
                            if(ArrayRectColIsZeroed(&QuadMap, QuadMap.Width - 1)) {
                                RenderContext.ToRender = ResizeQuadMap(&QuadContext, &AreaContext, -1, 0);
                            }
                            break;
                        case VK_DOWN:
                            RenderContext.ToRender = ResizeQuadMap(&QuadContext, &AreaContext, 0, 1);
                            break;
                        case VK_RIGHT:
                            RenderContext.ToRender = ResizeQuadMap(&QuadContext, &AreaContext, 1, 0);
                            break;
                        }
                    }
                    switch(Message.wParam) {
                    /*Current select control*/
                    case 'W':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, CreatePoint(0, -1));
                        break;
                    case 'A':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, CreatePoint(-1, 0));
                        break;
                    case 'S':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, CreatePoint(0, 1));
                        break;
                    case 'D':
                        RenderContext.ToRender = MoveSelectTile(&AreaContext, CurrentArea, CreatePoint(1, 0));
                        break;
                    /*Commands*/
                    case '0':
                        AreaContext.LockedMode ^= TRUE;
                        CurrentArea->Cursor = RectTopLeft(&CurrentArea->Rect);
                        CurrentArea = &AreaContext.QuadDataArea;
                        NextArea->Cursor = RectTopLeft(&NextArea->Rect);
                        NextArea = AreaContext.LockedMode ? &AreaContext.QuadMapArea : &AreaContext.TileDataArea;
                        RenderContext.ToRender = TRUE;
                        break;
                    }
                }
            }
            if(GetAsyncKeyState(VK_RETURN) && !WasCommandMode) {
                if(AreaContext.LockedMode) {
                    if(CurrentArea == &AreaContext.QuadMapArea) {
                        area *Area = &AreaContext.QuadDataArea;
                        int SetX = (Area->Cursor.X - Area->Rect.X) / 2;
                        int SetY = (Area->Cursor.Y - Area->Rect.Y) / 2;
                        int SelectedQuadTile = SetX + SetY * 8;

                        point PlacePt = GetPlacePoint(&AreaContext);
                        uint8_t *PlacePtr = ArrayRectPt(&QuadMap, PlacePt);
                        if(*PlacePtr != SelectedQuadTile) {
                            if((*PlacePtr & QuadPropMessage) && !(SelectedQuadTile & QuadPropMessage)) {
                                text *Text = FindTextPt(Texts, PlacePt); 
                                if(Text) {
                                    Text->HasText = FALSE;
                                }
                            }
                            *PlacePtr = SelectedQuadTile;
                            uint8_t *Tile = ArrayRectGet(&TileMap, CurrentArea->Cursor.X, CurrentArea->Cursor.Y);
                            FillQuad(&TileMap, Tile, QuadData + SelectedQuadTile * 4);
                            if(*PlacePtr & QuadPropMessage) {
                                text *Text = AllocText(Texts, PlacePt);
                                if(!Text) {
                                    CloseCommandContext(&CommandContext, "OOM");
                                }
                            }
                            CommandContext.Changed = TRUE;
                        }
                    }
                } else if(CurrentArea == &AreaContext.QuadDataArea) {
                    EditQuadAtCursor(&QuadContext, &AreaContext, CurrentArea, NextArea);
                }
                RenderContext.ToRender = TRUE;
            }
        }
        TranslateMessage(&Message);
        DispatchMessage(&Message);
        if(RenderContext.ToRender) {
            if(CurrentArea == &AreaContext.QuadMapArea) {
                point Place = GetPlacePoint(&AreaContext);
                if(GetQuadProp(&QuadMap, QuadProps, Place) & QuadPropMessage) {
                    const char *Data = "OOM";
                    text *Text = FindTextPt(Texts, Place);
                    if(Text) {
                        Data = Text->Data;
                    } else {
                        Text = AllocText(Texts, Place);
                        if(Text) {
                            Data = "";
                        }
                    }
                    PlaceFormattedText(&TileMap, &AreaContext.TextArea.Rect, 
                            "Dim %3d %3d" "\n" "Pos %3d %3d" "\n", 
                            QuadMap.Width, QuadMap.Height, Place.X, Place.Y, Data);
                } else {
                    PlaceFormattedText(&TileMap, &AreaContext.TextArea.Rect, 
                            "Dim %3d %3d" "\n" "Pos %3d %3d" "\n", 
                            QuadMap.Width, QuadMap.Height, Place.X, Place.Y);
                }
            } else if(CurrentArea == &AreaContext.QuadDataArea && AreaContext.LockedMode) {
                int SetNumber = GetRelativeQuadIndex(CurrentArea);
                int Prop = *GetCurrentQuadPropPtr(CurrentArea, QuadProps);
                PlaceFormattedText(&TileMap, &AreaContext.TextArea.Rect, 
                        "Solid %d" "\n" "Edge  %d" "\n" "Msg   %d" "\n" "Water %d" "\n", !!(Prop & QuadPropSolid), 
                        !!(Prop & QuadPropEdge), !!(Prop & QuadPropMessage), !!(Prop & QuadPropWater));
            } else {
                PlaceText(&TileMap, &AreaContext.TextArea.Rect, "");
            }
            rect LastRow = {
                .X = AreaContext.TextArea.Rect.X,
                .Y = RectBottom(&AreaContext.TextArea.Rect) - 1,
                .Width = AreaContext.TextArea.Rect.Width,
                .Height = 1,
            };
            PlaceFormattedText(&TileMap, &LastRow, ":%s", CommandContext.Command);

            RenderTiles(&RenderContext.Bitmap.Pixels, &TileMap, TileData, CreatePoint(0, 0));

            const int ColorIndexActive = 4;
            const int ColorIndexInactive = 5;
            if(!CommandContext.CommandMode && !CommandContext.InsertMode) {
                int Length = AreaContext.LockedMode ? 16 : 8;
                RenderSelect(&RenderContext.Bitmap.Pixels, ColorIndexActive, CurrentArea->Cursor, Length, Length);
                RenderSelect(&RenderContext.Bitmap.Pixels, ColorIndexInactive, NextArea->Cursor, Length, Length);
            }
            if(CurrentArea == &AreaContext.QuadDataArea && AreaContext.LockedMode) {
                point SelectPoint = CreatePoint(AreaContext.TextArea.Rect.X + 6, AreaContext.TextArea.Rect.Y + PropIndexSelected);
                RenderSelect(&RenderContext.Bitmap.Pixels, ColorIndexActive, SelectPoint, 8, 8);
            }
            InvalidateRect(Window, NULL, FALSE);
            RenderContext.ToRender = FALSE;
        }
    }
    return Message.wParam;
}

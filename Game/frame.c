#include "input.h"
#include "frame.h"
#include "render.h"
#include "timer.h"
#include "win32.h"
#include "world.h"

void BeginFrame(void) {
    StartFrameTimer();

    ProcessMessages();
    UpdateInput();

    if(VirtButtons[BT_FULLSCREEN] == 1) {
        ToggleFullscreen();
    }
}

void EndFrame(void) {
    UpdateMusicSwitch();
    UpdateActiveWorld();

    RenderTileMap(); 
    RenderSprites();
    RenderWindowMap();
    RenderTopSprites();

    UpdateFullscreen();
    EndFrameTimer();
    RenderWindow();
}

void NextFrame(void) {
    EndFrame();
    BeginFrame();
}

void Pause(int Delay) {
    while(Delay-- > 0) {
        NextFrame();
    }
}


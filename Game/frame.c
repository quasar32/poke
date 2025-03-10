#include "input.h"
#include "frame.h"
#include "render.h"
#include "world.h"
#include "misc.h"
#include <stdlib.h>
#include <SDL3/SDL.h>
#include <stdio.h>
#include <limits.h>

#define PERIOD_DELAY (1000000000 / 60)

static int g_KeyMappings[COUNTOF_BT] = {
    [BT_UP] = SDLK_UP,
    [BT_LEFT] = SDLK_LEFT,
    [BT_DOWN] = SDLK_DOWN,
    [BT_RIGHT] = SDLK_RIGHT,
    [BT_A] = SDLK_X,
    [BT_B] = SDLK_Z,
    [BT_START] = SDLK_RETURN,
    [BT_FULLSCREEN] = SDLK_F11,
};

static Uint64 g_BeginNS;

static void EndFrameTimer(void) {
    Uint64 EndNS = SDL_GetTicksNS(); 
    Uint64 DeltaNS = EndNS - g_BeginNS;
    if (DeltaNS < PERIOD_DELAY) {
        SDL_DelayPrecise(PERIOD_DELAY - DeltaNS);
    }
    g_BeginNS = SDL_GetTicksNS();
}

void BeginFrame(void) {
    for (int I = 0; I < COUNTOF_BT; I++) {
        if (VirtButtons[I] > 0 && VirtButtons[I] < INT_MAX) {
            VirtButtons[I]++;
        }
    }
    SDL_Event Event;
    while (SDL_PollEvent(&Event)) {
        switch (Event.type) {
        case SDL_EVENT_QUIT:
            exit(EXIT_SUCCESS);
        case SDL_EVENT_KEY_DOWN:
            for (int I = 0; I < COUNTOF_BT; I++) {
                if (Event.key.key == g_KeyMappings[I]) {
                    VirtButtons[I] = 1;
                }
            }
            break;
        case SDL_EVENT_KEY_UP:
            for (int I = 0; I < COUNTOF_BT; I++) {
                if (Event.key.key == g_KeyMappings[I]) {
                    VirtButtons[I] = 0;
                }
            }
            break;
        }
    }
    if (VirtButtons[BT_FULLSCREEN] == 1) {
        bool IsFullscreen = SDL_GetWindowFlags(g_Window) & SDL_WINDOW_FULLSCREEN;
        SDL_SetWindowFullscreen(g_Window, !IsFullscreen);
    }
}
    
void RenderFrame(void) {
    uint8_t(*Pixels)[BM_WIDTH][3];
    int Pitch;
    SDL_LockTexture(g_Texture, NULL, (void **) &Pixels, &Pitch);
    for (int Y = 0; Y < BM_HEIGHT; Y++) {
        for (int X = 0; X < BM_WIDTH; X++) {
            SDL_Color Color = g_Pallete[g_Pixels[Y][X]];
            Pixels[Y][X][0] = Color.b;
            Pixels[Y][X][1] = Color.g;
            Pixels[Y][X][2] = Color.r;
        }
    }
    SDL_UnlockTexture(g_Texture);
    SDL_RenderTexture(g_Renderer, g_Texture, NULL, NULL);
    SDL_RenderPresent(g_Renderer);
}

void EndFrame(void) {
    UpdateMusicSwitch();
    UpdateActiveWorld();

    RenderTileMap(); 
    RenderSprites();
    RenderWindowMap();
    RenderTopSprites();

    RenderFrame();
    EndFrameTimer();
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


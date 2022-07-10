#include <xinput.h>

#include "input.h"
#include "procs.h"
#include "win32.h"

typedef DWORD WINAPI xinput_get_state(DWORD, XINPUT_STATE *);

typedef struct virt_button_entry {
    int Button;
    int Key;
} virt_button_entry; 

static xinput_get_state *g_XInputGetState; 

static const virt_button_entry g_VirtButtonMap[] = {
    [BT_UP] = {
        .Button = XINPUT_GAMEPAD_DPAD_UP,
        .Key = VK_UP
    },
    [BT_LEFT] = {
        .Button = XINPUT_GAMEPAD_DPAD_LEFT,
        .Key = VK_LEFT,
    },
    [BT_DOWN] = {
        .Button = XINPUT_GAMEPAD_DPAD_DOWN,
        .Key = VK_DOWN
    },
    [BT_RIGHT] = {
        .Button = XINPUT_GAMEPAD_DPAD_RIGHT,
        .Key = VK_RIGHT
    },
    [BT_A] = {
        .Button = XINPUT_GAMEPAD_A,
        .Key = 'X' 
    },
    [BT_B] = {
        .Button = XINPUT_GAMEPAD_B,
        .Key = 'Z' 
    },
    [BT_START] = {
        .Button = XINPUT_GAMEPAD_START,
        .Key = VK_RETURN
    },
    [BT_FULLSCREEN] = {
        .Button = XINPUT_GAMEPAD_BACK,
        .Key = VK_F11 
    }
};

int VirtButtons[COUNTOF_BT];

static void GetUnifiedXInput(XINPUT_STATE *State) {
    if(g_XInputGetState && g_XInputGetState(0, State) == ERROR_SUCCESS) { 
        if(State->Gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
            State->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
        } 
        if(State->Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
            State->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
        }
        if(State->Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
            State->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
        }
        if(State->Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
            State->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        } 
    } else {
        *State = (XINPUT_STATE) {0}; 
    }
}

static void UpdateVirtButtons(const XINPUT_STATE *XInputState) {
    int IsAltDown = GetAsyncKeyState(VK_MENU);
    for(size_t I = 0; I < _countof(g_VirtButtonMap); I++) {
        int IsButtonDown = XInputState->Gamepad.wButtons & g_VirtButtonMap[I].Button;
        int IsKeyDown = !IsAltDown && GetAsyncKeyState(g_VirtButtonMap[I].Key);
        VirtButtons[I] = IsButtonDown || IsKeyDown ? VirtButtons[I] + 1 : 0;
    }
} 

static void ReleaseAllVirtButtons(void) {
    memset(VirtButtons, 0, sizeof(VirtButtons));
}

void InitXInput(void) {
    FARPROC Proc;
    LoadProcsVersioned(
        3,
        (const char *[]) {
            "xinput1_4.dll",
            "xinput1_3.dll",
            "xinput9_1_0.dll"
        },
        1,
        &(const char *) {"XInputGetState"},
        &Proc
    );

    g_XInputGetState = (xinput_get_state *) Proc;
}

void UpdateInput(void) {
    if(IsInputActive()) {
        XINPUT_STATE State;
        GetUnifiedXInput(&State);
        UpdateVirtButtons(&State);
    } else {
        ReleaseAllVirtButtons();
    }
}

#include "input.h"
#include "procs.h"

#include <xinput.h> /*NOTE: must be after input.h because windows.h*/

typedef DWORD WINAPI xinput_get_state(DWORD, XINPUT_STATE *);
typedef DWORD WINAPI xinput_set_state(DWORD, XINPUT_VIBRATION *);

struct {
    HMODULE Library;
    xinput_get_state *GetState;
    xinput_set_state *SetState;
} static XInput; 

int VirtButtons[COUNTOF_BT];

static DWORD XInputGetStateStub(DWORD, XINPUT_STATE *) {
    return ERROR_DEVICE_NOT_CONNECTED;
}

static DWORD XInputSetStateStub(DWORD, XINPUT_VIBRATION *) {
    return ERROR_DEVICE_NOT_CONNECTED;
}

BOOL InitXInput(void) {
    FARPROC Procs[2];
    XInput.Library = LoadProcsVersioned(
        3,
        (const char *[]) {
            "xinput1_4.dll",
            "xinput1_3.dll",
            "xinput9_1_0.dll"
        },
        2,
        (const char *[]) {
            "XInputGetState", 
            "XInputSetState" 
        },
        Procs
    );

    XInput.GetState = (xinput_get_state *) Procs[0] ? : XInputGetStateStub;
    XInput.SetState = (xinput_set_state *) Procs[1] ? : XInputSetStateStub;

    return TRUE;
}

void UpdateInput(BOOL IsActive) {
    if(IsActive) {
        /*UpdateXInput*/
        XINPUT_STATE XInputState;
        if(XInput.GetState && XInput.GetState(0, &XInputState) == ERROR_SUCCESS) { 
            if(XInputState.Gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                XInputState.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
            } 
            if(XInputState.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                XInputState.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
            }
            if(XInputState.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                XInputState.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
            }
            if(XInputState.Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                XInputState.Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
            } 
        } else {
            XInputState = (XINPUT_STATE) {0}; 
        }

        /*UpdateVirtButtons*/
        struct {
            int Button;
            int Key;
        } VirtButtonMap[] = {
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
        int IsAltDown = GetAsyncKeyState(VK_MENU);
        for(size_t I = 0; I < _countof(VirtButtonMap); I++) {
            int IsButtonDown = XInputState.Gamepad.wButtons & VirtButtonMap[I].Button;
            int IsKeyDown = !IsAltDown && GetAsyncKeyState(VirtButtonMap[I].Key);
            VirtButtons[I] = IsButtonDown || IsKeyDown ? VirtButtons[I] + 1 : 0;
        }
    } else {
        memset(VirtButtons, 0, sizeof(VirtButtons));
    }
}

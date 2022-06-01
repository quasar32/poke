#include "coord.h"

dim_rect WinToDimRect(RECT WinRect) {
    return (dim_rect) {
        .X = WinRect.left,
        .Y = WinRect.top,
        .Width = WinRect.right - WinRect.left,
        .Height = WinRect.bottom - WinRect.top
    };
}

dim_rect GetDimClientRect(HWND Window) {
    RECT WinClientRect;
    GetClientRect(Window, &WinClientRect);
    return WinToDimRect(WinClientRect);
}

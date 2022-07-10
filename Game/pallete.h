#ifndef PALLETE_H
#define PALLETE_H

#include "render.h"

typedef enum pallete {
    PAL_DEFAULT,
    PAL_PALLETE,
    PAL_ROUTE_1,
    PAL_OAK
} pallete;

static const RGBQUAD g_Palletes[][4] = {
    [PAL_DEFAULT] = {
        {0xFF, 0xEF, 0xFF, 0x00},
        {0xA8, 0xA8, 0xA8, 0x00},
        {0x80, 0x80, 0x80, 0x00},
        {0x10, 0x10, 0x18, 0x00}
    },
    [PAL_PALLETE] = {
        {0xFF, 0xEF, 0xFF, 0x00},
        {0xDE, 0xE7, 0xCE, 0x00},
        {0xFF, 0xD6, 0xA5, 0x00},
        {0x10, 0x10, 0x18, 0x00}
    },
    [PAL_ROUTE_1] = {
        {0xFF, 0xEF, 0xFF, 0x00},
        {0x5A, 0xE7, 0xAD, 0x00},
        {0xFF, 0xD6, 0xA5, 0x00},
        {0x10, 0x10, 0x18, 0x00}
    },
    [PAL_OAK] = {
        {0xFF, 0xEF, 0xFF, 0x00},
        {0x8C, 0xB5, 0xF7, 0x00},
        {0x9C, 0x73, 0x84, 0x00},
        {0x10, 0x10, 0x18, 0x00}
    }
};

static inline void SetPallete(pallete PalleteI) {
    memcpy(g_Bitmap.Colors, g_Palletes[PalleteI], sizeof(g_Palletes[PalleteI]));
}

static inline void SetPalleteTransition(pallete I, int C1, int C2, int C3) {
    g_Bitmap.Colors[1] = g_Palletes[I][C1];
    g_Bitmap.Colors[2] = g_Palletes[I][C2];
    g_Bitmap.Colors[3] = g_Palletes[I][C3];
}

static inline void ClearPallete(void) {
    for(int I = 0; I < 4; I++) {
        g_Bitmap.Colors[I] = g_Palletes[PAL_DEFAULT][0];
    }
}

#endif

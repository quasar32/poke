#pragma once

typedef enum option_names {
    OPT_TEXT_SPEED,
    OPT_BATTLE_ANIMATION, 
    OPT_BATTLE_STYLE,
    OPT_CANCEL
} option_names;

#define COUNTOF_OPTIONS 4

typedef struct option {
    const int Y;
    const int Xs[3];
    const int Count;
    int I;
} option;

extern option g_Options[4];

void OpenOptions(void);

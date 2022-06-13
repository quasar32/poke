#include "window_map.h"

typedef struct option {
    int Y;
    int Xs[3];
    int I;
    int Count;
} option;

typedef struct options_menu {
    window_task WindowTask;
    option E[4]; 
    int I;
} options_menu;

void GS_OPTIONS(void);

void PlaceOptionCursor(const option *Option, int Tile);
void ChangeOptionX(option *Option, int NewOptionI);
void PlaceOptionsMenu(options_menu *Options);

extern options_menu Options;


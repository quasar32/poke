#include "misc.h"
#include <stdlib.h>

void FatalSDL(const char *FuncName) {
    SDL_Log("%s: %s\n", FuncName, SDL_GetError());
    SDL_Quit();
    exit(EXIT_FAILURE);
}

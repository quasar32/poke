#pragma once

#include <SDL3/SDL.h>

#define countof(a) (sizeof(a) / sizeof((a)[0]))

[[noreturn]]
void FatalSDL(const char *FuncName);

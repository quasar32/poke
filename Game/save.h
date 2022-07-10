#ifndef SAVE_H
#define SAVE_H

#include <stdbool.h>

#include "world.h"
#include "window_task.h"

typedef struct save_rect {
    window_task WindowTask;
    const rect Rect;
} save_rect;

extern int SaveSec;
extern int StartSaveSec;

extern save_rect ContinueSaveRect; 
extern save_rect StartSaveRect;

void ReadSaveHeader(void);
void ReadSave(world *World);

BOOL WriteSaveHeader(void);
BOOL WriteSave(const world *World);

void PlaceSave(rect Rect);

#endif

#ifndef SAVE_H
#define SAVE_H

#include "world.h"
#include "window_task.h"
#include "misc.h"

typedef struct save_rect {
    window_task WindowTask;
    const rect Rect;
} save_rect;

extern save_rect g_ContinueSaveRect; 

extern char g_Name[8];
extern char g_Rival[8];

void ReadSaveHeader(void);
void ReadSave(void);

bool WriteSaveHeader(void);
bool WriteSave(void);

void PlaceSave(rect Rect);
void ResetSaveSec(void);
void UpdateSaveSec(void);
void StartSaveCounter(void);

#endif

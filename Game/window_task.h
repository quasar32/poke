#ifndef WINDOW_TASK_H
#define WINDOW_TASK_H

#include <windows.h>

#include "list.h"

typedef enum task_type {
    TT_INVENTORY,
    TT_MENU,
    TT_SAVE,
    TT_DISPLAY
} task_type;

typedef struct window_task {
    list_head Node; 
    task_type Type;
} window_task;

extern BOOL HasTextBox;

void PushWindowTask(window_task *Task);
window_task *PopWindowTask(void);
window_task *RemoveWindowTask(void);
void ClearWindowStack(void);
void ExecuteAllWindowTasks(void);

#endif

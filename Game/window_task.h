#ifndef WINDOW_TASK_H
#define WINDOW_TASK_H

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

extern bool HasTextBox;

void PushWindowTask(window_task *Task);
void PopWindowTask(void);
void RemoveWindowTask(void);
void ClearWindowStack(void);
void ExecuteAllWindowTasks(void);

#endif

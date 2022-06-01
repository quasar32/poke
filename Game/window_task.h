#ifndef WINDOW_TASK_H
#define WINDOW_TASK_H

#include <assert.h>
#include <stdbool.h>

#include "list.h"

typedef enum task_type {
    TT_INVENTORY,
    TT_MENU,
    TT_SAVE,
    TT_OPTIONS,
    TT_DISPLAY
} task_type;

typedef struct window_task {
    list_node Node; 
    task_type Type;
} window_task;

extern bool HasTextBox;
extern list_node WindowTaskHead;

void PushWindowTask(window_task *Task);
window_task *PopWindowTask(void);
window_task *RemoveWindowTask(void);
void ClearWindowStack(void);
void ExecuteAllWindowTasks(void);

[[maybe_unused]]
static inline window_task *GetTopWindowTask(void) {
    return LIST_FIRST_ENTRY(&WindowTaskHead, window_task, Node);
}

#define GET_WINDOW_TASK_TYPE(Type)          \
    _Generic(                               \
        (Type),                             \
        inventory *: TT_INVENTORY,          \
        menu *: TT_MENU,                    \
        save_rect *: TT_SAVE,               \
        options_menu *: TT_OPTIONS         \
    )

#define GET_TOP_WINDOW_TASK(TaskType) ({                                 \
    window_task *TopTask = GetTopWindowTask();                           \
    assert(GET_WINDOW_TASK_TYPE((TaskType *) NULL) == TopTask->Type);    \
    CONTAINER_OF(TopTask, TaskType, WindowTask);                         \
})

#endif

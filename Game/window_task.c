#include <assert.h>
#include <stdio.h>

#include "save.h"
#include "options.h"
#include "text.h"
#include "window_task.h"

BOOL HasTextBox;

static LIST_HEAD(WindowTaskHead);

static void ExecuteWindowTask(window_task *Task) {
    switch(Task->Type) {
    case TT_INVENTORY:
        {
            inventory *Inventory = CONTAINER_OF(Task, inventory, WindowTask);
            PlaceInventory(Inventory);
        }
        break;
    case TT_MENU:
        {
            menu *Menu = CONTAINER_OF(Task, menu, WindowTask);
            PlaceMenu(Menu); 
        }
        break;
    case TT_DISPLAY:
        {
            StartDisplayItem(g_Inventory); 
        }
        break;
    default:
        assert(!"ExecuteWindowTask Invalid");
    } 
}

void PushWindowTask(window_task *Task) {
    ListPush(&WindowTaskHead, &Task->Node); 
    switch(Task->Type) {
    case TT_MENU:
        menu *Menu = CONTAINER_OF(Task, menu, WindowTask);
        if(Menu->Flags & MF_AUTO_RESET) {
            Menu->SelectI = 0;
        }
        break;
    case TT_DISPLAY:
        DisplayItem = (item) {
            .Count = 1, 
            .ID = g_Inventory->Items[g_Inventory->ItemSelect].ID 
        };
        break;
    default:
        break;
    }
    ExecuteWindowTask(Task);
}


void PopWindowTask(void) {
    ListPop(&WindowTaskHead);
}

void RemoveWindowTask(void) {
    PopWindowTask();
    if(HasTextBox) {
        ClearWindowRect((rect) {0, 0, 20, 12});
    } else {
        ClearWindow();
    }
    ExecuteAllWindowTasks();
}

void ClearWindowStack(void) {
    while(!ListEmpty(&WindowTaskHead)) {
        PopWindowTask();
    }
    ClearWindow();
}

void ExecuteAllWindowTasks(void) {
    window_task *Task;
    LIST_FOR_EACH_PREV(Task, &WindowTaskHead, Node) {
        ExecuteWindowTask(Task);
    }
}


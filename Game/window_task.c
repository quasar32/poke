#include <stdio.h>

#include "container.h"
#include "save.h"
#include "options.h"
#include "window_map.h"
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
    case TT_SAVE:
        {
            save_rect *SaveRect = CONTAINER_OF(Task, save_rect, WindowTask);
            PlaceSave(SaveRect->Rect);
        }
        break;
    case TT_OPTIONS:
        {
            options_menu *OptionsMenu = CONTAINER_OF(Task, options_menu, WindowTask);
            PlaceOptionsMenu(OptionsMenu);
        }
        break;
    case TT_DISPLAY:
        {
            StartDisplayItem(g_Inventory); 
        }
        break;
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


window_task *PopWindowTask(void) {
    list_head *Node = ListPop(&WindowTaskHead);
    return LIST_ENTRY(Node, window_task, Node);
}

window_task *RemoveWindowTask(void) {
    window_task *PoppedTask = PopWindowTask();
    if(HasTextBox) {
        ClearWindowRect((rect) {0, 0, 20, 12});
    } else {
        ClearWindow();
    }
    ExecuteAllWindowTasks();
    return PoppedTask;
}

void ClearWindowStack(void) {
    while(PopWindowTask() != NULL);
    ClearWindow();
}

void ExecuteAllWindowTasks(void) {
    window_task *Task;
    LIST_FOR_EACH_PREV(Task, &WindowTaskHead, Node) {
        ExecuteWindowTask(Task);
    }
}


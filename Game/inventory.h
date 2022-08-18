#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdint.h>

#include "window_task.h"

typedef enum inventory_state {
    IS_ITEM,
    IS_WITHDRAW,
    IS_DEPOSIT,
    IS_TOSS
} inventory_state;

typedef enum red_pc_select_i {
    RPSI_WITHDRAW,
    RPSI_DEPOSIT,
    RPSI_TOSS
} red_pc_select_i;

typedef struct item {
    uint8_t ID;
    uint8_t Count;
} item;

typedef struct inventory {
    window_task WindowTask;
    item *Items;
    int ItemCapacity;

    int ItemCount;
    int ItemSelect;
    int Y;
} inventory;

typedef struct red_pc_select {
    inventory_state State; 
    inventory *Inventory;
    const char *Normal;
    const char *Empty;
} red_pc_select;

typedef enum item_id {
    ITEM_POTION,
    ITEM_MAP
} item_id;

extern const char ItemNames[256][8];

extern window_task DisplayWindowTask;
extern item DisplayItem;
extern inventory *g_Inventory;
extern inventory_state g_InventoryState; 

extern inventory g_RedPC;
extern inventory g_Bag;

extern const red_pc_select g_RedPCSelects[];

static inline item *GetSelectedItem(const inventory *Inventory) {
    return &Inventory->Items[Inventory->ItemSelect];
}

item RemoveItem(inventory *Inventory, int TossCount);
void AddItem(inventory *Inventory, item Item);
void MoveItem(inventory *Dest, inventory *Src, int ItemCount);
void StartDisplayItem(inventory *Inventory);
void UpdateDisplayItem(const inventory *Inventory);

#endif

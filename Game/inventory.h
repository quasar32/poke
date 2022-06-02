#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdint.h>

#include "window_task.h"

typedef enum inventory_state {
    IS_ITEM,
    IS_DEPOSIT,
    IS_WITHDRAW,
    IS_TOSS
} inventory_state;

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

typedef enum item_id {
    ITEM_POTION
} item_id;

extern const char ItemNames[256][8];

extern window_task DisplayWindowTask;
extern item DisplayItem;
extern inventory *Inventory;
extern inventory_state InventoryState; 

extern inventory RedPC;
extern inventory Bag;

item RemoveItem(inventory *Inventory, int TossCount);
void AddItem(inventory *Inventory, item Item);
void MoveItem(inventory *Dest, inventory *Src, int ItemCount);
int PlaceItemCount(const inventory *Inventory, int ItemCount);
void StartDisplayItem(inventory *Inventory);
void UpdateDisplayItem(const inventory *Inventory);


#endif

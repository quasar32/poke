#include "input.h"
#include "inventory.h"
#include "scalar.h"
#include "text.h"

const char ItemNames[256][8] = {
    [ITEM_POTION] = "POTION",
    [ITEM_MAP] = "MAP"
};

window_task DisplayWindowTask = {
    .Type = TT_DISPLAY
};

item DisplayItem = {
    .Count = 1
};

inventory *g_Inventory;
inventory_state g_InventoryState;

inventory g_RedPC = {
    .WindowTask.Type = TT_INVENTORY,
    .ItemCapacity = 50,
    .Items = (item[50]) {{ITEM_MAP, 99}, {ITEM_POTION, 50}},
    .ItemCount = 2 
};

inventory g_Bag = {
    .WindowTask.Type = TT_INVENTORY,
    .ItemCapacity = 20,
    .Items = (item[20]) {{0}}
};

const red_pc_select g_RedPCSelects[] = {
    [RPSI_WITHDRAW] = {
        .State = IS_WITHDRAW,
        .Inventory = &g_RedPC,
        .Normal = "What do you want to withdraw?",
        .Empty = "There is nothing stored."
    }, 
    [RPSI_DEPOSIT] = {
        .State = IS_DEPOSIT,
        .Inventory = &g_Bag,
        .Normal = "What do you want to deposit?",
        .Empty = "You have nothing to deposit."
    },
    [RPSI_TOSS] = {
        .State = IS_TOSS, 
        .Inventory = &g_RedPC,
        .Normal = "What do you want to toss away?", 
        .Empty = "There is nothing stored."
    }
};

item RemoveItem(inventory *Inventory, int TossCount) {
    item *SelectedItem = GetSelectedItem(Inventory);
    item RetItem = *SelectedItem;
    if(TossCount < SelectedItem->Count) { 
        SelectedItem->Count -= TossCount;
        RetItem.Count = TossCount;
    } else {
        Inventory->ItemCount--;
        for(int I = Inventory->ItemSelect; I < Inventory->ItemCount; I++) {
            Inventory->Items[I] = Inventory->Items[I + 1]; 
        }
        Inventory->ItemSelect = MIN(Inventory->ItemSelect, Inventory->ItemCount);
    }
    return RetItem;
}

static item *FindItem(const inventory *Inventory, item_id ID, int *I) {
    I = I ? : &((int) {0});
    for(; *I < Inventory->ItemCount; ++*I) {
        if(Inventory->Items[*I].ID == ID) { 
            return &Inventory->Items[(*I)++];
        }
    }
    return NULL;
}

void AddItem(inventory *Inventory, item Item) {
    item *ExistingItem;
    int I = 0;
    while(Item.Count > 0 && (ExistingItem = FindItem(Inventory, Item.ID, &I))) {
        int ToAdd = MIN(99 - ExistingItem->Count, Item.Count);
        ExistingItem->Count += ToAdd;
        Item.Count -= ToAdd; 
    } 
    if(Item.Count > 0 && Inventory->ItemCount < Inventory->ItemCapacity) {
        Inventory->Items[Inventory->ItemCount++] = Item; 
    }
}

void MoveItem(inventory *Dest, inventory *Src, int ItemCount) {
    if(Dest->ItemCount < Dest->ItemCapacity) {
        AddItem(Dest, RemoveItem(Src, ItemCount));
    }
} 

static int PlaceItemCount(const inventory *Inventory, int ItemCount) {
    item *SelectedItem = GetSelectedItem(Inventory);
    ItemCount = PosIntMod(ItemCount - 1, SelectedItem->Count) + 1; 
    PlaceTextF(16, 10, "*%02d", ItemCount);
    return ItemCount;
}

void StartDisplayItem(inventory *Inventory) {
    PlaceBox((rect) {15, 9, 20, 12});
    PlaceItemCount(Inventory, DisplayItem.Count);
}

void UpdateDisplayItem(const inventory *Inventory) {
    if(VirtButtons[BT_UP] == 1) {
        DisplayItem.Count = PlaceItemCount(Inventory, DisplayItem.Count + 1); 
    } else if(VirtButtons[BT_DOWN] == 1) {
        DisplayItem.Count = PlaceItemCount(Inventory, DisplayItem.Count - 1); 
    } 
} 


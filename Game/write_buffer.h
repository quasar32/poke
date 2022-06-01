#ifndef WRITE_BUFFER_H
#define WRITE_BUFFER_H

#include <stdint.h>

#include "inventory.h"
#include "world.h"

typedef struct write_buffer {
    uint8_t Data[4096];
    int Index;
} write_buffer;

void WriteBufferPushByte(write_buffer *WriteBuffer, int Byte);
void WriteBufferPushObject(write_buffer *WriteBuffer, const void *Object, size_t Size);
void WriteBufferPushSaveObject(write_buffer *WriteBuffer, const object *Object);
void WriteBufferPushSaveObjects(write_buffer *WriteBuffer, const map *Map);
void WriteBufferPushString(write_buffer *WriteBuffer, const char *Str, int Length);
void WriteInventory(write_buffer *WriteBuffer, const inventory *Inventory);

#endif

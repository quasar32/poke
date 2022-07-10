#ifndef READ_BUFFER_H
#define READ_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "inventory.h"
#include "world.h"

typedef struct read_buffer {
    uint8_t Data[65536];
    int Index;
    int Size;
} read_buffer;

void ReadBufferFromFile(read_buffer *ReadBuffer, const char *Path); 

uint8_t ReadBufferPopByte(read_buffer *ReadBuffer);
void ReadBufferPopObject(read_buffer *ReadBuffer, void *Object, int Size);
void ReadBufferPopString(read_buffer *ReadBuffer, char Str[256]);
void ReadBufferPopSaveObject(read_buffer *ReadBuffer, object *Object);
void ReadBufferPopSaveObjects(read_buffer *ReadBuffer, map *Map);
void ReadBufferPopInventory(read_buffer *ReadBuffer, inventory *Inventory);

#endif

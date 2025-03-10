#pragma once

#include <stdint.h>

#include "inventory.h"
#include "world.h"

typedef struct read_buffer {
    uint8_t Data[65536];
    int Index;
    int Size;
} read_buffer;

typedef struct write_buffer {
    uint8_t Data[4096];
    int Index;
} write_buffer;

int ReadAll(const char *Path, void *Bytes, int ToRead);

void ReadBufferFromFile(read_buffer *ReadBuffer, const char *Path); 
uint8_t ReadBufferPopByte(read_buffer *ReadBuffer);
void ReadBufferPopObject(read_buffer *ReadBuffer, void *Object, int Size);
void ReadBufferPopString(read_buffer *ReadBuffer, size_t Size, char Str[static Size]);
void ReadBufferPopCString(read_buffer *ReadBuffer, size_t Size, char Buf[static Size]);
void ReadBufferPopSaveObject(read_buffer *ReadBuffer, object *Object);
void ReadBufferPopSaveObjects(read_buffer *ReadBuffer, map *Map);
void ReadBufferPopInventory(read_buffer *ReadBuffer, inventory *Inventory);

bool WriteAll(const char *Path, void *Data, int Size);

void WriteBufferPushByte(write_buffer *WriteBuffer, int Byte);
void WriteBufferPushObject(write_buffer *WriteBuffer, const void *Object, size_t Size);
void WriteBufferPushSaveObject(write_buffer *WriteBuffer, const object *Object);
void WriteBufferPushSaveObjects(write_buffer *WriteBuffer, const map *Map);
void WriteBufferPushString(write_buffer *WriteBuffer, const char *Str, int Length);
void WriteBufferPushCString(write_buffer *WriteBuffer, const char *Buf);
void WriteInventory(write_buffer *WriteBuffer, const inventory *Inventory);

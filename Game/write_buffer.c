#include <stdlib.h>
#include <string.h>

#include "write_buffer.h"

void WriteBufferPushByte(write_buffer *WriteBuffer, int Byte) {
    if(WriteBuffer->Index < (int) _countof(WriteBuffer->Data)) {
        WriteBuffer->Data[WriteBuffer->Index] = Byte;
        WriteBuffer->Index++;
    } 
}

void WriteBufferPushObject(write_buffer *WriteBuffer, const void *Object, size_t Size) {
    if(WriteBuffer->Index + Size <= _countof(WriteBuffer->Data)) {
        memcpy(&WriteBuffer->Data[WriteBuffer->Index], Object, Size);
        WriteBuffer->Index += Size;
    }
}

void WriteBufferPushSaveObject(write_buffer *WriteBuffer, const object *Object) {
    WriteBufferPushByte(WriteBuffer, Object->Pos.X / 16);
    WriteBufferPushByte(WriteBuffer, Object->Pos.Y / 16);
    WriteBufferPushByte(WriteBuffer, Object->Speed == 0 ? Object->StartingDir : Object->Dir);
}

void WriteBufferPushSaveObjects(write_buffer *WriteBuffer, const map *Map) {
    for(int I = 0; I < Map->ObjectCount; I++) {
        WriteBufferPushSaveObject(WriteBuffer, &Map->Objects[I]);
    }
}

void WriteBufferPushString(write_buffer *WriteBuffer, const char *Str, int Length) {
    WriteBufferPushByte(WriteBuffer, Length);
    WriteBufferPushObject(WriteBuffer, Str, Length);
}

void WriteInventory(write_buffer *WriteBuffer, const inventory *Inventory) {
    WriteBufferPushByte(WriteBuffer, Inventory->ItemCount);
    for(int I = 0; I < Inventory->ItemCount; I++) {
        WriteBufferPushByte(WriteBuffer, Inventory->Items[I].ID);
        WriteBufferPushByte(WriteBuffer, Inventory->Items[I].Count);
    }
}


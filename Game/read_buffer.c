#include "read_buffer.h"
#include "scalar.h"

uint8_t ReadBufferPopByte(read_buffer *ReadBuffer) {
    uint8_t Result = 0;
    if(ReadBuffer->Index < ReadBuffer->Size) {
        Result = ReadBuffer->Data[ReadBuffer->Index]; 
        ReadBuffer->Index++;
    } 
    return Result;
}

void ReadBufferPopObject(read_buffer *ReadBuffer, void *Object, int Size) {
    if(ReadBuffer->Index + Size <= ReadBuffer->Size) {
        memcpy(Object, &ReadBuffer->Data[ReadBuffer->Index], Size); 
        ReadBuffer->Index += Size;
    } else {
        memset(Object, 0, Size);
    } 
}

void ReadBufferPopString(read_buffer *ReadBuffer, char Str[256]) {
    uint8_t Length = ReadBufferPopByte(ReadBuffer); 
    int Left = ReadBuffer->Size - ReadBuffer->Index; 
    if(Left < Length) {
        Length = Left;
    }
    memcpy(Str, &ReadBuffer->Data[ReadBuffer->Index], Length);
    Str[Length] = '\0';
    ReadBuffer->Index += Length;
}

void ReadBufferPopSaveObject(read_buffer *ReadBuffer, object *Object) {
    Object->Pos.X = ReadBufferPopByte(ReadBuffer) * 16;
    Object->Pos.Y = ReadBufferPopByte(ReadBuffer) * 16;
    Object->Dir = ReadBufferPopByte(ReadBuffer) % 4;
}

void ReadBufferPopSaveObjects(read_buffer *ReadBuffer, map *Map) {
    for(int I = 0; I < Map->ObjectCount; I++) {
        ReadBufferPopSaveObject(ReadBuffer, &Map->Objects[I]);
    }
}

void ReadBufferPopInventory(read_buffer *ReadBuffer, inventory *Inventory) {
    Inventory->ItemCount = MinInt(ReadBufferPopByte(ReadBuffer), Inventory->ItemCapacity);
    for(int I = 0; I < Inventory->ItemCount; I++) {
        Inventory->Items[I].ID = ReadBufferPopByte(ReadBuffer);
        Inventory->Items[I].Count = ReadBufferPopByte(ReadBuffer);
    }
}



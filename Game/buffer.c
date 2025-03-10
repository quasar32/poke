#include "buffer.h"
#include "scalar.h"
#include "misc.h"
#include <SDL3/SDL.h>

int ReadAll(const char *Path, void *Bytes, int ToRead) {
    int Size = 0;
    SDL_IOStream *Stream = SDL_IOFromFile(Path, "rb");
    if(Stream) {
        Size = SDL_ReadIO(Stream, Bytes, ToRead);
        SDL_CloseIO(Stream);
    }
    return Size;
}

void ReadBufferFromFile(read_buffer *ReadBuffer, const char *Path) {
    ReadBuffer->Index = 0;
    ReadBuffer->Size = ReadAll(Path, ReadBuffer->Data, sizeof(ReadBuffer->Data));
} 

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

void ReadBufferPopString(read_buffer *ReadBuffer, size_t Size, char Str[static Size]) {
    uint8_t Length = ReadBufferPopByte(ReadBuffer); 
    int Left = ReadBuffer->Size - ReadBuffer->Index; 
    if(Length >= Size) {
        Length = 0;
    }
    if(Left < Length) {
        Length = Left;
    }
    memcpy(Str, &ReadBuffer->Data[ReadBuffer->Index], Length);
    Str[Length] = '\0';
    ReadBuffer->Index += Length;
}

void ReadBufferPopCString(read_buffer *ReadBuffer, size_t Size, char Buf[static Size]) {
    int Ch;
    int I = 0;
    do {
        Ch = ReadBufferPopByte(ReadBuffer);
        if(I >= Size) {
            if(Size != 0) {
                Buf[0] = '\0';
            }
            break;
        }
        Buf[I++] = Ch;
    } while(Ch != '\0');
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
    Inventory->ItemCount = MIN(ReadBufferPopByte(ReadBuffer), Inventory->ItemCapacity);
    for(int I = 0; I < Inventory->ItemCount; I++) {
        Inventory->Items[I].ID = ReadBufferPopByte(ReadBuffer);
        Inventory->Items[I].Count = ReadBufferPopByte(ReadBuffer) % 100;
    }
}

bool WriteAll(const char *Path, void *Data, int Size) {
    return SDL_SaveFile(Path, Data, Size);
}

void WriteBufferPushByte(write_buffer *WriteBuffer, int Byte) {
    if(WriteBuffer->Index < (int) countof(WriteBuffer->Data)) {
        WriteBuffer->Data[WriteBuffer->Index] = Byte;
        WriteBuffer->Index++;
    } 
}

void WriteBufferPushObject(write_buffer *WriteBuffer, const void *Object, size_t Size) {
    if(WriteBuffer->Index + Size <= countof(WriteBuffer->Data)) {
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

void WriteBufferPushCString(write_buffer *WriteBuffer, const char *Buf) {
    do {
        WriteBufferPushByte(WriteBuffer, *Buf); 
    } while(*Buf++);
}

void WriteInventory(write_buffer *WriteBuffer, const inventory *Inventory) {
    WriteBufferPushByte(WriteBuffer, Inventory->ItemCount);
    for(int I = 0; I < Inventory->ItemCount; I++) {
        WriteBufferPushByte(WriteBuffer, Inventory->Items[I].ID);
        WriteBufferPushByte(WriteBuffer, Inventory->Items[I].Count);
    }
}


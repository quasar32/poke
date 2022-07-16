#include "buffer.h"
#include "scalar.h"

BOOL ReadObject(HANDLE FileHandle, LPVOID Object, DWORD ObjectSize) {
    DWORD BytesRead;
    ReadFile(FileHandle, Object, ObjectSize, &BytesRead, NULL);
    return ObjectSize == BytesRead;
}

DWORD ReadAll(LPCSTR Path, LPVOID Bytes, DWORD ToRead) {
    DWORD BytesRead = 0;
    HANDLE FileHandle = CreateFile(
        Path, 
        GENERIC_READ, 
        FILE_SHARE_READ, 
        NULL, 
        OPEN_EXISTING, 
        0, 
        NULL
    );
    if(FileHandle != INVALID_HANDLE_VALUE) { 
        ReadFile(FileHandle, Bytes, ToRead, &BytesRead, NULL);
        CloseHandle(FileHandle);
    }
                 
    return BytesRead;
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
    Inventory->ItemCount = MIN(ReadBufferPopByte(ReadBuffer), Inventory->ItemCapacity);
    for(int I = 0; I < Inventory->ItemCount; I++) {
        Inventory->Items[I].ID = ReadBufferPopByte(ReadBuffer);
        Inventory->Items[I].Count = ReadBufferPopByte(ReadBuffer) % 100;
    }
}

BOOL WriteAll(LPCSTR Path, LPCVOID Data, DWORD Size) {
    BOOL Success = FALSE;
    HANDLE FileHandle = CreateFile(
        Path,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    ); 
    if(FileHandle != INVALID_HANDLE_VALUE) {
        DWORD BytesWritten;
        WriteFile(FileHandle, Data, Size, &BytesWritten, NULL);
        if(BytesWritten == Size) {
            Success = TRUE;
        }
        CloseHandle(FileHandle);
    }
    return Success;
}

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


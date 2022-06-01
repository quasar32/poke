#include "read.h"

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

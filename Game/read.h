#ifndef READ_H
#define READ_H

#include <windows.h>

BOOL ReadObject(HANDLE FileHandle, LPVOID Object, DWORD ObjectSize);
DWORD ReadAll(LPCSTR Path, LPVOID Bytes, DWORD ToRead);

#endif

#ifndef PROCS_H
#define PROCS_H

#include <windows.h>

HMODULE LoadProcs(
    const char *Path, 
    size_t ProcCount, 
    const char *ProcNames[static ProcCount],
    FARPROC Procs[static ProcCount]
);

HMODULE LoadProcsVersioned(
    size_t PathCount,
    const char *Paths[static PathCount], 
    size_t ProcCount, 
    const char *ProcNames[static ProcCount],
    FARPROC Procs[static ProcCount]
);

#endif

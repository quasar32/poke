#include <windows.h>

#include "frame.h"
#include "procs.h"

typedef MMRESULT winmm_func(UINT uPeriod);

static int64_t PerfFreq;
static int64_t BeginCounter;
static int64_t DeltaCounter;
static int64_t PeriodCounter;

static HMODULE WinmmLib;
static winmm_func *TimeBeginPeriod;
static winmm_func *TimeEndPeriod;

static bool IsGranular;

int64_t QueryPerfFreq(void) {
    LARGE_INTEGER PerfFreq;
    QueryPerformanceFrequency(&PerfFreq);
    return PerfFreq.QuadPart;
}

int64_t QueryPerfCounter(void) {
    LARGE_INTEGER PerfCounter;
    QueryPerformanceCounter(&PerfCounter);
    return PerfCounter.QuadPart;
}

static int64_t CounterToMS(int64_t Counter, int64_t PerfFreq) {
    return 1000LL * Counter / PerfFreq;
}

BOOL CreateFrame(void) {
    PerfFreq = QueryPerfFreq();
    BeginCounter = QueryPerfCounter();
    DeltaCounter = 0LL;
    PeriodCounter = (int64_t) (PerfFreq / 60);

    FARPROC Procs[2];
    WinmmLib = LoadProcs(
        "winmm.dll",
        _countof(Procs),
        (const char *[]) {
            "timeBeginPeriod",
            "timeEndPeriod"
        },
        Procs
    ); 
    if(!WinmmLib) {
        return FALSE;
    }
    TimeBeginPeriod = (winmm_func *) Procs[0]; 
    TimeEndPeriod = (winmm_func *) Procs[1]; 
    IsGranular = (TimeBeginPeriod(1U) == TIMERR_NOERROR); 

    if(!IsGranular) {
        FreeLibrary(WinmmLib);
        TimeEndPeriod = NULL;
        TimeBeginPeriod = NULL;
        WinmmLib = NULL;
    }
    return IsGranular;
}

void DestroyFrame(void) {
    if(IsGranular) {
        TimeEndPeriod(1U);
    } 
}

void StartFrame(void) {
    BeginCounter = QueryPerfCounter();
}

void EndFrame(void) {
    DeltaCounter = GetDeltaCounter(BeginCounter);
    if(IsGranular) {
        int64_t SleepMS = CounterToMS(PeriodCounter - DeltaCounter, PerfFreq);
        if(SleepMS > 0LL) {
            Sleep(SleepMS);
        }
    }
    while(DeltaCounter < PeriodCounter) {
        DeltaCounter = GetDeltaCounter(BeginCounter);
    }
}

float GetFrameDelta(void) {
    return (
        (float) (DeltaCounter == 0LL ? PeriodCounter : DeltaCounter) / 
        (float) PerfFreq
    );
}

int64_t GetDeltaCounter(int64_t BeginCounter) {
    return QueryPerfCounter() - BeginCounter;
}

int64_t GetSecondsElapsed(int64_t BeginCounter) {
    return GetDeltaCounter(BeginCounter) / PerfFreq;
}


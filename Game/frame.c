#include <windows.h>

#include "frame.h"
#include "procs.h"

typedef MMRESULT winmm_func(UINT uPeriod);

static int64_t g_PerfFreq;
static int64_t g_BeginCounter;
static int64_t g_DeltaCounter;
static int64_t g_PeriodCounter;

static HMODULE g_WinmmLib;
static winmm_func *g_TimeBeginPeriod;
static winmm_func *g_TimeEndPeriod;

static bool IsGranular;

int64_t QueryPerfFreq(void) {
    LARGE_INTEGER g_PerfFreq;
    QueryPerformanceFrequency(&g_PerfFreq);
    return g_PerfFreq.QuadPart;
}

int64_t QueryPerfCounter(void) {
    LARGE_INTEGER PerfCounter;
    QueryPerformanceCounter(&PerfCounter);
    return PerfCounter.QuadPart;
}

static int64_t CounterToMS(int64_t Counter, int64_t g_PerfFreq) {
    return 1000LL * Counter / g_PerfFreq;
}

BOOL CreateFrameTimer(void) {
    g_PerfFreq = QueryPerfFreq();
    g_BeginCounter = QueryPerfCounter();
    g_DeltaCounter = 0LL;
    g_PeriodCounter = (int64_t) (g_PerfFreq / 60);

    FARPROC Procs[2];
    g_WinmmLib = LoadProcs(
        "winmm.dll",
        _countof(Procs),
        (const char *[]) {
            "timeBeginPeriod",
            "timeEndPeriod"
        },
        Procs
    ); 
    if(!g_WinmmLib) {
        return FALSE;
    }
    g_TimeBeginPeriod = (winmm_func *) Procs[0]; 
    g_TimeEndPeriod = (winmm_func *) Procs[1]; 
    IsGranular = (g_TimeBeginPeriod(1U) == TIMERR_NOERROR); 

    if(!IsGranular) {
        FreeLibrary(g_WinmmLib);
        g_TimeEndPeriod = NULL;
        g_TimeBeginPeriod = NULL;
        g_WinmmLib = NULL;
    }
    return IsGranular;
}

void DestroyFrameTimer(void) {
    if(IsGranular) {
        g_TimeEndPeriod(1U);
    } 
}

void StartFrameTimer(void) {
    g_BeginCounter = QueryPerfCounter();
}

void EndFrameTimer(void) {
    g_DeltaCounter = GetDeltaCounter(g_BeginCounter);
    if(IsGranular) {
        int64_t SleepMS = CounterToMS(g_PeriodCounter - g_DeltaCounter, g_PerfFreq);
        if(SleepMS > 0LL) {
            Sleep(SleepMS);
        }
    }
    while(g_DeltaCounter < g_PeriodCounter) {
        g_DeltaCounter = GetDeltaCounter(g_BeginCounter);
    }
}

float GetFrameTimerDelta(void) {
    return (
        (float) (g_DeltaCounter == 0LL ? g_PeriodCounter : g_DeltaCounter) / 
        (float) g_PerfFreq
    );
}

int64_t GetDeltaCounter(int64_t BeginCounter) {
    return QueryPerfCounter() - BeginCounter;
}

int64_t GetSecondsElapsed(int64_t BeginCounter) {
    return GetDeltaCounter(BeginCounter) / g_PerfFreq;
}


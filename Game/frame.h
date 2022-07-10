#ifndef FRAME_HPP
#define FRAME_HPP

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

__attribute__((constructor))
BOOL CreateFrameTimer(void);

__attribute__((destructor))
void DestroyFrameTimer(void);

void StartFrameTimer(void);

void EndFrameTimer(void);

float GetFrameTimerDelta(void);

int64_t QueryPerfFreq(void);
int64_t QueryPerfCounter(void);
int64_t GetDeltaCounter(int64_t BeginCounter);
int64_t GetSecondsElapsed(int64_t BeginCounter);

#endif

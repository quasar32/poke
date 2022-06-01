#ifndef FRAME_HPP
#define FRAME_HPP

#include <stdint.h>
#include <stdbool.h>
#include <windows.h>

__attribute__((constructor))
BOOL CreateFrame(void);

__attribute__((destructor))
void DestroyFrame(void);

void StartFrame(void);

void EndFrame(void);

float GetFrameDelta(void);

int64_t QueryPerfFreq(void);
int64_t QueryPerfCounter(void);
int64_t GetDeltaCounter(int64_t BeginCounter);
int64_t GetSecondsElapsed(int64_t BeginCounter);

#endif

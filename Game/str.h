#ifndef STR_H
#define STR_H

#include <stdbool.h>

bool DoesStartStringMatch(const char *Dst, const char *Src);
const char *StrMovePastSpan(const char *Str, const char *Span);
int AreStringsEqual(const char *A, const char *B);

#endif

#include <string.h>

#include "str.h"

bool DoesStartStringMatch(const char *Dst, const char *Src) {
    for(int I = 0; Src[I] != '\0'; I++) {
        if(Dst[I] != Src[I]) {
            return false;
        }
    }
    return true; 
}

const char *StrMovePastSpan(const char *Str, const char *Span) {
    while(*Span != '\0') {
        if(*Str != *Span) {
            return NULL;
        }
        Str++;
        Span++;
    }
    return Str;
}

int AreStringsEqual(const char *A, const char *B) {
    return A && B ? strcmp(A, B) == 0 : A != B;
}

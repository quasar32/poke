#include "str.h"

bool DoesStartStringMatch(const char *Dst, const char *Src) {
    for(int I = 0; Src[I] != '\0'; I++) {
        if(Dst[I] != Src[I]) {
            return false;
        }
    }
    return true; 
}

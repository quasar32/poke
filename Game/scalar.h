#ifndef SCALAR_H 
#define SCALAR_H 

#include <stdint.h>

#define SWAP(A, B) ({ \
    __auto_type A_ = (A); \
    __auto_type B_ = (B); \
    __auto_type Tmp = *A_; \
    *A_ = *B_; \
    *B_ = Tmp; \
})

#define MIN(A, B) ({ \
    __auto_type A_ = (A); \
    __auto_type B_ = (B); \
    A_ < B_ ? A_ : B_; \
})

#define MAX(A, B) ({ \
    __auto_type A_ = (A); \
    __auto_type B_ = (B); \
    A_ > B_ ? A_ : B_; \
})

static inline int AbsInt(int Val) {
  return Val < 0 ? -Val : Val; 
}

static inline int PosIntMod(int A, int B) {
  return A % B < 0 ? A % B + AbsInt(B) : A % B; 
}

#endif

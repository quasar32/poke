#ifndef SCALAR_H 
#define SCALAR_H 

#include <stdint.h>

#define SWAP(A, B) ({       \
    __auto_type A_ = (A);   \
    __auto_type B_ = (B);   \
    __auto_type Tmp = *A_;  \
    *A_ = *B_;              \
    *B_ = Tmp;              \
})

[[maybe_unused]] 
static int MinInt(int A, int B) {
  return A < B ? A : B;
}

[[maybe_unused]] 
static int MaxInt(int A, int B) {
  return A > B ? A : B;
}

[[maybe_unused]] 
static int AbsInt(int Val) {
  return Val < 0 ? -Val : Val; 
}

[[maybe_unused]] 
static int64_t MinInt64(int64_t A, int64_t B) {
  return A < B ? A : B;
}

[[maybe_unused]]
static int PosIntMod(int A, int B) {
  return A % B < 0 ? A % B + AbsInt(B) : A % B; 
}

#endif

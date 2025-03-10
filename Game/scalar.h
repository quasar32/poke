#ifndef SCALAR_H 
#define SCALAR_H 

#include <stdint.h>
#include <stddef.h>

#define IN_RANGE(Ary, I) ((size_t) (I)) < countof((Ary))
#define OUT_OF_RANGE(Ary, I) ((size_t) (I)) >= countof((Ary))

#define TYPEOF_MEMBER(Type, Member) __typeof(((Type *) 0)->Member)

#define CONTAINER_OF(Ptr, Type, Member) ({                  \
    const TYPEOF_MEMBER(Type, Member) *MemberPtr = (Ptr);   \
    (Type *) ((char *) MemberPtr - offsetof(Type, Member)); \
})

#define SAFE_INDEX(Ary, I, Def) (IN_RANGE(Ary, I) ? Ary[I] : Def)

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

#define ARY_FOR_EACH(Ptr, Counter) for(; Counter > 0; Ptr++, Counter--) 

#define ARY_PREV_CIRCLE(Key, Ary) PrevCircle((Key), (Ary), countof(Ary), sizeof(*Ary))
#define ARY_NEXT_CIRCLE(Key, Ary) NextCircle((Key), (Ary), countof(Ary), sizeof(*Ary))

static inline int AbsInt(int Val) {
    return Val < 0 ? -Val : Val; 
}

static inline int PosIntMod(int A, int B) {
    return A % B < 0 ? A % B + AbsInt(B) : A % B; 
}

static inline int IsEndAry(const void *Key, const void *Base, size_t Size) {
    return Key - Base == Size; 
}

static const void *EndAry(const void *Base, size_t Count, size_t Stride) {
    return Base + Count * Stride;
}

static inline const void *NextCircle(const void *Key, const void *Base, size_t Count, size_t Stride) {
    Key += Stride;
    return IsEndAry(Key, Base, Count * Stride) ? Base : Key;
}

static inline const void *PrevCircle(const void *Key, const void *Base, size_t Count, size_t Stride) {
    return (Key == Base ? EndAry(Base, Count, Stride) : Key) - Stride;
}

#endif

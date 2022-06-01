#ifndef CONTAINER_H
#define CONTAINER_H

#include <stdlib.h>

#define TYPEOF_MEMBER(Type, Member) __typeof(((Type *) 0)->Member)

#define CONTAINER_OF(Ptr, Type, Member) ({                  \
    const TYPEOF_MEMBER(Type, Member) *MemberPtr = (Ptr);   \
    (Type *) ((char *) MemberPtr - offsetof(Type, Member)); \
})

#endif

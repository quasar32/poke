#ifndef LIST_H
#define LIST_H

#include <stdbool.h>
#include <stdio.h>

#include "scalar.h"

#define LIST_HEAD_INIT(Name) {  \
    .Prev = &(Name),            \
    .Next = &(Name)             \
}

#define LIST_HEAD(Name) list_head Name = LIST_HEAD_INIT(Name)

#define LIST_ENTRY(Ptr, Type, Member) CONTAINER_OF(Ptr, Type, Member)
#define LIST_FIRST_ENTRY(Ptr, Type, Member) LIST_ENTRY((Ptr)->Next, Type, Member)

#define LIST_FOR_EACH(Pos, Head, Member)                            \
    for(                                                            \
        Pos = LIST_ENTRY((Head)->Next, __typeof(*Pos), Member);     \
        &Pos->Member != (Head);                                     \
        Pos = LIST_ENTRY(Pos->Member.Next, __typeof(*Pos), Member)  \
    )

#define LIST_FOR_EACH_SAFE(Pos, N, Head, Member)                    \
    for(                                                            \
        Pos = LIST_ENTRY((Head)->Next, __typeof(*Pos), Member),     \
        N = LIST_ENTRY(Pos->Member.Next, __typeof(*Pos), Member);  \
        &Pos->Member != (Head);                                     \
        Pos = N,                                                    \
        N = LIST_ENTRY(N->Member.Next, __typeof(*N), Member)        \
    ) 


#define LIST_FOR_EACH_PREV(Pos, Head, Member)                       \
    for(                                                            \
        Pos = LIST_ENTRY((Head)->Prev, __typeof(*Pos), Member);     \
        &Pos->Member != (Head);                                     \
        Pos = LIST_ENTRY(Pos->Member.Prev, __typeof(*Pos), Member)  \
    )

typedef struct list_head {
    struct list_head *Prev;
    struct list_head *Next; 
} list_head;

static inline void InitListHead(list_head *ListNode) {
    ListNode->Prev = ListNode; 
    ListNode->Next = ListNode; 
}

/*internal use only*/
static inline void __ListAdd(list_head *New, list_head *Prev, list_head *Next) {
    Next->Prev = New;
    New->Next = Next;
    New->Prev = Prev;
    Prev->Next = New; 
}

static inline void ListAdd(list_head *New, list_head *Head) {
    __ListAdd(New, Head, Head->Next); 
}

static inline void ListAddTail(list_head *New, list_head *Head) {
    __ListAdd(New, Head->Prev, Head->Next);
}

/*internal use only*/
static inline void __ListDel(list_head *Prev, list_head *Next) {
    Next->Prev = Prev;
    Prev->Next = Next;
}

static inline void ListDel(list_head *Entry) {
    __ListDel(Entry->Prev, Entry->Next);
    InitListHead(Entry);
}

/*Stacks*/
static inline void ListPush(list_head *Head, list_head *New) {
    ListAdd(New, Head);
}

static inline int ListEmpty(const list_head *Head) {
    return Head->Next == Head;
} 

static inline list_head *ListPop(list_head *Head) {
    if(ListEmpty(Head)) return NULL;
    list_head *Pop = Head->Next;
    ListDel(Pop);
    return Pop;
}

#endif

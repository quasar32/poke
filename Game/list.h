#ifndef LIST_H
#define LIST_H

#include "container.h"
#include <stdbool.h>
#include <stdio.h>

#define LIST_HEAD_INIT(Name) {\
    .Prev = &(Name),\
    .Next = &(Name)\
}

#define LIST_HEAD(Name) list_node Name = LIST_HEAD_INIT(Name)

#define LIST_ENTRY(Ptr, Type, Member) CONTAINER_OF(Ptr, Type, Member)
#define LIST_FIRST_ENTRY(Ptr, Type, Member) LIST_ENTRY((Ptr)->Next, Type, Member)
#define LIST_FOR_EACH(Pos, Head, Member)                            \
    for(                                                            \
        Pos = LIST_ENTRY((Head)->Next, __typeof(*Pos), Member);     \
        &Pos->Member != (Head);                                     \
        Pos = LIST_ENTRY(Pos->Member.Next, __typeof(*Pos), Member)  \
    )

#define LIST_FOR_EACH_PREV(Pos, Head, Member)                       \
    for(                                                            \
        Pos = LIST_ENTRY((Head)->Prev, __typeof(*Pos), Member);     \
        &Pos->Member != (Head);                                     \
        Pos = LIST_ENTRY(Pos->Member.Prev, __typeof(*Pos), Member)  \
    )

typedef struct list_node {
    struct list_node *Prev;
    struct list_node *Next; 
} list_node;

[[maybe_unused]]
static inline void InitListHead(list_node *ListNode) {
    ListNode->Prev = ListNode; 
    ListNode->Next = ListNode; 
}

/*internal use only*/
static inline void __ListAdd(list_node *New, list_node *Prev, list_node *Next) {
    Next->Prev = New;
    New->Next = Next;
    New->Prev = Prev;
    Prev->Next = New; 
}

[[maybe_unused]]
static inline void ListAdd(list_node *New, list_node *Head) {
    __ListAdd(New, Head, Head->Next); 
}

[[maybe_unused]]
static inline void ListAddTail(list_node *New, list_node *Head) {
    __ListAdd(New, Head->Prev, Head->Next);
}

/*internal use only*/
static inline void __ListDel(list_node *Prev, list_node *Next) {
    Next->Prev = Prev;
    Prev->Next = Next;
}

[[maybe_unused]]
static inline void ListDel(list_node *Entry) {
    __ListDel(Entry->Prev, Entry->Next);
    InitListHead(Entry);
}

/*Stacks*/
[[maybe_unused]]
static inline void ListPush(list_node *Head, list_node *New) {
    ListAdd(New, Head);
}

[[maybe_unused]]
static inline int ListEmpty(const list_node *Head) {
    return Head->Next == Head;
} 

[[maybe_unused]]
static inline list_node *ListPop(list_node *Head) {
    if(ListEmpty(Head)) return NULL;
    list_node *Pop = Head->Next;
    ListDel(Pop);
    return Pop;
}

#endif

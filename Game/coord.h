#ifndef COORD_H 
#define COORD_H 

#include <windows.h>

#include "scalar.h"

typedef struct rect {
    short Left;
    short Top;
    short Right;
    short Bottom;
} rect;

typedef struct point {
    short X;
    short Y; 
} point;

static inline point AddPoints(point A, point B)  {
    return (point) {
        .X = A.X + B.X,
        .Y = A.Y + B.Y
    };
}

static inline point SubPoints(point A, point B)  {
    return (point) {
        .X = A.X - B.X,
        .Y = A.Y - B.Y
    };
}

static inline point MulPoint(point A, short B) {
    return (point) {
        .X = A.X * B,
        .Y = A.Y * B 
    };
}

static inline point DivPoint(point A, short B) {
    return (point) {
        .X = A.X / B,
        .Y = A.Y / B
    };
}

static inline int EqualPoints(point A, point B) {
    return A.X == B.X && A.Y == B.Y;
}

static inline int CmpPoints(point A, point B) {
    point Delta = SubPoints(A, B);
    return Delta.X == 0 ? Delta.Y : Delta.X;
}

static inline int CmpPointsWrapper(const void *A, const void *B) {
    const point *PtA = A;
    const point *PtB = B;
    return CmpPoints(*PtA, *PtB);
} 

#endif

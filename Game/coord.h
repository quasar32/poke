#ifndef COORD_H 
#define COORD_H 

#include <windows.h>

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

typedef struct dim_rect {
    int X;
    int Y;
    int Width;
    int Height;
} dim_rect;

[[maybe_unused]] 
static point AddPoints(point A, point B)  {
    return (point) {
        .X = A.X + B.X,
        .Y = A.Y + B.Y
    };
}

[[maybe_unused]] 
static point SubPoints(point A, point B)  {
    return (point) {
        .X = A.X - B.X,
        .Y = A.Y - B.Y
    };
}

[[maybe_unused]] 
static point MulPoint(point A, short B) {
    return (point) {
        .X = A.X * B,
        .Y = A.Y * B 
    };
}

[[maybe_unused]]
static point DivPoint(point A, short B) {
    return (point) {
        .X = A.X / B,
        .Y = A.Y / B
    };
}

[[maybe_unused]] 
static int EqualPoints(point A, point B) {
    return A.X == B.X && A.Y == B.Y;
}

dim_rect WinToDimRect(RECT WinRect);
dim_rect GetDimClientRect(HWND Window);

#endif

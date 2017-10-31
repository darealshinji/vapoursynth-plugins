#ifndef COMMON_H
#define COMMON_H

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

enum yuv_planes {
    Y = 0,
    U,
    V
};

#endif

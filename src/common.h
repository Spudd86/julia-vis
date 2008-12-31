#ifndef COMMON_H
#define COMMON_H

#define IMIN(x,y) ((x)+((((y)-(x)) >> 31)&((y)-(x))))
#define IMAX(x,y) ((x)-((((x)-(y)) >> 31)&((x)-(y))))

#define TILED false

#endif
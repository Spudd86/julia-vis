#ifndef COMMON_H
#define COMMON_H

//#define IMIN(x,y) ((x)+((((y)-(x)) >> 31)&((y)-(x))))
//#define IMAX(x,y) ((x)-((((x)-(y)) >> 31)&((x)-(y))))
#define IMIN(x,y) (((x)<(y))?(x):(y))
#define IMAX(x,y) (((x)>(y))?(x):(y))

typedef struct {
	int w, h;
	int fullscreen;
	
}opt_data;

void optproc(int argc, char **argv, opt_data *res);

#endif

#ifndef COMMON_H
#define COMMON_H

#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <malloc.h>

//#define IMIN(x,y) ((x)+((((y)-(x)) >> 31)&((y)-(x))))
//#define IMAX(x,y) ((x)-((((x)-(y)) >> 31)&((x)-(y))))
#define IMIN(x,y) (((x)<(y))?(x):(y))
#define IMAX(x,y) (((x)>(y))?(x):(y))

typedef struct {
	int w, h;
	int fullscreen;
	int draw_rate;
	int doublebuf;
	int use_jack;
	int hw_pallet;
	const char *jack_opt;
}opt_data;

void optproc(int argc, char **argv, opt_data *res);

#ifdef USE_DIRECTFB
#define DFBCHECK(x...)                                         \
  {                                                            \
    DFBResult err = x;                                         \
                                                               \
    if (err != DFB_OK)                                         \
      {                                                        \
        fprintf( stderr, "%s <%d>:\n\t", __FILE__, __LINE__ ); \
        DirectFBErrorFatal( #x, err );                         \
      }                                                        \
  }
#endif

static inline void *xmalloc(size_t s) { void *res = malloc(s); if(!res) abort(); return res; }

#endif

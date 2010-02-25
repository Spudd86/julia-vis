#ifndef COMMON_H
#define COMMON_H

#include "config.h"
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif
#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifdef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#ifdef HAVE_MMAP
# include <sys/mman.h>
# ifndef MAP_ANONYMOUS
#  ifdef MAP_ANON
#   define MAP_ANONYMOUS MAP_ANON
#  else
#   undef HAVE_MMAP
#  endif
#endif
#endif

//#define IMIN(x,y) ((x)+((((y)-(x)) >> 31)&((y)-(x))))
//#define IMAX(x,y) ((x)-((((x)-(y)) >> 31)&((x)-(y))))
#define IMIN(x,y) (((x)<(y))?(x):(y))
#define IMAX(x,y) (((x)>(y))?(x):(y))

#ifndef MAX
#define MAX(a,b) IMAX(a,b)
#endif
#ifndef MIN
#define MIN(a,b) IMIN(a,b)
#endif

typedef enum { AUDIO_PORTAUDIO, AUDIO_PULSE, AUDIO_JACK } opt_audio_drv;

typedef struct {
	int w, h;
	int fullscreen;
	unsigned int maxsrc_rate;
	int draw_rate;
	int doublebuf;
	int hw_pallet;
	int rational_julia;
	int quality;

	opt_audio_drv audio_driver;
	const char *audio_opts;

	const char *gl_opts;

	const char *map_name;
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

#define pbattr restrict __attribute__((aligned (16)))
typedef uint16_t * restrict __attribute__ ((aligned (16)))  pixbuf_t;

typedef struct {
	uint16_t w, h;
	int pitch;
	int bpp;
	void *data;
}Pixbuf;

typedef struct {
	uint16_t w, h;
} ivec;

#endif

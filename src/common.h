#ifndef COMMON_H
#define COMMON_H

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

//TODO: maybe make configure check this stuff
#if defined(_WIN32)
#define aligned_alloc(align, size) _aligned_malloc(size, align)
#define aligned_free _aligned_free
#else
#define aligned_free free
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

// define single precision versions of handy mathematical constants

#ifndef M_E_F
#define  M_E_F           2.71828174591064f
#endif
#ifndef M_LOG2E_F
#define  M_LOG2E_F       1.44269502162933f
#endif
#ifndef M_LOG10E_F
#define  M_LOG10E_F      0.43429449200630f
#endif
#ifndef M_LN2_F
#define  M_LN2_F         0.69314718246460f
#endif
#ifndef M_LN10_F
#define  M_LN10_F        2.30258512496948f
#endif
#ifndef M_PI_F
#define  M_PI_F          3.14159274101257f
#endif
#ifndef M_PI_2_F
#define  M_PI_2_F        1.57079637050629f
#endif
#ifndef M_PI_4_F
#define  M_PI_4_F        0.78539818525314f
#endif
#ifndef M_1_PI_F
#define  M_1_PI_F        0.31830987334251f
#endif
#ifndef M_2_PI_F
#define  M_2_PI_F        0.63661974668503f
#endif
#ifndef M_2_SQRTPI_F
#define  M_2_SQRTPI_F    1.12837922573090f
#endif
#ifndef M_SQRT2_F
#define  M_SQRT2_F       1.41421353816986f
#endif
#ifndef M_SQRT1_2_F
#define  M_SQRT1_2_F     0.70710676908493f
#endif

#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define __func__ __FUNCTION__
# else
#  define __func__ "<unknown>"
# endif
#endif

#if __GNU_LIBRARY__
#	include <execinfo.h>
	static void __attribute__((unused)) print_backtrace_stderr(void)
	{
		void *bt_buf[20];
		fprintf(stderr, "Backtrace:\n");
		fflush(stderr);
		size_t size = backtrace(bt_buf, 20);
		backtrace_symbols_fd(bt_buf, size, STDERR_FILENO);
	}
#else
#	define print_backtrace_stderr() do { } while(0)
#endif

#define gccStyleMessage(type, msg, ...) do { \
		fprintf(stderr, "%s: In function '%s':\n%s:%d: %s: ", \
			__FILE__, __func__, __FILE__, __LINE__, type); \
		fprintf(stderr, msg, ## __VA_ARGS__); } while (0)

typedef enum { AUDIO_NONE, AUDIO_PORTAUDIO, AUDIO_PULSE, AUDIO_JACK, AUDIO_SNDFILE } opt_audio_drv;

typedef struct opt_data {
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

	const char *backend_opts;
} opt_data;

void optproc(int argc, char **argv, opt_data *res);

static inline void *xmalloc(size_t s) { void *res = malloc(s); if(!res) abort(); return res; }

#define pbattr restrict __attribute__((aligned (16)))
typedef uint16_t * restrict __attribute__ ((aligned (16)))  pixbuf_t;

typedef struct {
	uint16_t w, h;
} ivec;

#endif

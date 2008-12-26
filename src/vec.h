#ifndef VEC_H
#define VEC_H

#include <unistd.h>
#include <glib.h>

#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>

// sse
//typedef double v2df __attribute__ ((vector_size (16)));
typedef float  v4sf __attribute__ ((vector_size (16)));
typedef gint32 v4si __attribute__ ((vector_size (16)));
typedef gint16 v8hi __attribute__ ((vector_size (16)));
typedef gint8  v16i __attribute__ ((vector_size (16)));

// mmx
typedef gint32 v2si __attribute__ ((vector_size (8)));
typedef gint16 v4hi __attribute__ ((vector_size (8)));
typedef gint8  v8qi __attribute__ ((vector_size (8)));



#endif

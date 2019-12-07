
EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include ${top_builddir}/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-I${top_srcdir}/vis \
	-I${top_srcdir}/extern/tinycthread \
	-I${top_srcdir}/extern \
	-I${top_srcdir}/src \
	-I${top_srcdir}/gstreamer

AM_LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,--as-needed

EXTRA_LTLIBRARIES = libx86vec.la

lib_LTLIBRARIES = libgstjuliavis.la
libgstjuliavis_la_CFLAGS = $(GST_CFLAGS)
libgstjuliavis_la_LIBADD = $(GST_LIBS) libx86vec.la
libgstjuliavis_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -no-undefined
libgstjuliavis_la_LIBTOOLFLAGS = --tag=disable-static


libx86vec_la_CFLAGS = -O3 -fmerge-all-constants -finline-functions
libx86vec_la_SOURCES = \
vis/software/x86/cpuid.c \
vis/software/x86/x86_features.h \
vis/software/x86/mmx.c \
vis/software/x86/sse.c \
vis/software/x86/sse2.c \
vis/software/x86/ssse3.c \
vis/software/x86/sse4_1.c \
vis/software/x86/avx2.c \
vis/software/x86/palblit_mmxsse.h

libgstjuliavis_la_SOURCES = \
vis/common.h \
vis/colourspace.h \
vis/pallet.c \
vis/pallet.h \
vis/points.c \
vis/points.h \
vis/getsamp.h \
vis/audio/beat.c \
vis/audio/beat.h \
vis/audio/fft.c \
vis/software/pixmisc.c \
vis/software/pixmisc.h \
vis/software/pixformat.h \
vis/software/maxsrc.c \
vis/software/maxsrc.h \
vis/software/map.c \
vis/software/map.h \
vis/software/palletblit.c \
vis/software/simple_main.c \
vis/software/simple_main.h \
vis/software/resample_image.h \
vis/paratask/paratask.c \
vis/paratask/paratask.h \
extern/isaac/isaac.c \
extern/isaac/isaac.h \
extern/tinycthread/tinycthread.c \
extern/tinycthread/tinycthread.h \
src/element2.c \
src/element2.h \
src/plugin2.c \
src/gstaudiovisualizer2.c \
src/gstaudiovisualizer2.h

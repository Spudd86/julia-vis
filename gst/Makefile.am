
EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include ${top_builddir}/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-I${top_srcdir}/vis \
	-I${top_srcdir}/src \
	-I${top_srcdir}/gstreamer

AM_LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,--as-needed

plugin_LTLIBRARIES = libgstjuliavis.la

libgstjuliavis_la_SOURCES = \
vis/common.h \
vis/colourspace.h \
vis/pallet.c \
vis/pallet.h \
vis/points.c \
vis/points.h \
vis/getsamp.h \
vis/isaac/isaac.c \
vis/isaac/isaac.h \
vis/audio/beat.c \
vis/audio/beat.h \
vis/audio/fft.c \
vis/software/x86/cpuid.c \
vis/software/x86/x86_features.h \
vis/software/x86/mmx.c \
vis/software/x86/sse.c \
vis/software/x86/sse2.c \
vis/software/x86/sse4_1.c \
vis/software/x86/avx2.c \
vis/software/x86/palblit_mmxsse.h \
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
src/element2.c \
src/element2.h \
src/plugin2.c \
src/gstaudiovisualizer2.c \
src/gstaudiovisualizer2.h

libgstjuliavis_la_CFLAGS = $(GST_CFLAGS) -I${top_srcdir}/gstreamer
libgstjuliavis_la_LIBADD = $(GST_LIBS)
libgstjuliavis_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -no-undefined
libgstjuliavis_la_LIBTOOLFLAGS = --tag=disable-static
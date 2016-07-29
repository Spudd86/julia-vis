
EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS = \
	-include ${top_builddir}/config.h \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-I${top_srcdir}/../src \
	-I${top_srcdir}/src \
	-I${top_srcdir}/gstreamer

AM_LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,--as-needed

plugin_LTLIBRARIES = libgstjuliavis.la

libgstjuliavis_la_SOURCES = \
../src/common.h \
../src/colourspace.h \
../src/pallet.c \
../src/pallet.h \
../src/points.c \
../src/points.h \
../src/isaac/isaac.c \
../src/isaac/isaac.h \
../src/audio/beat.c \
../src/audio/beat.h \
../src/audio/fft.c \
../src/software/x86/cpuid.c \
../src/software/x86/x86_features.h \
../src/software/x86/mmx.c \
../src/software/x86/sse.c \
../src/software/x86/sse2.c \
../src/software/x86/sse4_1.c \
../src/software/x86/palblit_mmxsse.h \
../src/software/pixmisc.c \
../src/software/pixmisc.h \
../src/software/maxsrc.c \
../src/software/maxsrc.h \
../src/software/map.c \
../src/software/map.h \
../src/software/palletblit.c \
../src/software/simple_main.c \
../src/software/simple_main.h \
src/element2.c \
src/element2.h \
src/plugin2.c \
src/gstaudiovisualizer2.c \
src/gstaudiovisualizer2.h

libgstjuliavis_la_CFLAGS = $(GST_CFLAGS) -I${top_srcdir}/gstreamer
libgstjuliavis_la_LIBADD = $(GST_LIBS)
libgstjuliavis_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -no-undefined
libgstjuliavis_la_LIBTOOLFLAGS = --tag=disable-static
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([julia-vis], [0.1], [BUG-REPORT-ADDRESS], [julia-vis], [http://github.com/Spudd86/julia-vis])
AC_CONFIG_SRCDIR([src/optproc.c])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_AUX_DIR([build-aux])

m4_include([m4/ax_check_gl.m4])
m4_include([m4/ax_check_glu.m4])
m4_include([m4/ax_pthread.m4])
m4_include([m4/ax_check_compile_flag.m4])

AM_INIT_AUTOMAKE([
  foreign
  1.11
  -Wall
  -Wno-portability
  silent-rules
  tar-pax
  no-dist-gzip
  dist-xz
  subdir-objects
])

AC_PROG_CC
AC_PROG_CXX
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CONFIG_HEADERS([config.h])

AX_PTHREAD
AX_CHECK_COMPILE_FLAG([-std=gnu11], [CFLAGS+=" -std=gnu11"], [
	AX_CHECK_COMPILE_FLAG([-std=c11], [CFLAGS+=" -std=c11"], [
		echo "Compiler does not support a new enough C standard"
		exit -1
	])
])

AC_C_CONST
AC_TYPE_SIZE_T
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_INT64_T
AC_TYPE_INT32_T
AC_TYPE_INT16_T
AC_TYPE_INT8_T
AC_TYPE_UINT64_T
AC_TYPE_UINT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T
AC_HEADER_STDBOOL

AC_SEARCH_LIBS([sqrt], [m])

LT_INIT([
  pic-only
  win32-dll
])
LT_LANG([C])

dnl give error and exit if we don't have pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])

#TODO: check for asm/intrinsic support

AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [enable debugging code, including assertions (default: disabled)]))
AS_IF([ test "x$enable_debug" = xyes ], [
  AC_DEFINE(DEBUG, 1, [Define to 1 if debug code should be enabled])
], [
  AC_DEFINE(NDEBUG, 1, [Define to 1 if assertions should be disabled.])
])
AC_SUBST(enable_debug)
AM_CONDITIONAL([DEBUG], [ test "x$enable_debug" = xyes ])

AC_ARG_ENABLE([vector-intrinsics], AS_HELP_STRING([--enable-vector-intrinsics], [enable use of x86 vector intrinsics instructions (default: automatic)]))
AC_SUBST(enable_vector_intrinsics)
AS_IF([ test "x$enable_vector_intrinsics" = xno ], [
	AC_DEFINE([DISABLE_X86_INTRIN], 1, [Define to 1 to disable use x86 vector intrinsics])
])


PKG_CHECK_MODULES([SDL], [sdl])
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

AC_ARG_ENABLE([jack], AS_HELP_STRING([--enable-jack], [enable using jack for audio input (default: disabled)]))
AS_IF([ test "x$enable_jack" = xyes ], [
	PKG_CHECK_MODULES([JACK], [jack], [
			AC_DEFINE([HAVE_JACK], [1], [Define if jack is enabled])
			have_1_audio_driver=yes
		],
		[AC_MSG_FAILURE([--enable-jack was given, but test for jack failed])]
	)
])
AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)
#AC_SUBST(enable_jack)
AM_CONDITIONAL([ENABLE_JACK], [ test "x$enable_jack" = xyes ])

AC_ARG_ENABLE([pulseaudio], AS_HELP_STRING([--enable-pulseaudio], [enable using pulseaudio for audio input (default: enabled)]))
AS_IF([ test "x$enable_pulseaudio" != xno ], [
	PKG_CHECK_MODULES([PULSE], [libpulse], [have_pulse=yes], [have_pulse=no])
])
AS_IF([ test "x$enable_pulseaudio" = xyes && test "x$have_pulse" = xno ], [
	AC_MSG_FAILURE([--enable-pulseaudio was given, but test for pulseaudio failed])
])
AS_IF([ test "x$have_pulse" = xyes], [
	AC_DEFINE([HAVE_PULSE], [1], [Define if pulseaudio is enabled])
	enable_pulseaudio=yes
	have_1_audio_driver=yes
])
AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)
#AC_SUBST(enable_pulseaudio)
AM_CONDITIONAL([ENABLE_PULSE], [ test "x$enable_pulseaudio" = xyes ])

AC_ARG_ENABLE([portaudio], AS_HELP_STRING([--enable-portaudio], [enable using portaudio for audio input (default: disabled, unless nothing else is found)]))
AS_IF([ test "x$enable_portaudio" != xno  -a \( "x$enable_portaudio" = xyes -o "x$have_1_audio_driver" != xyes \) ], [
	PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0], [have_portaudio=yes], [have_portaudio=no])
])
AS_IF([ test "x$enable_portaudio" = xyes && test "x$have_portaudio" != xyes ], [
	AC_MSG_FAILURE([--enable-portaudio was given, but test for portaudio failed])
])
AS_IF([ test "x$have_portaudio" = xyes], [
	AC_DEFINE([HAVE_PORTAUDIO], [1], [Define if portaudio is enabled])
	enable_portaudio=yes
	have_1_audio_driver=yes
])
AC_SUBST(PORTAUDIO_CFLAGS)
AC_SUBST(PORTAUDIO_LIBS)
AM_CONDITIONAL([ENABLE_PORTAUDIO], [ test "x$enable_portaudio" = xyes ])

AC_ARG_ENABLE([sndfile], AS_HELP_STRING([--enable-sndfile], [enable using libsndfile for audio input (default: enabled)]))
AS_IF([ test "x$enable_sndfile" != xno ], [
	PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.2, have_sndfile=yes, have_sndfile=no)
])
AS_IF([ test "x$enable_sndfile" = xyes && test "x$have_sndfile" = xno ], [
	AC_MSG_FAILURE([--enable-sndfile was given, but test for libsndfile failed])
])
AS_IF([ test "x$have_sndfile" = xyes], [
	AC_DEFINE([HAVE_SNDFILE], [1], [Define if libsndfile is enabled])
	enable_sndfile=yes
	have_1_audio_driver=yes
])
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)
AM_CONDITIONAL([ENABLE_SNDFILE], [ test "x$enable_sndfile" = xyes ])

AS_IF([ test "x$have_1_audio_driver" != xyes], [
	AC_MSG_FAILURE([no audio drivers found!])
])

AC_ARG_ENABLE([opengl], AS_HELP_STRING([--disable-opengl], [enable opengl based versions (default: enabled)]))
#TODO: make work on windows
AS_IF([ test "x$enable_opengl" != xno], [
	#PKG_CHECK_MODULES([GL], [gl glu],[no_gl=no],[no_gl=yes])
	#AS_IF([ test "x$no_gl" = xyes ], [
	#	AX_CHECK_GL
	#	AX_CHECK_GLU
	#	AS_IF([ test "x$no_gl" = xyes ], [ no_gl=yes ])
	#])
	AC_PATH_X
	AX_CHECK_GL
	AX_CHECK_GLU
	echo no_gl = $no_gl no_glu = $no_glu
	AS_IF([ test "x$no_glu" = xyes ], [ no_gl=yes ])
	#TODO: test windows GL here if we havent found it yet
    AS_IF([ test "x$no_gl" = xyes && test "x$enable_opengl" = xyes ], [
    	AC_MSG_FAILURE([--enable-gl was given, but test for gl failed (old gl?)])
    ])
])
AS_IF([ test "x$no_gl" != xyes ], [
	AC_DEFINE([HAVE_GL], [1], [Define if we have gl])
	enable_opengl=yes
])
AS_IF([ test "x$no_gl" = xyes ], [ enable_opengl=no ])
#AC_SUBST(enable_opengl)
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)
AC_SUBST(GLU_CFLAGS)
AC_SUBST(GLU_LIBS)
AM_CONDITIONAL([ENABLE_GL], [ test "x$enable_opengl" = xyes ])

AC_ARG_ENABLE([x11], AS_HELP_STRING([--enable-x11], [enable x11 opengl based versions (default: enabled)]))
AS_IF([ test "x$enable_x11" != xno ], [
	PKG_CHECK_MODULES([X11], [x11], [have_x11=yes], [have_x11=no])
])
AS_IF([ test "x$enable_x11" = xyes && test "x$have_x11" = xno ], [
	AC_MSG_FAILURE([--enable-x11 was given, but test for x11 failed])
])
AS_IF([ test "x$have_x11" = xyes], [
	AC_DEFINE([HAVE_X11], [1], [Define if X is enabled])
	enable_x11=yes
])
AC_SUBST(X11_LIBS)
AC_SUBST(X11_CFLAGS)
#AC_SUBST(enable_x11)
AM_CONDITIONAL([ENABLE_X11], [ test "x$enable_x11" = xyes ])

AC_ARG_ENABLE([kms], AS_HELP_STRING([--enable-kms], [enable kms/dri opengl based version (default: disabled)]))
if test "x$enable_kms" != xyes; then
	enable_kms=no
fi
AM_CONDITIONAL([ENABLE_KMS], [ test "x$enable_kms" = xyes ])

AS_IF([ test "x$enable_opengl" = xyes], [
	AC_ARG_ENABLE([egl], AS_HELP_STRING([--enabled-egl], [enable egl based versions (default: automatic)]))
	AS_IF([ test "x$enable_egl" != xno], [
		PKG_CHECK_MODULES([EGL], [egl], [have_egl=yes], [have_egl=no])
	])
	AS_IF([ test "x$enable_egl" = xyes && test "x$have_egl" = xno ], [
		AC_MSG_FAILURE([--enable-egl was given, but test for egl failed])
	])
	AS_IF([ test "x$have_egl" = xyes], [
		AC_DEFINE([HAVE_EGL], [1], [Define if we have egl])
		enable_egl=yes
	])
	AC_SUBST(EGL_CFLAGS)
	AC_SUBST(EGL_LIBS)
	AM_CONDITIONAL([ENABLE_EGL], [ test "x$enable_egl" = xyes ])
])

AS_IF([ test "x$enable_kms" = xyes ], [
	PKG_CHECK_MODULES(GBM, [gbm], [gbm_enabled=yes], [gbm_enabled=no])
	PKG_CHECK_MODULES(DRM, [libdrm], [drm_enabled=yes], [drm_enabled=no])
])
AC_SUBST(GBM_CFLAGS)
AC_SUBST(GBM_LIBS)
AC_SUBST(DRM_CFLAGS)
AC_SUBST(DRM_LIBS)


AC_ARG_ENABLE([gst], AS_HELP_STRING([--enable-gst], [enable gstreamer visualization plugin (default: enabled)]))
AS_IF([ test "x$enable_gst" != xno ], [
	PKG_CHECK_MODULES(GST, [
      gstreamer-1.0
      gstreamer-base-1.0
      gstreamer-audio-1.0
      gstreamer-video-1.0
      gstreamer-pbutils-1.0 
    ], [ have_gst=yes ], [ have_gst=no ])
])
AS_IF([ test "x$enable_gst" = xyes && test "x$have_gst" = xno ], [
	AC_MSG_FAILURE([--enable-gst-plugin was given, but test for GStreamer failed])
])
AS_IF([ test "x$have_gst" = xyes], [
	enable_gst=yes
])
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)
AM_CONDITIONAL([ENABLE_GST], [ test "x$enable_gst" = xyes ])

#FIXME currently build is broken
#AC_ARG_ENABLE([orc], AS_HELP_STRING([--enable-orc], [enable using orc (default: disabled)]))
#AS_IF([ test "x$enable_orc" = xyes ], [
#	PKG_CHECK_MODULES([ORC], [orc-0.4], 
#		[AC_DEFINE([HAVE_ORC], [1], [Define if we have orc])],
#		[AC_MSG_FAILURE([--enable-orc was given, but test for orc failed])])
#])
AC_SUBST(ORC_CFLAGS)
AC_SUBST(ORC_LIBS)
AM_CONDITIONAL([ENABLE_ORC], [ test "x$enable_orc" = xyes ])


AC_ARG_ENABLE([atomics], AS_HELP_STRING([--disable-atomics], [enable using pthread locks to syncronize tribufs instead of atomic ops (default: disabled)]))
AM_CONDITIONAL([DISABLE_ATOMICS], [ test "x$enable_atomics" = xno ]) # Disable atomics ONLY if explicitly told to

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_INT64_T
AC_TYPE_INT32_T
AC_TYPE_INT16_T
AC_TYPE_INT8_T
AC_TYPE_UINT64_T
AC_TYPE_UINT32_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T
AC_HEADER_STDBOOL
AC_CHECK_HEADERS_ONCE([execinfo.h])
#AC_CHECK_HEADERS_ONCE([sys/mman.h])

#TODO: rig up a test and define HAVE_TYPEOF for ccan stuff

# Checks for library functions.
AC_FUNC_MMAP

AC_CONFIG_FILES(Makefile, echo timestamp > stamp-h)

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE $VERSION
        =====

        prefix:                 ${prefix}
        sysconfdir:             ${sysconfdir}
        libdir:                 ${libdir}
        includedir:             ${includedir}

        compiler:               ${CC}
        cflags:                 ${CFLAGS}
        ldflags:                ${LDFLAGS}

        logging:                ${enable_logging}
        debug:                  ${enable_debug}
])
#TODO: add outputs about enabled features and discovered libs

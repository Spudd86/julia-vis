#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([julia-opengl], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/optproc.c])

AC_USE_SYSTEM_EXTENSIONS
USE_LANGUAGES=gnu99

AC_ARG_ENABLE([mmx], AS_HELP_STRING([--enable-mmx], [enable use of mmx instructions (default: disabled)]))
if test "x$enable_mmx" = xyes; then
	CFLAGS="$CFLAGS -mmmx"
fi
AC_ARG_ENABLE([sse], AS_HELP_STRING([--enable-sse], [enable use of sse instructions (default: disabled)]))
if test "x$enable_sse" = xyes; then
	CFLAGS="$CFLAGS -mmmx -msse"
fi
AC_ARG_ENABLE([sse2], AS_HELP_STRING([--enable-sse2], [enable use of sse2 instructions (default: disabled)]))
if test "x$enable_sse2" = xyes; then
	CFLAGS="$CFLAGS -mmmx -msse -msse2"
fi
AC_ARG_ENABLE([3dnow], AS_HELP_STRING([--enable-3dnow], [enable use of 3dnow instructions (default: disabled)]))
if test "x$enable_3dnow" = xyes; then
	CFLAGS="$CFLAGS -mmmx -m3dnow"
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99

AC_C_CONST
AC_TYPE_SIZE_T
AC_EXEEXT
AC_CHECK_LIB([m], [sqrt]) #see if libm exists, if it does we probably need it

PKG_CHECK_MODULES([SDL], [sdl])
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

PKG_CHECK_MODULES([FFTW], [fftw3f])
AC_SUBST(FFTW_CFLAGS)
AC_SUBST(FFTW_LIBS)

PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0])
AC_SUBST(PORTAUDIO_CFLAGS)
AC_SUBST(PORTAUDIO_LIBS)

#PKG_CHECK_MODULES([GL], [gl])
#AC_SUBST(GL_CFLAGS)
#AC_SUBST(GL_LIBS)

AC_ARG_ENABLE([directfb], AS_HELP_STRING([--enable-directfb], [build test-directfb (default: disabled)]))
if test "x$enable_directfb" = xyes; then
	PKG_CHECK_MODULES([DIRECTFB], [directfb],
		[AC_DEFINE([HAVE_DIRECTFB], [1], [Define if directfb is enabled])],
		[AC_MSG_FAILURE([--enable-directfb was given, but test for directfb failed])]
	)
fi
AC_SUBST(DIRECTFB_CFLAGS)
AC_SUBST(DIRECTFB_LIBS)
AC_SUBST(enable_directfb)

AC_ARG_ENABLE([jack], AS_HELP_STRING([--enable-jack], [enable using jack for audio input (default: disabled)]))
if test "x$enable_jack" = xyes; then
	PKG_CHECK_MODULES([JACK], [jack],
		[AC_DEFINE([HAVE_JACK], [1], [Define if jack is enabled])],
		[AC_MSG_FAILURE([--enable-jack was given, but test for jack failed])]
	)
fi
AC_SUBST(JACK_CFLAGS)
AC_SUBST(JACK_LIBS)
AC_SUBST(enable_jack)

AC_ARG_ENABLE([pulseaudio], AS_HELP_STRING([--enable-pulseaudio], [enable using pulseaudio for audio input (default: disabled)]))
if test "x$enable_pulseaudio" = xyes; then
	PKG_CHECK_MODULES([PULSE], [libpulse],
		[AC_DEFINE([HAVE_PULSE], [1], [Define if pulseaudio is enabled])
		 
		],
		[AC_MSG_FAILURE([--enable-pulseaudio was given, but test for pulseaudio failed])]
	)
fi
AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)
AC_SUBST(enable_pulseaudio)

AC_ARG_ENABLE([opengl], AS_HELP_STRING([--disable-opengl], [enable opengl based versions (default: enabled)]))
if test "x$enable_opengl" != xno; then
	enable_opengl=yes
fi
AC_SUBST(enable_opengl)

#TODO: make work on windows
if test "x$enable_opengl" = xyes; then
	PKG_CHECK_MODULES([GL], [glu],
        [AC_DEFINE([HAVE_GL], [1], [Define if we have gl])],
        [AC_MSG_FAILURE([--enable-gl was given, but test for gl failed (old gl?)])])
fi
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)


AC_ARG_ENABLE([orc], AS_HELP_STRING([--enable-orc], [enable using orc (default: disabled)]))
if test "x$enable_orc" = xyes; then
	PKG_CHECK_MODULES([ORC], [orc-0.4], 
		[AC_DEFINE([HAVE_ORC], [1], [Define if we have orc])],
		[AC_MSG_FAILURE([--enable-orc was given, but test for orc failed])])
fi
AC_SUBST(ORC_CFLAGS)
AC_SUBST(ORC_LIBS)
AC_SUBST(enable_orc)

AC_ARG_ENABLE([locking-tb], AS_HELP_STRING([--enable-locking-tb], [enable using pthread locks to syncronize tribufs instead of atomic ops (default: disabled)]))
AC_SUBST(enable_locking_tb)

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
#AC_CHECK_HEADERS_ONCE([sys/mman.h])

# Checks for library functions.
AC_FUNC_MMAP

AC_CONFIG_FILES(Makefile, echo timestamp > stamp-h)

AC_OUTPUT

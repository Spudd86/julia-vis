#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([julia-opengl], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/optproc.c])
#INCLUDES = 'AC_INCLUDES_DEFAULT'

USE_LANGUAGES=gnu99

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99

AC_C_CONST
AC_TYPE_SIZE_T
AC_EXEEXT
AC_CHECK_LIB([m], [sqrt])

PKG_CHECK_MODULES([SDL], [sdl])
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)
AC_CHECK_LIB([SDL_ttf], [TTF_Init])

PKG_CHECK_MODULES([FFTW], [fftw3f])
AC_SUBST(FFTW_CFLAGS)
AC_SUBST(FFTW_LIBS)

PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0])
AC_SUBST(PORTAUDIO_CFLAGS)
AC_SUBST(PORTAUDIO_LIBS)

AC_ARG_ENABLE([directfb], AS_HELP_STRING([--enable-directfb], [build test-directfb (default: disabled)]))
if test "x$enable_directfb" = xyes; then
	PKG_CHECK_MODULES([DIRECTFB], [directfb],
		[AC_DEFINE([HAVE_DIRECTFB], [1], [Define if directfb is enabled])
		 AC_SUBST(DIRECTFB_CFLAGS)
		 AC_SUBST(DIRECTFB_LIBS)
		],
		[AC_MSG_FAILURE([--enable-directfb was given, but test for directfb failed])]
	)
fi

AC_ARG_ENABLE([jack], AS_HELP_STRING([--enable-jack], [enable using jack for audio input (default: disabled)]))
if test "x$enable_jack" = xyes; then
	PKG_CHECK_MODULES([JACK], [jack],
		[AC_DEFINE([HAVE_JACK], [1], [Define if jack is enabled])
		 AC_SUBST(JACK_CFLAGS)
		 AC_SUBST(JACK_LIBS)
		 AC_SUBST(HAVE_JACK)
		],
		[AC_MSG_FAILURE([--enable-jack was given, but test for jack failed])]
	)
fi

AC_ARG_ENABLE([pulseaudio], AS_HELP_STRING([--enable-pulseaudio], [enable using pulseaudio for audio input (default: disabled)]))
if test "x$enable_pulseaudio" = xyes; then
	PKG_CHECK_MODULES([PULSE], [libpulse],
		[AC_DEFINE([HAVE_PULSE], [1], [Define if pulseaudio is enabled])
		 AC_SUBST(PULSE_CFLAGS)
		 AC_SUBST(PULSE_LIBS)
		 AC_SUBST(HAVE_PULSE)
		],
		[AC_MSG_FAILURE([--enable-pulseaudio was given, but test for pulseaudio failed])]
	)
fi

AC_ARG_ENABLE([locking-tb], AS_HELP_STRING([--enable-locking-tb], [enable using pthread locks to syncronize tribufs instead of atomic ops (default: disabled)]))
if test "x$enable_locking_tb" = xyes; then
	AC_SUBST(LOCKING_TB, [1])
fi

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

# Checks for library functions.
AC_CHECK_FUNCS([atexit])

#AH_BOTTOM(echo 'AC_INCLUDES_DEFAULT')
AC_CONFIG_FILES(Makefile, echo timestamp > stamp-h)

AC_OUTPUT

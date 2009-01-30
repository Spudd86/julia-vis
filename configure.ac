#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([julia-opengl], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([src/optproc.c])
#INCLUDES = `AC_INCLUDES_DEFAULT'

USE_LANGUAGES=gnu99

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99

AC_C_CONST
AC_TYPE_SIZE_T
AC_EXEEXT

PKG_CHECK_MODULES([SDL], [sdl])
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

PKG_CHECK_MODULES([FFTW], [fftw3f])
AC_SUBST(FFTW_CFLAGS)
AC_SUBST(FFTW_LIBS)

PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0])
AC_SUBST(PORTAUDIO_CFLAGS)
AC_SUBST(PORTAUDIO_LIBS)

AC_ARG_ENABLE([directfb], 
		[AS_HELP_STRING([--enable-directfb], [build test-directfb])],
		[], [enable_directfb=no])
AS_IF([test "x$enable_directfb" != xno],
	[
		PKG_CHECK_MODULES([DIRECTFB], [directfb],
			[AC_DEFINE([HAVE_DIRECTFB], [1], [Define if you have libreadline])
			 AC_SUBST(DIRECTFB_CFLAGS)
			 AC_SUBST(DIRECTFB_LIBS)
			],
			[AC_MSG_FAILURE([--enable-directfb was given, but test for directfb failed])]
		)
	])


AC_CHECK_LIB([m], [sqrt])
AC_CHECK_LIB([SDL_ttf], [TTF_Init])

# Checks for typedefs, structures, and compiler characteristics.
#AC_HEADER_STDBOOL
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
AC_FUNC_MALLOC
AC_CHECK_FUNCS([atexit])

#AH_BOTTOM(echo 'AC_INCLUDES_DEFAULT')
AC_CONFIG_FILES(Makefile, echo timestamp > stamp-h)

AC_OUTPUT

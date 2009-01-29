#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([julia-opengl], [0.1], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([src/optproc.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lSDL_ttf':
AC_CHECK_LIB([SDL_ttf], [main])
# FIXME: Replace `main' with a function in `-lasound':
AC_CHECK_LIB([asound], [main])
# FIXME: Replace `main' with a function in `-lfftw3f':
AC_CHECK_LIB([fftw3f], [main])
# FIXME: Replace `main' with a function in `-ljack':
AC_CHECK_LIB([jack], [main])
# FIXME: Replace `main' with a function in `-lm':
AC_CHECK_LIB([m], [main])
# FIXME: Replace `main' with a function in `-lportaudio':
AC_CHECK_LIB([portaudio], [main])
# FIXME: Replace `main' with a function in `-lpthread':
AC_CHECK_LIB([pthread], [main])
# FIXME: Replace `main' with a function in `-lrt':
AC_CHECK_LIB([rt], [main])

# Checks for header files.
AC_CHECK_HEADERS([malloc.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_C_RESTRICT
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([atexit memset sqrt])

AC_CONFIG_FILES(Makefile, echo timestamp > stamp-h)
AC_OUTPUT

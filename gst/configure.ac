dnl required version of autoconf
AC_PREREQ(2.60)

AC_INIT([julia-vis],[1.0.0])

AC_CONFIG_SRCDIR([src/element2.c])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([
  check-news
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
AC_CONFIG_HEADERS([config.h])

dnl check for tools (compiler etc.)
AC_PROG_CC_STDC
AC_USE_SYSTEM_EXTENSIONS
USE_LANGUAGES=gnu11

AC_SYS_LARGEFILE
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])

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
AC_HEADER_ASSERT

AC_SEARCH_LIBS([sqrt], [m])

LT_INIT([
  disable-static
  pic-only
  win32-dll
])

dnl give error and exit if we don't have pkgconfig
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, [ ], [
  AC_MSG_ERROR([You need to have pkg-config installed!])
])

# >= 1.8.0
PKG_CHECK_MODULES(GST, [
  gstreamer-1.0
  gstreamer-base-1.0
  gstreamer-audio-1.0
  gstreamer-video-1.0
  gstreamer-pbutils-1.0 
], [
  AC_SUBST(GST_CFLAGS)
  AC_SUBST(GST_LIBS)
], [
  AC_MSG_ERROR([
      You need to install or upgrade the GStreamer development
      packages on your system. On debian-based systems these are
      libgstreamer1.0-dev and libgstreamer-plugins-base1.0-dev.
      on RPM-based systems gstreamer1.0-devel, libgstreamer1.0-devel
      or similar. The minimum version required is $GST_REQUIRED.
  ])
])

dnl check if compiler understands -Wall (if yes, add -Wall to GST_CFLAGS)
AC_MSG_CHECKING([to see if compiler understands -Wall])
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wall"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([ ], [ ])], [
  GST_CFLAGS="$GST_CFLAGS -Wall"
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])
])

dnl set the plugindir where plugins should be installed (for src/Makefile.am)
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-1.0/plugins"
else
  plugindir="\$(libdir)/gstreamer-1.0"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AC_CONFIG_FILES([Makefile])
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
])
EXTRA_DIST =
CLEANFILES =
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory

AM_CFLAGS = $(PTHREAD_CFLAGS) ${my_CFLAGS}
AM_CPPFLAGS = ${my_CPPFLAGS} \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	-include ${top_builddir}/config.h \
	-I${top_srcdir}/src \
	-I${top_srcdir}/extern/tinycthread \
	-I${top_srcdir}/extern \
	-I${top_srcdir}/nuklear
AM_LDFLAGS = \
	-Wl,--gc-sections \
	-Wl,--as-needed \
	$(PTHREAD_LIBS)

AUDIO_CFLAGS = $(PA_CFLAGS) $(JACK_CFLAGS) $(PULSE_CFLAGS) $(SNDFILE_CFLAGS)
AUDIO_LIBS = $(PA_LIBS) $(JACK_LIBS) $(PULSE_LIBS) $(SNDFILE_LIBS)

GL_CFLAGS += $(GLU_CFLAGS) -DUSE_GL
GL_LIBS += $(GLU_LIBS)
KMS_CFLAGS = $(GBM_CFLAGS) $(DRM_CFLAGS)
KMS_LIBS = $(GBM_LIBS) $(DRM_LIBS)

if DEBUG
AM_CFLAGS += -ggdb3 -O0
else
AM_CFLAGS += -DNDEBUG
AM_CFLAGS += -g3 -O2
#AM_CFLAGS += -finline-functions -maccumulate-outgoing-args -fdelete-null-pointer-checks
#AM_CFLAGS += -funroll-loops -fpeel-loops -funswitch-loops -fvariable-expansion-in-unroller
#AM_CFLAGS += -ftree-vectorize -ftree-loop-ivcanon -ftree-loop-im -ftree-loop-linear
#AM_CFLAGS += -funsafe-loop-optimizations
#AM_CFLAGS += -fgcse-las -fgcse-sm -fmodulo-sched
#AM_CFLAGS += -fmerge-all-constants

# on gcc 4.3 set precsicion of FPU to 32 bits (can get speed up for floats)
# do this since everywhere inside our code we use only floats
# -mrecip = let gcc generate the RCPSS and RSQRTSS instructions (fast recipricol)
#AM_CFLAGS += -mpc32 -mrecip=all
#AM_CFLAGS += -ffast-math  -fassociative-math -freciprocal-math -fno-signed-zeros
endif

if DISABLE_ATOMICS
AM_CPPFLAGS += -DTB_NO_ATOMIC
endif

########################################################################################################################################
# Sources
########################################################################################################################################

src_audio_libaudio_drv_la_SOURCES = src/audio/audio.h
if ENABLE_JACK
src_audio_libaudio_drv_la_SOURCES += src/audio/jack.c
endif
if ENABLE_PULSE
src_audio_libaudio_drv_la_SOURCES += src/audio/pulseaudio.c
endif
if ENABLE_PORTAUDIO
src_audio_libaudio_drv_la_SOURCES += src/audio/portaudio.c
endif
if ENABLE_SNDFILE
src_audio_libaudio_drv_la_SOURCES += src/audio/filedecode.c
endif

GL_COMMON_HDRS = src/opengl/glfract.h src/opengl/glmaxsrc.h src/opengl/glpallet.h src/opengl/glscope.h

GL_COMMON_SRCS = src/opengl/glmain.c src/opengl/glmisc.c src/opengl/glscope.c src/opengl/mandelbrot.c src/opengl/offscreen.c

GL_FIXED_SRCS = src/opengl/fixed/glf_maxsrc.c src/opengl/fixed/glf_pal.c src/opengl/fixed/glmap.c src/opengl/fixed/glmap.h src/opengl/fixed/glf_fract.c

GL_GLSL_SRCS =  src/opengl/glsl/glsl_maxsrc.c src/opengl/glsl/glsl_pallet.c src/opengl/glsl/glsl_fract.c

########################################################################################################################################
# Internal libraries
########################################################################################################################################

EXTRA_LTLIBRARIES = \
src/audio/libaudio-drv.la src/audio/libaudio.la \
extern/libtinycthread.la \
src/libtribuf.la \
src/libparatask.la \
src/libcommon.la \
src/opengl/libglloader.la \
src/opengl/libglx.la \
src/opengl/libglcommon.la \
src/software/libsdlcommon.la \
src/software/libx86.la \
src/software/libsoftcore.la

if ENABLE_JACK
src_audio_libaudio_drv_la_SOURCES += src/audio/jack.c
endif
if ENABLE_PULSE
src_audio_libaudio_drv_la_SOURCES += src/audio/pulseaudio.c
endif
if ENABLE_PORTAUDIO
src_audio_libaudio_drv_la_SOURCES += src/audio/portaudio.c
endif
if ENABLE_SNDFILE
src_audio_libaudio_drv_la_SOURCES += src/audio/filedecode.c
endif
src_audio_libaudio_drv_la_CFLAGS = $(AM_CFLAGS) $(AUDIO_CFLAGS)
src_audio_libaudio_drv_la_LDFLAGS = $(AUDIO_LIBS)

src_audio_libaudio_la_SOURCES = \
src/audio/audio.c src/audio/audio.h \
src/audio/beat.c src/audio/beat.h \
src/audio/fft.c \
src/audio/audio-private.h
src_audio_libaudio_la_CFLAGS = $(AM_CFLAGS) $(AUDIO_CFLAGS)
src_audio_libaudio_la_LDFLAGS = $(AUDIO_LIBS)
src_audio_libaudio_la_LIBADD = src/audio/libaudio-drv.la src/libtribuf.la


src_opengl_libglloader_la_SOURCES = src/opengl/gl_14.c src/opengl/gl_14.h
src_opengl_libglloader_la_CFLAGS  = $(AM_CFLAGS) $(GL_CFLAGS)
src_opengl_libglloader_la_LIBADD  = $(GL_LIBS)

src_opengl_libglx_la_SOURCES = src/opengl/glx/glx_gen.c src/opengl/glx/glx_gen.h
src_opengl_libglx_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS) $(X11_CFLAGS)

src_opengl_libglcommon_la_SOURCES = $(GL_COMMON_SRCS) $(GL_COMMON_HDRS) $(GL_FIXED_SRCS) $(GL_GLSL_SRCS)
src_opengl_libglcommon_la_CFLAGS = $(AM_CFLAGS) $(GL_CFLAGS)
src_opengl_libglcommon_la_LIBADD = src/opengl/libglloader.la src/libcommon.la


src_software_libsdlcommon_la_SOURCES = src/optproc.c src/software/sdl/sdl-text.c src/sdl/sdlsetup.c src/software/sdl/sdl-palblit.c
src_software_libsdlcommon_la_CFLAGS = $(AM_CFLAGS) $(SDL_CFLAGS)
src_software_libsdlcommon_la_LIBADD = $(SDL_LIBS) src/software/libsoftcore.la

extern_libtinycthread_la_SOURCES = extern/tinycthread/tinycthread.c extern/tinycthread/tinycthread.h

src_libparatask_la_SOURCES = src/paratask/paratask.c src/paratask/paratask.h
src_libparatask_la_LIBADD  = extern/libtinycthread.la

src_libtribuf_la_SOURCES = src/tribuf.c src/tribuf.h src/common.h

src_libcommon_la_SOURCES = src/terminusIBM.c extern/isaac/isaac.c extern/isaac/isaac.h src/pallet.c src/pallet.h src/points.c src/points.h src/getsamp.h src/common.h src/colourspace.h

if !DEBUG
src_software_libx86_la_CFLAGS  = $(AM_CFLAGS) -O3 -fmerge-all-constants
endif
src_software_libx86_la_SOURCES = \
src/software/x86/cpuid.c \
src/software/x86/mmx.c \
src/software/x86/3dNOW.c \
src/software/x86/sse.c \
src/software/x86/sse2.c \
src/software/x86/ssse3.c \
src/software/x86/sse4_1.c \
src/software/x86/avx2.c \
src/software/x86/x86_features.h \
src/software/x86/palblit_mmxsse.h \
src/software/pixmisc.h \
src/common.h

src_software_libsoftcore_la_SOURCES = \
src/software/map.c src/software/map.h \
src/software/maxsrc.c src/software/maxsrc.h \
src/software/palletblit.c \
src/software/pixmisc.c src/software/pixmisc.h \
src/software/pixformat.h \
src/software/resample_image.h
src_software_libsoftcore_la_LIBADD = src/software/libx86.la src/libparatask.la src/libcommon.la

#nuklear_libnuklear_la_SOURCES = 

########################################################################################################################################
# Programs
########################################################################################################################################

bin_PROGRAMS = bin/sdl-test bin/sdlthread-test bin/audio-test
EXTRA_PROGRAMS = bin/glscope-test bin/pal-test bin/pal-test2 bin/palblit-test

if ENABLE_GL
bin_PROGRAMS += bin/gl-test
endif
if ENABLE_X11
bin_PROGRAMS += bin/glx-test
EXTRA_PROGRAMS += bin/fps-test
if ENABLE_EGL
bin_PROGRAMS += bin/egl-test
endif
endif
if ENABLE_KMS
bin_PROGRAMS += bin/kms-test
endif

# OpenGL versions

bin_gl_test_SOURCES = src/sdl/sdlglmain.c src/sdl/sdlsetup.c src/optproc.c
bin_gl_test_CFLAGS  = $(AM_CFLAGS) $(SDL_CFLAGS) $(GL_CFLAGS)
bin_gl_test_LDFLAGS = $(AM_LDFLAGS) $(SDL_LIBS)
bin_gl_test_LDADD = src/audio/libaudio.la src/libcommon.la src/libtribuf.la src/opengl/libglcommon.la

bin_glx_test_SOURCES = src/opengl/glx/glxmain.c src/optproc.c
bin_glx_test_CFLAGS  = $(AM_CFLAGS) $(X11_CFLAGS) $(GL_CFLAGS)
bin_glx_test_LDFLAGS = $(AM_LDFLAGS) $(X11_LIBS)
bin_glx_test_LDADD = extern/libtinycthread.la src/libtribuf.la src/audio/libaudio.la src/libcommon.la src/opengl/libglcommon.la src/opengl/libglx.la

bin_fps_test_SOURCES = src/opengl/glx/fpsmain.c src/opengl/fpsservo/fpsservo.c src/opengl/fpsservo/fpsservo.h src/optproc.c
bin_fps_test_CFLAGS  = $(AM_CFLAGS) $(X11_CFLAGS) $(GL_CFLAGS)
bin_fps_test_LDADD = extern/libtinycthread.la src/libtribuf.la src/audio/libaudio.la src/libcommon.la src/opengl/libglcommon.la src/opengl/libglx.la $(X11_LIBS)

bin_egl_test_SOURCES = src/opengl/egl/xegl.c src/opengl/eglmain.c src/opengl/egl/eglmisc.h src/optproc.c
bin_egl_test_CFLAGS  = $(AM_CFLAGS) $(X11_CFLAGS) $(GL_CFLAGS) $(EGL_CFLAGS)
bin_egl_test_LDADD = extern/libtinycthread.la src/libtribuf.la src/audio/libaudio.la src/libcommon.la src/opengl/libglcommon.la $(X11_LIBS) $(EGL_LIBS)

bin_kms_test_SOURCES = src/opengl/egl/kms.c src/optproc.c
bin_kms_test_CFLAGS  = $(AM_CFLAGS) $(KMS_CFLAGS) $(GL_CFLAGS)
bin_kms_test_LDADD = src/libtribuf.la src/audio/libaudio.la src/libcommon.la src/opengl/libglcommon.la $(KMS_LIBS) $(EGL_LIBS)

bin_glscope_test_SOURCES = $(GL_SDL_SRCS) src/opengl/glscope-test.c src/opengl/glscope.c src/opengl/glmisc.c src/optproc.c
bin_glscope_test_CFLAGS  = $(AM_CFLAGS) $(SDL_CFLAGS) $(GL_CFLAGS)
bin_glscope_test_LDADD = extern/libtinycthread.la src/libtribuf.la src/audio/libaudio.la src/libcommon.la src/opengl/libglloader.la 


# Software versions

bin_sdl_test_SOURCES = src/software/sdl/sdl.c
bin_sdl_test_CFLAGS  = $(AM_CFLAGS) $(SDL_CFLAGS)
bin_sdl_test_LDFLAGS = $(AM_LDFLAGS) $(SDL_LIBS)
bin_sdl_test_LDADD   = src/software/libsdlcommon.la src/audio/libaudio.la src/libcommon.la src/software/libsoftcore.la

bin_sdlthread_test_SOURCES = src/software/sdl/sdl-thread.c
bin_sdlthread_test_CFLAGS  = $(AM_CFLAGS) $(SDL_CFLAGS)
bin_sdlthread_test_LDFLAGS = $(AM_LDFLAGS) $(SDL_LIBS)
bin_sdlthread_test_LDADD   = src/software/libsdlcommon.la src/audio/libaudio.la src/libcommon.la src/software/libsoftcore.la


# Misc test programs

bin_audio_test_SOURCES = src/sdl/audiotest.c src/audio/beat.c src/audio/fft.c src/audio/rb.c src/audio/rb.h src/optproc.c src/software/sdl/sdl-text.c src/sdl/sdlsetup.c src/terminusIBM.c src/software/sdl/line.c
bin_audio_test_CFLAGS  = $(AM_CFLAGS) $(SDL_CFLAGS)
bin_audio_test_LDFLAGS = $(AM_LDFLAGS) $(SDL_LIBS)
bin_audio_test_LDADD   = extern/libtinycthread.la src/libtribuf.la src/audio/libaudio-drv.la

bin_pal_test_SOURCES = src/software/sdl/test_palletblit.c
bin_pal_test_CFLAGS  = $(AM_CFLAGS) $(SDL_CFLAGS)
bin_pal_test_LDFLAGS = $(AM_LDFLAGS) $(SDL_LIBS)
bin_pal_test_LDADD   = src/software/libsoftcore.la src/software/libsdlcommon.la src/libcommon.la

bin_pal_test2_SOURCES = src/software/sdl/test_pallets.c
bin_pal_test2_CFLAGS  = $(AM_CFLAGS) $(SDL_CFLAGS)
bin_pal_test2_LDFLAGS = $(AM_LDFLAGS) $(SDL_LIBS)
bin_pal_test2_LDADD   = src/software/libsoftcore.la src/software/libsdlcommon.la src/libcommon.la

bin_palblit_test_SOURCES = src/software/test/palblit.c
bin_palblit_test_CFLAGS  = $(AM_CFLAGS) $(SDL_CFLAGS)
bin_palblit_test_LDFLAGS = $(AM_LDFLAGS) $(SDL_LIBS)
bin_palblit_test_LDADD   = src/software/libsoftcore.la src/software/libsdlcommon.la src/libcommon.la



########################################################################################################################################
# GStreamer Plugin
########################################################################################################################################

if ENABLE_GST
lib_LTLIBRARIES = bin/libgstjuliavis.la
bin_libgstjuliavis_la_CPPFLAGS = $(AM_CPPFLAGS) -I${top_srcdir}/gst/src
bin_libgstjuliavis_la_CFLAGS = $(AM_CFLAGS) $(GST_CFLAGS)
bin_libgstjuliavis_la_LIBADD = $(GST_LIBS) src/software/libsoftcore.la
bin_libgstjuliavis_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -no-undefined
bin_libgstjuliavis_la_LIBTOOLFLAGS = --tag=disable-static
bin_libgstjuliavis_la_SOURCES = \
src/common.h \
src/audio/beat.c \
src/audio/beat.h \
src/audio/fft.c \
src/software/simple_main.c \
src/software/simple_main.h \
gst/src/element2.c \
gst/src/element2.h \
gst/src/plugin2.c \
gst/src/gstaudiovisualizer2.c \
gst/src/gstaudiovisualizer2.h
endif
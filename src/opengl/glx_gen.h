#ifndef POINTER_C_GENERATED_HEADER_GLXWIN_H
#define POINTER_C_GENERATED_HEADER_GLXWIN_H

#ifdef __glxext_h_
#error Attempt to include glx_exts after including glxext.h
#endif

#define __glxext_h_

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#ifdef CODEGEN_FUNCPTR
#undef CODEGEN_FUNCPTR
#endif /*CODEGEN_FUNCPTR*/
#define CODEGEN_FUNCPTR

#ifndef GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS
#define GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
#define GLvoid void

#endif /*GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS*/


#ifndef GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS
#define GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS


#endif /*GL_LOAD_GEN_BASIC_OPENGL_TYPEDEFS*/


#ifndef GLEXT_64_TYPES_DEFINED
/* This code block is duplicated in glext.h, so must be protected */
#define GLEXT_64_TYPES_DEFINED
/* Define int32_t, int64_t, and uint64_t types for UST/MSC */
/* (as used in the GLX_OML_sync_control extension). */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <inttypes.h>
#elif defined(__sun__) || defined(__digital__)
#include <inttypes.h>
#if defined(__STDC__)
#if defined(__arch64__) || defined(_LP64)
typedef long int int64_t;
typedef unsigned long int uint64_t;
#else
typedef long long int int64_t;
typedef unsigned long long int uint64_t;
#endif /* __arch64__ */
#endif /* __STDC__ */
#elif defined( __VMS ) || defined(__sgi)
#include <inttypes.h>
#elif defined(__SCO__) || defined(__USLC__)
#include <stdint.h>
#elif defined(__UNIXOS2__) || defined(__SOL64__)
typedef long int int32_t;
typedef long long int int64_t;
typedef unsigned long long int uint64_t;
#elif defined(_WIN32) && defined(__GNUC__)
#include <stdint.h>
#elif defined(_WIN32)
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
/* Fallback if nothing above works */
#include <inttypes.h>
#endif
#endif
	typedef struct __GLXFBConfigRec *GLXFBConfig;
	typedef XID GLXContextID;
	typedef struct __GLXcontextRec *GLXContext;
	typedef XID GLXPixmap;
	typedef XID GLXDrawable;
	typedef XID GLXPbuffer;
	typedef void (APIENTRY *__GLXextFuncPtr)(void);
	typedef XID GLXVideoCaptureDeviceNV;
	typedef unsigned int GLXVideoDeviceNV;
	typedef XID GLXVideoSourceSGIX;
	typedef struct __GLXFBConfigRec *GLXFBConfigSGIX;
	typedef XID GLXPbufferSGIX;
	typedef struct {
    char    pipeName[80]; /* Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX] */
    int     networkId;
} GLXHyperpipeNetworkSGIX;
	typedef struct {
    char    pipeName[80]; /* Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX] */
    int     channel;
    unsigned int participationType;
    int     timeSlice;
} GLXHyperpipeConfigSGIX;
	typedef struct {
    char pipeName[80]; /* Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX] */
    int srcXOrigin, srcYOrigin, srcWidth, srcHeight;
    int destXOrigin, destYOrigin, destWidth, destHeight;
} GLXPipeRect;
	typedef struct {
    char pipeName[80]; /* Should be [GLX_HYPERPIPE_PIPE_NAME_LENGTH_SGIX] */
    int XOrigin, YOrigin, maxHeight, maxWidth;
} GLXPipeRectLimits;

#ifdef __cplusplus
extern "C" {
#endif /*__cplusplus*/

extern int glx_ext_OML_sync_control;
extern int glx_ext_ARB_create_context;

#define GLX_CONTEXT_DEBUG_BIT_ARB 0x00000001
#define GLX_CONTEXT_FLAGS_ARB 0x2094
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092

#ifndef GLX_OML_sync_control
#define GLX_OML_sync_control 1
extern Bool (CODEGEN_FUNCPTR *_ptrc_glXGetMscRateOML)(Display *, GLXDrawable, int32_t *, int32_t *);
#define glXGetMscRateOML _ptrc_glXGetMscRateOML
extern Bool (CODEGEN_FUNCPTR *_ptrc_glXGetSyncValuesOML)(Display *, GLXDrawable, int64_t *, int64_t *, int64_t *);
#define glXGetSyncValuesOML _ptrc_glXGetSyncValuesOML
extern int64_t (CODEGEN_FUNCPTR *_ptrc_glXSwapBuffersMscOML)(Display *, GLXDrawable, int64_t, int64_t, int64_t);
#define glXSwapBuffersMscOML _ptrc_glXSwapBuffersMscOML
extern Bool (CODEGEN_FUNCPTR *_ptrc_glXWaitForMscOML)(Display *, GLXDrawable, int64_t, int64_t, int64_t, int64_t *, int64_t *, int64_t *);
#define glXWaitForMscOML _ptrc_glXWaitForMscOML
extern Bool (CODEGEN_FUNCPTR *_ptrc_glXWaitForSbcOML)(Display *, GLXDrawable, int64_t, int64_t *, int64_t *, int64_t *);
#define glXWaitForSbcOML _ptrc_glXWaitForSbcOML
#endif /*GLX_OML_sync_control*/ 

#ifndef GLX_ARB_create_context
#define GLX_ARB_create_context 1
extern GLXContext (CODEGEN_FUNCPTR *_ptrc_glXCreateContextAttribsARB)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
#define glXCreateContextAttribsARB _ptrc_glXCreateContextAttribsARB
#endif /*GLX_ARB_create_context*/ 

enum glx_LoadStatus
{
	glx_LOAD_FAILED = 0,
	glx_LOAD_SUCCEEDED = 1,
};

int glx_LoadFunctions(Display *display, int screen);


#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif //POINTER_C_GENERATED_HEADER_GLXWIN_H

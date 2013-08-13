/**
 * glmisc.h
 *
 */

#ifndef GLMISC_H_
#define GLMISC_H_

#include "gl_14.h"
#if HAVE_GL_GLU_H
#include <GL/glu.h>
#else
#include <OpenGL/glu.h>
#endif

//FBO management
struct oscr_ctx;
struct oscr_ctx *offscr_new(int width, int height, GLboolean force_fixed, GLboolean redonly);
GLuint offscr_get_src_tex(struct oscr_ctx *ctx);
GLuint offscr_start_render(struct oscr_ctx *ctx);
void offscr_finish_render(struct oscr_ctx *ctx);


void swap_buffers(void);
uint32_t get_ticks(void);
void dodelay(uint32_t ms);
uint64_t uget_ticks(void);
void udodelay(uint64_t us);

void draw_hist_array(int off, float scl, const int *array, int len);
void draw_hist_array_col(int off, float scl, const int *array, int len, float r, float g, float b);
void draw_hist_array_xlate(int off, float scl, float xlate, const int *array, int len, float r, float g, float b);

void render_debug_overlay(void);
void render_frame(GLboolean debug_maxsrc, GLboolean debug_pal, GLboolean show_mandel, GLboolean show_fps_hist);
void init_gl(const opt_data *opt_data, int width, int height);

GLhandleARB compile_program(const char *vert_shader, const char *frag_shader);
GLhandleARB compile_program_defs(const char *defs, const char *vert_shader, const char *frag_shader);

void setup_viewport(int width, int height);
void draw_string(const char *str);

#define CHECK_GL_ERR do { GLint glerr = glGetError(); if(glerr != GL_NO_ERROR) {\
	fprintf(stderr, "%s: In function '%s':\n%s:%d: Warning: %s\n", \
		__FILE__, __func__, __FILE__, __LINE__, gluErrorString(glerr)); \
		glerr = glGetError();\
		}\
	} while(0)

#ifdef NDEBUG
#define DEBUG_CHECK_GL_ERR
#else
#define DEBUG_CHECK_GL_ERR CHECK_GL_ERR
#endif

#define FLOAT_PACK_FUNCS \
		"vec4 encode( float v ) {\n"\
		"// use vec3's here because it'll be slightly faster on ATI...\n"\
		"// TODO: figure out fastest way to make this do what we want on non ati...\n"\
		"//	vec3 enc = vec3(1.0, 255.0, 65025.0) * clamp(v, 0.0, 1.0);\n"\
		"//	vec3 enc = vec3(1.0, 255.0, 65025.0) * v;\n"\
		"//	enc = enc - floor(enc);\n"\
		\
		"//	vec3 enc = fract(vec3(1.0, 255.0, 65025.0) * v);\n"\
		"//	enc -= enc.yzz * vec3(1.0/255.0,1.0/255.0,1.0/255.0);\n"\
		"//	return vec4(enc, 0.0);\n"\
		\
		"	vec2 enc = fract(vec2(1.0, 255.0) * v);\n"\
		"	enc -= enc.yy * vec2(1.0/255.0,1.0/255.0);\n"\
		"	return vec4(enc, 0.0, 1.0);\n"\
		"}\n"\
		"float decode(vec4 rgba ) {\n"\
		"//	return clamp(dot(rgba.xyz, vec3(1.0, 1/255.0, 1/65025.0) ), 0.0, 1.0);\n"\
		"//	return dot(rgba.xyz, vec3(1.0, 1.0/255.0, 1.0/65025.0) );\n"\
		"	return dot(rgba.xy, vec2(1.0, 1.0/255.0) );\n"\
		"//	return clamp( dot(rgba.xy, vec2(1.0-1.0/255.0, 1.0/255.0) ), 0.0, 1.0);\n"\
		"//	return rgba.x;\n"\
		"}\n"

#endif /* include guard */

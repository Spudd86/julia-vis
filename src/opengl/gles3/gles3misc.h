#ifndef GLES3MISC_H
#define GLES3MISC_H

#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#define CHECK_GL_ERR do { GLint glerr = glGetError(); while(glerr != GL_NO_ERROR) {\
	fprintf(stderr, "%s: In function '%s':\n%s:%d: Warning: %s\n", \
		__FILE__, __func__, __FILE__, __LINE__, gl_error_string(glerr)); \
		glerr = glGetError();\
		}\
	} while(0)

#ifdef NDEBUG
#define DEBUG_CHECK_GL_ERR
#else
#define DEBUG_CHECK_GL_ERR CHECK_GL_ERR
#endif

//FBO management
struct oscr_ctx;
struct oscr_ctx *offscr_new(int width, int height, GLboolean force_fixed, GLboolean redonly);
GLuint offscr_get_src_tex(struct oscr_ctx *ctx);
GLuint offscr_start_render(struct oscr_ctx *ctx);
void offscr_finish_render(struct oscr_ctx *ctx);

uint32_t get_ticks(void);
void dodelay(uint32_t ms);
uint64_t uget_ticks(void);
void udodelay(uint64_t us);

char const* gl_error_string(GLenum const err);
GLuint compile_program_defs(const char *defs, const char *vert_shader, const char *frag_shader);

#endif

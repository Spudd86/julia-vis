/**
 * glpallet.c
 *
 */

#include "common.h"

#include <SDL.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "points.h"
#include "sdl-misc.h"
#include "glmisc.h"
#include "gl_maxsrc.h"
#include "pixmisc.h"
#include "mymm.h"

#include "glpallet.h"

static int im_w, im_h;

uint32_t *get_active_pal(void);

static uint32_t *active_pal;
static GLuint pal_tex;
static GLhandleARB pal_prog = 0;
static GLint pal_loc=0, src_loc=0;

static void draw_palleted_glsl(GLuint draw_tex)
{
	glPushAttrib(GL_TEXTURE_BIT);
		glUseProgramObjectARB(pal_prog);
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_2D, draw_tex);
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_1D, pal_tex);
		glUniform1iARB(src_loc, 0);
		glUniform1iARB(pal_loc, 1);

		glBegin(GL_QUADS);
			glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
			glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
			glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
			glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
		glEnd();
		glUseProgramObjectARB(0);
	glPopAttrib();
}

//TODO: test this
//use NV_texture_shader (http://www.opengl.org/registry/specs/NV/texture_shader.txt)
static void draw_palleted_NV(GLuint draw_tex)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glEnable(GL_TEXTURE_SHADER_NV);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_2D, draw_tex);
	glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_TEXTURE_2D);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
	glTexEnvi(GL_TEXTURE_SHADER_NV, GL_PREVIOUS_TEXTURE_INPUT_NV, GL_TEXTURE0_ARB);
	glTexEnvi(GL_TEXTURE_SHADER_NV, GL_SHADER_OPERATION_NV, GL_DEPENDENT_AR_TEXTURE_2D_NV);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE); //TODO: make sure this is what we want
	glActiveTextureARB(GL_TEXTURE0_ARB);

	glBegin(GL_QUADS);
		glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
		glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
		glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
		glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
	glEnd();
	glPopClientAttrib();
	glPopAttrib();
}

//TODO: write code to do this using these extensions:
//		ATI_fragment_shader (http://www.opengl.org/registry/specs/ATI/fragment_shader.txt)
static const char *pal_frag_mix =
	"#version 120\n"
	FLOAT_PACK_FUNCS
	"uniform sampler2D src;\n"
	"uniform sampler1D pal;\n"
	"void main() {\n"
	"	gl_FragColor = texture1D(pal, decode(texture2D(src, gl_TexCoord[0].xy)));\n"
//	"	gl_FragColor = texture1D(pal, pow(decode(texture2D(src, gl_TexCoord[0].xy)), 1/3));\n"
//	"	gl_FragColor = pow(texture1D(pal, pow(decode(texture2D(src, gl_TexCoord[0].xy)), 1.5)), vec4(1/1.8));\n"
	"}";

static const char *pal_frag_shader =
	"#version 100\n"
	"uniform sampler2D src;\n"
	"uniform sampler1D pal;\n"
	"void main() {\n"
	"	gl_FragColor = texture1D(pal, texture2D(src, gl_TexCoord[0].xy).x);\n"
	"}";

static void pal_init_glsl(GLboolean float_packed_pixels)
{
	glPushAttrib(GL_TEXTURE_BIT);
	glGenTextures(1, &pal_tex);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, active_pal);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib();

	printf("Compiling pallet shader:\n");
	if(!float_packed_pixels)
		pal_prog = compile_program(NULL, pal_frag_shader);
	else
		pal_prog = compile_program(NULL, pal_frag_mix);
	pal_loc = glGetUniformLocationARB(pal_prog, "pal");
	src_loc = glGetUniformLocationARB(pal_prog, "src");
	printf("Pallet shader compiled\n");
}

// ********* FIXED FUNCTION STUFF
#define PALLET_OFFSCREEN_TEMP
static void pallet_blit32(uint32_t *restrict dest, const uint32_t *restrict src, unsigned int w, unsigned int h, const uint32_t *restrict pal);

static GLhandleARB pbos[2] = {0, 0}, srcpbos[2] = {0, 0};
static GLuint disp_texture;
static Pixbuf *disp_surf = NULL;
static GLboolean have_pbo = GL_FALSE;
#ifdef PALLET_OFFSCREEN_TEMP
static GLint fbo = 0, rbos[2];
#endif

static void pal_init_fixed() //FIXME
{
	disp_surf = malloc(sizeof(Pixbuf));
	disp_surf->bpp  = 32; disp_surf->w = im_w; disp_surf->h = im_h; // TODO: these should be the actual window size...
	disp_surf->pitch = disp_surf->w*sizeof(uint32_t);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

#ifdef PALLET_OFFSCREEN_TEMP
	glGenFramebuffersEXT(1, &fbo);
	glGenRenderbuffersEXT(2, rbos);
	for(int i=0; i<2; i++) {
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbos[i]);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, disp_surf->w, disp_surf->h);
	}
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
#endif

	glGenTextures(1, &disp_texture);
	glBindTexture(GL_TEXTURE_2D, disp_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	if(GLEW_ARB_pixel_buffer_object) {
		have_pbo = GL_TRUE;
		glGenBuffersARB(2, pbos);
		for(int i=0; i<2; i++) {
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[i]);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, disp_surf->w * disp_surf->h * sizeof(uint32_t), 0, GL_STREAM_DRAW_ARB);
		}
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);

		glGenBuffersARB(2, srcpbos);
		for(int i=0; i<2; i++) {
			glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[i]);
			glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, disp_surf->w * disp_surf->h * sizeof(uint32_t), 0, GL_STREAM_READ_ARB);
		}
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	} else {
		disp_surf->data = _mm_malloc(im_w * im_h * sizeof(uint32_t), 32);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,  im_w, im_h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, disp_surf->data);
	}

	glPopClientAttrib();
	glPopAttrib();
}

static GLuint frm = 0;

static void draw_palleted_fixed(GLint srctex) //FIXME
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_ALL_ATTRIB_BITS);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
	const GLint vp_w = vp[2], vp_h = vp[3];

#ifdef PALLET_OFFSCREEN_TEMP
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbos[frm%2]);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, vp_w, vp_h);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo); CHECK_GL_ERR;
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rbos[frm%2]);
	setup_viewport(vp_w, vp_h);
#endif

	glBindTexture(GL_TEXTURE_2D, srctex);
	glBegin(GL_QUADS);
		glTexCoord2d( 0, 0); glVertex2d(-1, -1);
		glTexCoord2d( 1, 0); glVertex2d( 1, -1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
		glTexCoord2d( 0, 1); glVertex2d(-1,  1);
	glEnd();
	int read_buf; glGetIntegerv(GL_DRAW_BUFFER, &read_buf);
	glReadBuffer(read_buf);

	if(have_pbo) {
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[(frm+1)%2]);
		//TODO: figure out how to make sure this uses a fast format
		glReadPixels(0, 0, vp[2], vp[3], GL_BGRA, GL_UNSIGNED_BYTE, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[(frm)%2]);

		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[frm%2]);

		void *dstdata = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		const void *srcdata = glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if(dstdata && srcdata)
			pallet_blit32(dstdata, srcdata, vp_w, vp_h, active_pal);
		if(srcdata) { glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB); }
		if(dstdata) { glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB); }

		glBindTexture(GL_TEXTURE_2D, disp_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, vp_w, vp_h, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	} else {
		void *srcdata = malloc(vp_w * vp_h * sizeof(uint32_t));
		glReadPixels(0, 0, vp[2], vp[3], GL_BGRA, GL_UNSIGNED_BYTE, srcdata);
		pallet_blit32(disp_surf->data, srcdata, vp_w, vp_h, active_pal);
		free(srcdata);
		glBindTexture(GL_TEXTURE_2D, disp_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, vp_w, vp_h, 0, GL_BGRA, GL_UNSIGNED_BYTE, disp_surf->data);
	}

#ifdef PALLET_OFFSCREEN_TEMP
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
#endif

	glBindTexture(GL_TEXTURE_2D, disp_texture);
	glBegin(GL_QUADS);
		glTexCoord2f(0,0); glVertex2d(-1, -1);
		glTexCoord2f(1,0); glVertex2d( 1, -1);
		glTexCoord2f(1,1); glVertex2d( 1,  1);
		glTexCoord2f(0,1); glVertex2d(-1,  1);
	glEnd();

	glPopClientAttrib();
	glPopAttrib();

	frm++;
}

static GLboolean use_glsl = GL_FALSE;
void pal_init(int width, int height, GLboolean packed_intesity_pixels, GLboolean force_fixed)
{
	im_w = width, im_h = height;

	if(GLEW_ARB_shading_language_100 && !force_fixed) {
		pallet_init(1);
		active_pal = get_active_pal();
		pal_init_glsl(packed_intesity_pixels);
		use_glsl = GL_TRUE;
	} else {
		pallet_init(0);
		active_pal = get_active_pal();
		pal_init_fixed();
	}
	CHECK_GL_ERR;
}

void pal_render(GLuint srctex)
{
	if(use_glsl)
		draw_palleted_glsl(srctex);
	else
		draw_palleted_fixed(srctex);
	CHECK_GL_ERR;
}

void pal_pallet_changed(void)
{
	if(use_glsl) { // only GLSL version uses a texture for now
		glPushAttrib(GL_TEXTURE_BIT);
		glBindTexture(GL_TEXTURE_1D, pal_tex);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, active_pal);
		glPopAttrib();
		CHECK_GL_ERR;
	}
}


static void pallet_blit32(uint32_t *restrict dest, const uint32_t *restrict src, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++)
		for(unsigned int x = 0; x < w; x++, dest++)
			*dest = pal[src[y*w + x]&0xFF]; // BGRA use blue
//			*dest = pal[(src[y*w + x]>>16)&0xFF]; // BGRA use red
}

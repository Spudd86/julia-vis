/**
 * glpallet.c
 *
 */

#include "common.h"

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "glmisc.h"
#include "pallet.h"
#include "glpallet.h"

static int im_w = 0, im_h = 0;

static const uint32_t *active_pal = NULL;
static GLuint pal_tex = 0;
static GLhandleARB pal_prog = 0;
static GLint pal_loc=0, src_loc=0;

static void draw_palleted_glsl(GLuint draw_tex)
{DEBUG_CHECK_GL_ERR;
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
	DEBUG_CHECK_GL_ERR;
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
	"}";

static const char *pal_frag_shader =
	"#version 100\n"
	"uniform sampler2D src;\n"
	"uniform sampler1D pal;\n"
	"void main() {\n"
	"	gl_FragColor = texture1D(pal, texture2D(src, gl_TexCoord[0].xy).x);\n"
	"}";

static void pal_init_glsl(GLboolean float_packed_pixels)
{CHECK_GL_ERR;
	glPushAttrib(GL_TEXTURE_BIT);
	glGenTextures(1, &pal_tex);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, active_pal);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib();
	DEBUG_CHECK_GL_ERR;

	printf("Compiling pallet shader:\n");
	if(!float_packed_pixels)
		pal_prog = compile_program(NULL, pal_frag_shader);
	else
		pal_prog = compile_program(NULL, pal_frag_mix);
	pal_loc = glGetUniformLocationARB(pal_prog, "pal");
	src_loc = glGetUniformLocationARB(pal_prog, "src");
	printf("Pallet shader compiled\n");
	CHECK_GL_ERR;
}

// ********* FIXED FUNCTION STUFF
//#define PALLET_OFFSCREEN_TEMP
static void pallet_blit32(uint32_t *restrict dest, const uint32_t *restrict src, unsigned int w, unsigned int h, const uint32_t *restrict pal);

static GLhandleARB pbos[2] = {-1, -1}, srcpbos[2] = {-1, -1};
static GLuint disp_texture = 0;
static void *fxdsrcbuf = NULL, *fxddstbuf = NULL;
static GLboolean have_pbo = GL_FALSE;
static int buf_w = 0,  buf_h = 0;
#ifdef PALLET_OFFSCREEN_TEMP
static GLint fbo = -1;
static GLint rbo = -1, rbos[] = {-1,-1};
static int rbow = 0, rboh = 0;
#endif

static void pal_init_fixed() //FIXME
{CHECK_GL_ERR;
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

#ifdef PALLET_OFFSCREEN_TEMP
	glGenFramebuffersEXT(1, &fbo);
//	glGenRenderbuffersEXT(1, &rbo);
	glGenRenderbuffersEXT(2, rbos);
#endif

	glGenTextures(1, &disp_texture);
	glBindTexture(GL_TEXTURE_2D, disp_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	//TODO: remove requirment for NPOT textures... (easy but broken on my laptop's mesa)
	if(GLEW_ARB_pixel_buffer_object) {
//	if(0) {
		have_pbo = GL_TRUE;
		glGenBuffersARB(2, pbos);
		glGenBuffersARB(2, srcpbos);
	} else {
		fxddstbuf = malloc(im_w * im_h * sizeof(uint32_t));
		fxdsrcbuf = malloc(im_w * im_h * sizeof(uint32_t));
	}

	glPopClientAttrib();
	glPopAttrib();
}

static GLuint frm = 0;

static void draw_palleted_fixed(GLint srctex) //FIXME
{DEBUG_CHECK_GL_ERR;
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_ALL_ATTRIB_BITS);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
	const GLint vp_w = vp[2], vp_h = vp[3];

	DEBUG_CHECK_GL_ERR;
	if(have_pbo && buf_w != vp_w && buf_h != vp_h) {
		buf_w = vp_w; buf_h = vp_h;
		for(int i=0; i<2; i++) {
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[i]);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, vp_w * vp_h * sizeof(uint32_t), 0, GL_STREAM_DRAW_ARB);
			glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[i]);
			glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, vp_w * vp_h * sizeof(uint32_t), 0, GL_STREAM_READ_ARB);
		}
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
		printf("Changed PBO size!\n");
		DEBUG_CHECK_GL_ERR;
	}

#ifdef PALLET_OFFSCREEN_TEMP
	glPushAttrib(GL_VIEWPORT_BIT); CHECK_GL_ERR;
	if(rbow != vp_w || rboh != vp_h) {
//		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbo);
//		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, vp_w, vp_h);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbos[0]);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, vp_w, vp_h);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbos[1]);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, vp_w, vp_h);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		rbow = vp_w; rboh = vp_h;
		printf("Changed RBO size!\n");
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo); CHECK_GL_ERR;
//	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rbo);
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

	//TODO: figure out how to make sure this uses a fast format
	if(have_pbo) {
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[(frm+1)%2]);
		glReadPixels(0, 0, vp_w, vp_h, GL_BGRA, GL_UNSIGNED_BYTE, 0);
#ifdef PALLET_OFFSCREEN_TEMP
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glPopAttrib();
#endif
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[(frm+1)%2]);

#ifndef PAL_BROKEN_DRAW_PIXELS
		glRasterPos2f(-1, -1); // currently this is broken in mesa 7.7 (fixed in master)
		glDrawPixels(vp_w, vp_h, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
#else
		glBindTexture(GL_TEXTURE_2D, disp_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, vp_w, vp_h, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL); //TODO: this could come out wrong some some hardware... (non pow2 tex)
		glBegin(GL_QUADS);
			glTexCoord2d( 0, 0); glVertex2d(-1, -1);
			glTexCoord2d( 1, 0); glVertex2d( 1, -1);
			glTexCoord2d( 1, 1); glVertex2d( 1,  1);
			glTexCoord2d( 0, 1); glVertex2d(-1,  1);
		glEnd();
#endif

		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[(frm)%2]);
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[frm%2]);
		void *dstdata = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		const void *srcdata = glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if(dstdata && srcdata) pallet_blit32(dstdata, srcdata, vp_w, vp_h, active_pal);
		else printf("Failed to map a pbo\n");
		if(srcdata) { glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB); }
		if(dstdata) { glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB); }
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	} else {
		glReadPixels(0, 0, vp[2], vp[3], GL_BGRA, GL_UNSIGNED_BYTE, fxdsrcbuf);
#ifdef PALLET_OFFSCREEN_TEMP
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glPopAttrib();
#endif
		pallet_blit32(fxddstbuf, fxdsrcbuf, vp_w, vp_h, active_pal);
		glRasterPos2f(-1, -1);
		glDrawPixels(vp_w, vp_h, GL_BGRA, GL_UNSIGNED_BYTE, fxddstbuf);
	}
	glPopClientAttrib();
	glPopAttrib();

	frm++;
}

static GLboolean use_glsl = GL_FALSE;
void pal_init(int width, int height, GLboolean packed_intesity_pixels, GLboolean force_fixed) {	CHECK_GL_ERR;
	pallet_init(0);
	active_pal = get_active_pal();
	im_w = width, im_h = height;
	use_glsl = GLEW_ARB_shading_language_100 && !force_fixed;
	if(use_glsl) pal_init_glsl(packed_intesity_pixels);
	else pal_init_fixed();
	CHECK_GL_ERR;
}

void pal_render(GLuint srctex) { DEBUG_CHECK_GL_ERR;
	if(use_glsl) draw_palleted_glsl(srctex);
	else draw_palleted_fixed(srctex);
	DEBUG_CHECK_GL_ERR;
}

void pal_pallet_changed(void) {
	if(use_glsl) { // only GLSL version uses a texture for now
		DEBUG_CHECK_GL_ERR;
		glPushAttrib(GL_TEXTURE_BIT);
		glBindTexture(GL_TEXTURE_1D, pal_tex);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, active_pal);
		glPopAttrib();
		DEBUG_CHECK_GL_ERR;
	}
}


static void pallet_blit32(uint32_t *restrict dest, const uint32_t *restrict src, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++)
		for(unsigned int x = 0; x < w; x++, dest++)
			*dest = pal[src[y*w + x]&0xFF]; // BGRA use blue
//			*dest = pal[(src[y*w + x]>>16)&0xFF]; // BGRA use red
}

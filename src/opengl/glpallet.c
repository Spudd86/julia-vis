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
static GLuint *map_tex;
static GLboolean have_glsl = GL_FALSE;

static void draw_palleted_glsl(GLuint draw_tex)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
		glUseProgramObjectARB(pal_prog);
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_2D, map_tex[draw_tex]);
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
	glBindTexture(GL_TEXTURE_2D, map_tex[draw_tex]);
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
static void pallet_blit32(uint32_t *restrict dest, const uint32_t *restrict src, GLboolean src10bit, unsigned int w, unsigned int h, const uint32_t *restrict pal);

static GLhandleARB pbos[2], srcpbos[2];
static GLuint disp_texture;
static Pixbuf *disp_surf = NULL;
static GLboolean have_pbo = GL_FALSE;
static GLuint fbo = 0;

static void pal_init_fixed() //FIXME
{
	glGenFramebuffersEXT(1, &fbo);

	disp_surf = malloc(sizeof(Pixbuf));
	disp_surf->bpp  = 32; disp_surf->w = im_w; disp_surf->h = im_h;
	disp_surf->pitch = disp_surf->w*sizeof(uint32_t);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	glGenTextures(1, &disp_texture);
	glBindTexture(GL_TEXTURE_2D, disp_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

//	if(GLEW_ARB_pixel_buffer_object) {
	if(0) {
		have_pbo = GL_TRUE;
		glGenBuffersARB(2, pbos);
		for(int i=0; i<2; i++) {
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[i]);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, disp_surf->w * disp_surf->h * sizeof(uint32_t), 0, GL_STREAM_DRAW_ARB);
		}

		glGenBuffersARB(2, srcpbos);
		for(int i=0; i<2; i++) {
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, srcpbos[i]);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, disp_surf->w * disp_surf->h * sizeof(uint32_t), 0, GL_STREAM_DRAW_ARB);
		}

		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
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

	glBindTexture(GL_TEXTURE_2D, map_tex[srctex]);
	GLboolean src30bit = GL_FALSE;
	GLint redsize; glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &redsize);
	if(redsize == 10) src30bit = GL_TRUE;

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); CHECK_GL_ERR;

	if(have_pbo) { // TODO: maybe use a FBO then attach our texture to it and do glReadPixels or something
////		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[srctex]); CHECK_GL_ERR;
//		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);CHECK_GL_ERR;
//		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, map_tex[srctex], 0);CHECK_GL_ERR;
//		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[frm%2]); CHECK_GL_ERR;
//		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);CHECK_GL_ERR;
//		glReadPixels(0, 0, im_w, im_h, GL_RGBA, GL_UNSIGNED_BYTE, 0);CHECK_GL_ERR;
//		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);CHECK_GL_ERR;
//
//		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[frm%2]); CHECK_GL_ERR;
////		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, disp_surf->w * disp_surf->h * sizeof(uint32_t), 0, GL_STREAM_DRAW_ARB);
////		disp_surf->data = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB); CHECK_GL_ERR;
//		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[(frm+1)%2]); CHECK_GL_ERR;
//		const void *srcdata = glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB); CHECK_GL_ERR;
//
//		disp_surf->data = malloc(disp_surf->w * disp_surf->h * sizeof(uint32_t));
//		if(disp_surf->data && srcdata)
//			pallet_blit32(disp_surf->data, disp_surf->pitch, srcdata, src30bit, disp_surf->w, disp_surf->w, disp_surf->h, active_pal);
//		if(srcdata) { glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB); CHECK_GL_ERR; }
//		if(disp_surf->data) { glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB); CHECK_GL_ERR; }
//		free(disp_surf->data);
//		disp_surf->data = NULL; srcdata = NULL;
//
//		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0); CHECK_GL_ERR;
//
//		glBindTexture(GL_TEXTURE_2D, disp_texture); CHECK_GL_ERR;
////		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, disp_surf->w, disp_surf->h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL); CHECK_GL_ERR;
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, disp_surf->w, disp_surf->h, 0, GL_RGB, GL_UNSIGNED_BYTE, disp_surf->data); CHECK_GL_ERR;
//
//		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0); CHECK_GL_ERR;
	} else {
		glBindTexture(GL_TEXTURE_2D, disp_texture);
//		printf("ARRRG\n");
		void *srcdata = malloc(disp_surf->w * disp_surf->h * sizeof(uint32_t));

		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, srcdata);  CHECK_GL_ERR;
//		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, srcdata);  CHECK_GL_ERR;
//		pallet_blit_Pixbuf(disp_surf, srcdata, disp_surf->w, disp_surf->h);
		pallet_blit32(disp_surf->data, srcdata, GL_FALSE, disp_surf->w, disp_surf->h, active_pal);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, disp_surf->w, disp_surf->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, disp_surf->data); CHECK_GL_ERR;
		free(srcdata);
	}

	glBindTexture(GL_TEXTURE_2D, disp_texture);
//	glBindTexture(GL_TEXTURE_2D, map_tex[srctex]);
	glBegin(GL_QUADS);
		glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
		glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
		glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
		glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
	glEnd(); CHECK_GL_ERR;

	glPopClientAttrib();
	glPopAttrib();

	frm++;
}

void pal_init(int width, int height, GLuint *textures, GLboolean float_packed_pixels)
{
	pallet_init(1);
	active_pal = get_active_pal();
	im_w = width, im_h = height;
	map_tex = textures;

	if(GLEW_ARB_shading_language_100) {
//	if(0) {
		pal_init_glsl(float_packed_pixels);
		have_glsl = GL_TRUE;
	} else
		pal_init_fixed();
}

void pal_render(GLuint srctex)
{
	if(have_glsl)
		draw_palleted_glsl(srctex);
	else
		draw_palleted_fixed(srctex);
}

void pal_pallet_changed(void)
{
	if(have_glsl) { // only GLSL version uses a texture for now
		glPushAttrib(GL_TEXTURE_BIT);
		glBindTexture(GL_TEXTURE_1D, pal_tex);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, active_pal);
		glPopAttrib();
	}
}

//#ifdef __MMX__
#if 0
static void pallet_blit32(uint32_t *restrict dest, const uint32_t *restrict src, GLboolean src10bit, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	const uint32_t src_mask = src10bit?(0x3FF<<6):0xFF00; const uint32_t src_shft = src10bit?16:18;
	const __m64 zero = _mm_cvtsi32_si64(0ll);
	const __m64 mask = (__m64)(0x00ff00ff00ff);
	const unsigned int dst_stride = w/4;
	const unsigned int src_stride = w;

	for(unsigned int y = 0; y < h; y++) {
		for(unsigned int x = 0; x < w; x+=4) {
			int v = (src[y*src_stride + x]>>src_shft)&src_mask;
			__builtin_prefetch(src + y*src_stride + x + 4, 0, 0);

			__m64 col1 = *(__m64 *)(pal+(v/256));
			__m64 col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

		    //col1 = (col2*v + col1*(0xff-v))/256;
			__m64 vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			__m64 tmp = col1;

			v = (src[y*src_stride + x + 1]>>src_shft)&src_mask;
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = _mm_packs_pu16(tmp, col1);
			//_mm_stream_pi((__m64 *)(dest + y*dst_stride + x), tmp);
			*(__m64 *)(dest + y*dst_stride + x) = tmp;

			v = (src[y*src_stride + x + 2]>>src_shft)&src_mask;
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = col1;

			v = (src[y*src_stride + x + 3]>>src_shft)&src_mask;
			col1 = *(__m64 *)(pal+(v/256));
			col2 = col1;
			col1 = _mm_unpacklo_pi8(col1, zero);
    		col2 = _mm_unpackhi_pi8(col2, zero);

			vt = _mm_set1_pi16(v);
			vt = _mm_and_si64(vt, mask);
			col2 = _mm_mullo_pi16(col2, vt);
			vt = _mm_andnot_si64(vt, mask); // vt = 255 - vt
			col1 = _mm_mullo_pi16(col1, vt);
    		col1 = _mm_add_pi16(col1, col2);
    		col1 = _mm_srli_pi16(col1, 8);

			tmp = _mm_packs_pu16(tmp, col1);
			//_mm_stream_pi((__m64 *)(dest + y*dst_stride + x+2), tmp);
			*(__m64 *)(dest + y*dst_stride + x + 2) = tmp;
		}
	}
	_mm_empty();
}
#else
static void pallet_blit32(uint32_t *restrict dest, const uint32_t *restrict src, GLboolean src10bit, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
	for(unsigned int y = 0; y < h; y++)
		for(unsigned int x = 0; x < w; x++)
			*(uint32_t *)(dest + y*w + x) = pal[(src[y*w + x]>>24)&0xFF];
}
#endif

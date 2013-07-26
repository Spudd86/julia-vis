
#include "common.h"
#include "pallet.h"
#include "opengl/glmisc.h"
#include "opengl/glpallet.h"

#define PALLET_OFFSCREEN_TEMP
//#define PAL_BROKEN_DRAW_PIXELS 1

static const uint32_t *active_pal = NULL;
static int im_w = 0, im_h = 0;
static GLuint pbos[2] = {-1, -1}, srcpbos[2] = {-1, -1};
static GLuint disp_texture = 0;
static void *fxdsrcbuf = NULL, *fxddstbuf = NULL;
static bool use_pbo = false;
static int buf_w = 0,  buf_h = 0;
static GLuint frm = 0;

#ifdef PALLET_OFFSCREEN_TEMP
static GLuint fbo = -1;
static GLuint rbo = -1;
static int rbow = 0, rboh = 0;
#endif

void pal_init_fixed(int width, int height) //FIXME
{CHECK_GL_ERR;
	
	pallet_init(0);
	active_pal = get_active_pal();
	im_w = width, im_h = height;

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

#ifdef PALLET_OFFSCREEN_TEMP
	glGenFramebuffersEXT(1, &fbo);
	glGenRenderbuffersEXT(1, &rbo);
#endif

	glGenTextures(1, &disp_texture);
	glBindTexture(GL_TEXTURE_2D, disp_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	//TODO: remove requirment for NPOT textures... (easy but broken on my laptop's mesa)
	if(GLEE_ARB_pixel_buffer_object) {
	//if(0) {
		use_pbo = GL_TRUE;
		glGenBuffersARB(2, pbos);
		glGenBuffersARB(2, srcpbos);
	} else {
		fxddstbuf = malloc(im_w * im_h * sizeof(uint32_t));
		fxdsrcbuf = malloc(im_w * im_h * sizeof(uint32_t));
	}

	glPopClientAttrib();
	glPopAttrib();
	CHECK_GL_ERR;
}

static void pallet_blit32(uint32_t *restrict dest, const uint32_t *restrict src, unsigned int w, unsigned int h, const uint32_t *restrict pal)
{
//TODO: figure out how to make sure we get the right pixels...
	for(unsigned int y = 0; y < h; y++)
		for(unsigned int x = 0; x < w; x++, dest++)
//			*dest = pal[src[y*w + x]&0xFF]; // BGRA use blue
			*dest = pal[(src[y*w + x]>>16)&0xFF]; // BGRA use red
}

void pal_render_fixed(GLuint srctex) //FIXME
{DEBUG_CHECK_GL_ERR;
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_ALL_ATTRIB_BITS);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
	const GLint vp_w = vp[2], vp_h = vp[3];

	DEBUG_CHECK_GL_ERR;
	if(use_pbo && buf_w != vp_w && buf_h != vp_h) {
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

	if(use_pbo) {
#ifdef PALLET_OFFSCREEN_TEMP
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		if(rbow != vp_w || rboh != vp_h) {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo); DEBUG_CHECK_GL_ERR;
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, 0);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbo);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA, vp_w, vp_h);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rbo);

			rbow = vp_w; rboh = vp_h;
			printf("Changed RBO size!\n");
		} else {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo); DEBUG_CHECK_GL_ERR;
		}
		setup_viewport(vp_w, vp_h);
#endif
		glBindTexture(GL_TEXTURE_2D, srctex);
		glBegin(GL_TRIANGLE_STRIP);
			glTexCoord2d( 0, 0); glVertex2d(-1, -1);
			glTexCoord2d( 1, 0); glVertex2d( 1, -1);
			glTexCoord2d( 0, 1); glVertex2d(-1,  1);
			glTexCoord2d( 1, 1); glVertex2d( 1,  1);
		glEnd();
		int read_buf; glGetIntegerv(GL_DRAW_BUFFER, &read_buf);
		glReadBuffer(read_buf);

		//TODO: figure out how to make sure this uses a fast format
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, srcpbos[(frm+1)%2]);
		glReadPixels(0, 0, vp_w, vp_h, GL_BGRA, GL_UNSIGNED_BYTE, 0);
#ifdef PALLET_OFFSCREEN_TEMP
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glMatrixMode(GL_PROJECTION);
		glPopAttrib();
#endif
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[(frm+1)%2]);
		glRasterPos2f(-1, -1);
		glDrawPixels(vp_w, vp_h, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

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
		glBindTexture(GL_TEXTURE_2D, srctex);
		glBegin(GL_TRIANGLE_STRIP);
			glTexCoord2d( 0, 0); glVertex2d(-1, -1);
			glTexCoord2d( 1, 0); glVertex2d( 1, -1);
			glTexCoord2d( 0, 1); glVertex2d(-1,  1);
			glTexCoord2d( 1, 1); glVertex2d( 1,  1);
		glEnd();
		int read_buf; glGetIntegerv(GL_DRAW_BUFFER, &read_buf);
		glReadBuffer(read_buf);
		glReadPixels(0, 0, vp[2], vp[3], GL_BGRA, GL_UNSIGNED_BYTE, fxdsrcbuf);
		pallet_blit32(fxddstbuf, fxdsrcbuf, vp_w, vp_h, active_pal);
		glRasterPos2f(-1, -1);
		glDrawPixels(vp_w, vp_h, GL_BGRA, GL_UNSIGNED_BYTE, fxddstbuf);
	}
	glPopClientAttrib();
	glPopAttrib();

	frm++;
	DEBUG_CHECK_GL_ERR;
}

#if 0
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

	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
		glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
		glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
		glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
	glEnd();
	glPopClientAttrib();
	glPopAttrib();
}

//TODO: write code to do this using these extensions:
//		ATI_fragment_shader (http://www.opengl.org/registry/specs/ATI/fragment_shader.txt)
#endif


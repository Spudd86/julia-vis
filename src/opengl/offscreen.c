#include "common.h"
#include "glmisc.h"

#define NUM_FBO_TEX 2

struct oscr_ctx {
	GLuint fbos[NUM_FBO_TEX], tex[NUM_FBO_TEX];
	uint32_t frm;
	int w, h;
};

struct oscr_ctx *offscr_new(int width, int height, GLboolean force_fixed, GLboolean redonly)
{(void)force_fixed;
	struct oscr_ctx *ctx = malloc(sizeof(*ctx));
	ctx->frm = 0;
	ctx->w = width, ctx->h = height;
	
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glGenFramebuffersEXT(NUM_FBO_TEX, ctx->fbos);
	glGenTextures(NUM_FBO_TEX, ctx->tex);
	
	for(int i=0; i<NUM_FBO_TEX; i++) {
		glBindTexture(GL_TEXTURE_2D, ctx->tex[i]);
		
		if(ogl_ext_ARB_texture_rg && redonly) { //TODO: use RG8 if we're doing float pack stuff
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, ctx->w, ctx->h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			printf("using R16 textures\n");
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, ctx->w, ctx->h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		}

		CHECK_GL_ERR;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ctx->fbos[i]);		
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, ctx->tex[i], 0);
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);	
	glPopAttrib();
	CHECK_GL_ERR;
	
	return ctx;
}

GLuint offscr_get_src_tex(struct oscr_ctx *ctx)
{
	return ctx->tex[(ctx->frm)%NUM_FBO_TEX];
}

//TODO: add support for re-sizing, allocating new textures just before
// binding the fbo

GLuint offscr_start_render(struct oscr_ctx *ctx)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
//	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_TEXTURE_BIT);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, ctx->fbos[ctx->frm%NUM_FBO_TEX]);
	
	glViewport(0, 0, ctx->w, ctx->h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	
	return ctx->tex[(ctx->frm+1)%NUM_FBO_TEX];
}

void offscr_finish_render(struct oscr_ctx *ctx)
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopAttrib();
	DEBUG_CHECK_GL_ERR;
	ctx->frm++;
}



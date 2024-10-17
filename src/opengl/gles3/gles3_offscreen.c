#include "common.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
#include "gles3misc.h"

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

	GLuint rbos[2];
	glGenRenderbuffers(NUM_FBO_TEX, rbos);
	glGenFramebuffers(NUM_FBO_TEX, ctx->fbos);
	glGenTextures(NUM_FBO_TEX, ctx->tex);

	for(int i=0; i<NUM_FBO_TEX; i++) {
		glBindTexture(GL_TEXTURE_2D, ctx->tex[i]);
		// glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, ctx->w, ctx->h, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL); CHECK_GL_ERR;
		// glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16F, ctx->w, ctx->h);
		glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, ctx->w, ctx->h);
		// glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16_EXT, ctx->w, ctx->h);
		// glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, ctx->w, ctx->h);
		CHECK_GL_ERR;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbos[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->tex[i], 0);
		glBindRenderbuffer(GL_RENDERBUFFER, rbos[i]);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, ctx->w, ctx->h);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbos[i]);
		CHECK_GL_ERR;
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
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
	glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbos[ctx->frm%NUM_FBO_TEX]);
	// glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ctx->fbos[ctx->frm%NUM_FBO_TEX]);
	// glBindFramebuffer(GL_READ_FRAMEBUFFER, ctx->fbos[ctx->frm%NUM_FBO_TEX]);
	// glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->tex[ctx->frm%NUM_FBO_TEX], 0);

	// TODO: save viewport
	glViewport(0, 0, ctx->w, ctx->h);
	return ctx->tex[(ctx->frm+1)%NUM_FBO_TEX];
}

void offscr_finish_render(struct oscr_ctx *ctx)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// TODO: restore viewport
	CHECK_GL_ERR;
	ctx->frm++;
}



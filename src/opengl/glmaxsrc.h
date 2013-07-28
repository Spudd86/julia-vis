/**
 * gl_maxsrc.h
 *
 */

#ifndef GL_MAXSRC_H_
#define GL_MAXSRC_H_

struct glmaxsrc_ctx {
	struct oscr_ctx *offscr;
	void (*update)(struct glmaxsrc_ctx *ctx, const float *audio, int audiolen);
};

struct glmaxsrc_ctx *maxsrc_new_glsl(int width, int height, GLboolean packed_intesity);
struct glmaxsrc_ctx *maxsrc_new_fixed(int width, int height);

static inline GLuint maxsrc_get_tex(struct glmaxsrc_ctx *ctx) {
	return offscr_get_src_tex(ctx->offscr);
}

static inline void maxsrc_update(struct glmaxsrc_ctx *ctx, const float *audio, int audiolen) {
	ctx->update(ctx, audio, audiolen);
}

#endif /* include guard */

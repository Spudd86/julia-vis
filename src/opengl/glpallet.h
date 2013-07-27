/**
 * glpallet.h
 *
 */

#ifndef GLPALLET_H_
#define GLPALLET_H_

struct glpal_ctx {
	struct pal_ctx *pal;
	void (*render)(struct glpal_ctx *, GLuint);
};

struct glpal_ctx *pal_init_glsl(GLboolean float_packed_pixels);
struct glpal_ctx *pal_init_fixed(int width, int height);

static inline void gl_pal_render(struct glpal_ctx *ctx, GLuint srctex) {
	ctx->render(ctx, srctex);
}

static inline int gl_pal_step(struct glpal_ctx *ctx, int step) {
	return pal_ctx_step(ctx->pal, step);
}

static inline void gl_pal_start_switch(struct glpal_ctx *ctx, int next) {
	pal_ctx_start_switch(ctx->pal, next);
}

static inline bool gl_pal_changing(struct glpal_ctx *ctx) {
	return pal_ctx_changing(ctx->pal);
}

#endif /* include guard */

/**
 * glpallet.h
 *
 */

#ifndef GLPALLET_H_
#define GLPALLET_H_

struct glpal_ctx {
	void (*render)(struct glpal_ctx *, GLuint);
	void (*start_switch)(struct glpal_ctx *ctx, int next);
	bool (*step)(struct glpal_ctx *ctx, uint8_t step);
	bool (*changing)(struct glpal_ctx *ctx);
};

struct glpal_ctx *pal_init_glsl(GLboolean float_packed_pixels);
struct glpal_ctx *pal_init_fixed(int width, int height);

static inline void gl_pal_render(struct glpal_ctx *ctx, GLuint srctex) {
	ctx->render(ctx, srctex);
}

static inline int gl_pal_step(struct glpal_ctx *ctx, int step) {
	return ctx->step(ctx, step);
}

static inline void gl_pal_start_switch(struct glpal_ctx *ctx, int next) {
	ctx->start_switch(ctx, next);
}

static inline bool gl_pal_changing(struct glpal_ctx *ctx) {
	return ctx->changing(ctx);
}

#endif /* include guard */

#ifndef GL_FRACT_H
#define GL_FRACT_H 1

struct glfract_ctx {
	struct oscr_ctx *offscr;
	void (*render)(struct glfract_ctx *ctx, const struct point_data *pd);
};

static inline GLuint fract_get_tex(struct glfract_ctx *ctx) {
	return offscr_get_src_tex(ctx->offscr);
}

static inline void render_fractal(struct glfract_ctx *ctx, const struct point_data *pd) {
	ctx->render(ctx, pd);
}

struct glfract_ctx *fractal_glsl_init(const opt_data *opts, int width, int height, GLboolean packed_intesity_pixels);
struct glfract_ctx *fractal_fixed_init(const opt_data *opts, int width, int height);


#endif

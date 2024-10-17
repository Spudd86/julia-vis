#ifndef GLES_FRACT_H
#define GLES_FRACT_H 1

typedef enum {
	GLES_MAP_FUNC_NORMAL,
	GLES_MAP_FUNC_RATIONAL,
	GLES_MAP_FUNC_BUTTERFLY,
	GLES_MAP_FUNC_POLY,
	GLES_MAP_FUNC_NUM_FUNCS
} gles_map_func;

struct glfract_ctx {
	struct oscr_ctx *offscr;
	void (*render)(struct glfract_ctx *ctx, const struct point_data *pd, GLuint maxsrc_tex);
	void (*change_func)(struct glfract_ctx *ctx, gles_map_func f);
};

static inline GLuint fract_get_tex(struct glfract_ctx *ctx) {
	return offscr_get_src_tex(ctx->offscr);
}

static inline void render_fractal(struct glfract_ctx *ctx, const struct point_data *pd, GLuint maxsrc_tex) {
	ctx->render(ctx, pd, maxsrc_tex);
}

static inline void change_map_func(struct glfract_ctx *ctx, gles_map_func f) {
	ctx->change_func(ctx, f);
}

struct glfract_ctx *fractal_gles_init(gles_map_func f, int width, int height);


#endif

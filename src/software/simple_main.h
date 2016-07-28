#ifndef JULIA_VIS_SIMPLE_MAIN_H__
#define JULIA_VIS_SIMPLE_MAIN_H__ 1

#include "software/pixformat.h"

typedef enum {
	SOFT_MAP_FUNC_NORMAL,
	SOFT_MAP_FUNC_NORMAL_INTERP,
	SOFT_MAP_FUNC_RATIONAL,
	SOFT_MAP_FUNC_RATIONAL_INTERP,
	SOFT_MAP_FUNC_BUTTERFLY,
	SOFT_MAP_FUNC_BUTTERFLY_INTERP,
} simple_soft_map_func;

struct simple_soft_ctx;

struct simple_soft_ctx *simple_soft_init(int w, int h, simple_soft_map_func map_func, int audio_rate, int audio_channels, julia_vis_pixel_format format);
void simple_soft_destroy(struct simple_soft_ctx *ctx);
int simple_soft_add_audio(struct simple_soft_ctx *ctx, const float *in, size_t n);
void simple_soft_render(struct simple_soft_ctx *ctx, Pixbuf *out, int64_t now, int64_t tick0);

void simple_soft_change_map_func(struct simple_soft_ctx *ctx, simple_soft_map_func func);

void simple_soft_set_pixel_format(struct simple_soft_ctx *ctx, julia_vis_pixel_format format);

#endif
#include "common.h"

#include "pallet.h"

#include "audio/beat.h"

#include "software/pixmisc.h"
#include "software/maxsrc.h"
#include "software/map.h"

#include "softcore.h"

struct softcore_ctx {
	soft_map_func map_func;
	bool rational_julia;

	int im_w, im_h;

	int m;
	uint16_t *map_surf[2];

	struct maxsrc *maxsrc;
	struct pal_ctx *pal_ctx;
	struct point_data *pd;

	uint32_t maxsrc_rate;
	uint64_t maxfrms;

	uint64_t beats;
	uint64_t last_beat_time;
};

void softcore_destroy(struct softcore_ctx *ctx)
{
	if(!ctx) return;

	if(ctx->map_surf[0]) aligned_free(ctx->map_surf[0]);
	if(ctx->map_surf[1]) aligned_free(ctx->map_surf[1]);

	if(ctx->maxsrc) maxsrc_delete(ctx->maxsrc);
	if(ctx->pd) destroy_point_data(ctx->pd);

	ctx->map_surf[0] = ctx->map_surf[1] = NULL;
	ctx->maxsrc = NULL;
	ctx->pd = NULL;
	free(ctx);
}

struct softcore_ctx *softcore_init(int w, int h, simple_soft_map_func map_func)
{
	struct softcore_ctx *ctx = malloc(sizeof(*ctx));
	if(!ctx) return NULL;

	ctx->map_surf[0] = ctx->map_surf[1] = NULL;
	ctx->maxsrc = NULL;
	ctx->pd = NULL;

	// force divisible by 16
	ctx->im_w = w - w%16;
	ctx->im_h = h - h%16;

	softcore_change_map_func(ctx, map_func);

	ctx->m = 0;
	ctx->map_surf[0] = aligned_alloc(64, ctx->im_w * ctx->im_h * sizeof(uint16_t));
	ctx->map_surf[1] = aligned_alloc(64, ctx->im_w * ctx->im_h * sizeof(uint16_t));
	if(!ctx->map_surf[0] || !ctx->map_surf[1]) goto fail;
	memset(ctx->map_surf[0], 0, ctx->im_w * ctx->im_h * sizeof(uint16_t));
	memset(ctx->map_surf[1], 0, ctx->im_w * ctx->im_h * sizeof(uint16_t));

	ctx->maxfrms = 1;
	ctx->maxsrc_rate = 24; //TODO: add a property that can change this
	ctx->maxsrc = maxsrc_new(ctx->im_w, ctx->im_h);
	ctx->pd = new_point_data(ctx->rational_julia?4:2);
	if(!ctx->maxsrc || !ctx->pd) goto fail;

	ctx->last_beat_time = 0;
	ctx->beats = 0;

	return ctx;
fail:
	softcore_destroy(ctx);
	return NULL;
}

uint64_t softcore_get_last_beat_count(struct softcore_ctx *ctx)
{
	return ctx->beats;
}

void softcore_get_buffer_dims(struct softcore_ctx *ctx, int* im_w, int* im_h)
{
	if(im_w) *im_w = ctx->im_w;
	if(im_h) *im_h = ctx->im_h;
}

void softcore_change_map_func(struct softcore_ctx *ctx, simple_soft_map_func func)
{
	switch(func) {
		case SOFT_MAP_FUNC_NORMAL:
			ctx->rational_julia = 0;
			ctx->map_func = soft_map;
			break;
		case SOFT_MAP_FUNC_NORMAL_INTERP:
			ctx->rational_julia = 0;
			ctx->map_func = soft_map_interp;
			break;
		case SOFT_MAP_FUNC_RATIONAL:
			ctx->rational_julia = 1;
			ctx->map_func = soft_map_rational;
			break;
		case SOFT_MAP_FUNC_RATIONAL_INTERP:
			ctx->rational_julia = 1;
			ctx->map_func = soft_map_rational_interp;
			break;
		case SOFT_MAP_FUNC_BUTTERFLY:
			ctx->rational_julia = 0;
			ctx->map_func = soft_map_butterfly;
			break;
		case SOFT_MAP_FUNC_BUTTERFLY_INTERP:
			ctx->rational_julia = 0;
			ctx->map_func = soft_map_butterfly_interp;
			break;
		default:
			break;
	}
}

const uint16_t *softcore_render(struct softcore_ctx *ctx, int64_t now, int64_t tick0, int64_t newbeat, const float *audio, size_t nsamp)
{
	ctx->m = (ctx->m+1)&0x1;

	if(tick0+(ctx->maxfrms*1000)/ctx->maxsrc_rate - now > 1000/ctx->maxsrc_rate) {
		maxsrc_update(ctx->maxsrc, audio, nsamp);
		ctx->maxfrms++;
	}

	if(!ctx->rational_julia) {
		ctx->map_func(ctx->map_surf[ctx->m], ctx->map_surf[(ctx->m+1)&0x1], ctx->im_w, ctx->im_h, ctx->pd);
		maxblend(ctx->map_surf[ctx->m], maxsrc_get(ctx->maxsrc), ctx->im_w, ctx->im_h);
	}

	if(ctx->rational_julia) {
		maxblend(ctx->map_surf[(ctx->m+1)&0x1], maxsrc_get(ctx->maxsrc), ctx->im_w, ctx->im_h);
		ctx->map_func(ctx->map_surf[ctx->m], ctx->map_surf[(ctx->m+1)&0x1], ctx->im_w, ctx->im_h, ctx->pd);
	}

	if(newbeat != ctx->beats && now - ctx->last_beat_time > 1000) {
		ctx->last_beat_time = now;
		update_points(ctx->pd, (now - tick0), 1);
	} else update_points(ctx->pd, (now - tick0), 0);
	ctx->beats = newbeat;

	return ctx->map_surf[ctx->m];
}

const uint16_t *get_last_maxsrc_buffer(struct softcore_ctx *ctx)
{
	return maxsrc_get(ctx->maxsrc);
}

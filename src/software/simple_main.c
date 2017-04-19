#include "common.h"

#include "pallet.h"

#include "audio/beat.h"

#include "software/pixmisc.h"
#include "software/maxsrc.h"
#include "software/map.h"

#include "simple_main.h"

//TODO: add ability to change audio sample-rate without resetting video
//TODO: add ability to change output video channel order without reset

struct simple_soft_ctx {
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
	uint64_t lastpalstep;

	struct beat_ctx *beat;
	uint64_t beats;
	uint64_t last_beat_time;

	int sample_rate;
	uint64_t sample_count;

	size_t audio_bufp;
	int channels;
	int nr_samp;
	float *cur_buf;
	float *prev_buf;
	float *fft_tmp;
	//kiss_fft_cpx *fft_out;
	//kiss_fftr_cfg fft_cfg;
};

void simple_soft_destroy(struct simple_soft_ctx *ctx)
{
	if(!ctx) return;

	if(ctx->map_surf[0]) aligned_free(ctx->map_surf[0]);
	if(ctx->map_surf[1]) aligned_free(ctx->map_surf[1]);
	if(ctx->fft_tmp) aligned_free(ctx->fft_tmp);
	if(ctx->cur_buf) aligned_free(ctx->cur_buf);
	if(ctx->prev_buf) aligned_free(ctx->prev_buf);

	if(ctx->maxsrc) maxsrc_delete(ctx->maxsrc);
	if(ctx->pal_ctx) pal_ctx_delete(ctx->pal_ctx);
	if(ctx->pd) destroy_point_data(ctx->pd);
	if(ctx->beat) beat_delete(ctx->beat);

	ctx->map_surf[0] = ctx->map_surf[1] = NULL;
	ctx->maxsrc = NULL;
	ctx->pal_ctx = NULL;
	ctx->pd = NULL;
	ctx->beat = NULL;
	ctx->cur_buf = ctx->prev_buf = ctx->fft_tmp = NULL;
	free(ctx);
}

struct simple_soft_ctx *simple_soft_init(int w, int h, simple_soft_map_func map_func, int audio_rate, int audio_channels, julia_vis_pixel_format format)
{
	struct simple_soft_ctx *ctx = malloc(sizeof(*ctx));
	if(!ctx) return NULL;

	ctx->map_surf[0] = ctx->map_surf[1] = NULL;
	ctx->maxsrc = NULL;
	ctx->pal_ctx = NULL;
	ctx->pd = NULL;
	ctx->beat = NULL;
	ctx->cur_buf = ctx->prev_buf = ctx->fft_tmp = NULL;

	// force divisible by 16
	ctx->im_w = w - w%16;
	ctx->im_h = h - h%16;

	simple_soft_change_map_func(ctx, map_func);

	ctx->m = 0;
	ctx->map_surf[0] = aligned_alloc(64, ctx->im_w * ctx->im_h * sizeof(uint16_t));
	ctx->map_surf[1] = aligned_alloc(64, ctx->im_w * ctx->im_h * sizeof(uint16_t));
	if(!ctx->map_surf[0] || !ctx->map_surf[1]) goto fail;
	memset(ctx->map_surf[0], 0, ctx->im_w * ctx->im_h * sizeof(uint16_t));
	memset(ctx->map_surf[1], 0, ctx->im_w * ctx->im_h * sizeof(uint16_t));

	ctx->pal_ctx = pal_ctx_pix_format_new(format);
	ctx->maxsrc_rate = 24; //TODO: add a property that can change this
	ctx->maxsrc = maxsrc_new(ctx->im_w, ctx->im_h);
	ctx->pd = new_point_data(ctx->rational_julia?4:2);
	if(!ctx->maxsrc || !ctx->pal_ctx || !ctx->pd) goto fail;

	ctx->last_beat_time = 0;
	ctx->lastpalstep = 0;
	ctx->maxfrms = 1;
	ctx->beats = 0;

	ctx->beat = beat_new();
	if(!ctx->beat) goto fail;
	ctx->audio_bufp = 0;
	ctx->sample_count = 0;
	ctx->sample_rate = audio_rate;
	ctx->channels = audio_channels;
	int nr_samp = ctx->nr_samp = (audio_rate<50000)?1024:2048;
	ctx->fft_tmp = aligned_alloc(64, nr_samp*sizeof(float));
	ctx->cur_buf = aligned_alloc(64, sizeof(float)*nr_samp/2);
	ctx->prev_buf = aligned_alloc(64, sizeof(float)*nr_samp/2);
	if(!ctx->fft_tmp || !ctx->cur_buf || !ctx->prev_buf) goto fail;
	memset(ctx->fft_tmp, 0, nr_samp*sizeof(float));
	memset(ctx->cur_buf, 0, sizeof(float)*nr_samp/2);
	memset(ctx->prev_buf, 0, sizeof(float)*nr_samp/2);

	return ctx;
fail:
	simple_soft_destroy(ctx);
	return NULL;
}

void simple_soft_set_pixel_format(struct simple_soft_ctx *ctx, julia_vis_pixel_format format)
{
	//TODO: remember old format and only do this if it's different
	//TODO: maybe try to preserve current pallet
	if(ctx->pal_ctx) pal_ctx_delete(ctx->pal_ctx);
	ctx->pal_ctx = pal_ctx_pix_format_new(format);
}

void simple_soft_change_map_func(struct simple_soft_ctx *ctx, simple_soft_map_func func)
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

// TODO: need a "now" take it as an argument
void simple_soft_render(struct simple_soft_ctx *ctx, Pixbuf *out, int64_t now, int64_t tick0)
{
	ctx->m = (ctx->m+1)&0x1;

	if(tick0+(ctx->maxfrms*1000)/ctx->maxsrc_rate - now > 1000/ctx->maxsrc_rate) {
		maxsrc_update(ctx->maxsrc, ctx->prev_buf, ctx->nr_samp/2);
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

	// rather than just audio clock
	if((now - ctx->lastpalstep)*256/1024 >= 1) { // want pallet switch to take ~2 seconds
		pal_ctx_step(ctx->pal_ctx, IMIN((now - ctx->lastpalstep)*256/1024, 32));
		ctx->lastpalstep = now;
	}

	pallet_blit_Pixbuf(out, ctx->map_surf[ctx->m], ctx->im_w, ctx->im_h, pal_ctx_get_active(ctx->pal_ctx));
	//pallet_blit_Pixbuf(out, maxsrc_get(ctx->maxsrc), ctx->im_w, ctx->im_h, pal_ctx_get_active(ctx->pal_ctx));

	int newbeat = beat_ctx_count(ctx->beat);
	if(newbeat != ctx->beats) pal_ctx_start_switch(ctx->pal_ctx, newbeat);

	if(newbeat != ctx->beats && now - ctx->last_beat_time > 1000) {
		ctx->last_beat_time = now;
		update_points(ctx->pd, (now - tick0), 1);
	} else update_points(ctx->pd, (now - tick0), 0);
	ctx->beats = newbeat;
}


//TODO: split this out so we can share with audiotest.c

void split_radix_real_complex_fft(float *x, uint32_t n);


/**
 * take nr_samp frames of audio and do update beat detection
 * http://www.gamedev.net/reference/programming/features/beatdetection/
 */
static float *do_fft(struct simple_soft_ctx *ctx, const float *in1, const float *in2)
{
	int nr_samp = ctx->nr_samp;
	float *fft = ctx->fft_tmp;

	for(int i=0; i<nr_samp; i++) { // window samples
		float w = 0.5f*(1.0f - cosf((2*M_PI_F*i)/(nr_samp-1)));
		fft[i] = ((i < nr_samp/2)?in1[i]:in2[i-nr_samp/2])*w;
	}

	split_radix_real_complex_fft(ctx->fft_tmp, nr_samp);

	const float scl = 2.0f/nr_samp;
	fft[0] = fabsf(fft[0])*scl;
	for(int i=1; i < nr_samp/2; i++)
		fft[i] = sqrtf(fft[i]*fft[i] + fft[nr_samp-i]*fft[nr_samp-i])*scl;
	fft[nr_samp/2] = fabsf(fft[nr_samp/2])*scl;
#endif

	return fft;
}

// want compiler to be able to make the tail recursion optimisation
static int real_add_audio(struct simple_soft_ctx *ctx, const float *in, size_t n, int buf_count)
{
	int bufp = ctx->audio_bufp;
	const int nr_samp = ctx->nr_samp;

	size_t remain = 0;
	size_t cpy = MIN(n, nr_samp/2-bufp);
	if(ctx->channels == 1) {
		memcpy(ctx->cur_buf+bufp, in, sizeof(float)*cpy);
		in += cpy;
	} else {
		float *p = ctx->cur_buf+bufp;
		for(unsigned int i=0; i<cpy; i++, p++, in += 2) *p = (in[0]+in[1])/2;
	}
	remain = n - cpy;
	bufp = (bufp + cpy)%(nr_samp/2);

	//ctx->sample_count += cpy;
	ctx->audio_bufp = bufp;

	if(bufp == 0) { // do we have enough samples to do beat detection?
		float *fft = do_fft(ctx, ctx->cur_buf, ctx->prev_buf);
		beat_ctx_update(ctx->beat, fft, nr_samp/2);

		float *tmp = ctx->cur_buf;
		ctx->cur_buf = ctx->prev_buf;
		ctx->prev_buf = tmp;

		buf_count++;
	}

	if(remain > 0) return real_add_audio(ctx, in, remain, buf_count);
	return buf_count;
}

int simple_soft_add_audio(struct simple_soft_ctx *ctx, const float *in, size_t n)
{
	ctx->sample_count += n;
	return real_add_audio(ctx, in, n, 0);
}

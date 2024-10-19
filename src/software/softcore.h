#ifndef JULIA_VIS_SOFTCORE_H__
#define JULIA_VIS_SOFTCORE_H__ 1

#include "software/pixformat.h"

//TODO: rename these to something better, maybe FRACTAL_TYPE
// also maybe split out the interpolation as a quality setting?
typedef enum {
	SOFT_MAP_FUNC_NORMAL,
	SOFT_MAP_FUNC_NORMAL_INTERP,
	SOFT_MAP_FUNC_RATIONAL,
	SOFT_MAP_FUNC_RATIONAL_INTERP,
	SOFT_MAP_FUNC_BUTTERFLY,
	SOFT_MAP_FUNC_BUTTERFLY_INTERP,
} simple_soft_map_func;

struct softcore_ctx;

struct softcore_ctx *softcore_init(int w, int h, simple_soft_map_func map_func);
void softcore_destroy(struct softcore_ctx *ctx);

void softcore_change_map_func(struct softcore_ctx *ctx, simple_soft_map_func func);

uint64_t softcore_get_last_beat_count(struct softcore_ctx *ctx);

/**
 * Query real buffer dimensions, for indexing buffer returned from softcore_render()
 */
void softcore_get_buffer_dims(struct softcore_ctx *ctx, int* im_w, int* im_h);

/**
 * Update state and render new frame, returns pointer to current frame. Pointer remains valid, but may be overwritten by subsequent calls.
 *
 * now - tick0 should form a monotonic sequence at every call, that is for every call now - tick0 is not less than it has ever been before
 * now and tick0 should be timestamps in milliseconds
 */
const uint16_t *softcore_render(struct softcore_ctx *ctx, int64_t now, int64_t tick0, int64_t newbeat, const float *audio, size_t nsamp);

const uint16_t *get_last_maxsrc_buffer(struct softcore_ctx *ctx);

#endif

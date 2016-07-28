/**
 * pallet.h
 *
 */

#ifndef PALLET_H_
#define PALLET_H_

#include "software/pixformat.h"

//TODO: now that we've dropped the global variable based version rename these functions
struct pal_ctx;
struct pal_ctx *pal_ctx_new(int bswap);
struct pal_ctx *pal_ctx_pix_format_new(julia_vis_pixel_format format);
void pal_ctx_delete(struct pal_ctx *self);
const uint32_t *pal_ctx_get_active(struct pal_ctx *self);
void pal_ctx_start_switch(struct pal_ctx *self, int next);
int pal_ctx_step(struct pal_ctx *self, uint8_t step);
int pal_ctx_changing(struct pal_ctx *self);

float pal_ctx_get_pos(struct pal_ctx *self);
uint32_t *pal_ctx_get_pal(int pal);

struct pal_lst {
	int numpals;
	uint32_t pallets[][256];
};
struct pal_lst * pallet_get_palettes(void);

#endif /* include guard */

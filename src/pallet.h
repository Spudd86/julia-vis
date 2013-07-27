/**
 * pallet.h
 *
 */

#ifndef PALLET_H_
#define PALLET_H_

const uint32_t *get_active_pal(void);
void pallet_init(int bswap);
int pallet_step(int step);
void pallet_start_switch(int nextpal);
int get_pallet_changing(void);

int pallet_num_pal(void);
uint32_t *pallet_get_pal(int pal);

struct pal_ctx;
struct pal_ctx *pal_ctx_new(void);
const uint32_t *pal_ctx_get_active(struct pal_ctx *self);
int pal_ctx_changing(struct pal_ctx *self);
float pal_ctx_get_pos(struct pal_ctx *self);
void pal_ctx_start_switch(struct pal_ctx *self, int next);
int pal_ctx_step(struct pal_ctx *self, uint8_t step);
uint32_t *pal_ctx_get_pal(int pal);

#endif /* include guard */

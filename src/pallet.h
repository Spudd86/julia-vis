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

#endif /* include guard */

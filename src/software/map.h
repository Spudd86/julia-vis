#ifndef MAP_H
#define MAP_H

#include "points.h"

//#define MAP_FUNC_ATTR __attribute__((sseregparm,fastcall,nothrow))
#define MAP_FUNC_ATTR __attribute__((nothrow))

void soft_map(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd) MAP_FUNC_ATTR;
void soft_map_bl(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd) MAP_FUNC_ATTR;
void soft_map_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd) MAP_FUNC_ATTR;
void soft_map_rational(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd) MAP_FUNC_ATTR;
void soft_map_rational_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd) MAP_FUNC_ATTR;
void soft_map_butterfly(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd) MAP_FUNC_ATTR;
#endif

#ifndef MAP_H
#define MAP_H

#include "points.h"

typedef void (*soft_map_func)(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);

void soft_map(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);
void soft_map_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);
void soft_map_rational(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);
void soft_map_rational_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);
void soft_map_butterfly(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);
void soft_map_butterfly_interp(uint16_t *restrict out, uint16_t *restrict in, int w, int h, const struct point_data *pd);
#endif

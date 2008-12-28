#ifndef MAP_H
#define MAP_H

#include <unistd.h>
#include <stdint.h>

void soft_map_ref(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0);
void soft_map_ref2(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0);
void soft_map_bl(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0);
void soft_map_bl8x8(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0);
void soft_map8x8(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0);

#endif
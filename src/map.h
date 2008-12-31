#ifndef MAP_H
#define MAP_H

#include <unistd.h>
#include <stdint.h>

//#define MAP_FUNC_ATTR __attribute__((sseregparm,fastcall,nothrow))
#define MAP_FUNC_ATTR __attribute__((nothrow))

void soft_map(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0) MAP_FUNC_ATTR;
void soft_map_bl(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0) MAP_FUNC_ATTR;
void soft_map_bl8x8(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0) MAP_FUNC_ATTR;
void soft_map8x8(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0) MAP_FUNC_ATTR;

void soft_map_interp8x8(uint16_t *out, uint16_t *in, int w, int h, float x0, float y0) MAP_FUNC_ATTR;

#endif
/**
 * glpallet.h
 *
 */

#ifndef GLPALLET_H_
#define GLPALLET_H_

void pal_init(int width, int height, GLuint *textures, GLboolean float_packed_pixels);
void pal_pallet_changed(void);
void pal_render(GLuint srctex);

#endif /* include guard */

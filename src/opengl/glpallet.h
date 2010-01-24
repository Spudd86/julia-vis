/**
 * glpallet.h
 *
 */

#ifndef GLPALLET_H_
#define GLPALLET_H_

void pal_init(int width, int height, GLboolean float_packed_pixels, GLboolean force_fixed);
void pal_pallet_changed(void);
void pal_render(GLuint srctex);

#endif /* include guard */

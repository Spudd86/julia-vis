/**
 * gl_maxsrc.h
 *
 */

#ifndef GL_MAXSRC_H_
#define GL_MAXSRC_H_

void gl_maxsrc_init(int width, int height, GLboolean float_packed_pixels);
void gl_maxsrc_update(Uint32 now);
GLuint gl_maxsrc_get();

#endif /* include guard */

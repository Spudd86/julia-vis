/**
 * gl_maxsrc.h
 *
 */

#ifndef GL_MAXSRC_H_
#define GL_MAXSRC_H_

void gl_maxsrc_init(int width, int height, GLboolean packed_intesity, GLboolean force_fixed);
void gl_maxsrc_update(void);
GLuint gl_maxsrc_get(void);

#endif /* include guard */
/**
 * glmisc.h
 *
 */

#ifndef GLMISC_H_
#define GLMISC_H_

GLhandleARB compile_program(const char *vert_shader, const char *frag_shader);
void pixbuf_to_texture(Pixbuf *src, GLuint *tex, GLint clamp_mode, int rgb);

#endif /* include guard */

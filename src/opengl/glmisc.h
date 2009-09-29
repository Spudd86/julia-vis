/**
 * glmisc.h
 *
 */

#ifndef GLMISC_H_
#define GLMISC_H_

typedef struct {
	float x, y;
}vec2f;

typedef struct {
	float x, y, z;
}vec3f;

GLhandleARB compile_program(const char *vert_shader, const char *frag_shader);
void pixbuf_to_texture(Pixbuf *src, GLuint *tex, GLint clamp_mode, int rgb);

int scope_init(int width, int height);
void render_scope();

#endif /* include guard */

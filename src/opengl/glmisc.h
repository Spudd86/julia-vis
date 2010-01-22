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
GLhandleARB compile_program_defs(const char *defs, const char *vert_shader, const char *frag_shader);
void pixbuf_to_texture(Pixbuf *src, GLuint *tex, GLint clamp_mode, int rgb);

void draw_string(const char *str);

#define CHECK_GL_ERR do { GLint glerr = glGetError(); if(glerr != GL_NO_ERROR)\
	fprintf(stderr, "%s: In function '%s':\n%s:%d: Warning: %s\n", \
		__FILE__, __func__, __FILE__, __LINE__, gluErrorString(glerr)); } while(0)

#define FLOAT_PACK_FUNCS \
		"vec4 encode( float v ) {\n"\
		"	vec4 enc = vec4(1.0, 255.0, 65025.0, 160581375.0) * clamp(v, 0, 1);\n"\
		"	enc = fract(enc);\n"\
		"	enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);\n"\
		"	return enc;\n"\
		"}\n"\
		"float decode(vec4 rgba ) {\n"\
		"	return dot(rgba, vec4(1.0, 1/255.0, 1/65025.0, 1/160581375.0) );\n"\
		"}\n"

#endif /* include guard */

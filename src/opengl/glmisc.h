/**
 * glmisc.h
 *
 */

#ifndef GLMISC_H_
#define GLMISC_H_

void swap_buffers(void);
uint32_t get_ticks(void);
void dodelay(uint32_t ms);

void render_frame(GLboolean debug_maxsrc, GLboolean debug_pal, GLboolean show_mandel, GLboolean show_fps_hist);
void init_gl(const opt_data *opt_data, int width, int height);

typedef struct {
	float x, y;
} __attribute__((__packed__)) vec2f;

typedef struct {
	float x, y, z;
}vec3f;

typedef struct {
	float x, y, z, w;
}vec4f;

typedef struct Map_s Map;
typedef void (*map_texco_cb)(int grid_size, vec2f *restrict txco_buf, const void *cb_data);
typedef void (*map_texco_vxt_func)(float u, float v, vec2f *restrict txco, const void *cb_data);

Map *map_new(int grid_size, map_texco_cb callback);
void map_destroy(Map *self);
void map_render(Map *self, const void *cb_data);

#define GEN_MAP_CB(map_cb, vtx_func) \
		static void map_cb(int grid_size, vec2f *restrict txco_buf, const void *cb_data) {\
			const float step = 2.0f/(grid_size);\
			for(int yd=0; yd<=grid_size; yd++) {\
				vec2f *row = txco_buf + yd*(grid_size+1);\
				for(int xd=0; xd<=grid_size; xd++)\
					vtx_func(xd*step - 1.0f, yd*step - 1.0f, row + xd, cb_data);\
			}\
		}

GLhandleARB compile_program(const char *vert_shader, const char *frag_shader);
GLhandleARB compile_program_defs(const char *defs, const char *vert_shader, const char *frag_shader);
void pixbuf_to_texture(Pixbuf *src, GLuint *tex, GLint clamp_mode, int rgb);

void setup_viewport(int width, int height);
void draw_string(const char *str);

#define CHECK_GL_ERR do { GLint glerr = glGetError(); if(glerr != GL_NO_ERROR)\
	fprintf(stderr, "%s: In function '%s':\n%s:%d: Warning: %s\n", \
		__FILE__, __func__, __FILE__, __LINE__, gluErrorString(glerr)); \
		glerr = glGetError();\
	} while(0)

#define FLOAT_PACK_FUNCS \
		"vec4 encode( float v ) {\n"\
		"	vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * clamp(v, 0, 1);\n"\
		"	enc = fract(enc);\n"\
		"	enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);\n"\
		"	return enc;\n"\
		"}\n"\
		"float decode(vec4 rgba ) {\n"\
		"	return clamp(dot(rgba, vec4(1.0, 1/255.0, 1/65025.0, 1/16581375.0) ), 0, 1);\n"\
		"}\n"

#endif /* include guard */

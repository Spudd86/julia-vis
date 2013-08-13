#ifndef GLMAP_H
#define GLMAP_H

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

#endif


#ifndef GL_FIXED_H_
#define GL_FIXED_H_

typedef struct {
	int res;
	GLushort *ind;
	vec2f *verts;
	vec2f *tex;

	int have_vbos;
	union {
		struct {GLint indbo, vertbo, texbo; };
		GLuint bos[3];
	};
}MapMesh;

MapMesh *new_map_mesh(int res);
void render_map_mesh(MapMesh *m, GLuint tex);
vec2f *map_mesh_begin_mod_texco(MapMesh *m);
void map_mesh_end_mod_texco(MapMesh *m);

#endif

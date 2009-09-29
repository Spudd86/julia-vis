
#include "../common.h"
#include <stdio.h>
#include <GL/glew.h>

#include "glmisc.h"
#include "glfixed.h"

// number of quads is (n-1)^2

/**
 * @param n resolution of mesh
 */
MapMesh *new_map_mesh(int n)
{
	MapMesh *m = malloc(sizeof(MapMesh));
	m->res = n;
	m->verts = malloc(sizeof(*m->verts)*n*n);
	m->tex = malloc(sizeof(*m->tex)*n*n);

	m->ind = malloc(sizeof(*m->ind)*(n-1)*(n-1)*4);

	for(int y=0; y<(n-1); y++) {
		for(int x=0; x<(n-1); x++) {
			m->ind[(y*(n-1)+x)*4+0] = (y+1)*n+(x+0);
			m->ind[(y*(n-1)+x)*4+1] = (y+0)*n+(x+0);
			m->ind[(y*(n-1)+x)*4+2] = (y+0)*n+(x+1);
			m->ind[(y*(n-1)+x)*4+3] = (y+1)*n+(x+1);
		}
	}

	for(int y=0; y<n; y++) {
		for(int x=0; x<n; x++) {
			m->verts[y*n+x].x = (x/(n-1.0f) - 0.5f)*2;
			m->verts[y*n+x].y = (y/(n-1.0f) - 0.5f)*2;
			m->tex[y*n+x].x = x/(n-1.0f);
			m->tex[y*n+x].y = y/(n-1.0f);
		}
	}
	//TODO: see if we have VBOs, if so set them up

//	m->have_vbos = 0;
	m->have_vbos = GLEW_ARB_vertex_buffer_object;
	m->indbo = m->texbo = m->vertbo = 0;
	if(m->have_vbos) {
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
		glGenBuffersARB(3, m->bos);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, m->indbo);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, (n-1)*(n-1)*4*sizeof(*m->ind), m->ind, GL_STATIC_DRAW_ARB);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, m->vertbo);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, n*n*sizeof(*m->verts), m->verts, GL_STATIC_DRAW_ARB);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, m->texbo);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, n*n*sizeof(*m->tex), NULL, GL_STREAM_DRAW_ARB);
		glPopClientAttrib();
		glPopAttrib();

		free(m->ind); m->ind = NULL;
		free(m->verts); m->verts = NULL;
		free(m->tex); m->tex = NULL;
	}

	return m;
}


//TODO: use vertex buffer object (if available)
void render_map_mesh(MapMesh *m, GLuint tex)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glBindTexture(GL_TEXTURE_2D, tex);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	if(m->have_vbos) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, m->vertbo);
		glVertexPointer(2, GL_FLOAT, 0, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, m->texbo);
		glTexCoordPointer(2, GL_FLOAT, 0, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, m->indbo);
		glDrawElements(GL_QUADS, (m->res-1)*(m->res-1)*4, GL_UNSIGNED_SHORT, 0);
	} else { //No VBOs
		glTexCoordPointer(2, GL_FLOAT, 0, m->tex);
		glVertexPointer(2, GL_FLOAT, 0, m->verts);
		glDrawElements(GL_QUADS, (m->res-1)*(m->res-1)*4, GL_UNSIGNED_SHORT, m->ind); //TODO: fix
	}
	glPopClientAttrib();
	glPopAttrib();
}

vec2f *map_mesh_begin_mod_texco(MapMesh *m)
{
	if(m->have_vbos) {
		// TODO? To avoid waiting (idle), you can call first glBufferDataARB()
		// with NULL pointer, then call glMapBufferARB(). In this case, the
		// previous data will be discarded and glMapBufferARB() returns a new
		// allocated pointer immediately even if GPU is still working with the previous data.
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, m->texbo);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, m->res*m->res*sizeof(*m->tex), NULL, GL_STREAM_DRAW_ARB);
		void *tex = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		glPopClientAttrib();
		glPopAttrib();
		return tex;
	}
	return m->tex;
}

void map_mesh_end_mod_texco(MapMesh *m)
{
	if(m->have_vbos) {
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, m->texbo);
		glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
		glPopClientAttrib();
		glPopAttrib();
	}
}

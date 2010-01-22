/**
 * glmap.c
 *
 */

#include "common.h"

#include <SDL.h>
#include <GL/glew.h>

#include "glmisc.h"

typedef GLint quadind[4];
typedef GLfloat vtx[2];

struct Map_s {
	union {
		struct { GLhandleARB ind, vtx, txco; };
		GLhandleARB handles[3];
	};
	vec2f *txco_buf;
	vec2f *vtx_buf;
	quadind *ind_buf;
	int grid_size;

	GLboolean use_vbo;
	map_texco_cb callback;
};

//static void vtx_func(float u, float v, vec2f *txco, void *cb_data) {(void)cb_data;
//	txco->x = (u+1)/2; txco->y = (v+1)/2;
//}
//static void map_cb(int grid_size, vec2f *txco_buf, void *cb_data) {
//	const float step = 2.0f/(grid_size);
//	for(int yd=0; yd<=grid_size; yd++) {
//		vec2f *row = txco_buf + yd*(grid_size+1);
//		for(int xd=0; xd<=grid_size; xd++)
//			vtx_func(xd*step - 1.0f, yd*step - 1.0f, row + xd, cb_data);
//	}
//}

Map *map_new(int grid_size, map_texco_cb callback)
{
	Map *self = malloc(sizeof(Map)); memset(self, 0, sizeof(Map));
	self->callback = callback;
	self->grid_size = grid_size;

	quadind *ind_buf = NULL; vec2f *vtx_buf = NULL;
	self->ind_buf = ind_buf = malloc(sizeof(quadind)*self->grid_size*self->grid_size);
	self->vtx_buf = vtx_buf = malloc(sizeof(vec2f)*(self->grid_size+1)*(self->grid_size+1));

	for(int y=0; y<self->grid_size; y++) {
		quadind *row = ind_buf + y*self->grid_size;
		for(int x=0; x<self->grid_size; x++) {
			row[x][0] = y*(self->grid_size+1) + x;
			row[x][1] = y*(self->grid_size+1) + x+1;
			row[x][2] = (y+1)*(self->grid_size+1) + x+1;
			row[x][3] = (y+1)*(self->grid_size+1) + x;
		}
	}

	const float step = 2.0f/(self->grid_size);
	for(int yd=0; yd<=self->grid_size; yd++) {
		vec2f *row = vtx_buf + yd*(self->grid_size+1);
		for(int xd=0; xd<=self->grid_size; xd++)
			row[xd].x = xd*step - 1.0f, row[xd].y = yd*step - 1.0f;
	}

	if(GLEW_ARB_vertex_buffer_object) {
		self->use_vbo = GL_TRUE;
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

		glGenBuffersARB(3, self->handles);

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, self->ind);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(quadind)*self->grid_size*self->grid_size, self->ind_buf, GL_STATIC_DRAW_ARB);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, self->vtx);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(vec2f)*(self->grid_size+1)*(self->grid_size+1), self->vtx_buf, GL_STATIC_DRAW_ARB);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, self->txco);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(vec2f)*(self->grid_size+1)*(self->grid_size+1), NULL, GL_STREAM_DRAW_ARB);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glPopClientAttrib();
		glPopAttrib();

		free(self->ind_buf); self->ind_buf = NULL;
		free(self->vtx_buf); self->vtx_buf = NULL;
	} else {
		self->txco_buf = malloc(sizeof(vec2f)*(self->grid_size+1)*(self->grid_size+1));
		memcpy(self->txco_buf, self->vtx_buf, sizeof(vec2f)*(self->grid_size+1)*(self->grid_size+1));
	}

	return self;
}

void map_destroy(Map *self)
{
	if(self->use_vbo) glDeleteBuffersARB(3, self->handles);
	else {
		free(self->txco_buf);
		free(self->ind_buf); self->ind_buf = NULL;
		free(self->vtx_buf); self->vtx_buf = NULL;
	}
	free(self);
}

void map_render(Map *self, void *cb_data)
{
//	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

	if(self->use_vbo) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, self->txco);
		vec2f *ptr = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if(!ptr) return;
		self->callback(self->grid_size, ptr, cb_data);
		glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	} else
		self->callback(self->grid_size, self->txco_buf, cb_data);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	if(self->use_vbo) {
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, self->ind);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, self->vtx);
		glVertexPointer(2, GL_FLOAT, 0, NULL);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, self->txco);
		glTexCoordPointer(2, GL_FLOAT, 0, NULL);
		glDrawElements(GL_QUADS, self->grid_size*self->grid_size*4, GL_UNSIGNED_INT, NULL);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	} else {
		glTexCoordPointer(2, GL_FLOAT, 0, self->txco_buf);
		glVertexPointer(2, GL_FLOAT, 0, self->vtx_buf);
		glDrawElements(GL_QUADS, self->grid_size*self->grid_size*4, GL_UNSIGNED_INT, self->ind_buf);
	}
	glPopClientAttrib();
//	glPopAttrib();
}

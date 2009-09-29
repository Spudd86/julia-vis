/**
 * rendertotex.c
 *
 */

#include "common.h"

#include <SDL.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include "glmisc.h"

#include "rendertotex.h"

typedef enum {RENDERTEX_MODE_NONE=0, FBO_MODE, COPYSUB_MODE, COPY_MODE} RenderMode;

struct _TexRenderContext_s {
	GLuint tex[2];
	int last_tex;

	int width, height;

	RenderMode mode;

	GLuint fbo;
};

TexRenderContext *tex_render_create(int width, int height, GLboolean try_float)
{
	TexRenderContext *self = malloc(sizeof(TexRenderContext));
	memset(self, 0, sizeof(TexRenderContext));
	self->width = width, self->height = height;

	glGenTextures(2, self->tex);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, self->tex[i]);
		if(try_float && GLEW_ARB_half_float_pixel)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F_ARB,  width, height, 0, GL_RGB, GL_HALF_FLOAT_ARB, NULL);
		else if(try_float && GLEW_ARB_color_buffer_float)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB,  width, height, 0, GL_RGB, GL_FLOAT, NULL);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,  width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		static float foo[] = {0.0, 0.0, 0.0 };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, foo);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glPopAttrib();

	if(GLEW_EXT_framebuffer_object) {
		self->mode = FBO_MODE;
		glGenFramebuffersEXT(1, &self->fbo);
	} else if(glewIsSupported("GL_VERSION_1_3")) {
		self->mode = COPYSUB_MODE;
	} else {
		self->mode = COPY_MODE;
	}
}


void set_tex_clamp(TexRenderContext *self, GLint clamp_mode)
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, self->tex[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	}
	glPopAttrib();
}

/**
 * glmisc.c
 *
 */
#define GL_GLEXT_PROTOTYPES


#include "common.h"
#include "glmisc.h"

static void check_gl_obj_msg(GLhandleARB obj);

static GLboolean do_shader_compile(GLhandleARB shader, int count, const char **source) {
	GLint compiled = GL_FALSE;
	glShaderSourceARB(shader, count, source, NULL);
	glCompileShaderARB(shader);
	glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
	check_gl_obj_msg(shader);
	if (!compiled) {
		printf("Compile failed\n");
		return GL_FALSE;
	}
	return GL_TRUE;
}

static GLboolean do_shader_compile_defs(GLhandleARB shader, const char *defs, const char *source) {
	if(defs) {
		const char *sources[] = { defs, source };
		return do_shader_compile(shader, 2, sources);
	} else
		return do_shader_compile(shader, 1, &source);
}

//TODO: add stuff to allow a second set of code for functions
GLhandleARB compile_program_defs(const char *defs, const char *vert_shader, const char *frag_shader) {
	GLhandleARB vert=0, frag=0;

	if(vert_shader != NULL) {
		vert = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		do_shader_compile_defs(vert, defs, vert_shader);
	}
	if(frag_shader != NULL) {
		frag = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
		do_shader_compile_defs(frag, defs, frag_shader);
	}

	// delete so that the shaders go away as soon as they are detached from the program
	GLhandleARB prog = glCreateProgramObjectARB();
	if(vert_shader != NULL) { glAttachObjectARB(prog, vert); glDeleteObjectARB(vert); }
	if(frag_shader != NULL) { glAttachObjectARB(prog, frag); glDeleteObjectARB(frag); }
	glLinkProgramARB(prog);

	GLint linked = GL_FALSE;
	glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &linked);
	check_gl_obj_msg(prog);
	if (!linked) {
		printf("Link failed shader dump:\n\n");
		if(vert_shader) {
			printf("Vertex Shader dump:\n");
			if(defs) printf("%s", defs);
			printf("%s\n\n", vert_shader);
		}
		if(frag_shader) {
			printf("Fragment Shader dump:\n");
			if(defs) printf("%s", defs);
			printf("%s\n\n", frag_shader);
		}

		exit(1);
	}
	return prog;
}

GLhandleARB compile_program(const char *vert_shader, const char *frag_shader) {
	return compile_program_defs(NULL, vert_shader, frag_shader);
}

static void check_gl_obj_msg(GLhandleARB obj) {
	GLcharARB *pInfoLog = NULL;
	GLint     length, maxLength;
	glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &maxLength);
	pInfoLog = (GLcharARB *) malloc(maxLength * sizeof(GLcharARB));
	glGetInfoLogARB(obj, maxLength, &length, pInfoLog);
	if(length != 0) printf("%s\n", pInfoLog);
	free(pInfoLog);
}


void pixbuf_to_texture(Pixbuf *src, GLuint *tex, GLint clamp_mode, int rgb) {
	glPushAttrib(GL_TEXTURE_BIT);
	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, *tex);
	//TODO: handle different bpp
	if(rgb) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src->w, src->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, src->data);
	else glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, src->w, src->h, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  clamp_mode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  clamp_mode);
	static float foo[] = {0.0, 0.0, 0.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, foo);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib();
}

void setup_viewport(int width, int height) {
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
//	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

#include "terminusIBM.h"

void draw_string(const char *str)
{
	glPushClientAttrib( GL_CLIENT_PIXEL_STORE_BIT );
	glPixelStorei( GL_UNPACK_SWAP_BYTES,  GL_FALSE );
	glPixelStorei( GL_UNPACK_LSB_FIRST,   GL_FALSE );
	glPixelStorei( GL_UNPACK_ROW_LENGTH,  0        );
	glPixelStorei( GL_UNPACK_SKIP_ROWS,   0        );
	glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0        );
	glPixelStorei( GL_UNPACK_ALIGNMENT,   1        );

	const char *c = str;
	while(*c) {
		//TODO: fix the font
		if(*c == '\n') {
			float pos[4];
			glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
			pos[0] = 0;
			pos[1] -= 16;
			glWindowPos2fv(pos);
			c++; continue;
		}
		if(*c == '\t') {
			float pos[4];
			glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);
			pos[0] += 8*4;
			glWindowPos2fv(pos);
			c++; continue;
		}

		uint8_t tmp[16]; const uint8_t *src = terminusIBM + 16 * *c;
		for(int i = 0; i<16; i++) { //FIXME draws upsidedown on ATI's gl on windows
			tmp[i] = src[15-i];
		}

		glBitmap(8, 16, 0,0, 8, 0, tmp);
		c++;
	}
	glPopClientAttrib();
	DEBUG_CHECK_GL_ERR;
}

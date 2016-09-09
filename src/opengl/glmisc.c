/**
 * glmisc.c
 *
 */

#include "common.h"
#include "glmisc.h"

#include <assert.h>

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

void dump_shader_src(const char *defs, const char *shad)
{
	int lineno = 1;
	if(defs) {
		char *src = strdup(defs);
		const char *cur_line = strtok(src, "\n");
		int ll = 1;
		do {
			printf("%3i:%3i: %s\n", ll, lineno, cur_line);
			lineno++; ll++;
		} while((cur_line = strtok(NULL, "\n")));
		free(src);
	}
	char *src = strdup(shad);
	const char *cur_line = strtok(src, "\n");
	int ll = 1;
	do {
		printf("%3i:%3i: %s\n", ll, lineno, cur_line);
		lineno++; ll++;
	} while((cur_line = strtok(NULL, "\n")));
	free(src);
	printf("\n\n");
}

//TODO: add stuff to allow a second set of code for functions
GLhandleARB compile_program_defs(const char *defs, const char *vert_shader, const char *frag_shader)
{
	if(!ogl_ext_ARB_shading_language_100 && !(ogl_ext_ARB_fragment_shader && ogl_ext_ARB_vertex_shader && ogl_ext_ARB_shader_objects)) return 0;

	GLuint vert=0, frag=0;

	if(vert_shader != NULL) {
		vert = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		if(!do_shader_compile_defs(vert, defs, vert_shader)) {
			printf("Vertex Shader Failed compile\nSource dump:\n");
			dump_shader_src(defs, vert_shader);
			return 0;
		}
	}
	if(frag_shader != NULL) {
		frag = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
		if(!do_shader_compile_defs(frag, defs, frag_shader)) {
			printf("Fragment Shader Failed compile\nSource dump:\n");
			dump_shader_src(defs, frag_shader);
			return 0;
		}
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
			dump_shader_src(defs, vert_shader);
//			dump_shader_src(vert);
		}
		if(frag_shader) {
			printf("Fragment Shader dump:\n");
			dump_shader_src(defs, frag_shader);
//			dump_shader_src(frag);
		}
		return 0;
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

void setup_viewport(int width, int height) {
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void draw_hist_array_xlate(int off, float scl, float xlate, const int *array, int len, float r, float g, float b)
{
	float bg_lines[] = {
		1.0f, 1.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 1.0f, 1.0f, 0.0f
	};

	float pnts[len][5];
	for(int i=0; i<len; i++) {
		int idx = (i + off)%len;
		pnts[i][0] = r;
		pnts[i][1] = g;
		pnts[i][2] = b;
		pnts[i][3] = ((float)i)/(len-1);
		pnts[i][4] = scl*array[idx]+xlate;
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	
	glColorPointer( 3, GL_FLOAT, sizeof(float)*5, bg_lines);
	glVertexPointer(2, GL_FLOAT, sizeof(float)*5, bg_lines + 3);
	glDrawArrays(GL_LINES, 0, 4);

	glColorPointer( 3, GL_FLOAT, sizeof(float)*5, (float *)pnts);
	glVertexPointer(2, GL_FLOAT, sizeof(float)*5, (float *)pnts + 3);
	glDrawArrays(GL_LINE_STRIP, 0, len);

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	DEBUG_CHECK_GL_ERR;
}


void draw_hist_array_col(int off, float scl, const int *array, int len, float r, float g, float b)
{	
	draw_hist_array_xlate(off, scl, 0.0f, array, len, r, g, b);
}

void draw_hist_array(int off, float scl, const int *array, int len) {
	draw_hist_array_col(off, scl, array, len, 0.0f, 1.0f, 0.0f);
}

#include "terminusIBM.h"

#define TEXTURE_TEXT 1
#if TEXTURE_TEXT

void draw_string(const char *str)
{
	static GLuint txt_texture = 0;
	
	if(!txt_texture) {
		uint8_t *data = calloc(sizeof(*data),128*256);
		// unpack font into data
		for(int i=0; i<256; i++) {
			int xoff = (i%16)*8, yoff = (i/16)*16;
			const uint8_t * restrict src = terminusIBM + 16 * i;
			for(int y=0; y < 16; y++) {
				uint8_t *dst = data + (yoff+y)*128 + xoff;
				uint8_t line = *src++;
				for(int o=0; o < 8; o++) {
					if(line & (1<<(7-o))) dst[o] = UINT8_MAX;
					else dst[o] = 0;
				}
			}
		}
		glGenTextures(1, &txt_texture);
		glBindTexture(GL_TEXTURE_2D, txt_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 128, 256, 0, GL_ALPHA, GL_UNSIGNED_BYTE, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
		CHECK_GL_ERR;
		free(data);
	}
	
	int vpw[4]; float pos[4];
	glGetIntegerv(GL_VIEWPORT, vpw);
	glGetFloatv(GL_CURRENT_RASTER_POSITION, pos);

	struct draw_string_quad_ {
		float v[4*4];
	} verts[strlen(str)];
	size_t nstrips = 0;
	for(const char *c = str; *c; c++) {
		if(*c == '\n') {
			pos[0] = 0;
			pos[1] -= 16;
			continue;
		}
		if(*c == '\t') {
			pos[0] += 8*4;
			continue;
		}
		if(*c == ' ') {
			pos[0] += 8;
			continue;
		}
	
		int tx = (*c%16)*8, ty = (*c/16)*16;
		struct draw_string_quad_ tmp = {  {
			(tx+0)/128.0f, (ty+ 0)/256.0f, 2*(pos[0]+0)/vpw[2]-1, 2*(pos[1]+16)/vpw[3]-1,
			(tx+8)/128.0f, (ty+ 0)/256.0f, 2*(pos[0]+8)/vpw[2]-1, 2*(pos[1]+16)/vpw[3]-1,
			(tx+0)/128.0f, (ty+16)/256.0f, 2*(pos[0]+0)/vpw[2]-1, 2*(pos[1]- 0)/vpw[3]-1,
			(tx+8)/128.0f, (ty+16)/256.0f, 2*(pos[0]+8)/vpw[2]-1, 2*(pos[1]- 0)/vpw[3]-1,
			}
		};
		verts[nstrips] = tmp;
		nstrips++;
		
		pos[0] += 8;
	}

	// Now build the index data
	int ndegenerate = 2 * (nstrips - 1);
	int verts_per_strip = 4;
	int total_verts = verts_per_strip*nstrips + ndegenerate;
	uint16_t idx_buf[total_verts];
 
	for(size_t i = 0, offset = 0; i < nstrips; i++) {
		if(i > 0) { // Degenerate begin: repeat first vertex
			idx_buf[offset++] = i*4;
		}
		for(size_t j = 0; j < 4; j++) { // One part of the strip
			idx_buf[offset++] = i*4 + j;
		}
		if(i < nstrips-1) { // Degenerate end: repeat last vertex
			idx_buf[offset++] = i*4 + 3;
		}

		assert(offset <= total_verts);
	}

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.5f);
	glBindTexture(GL_TEXTURE_2D, txt_texture);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(float)*4, (float *)verts);
	glVertexPointer(2, GL_FLOAT, sizeof(float)*4, (float *)verts + 2);
	glDrawRangeElements(GL_TRIANGLE_STRIP, 0, nstrips*verts_per_strip, total_verts, GL_UNSIGNED_SHORT, idx_buf); // core since GL 1.2
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_ALPHA_TEST);
	
	glWindowPos2fv(pos); // core since GL 1.5
	DEBUG_CHECK_GL_ERR;
}

#else

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
#endif


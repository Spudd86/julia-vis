#include "common.h"
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

#include <GLES3/gl3.h>

#include "gles3misc.h"

#include "terminusIBM.h"

#if 1
static const char *vtx_shader_src =
	"layout(location = 0) in vec4 vertex;\n"
	"out vec2 uv;"
	"void main() {\n"
	"	uv = vertex.xy;\n"
	"	gl_Position = vec4(vertex.zw, 0. , 1. );\n"
	"}";
#else
static const char *vtx_shader_src =
	"uniform highp ivec2 isize;\n"
	// "uniform lowp ivec2 csize;\n"
	"uniform sampler2D src;\n"
	"layout(location = 0) in mediump ivec2 pos;\n"
	"layout(location = 1) in lowp uint char;\n"
	"out vec2 uv;\n"
	// int tx = (*c%16)*8, ty = (*c/16)*16;
	// struct draw_string_quad_ tmp = {  {
	// 	(tx+0)/128.0f, (ty+16)/256.0f, 2*(pos[0]+0)/vpw[2]-1, -(2*(pos[1]+16)/vpw[3]-1),
	// 	(tx+8)/128.0f, (ty+16)/256.0f, 2*(pos[0]+8)/vpw[2]-1, -(2*(pos[1]+16)/vpw[3]-1),
	// 	(tx+0)/128.0f, (ty+ 0)/256.0f, 2*(pos[0]+0)/vpw[2]-1, -(2*(pos[1]- 0)/vpw[3]-1),
	// 	(tx+8)/128.0f, (ty+ 0)/256.0f, 2*(pos[0]+8)/vpw[2]-1, -(2*(pos[1]- 0)/vpw[3]-1),
	// 	}
	// };
	"const lowp ivec2 to_lut[4] = ivec2[4](ivec2(0, 1), ivec2(1, 1), ivec2(0, 0), ivec2(1, 0));"
	"void main() {\n"
	"	lowp ivec2 csize = textureSize(src, 0).xy/16;\n"
	"	lowp ivec2 to = to_lut[gl_VertexID] * csize;\n"
	"	ivec2 t = ivec2((char % csize.y)*csize.x, (char/csize.y)*csize.y);\n"
	"	uv = (t + to)/(csize * 16.0);\n" // glyphs are in 16x16 grid
	"	vec2 v = (2.0*(pos+to))/isize;\n"
	"	gl_Position = vec4(v, 0. , 1. );\n"
	"}";
#endif
static const char *frag_shader_src =
	"uniform sampler2D src;\n"
	"in vec2 uv;\n"
	"out vec4 fragColour;\n"
	"void main() {\n"
	"   fragColour = vec4(1.0);\n"
	"   if(texture(src, uv).r < 0.25) discard;\n"
	"}\n";

// FIXME: need to track position
void gl_draw_string(const char *str)
{
	static GLuint txt_texture = 0;
	static GLuint shader_program = 0;
	static GLuint vbo = 0;

	if(!vbo)
	{
		glGenBuffers(1, &vbo);
	}

	if(!shader_program)
	{
		shader_program = compile_program_defs(NULL, vtx_shader_src, frag_shader_src);
		if(!shader_program) exit(1);

		glUseProgram(shader_program);
		glUniform1i(glGetUniformLocation(shader_program, "src"), 0);
		glUseProgram(0);

		printf("loaded text shader\n");
	}

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
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 128, 256, 0, GL_RED, GL_UNSIGNED_BYTE, data);CHECK_GL_ERR;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
		CHECK_GL_ERR;
		free(data);
	}

	int vpw[4];
	glGetIntegerv(GL_VIEWPORT, vpw);
	// glGetIntegerv(GL_MAX_VIEWPORT_DIMS, vpw + 2);
	// glGetFloatv(GL_CURRENT_RASTER_POSITION, pos); // TODO: need to replace this
	// printf("%4d %4d %4d %4d\n", vpw[0], vpw[1], vpw[2], vpw[3]);

	// float pos[2] = {-vpw[3]/2.0f, -vpw[4]/2.0f};
	float pos[2] = {0.0f, 0.0f};

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
			(tx+0)/128.0f, (ty+16)/256.0f, 2*(pos[0]+0)/vpw[2]-1, -(2*(pos[1]+16)/vpw[3]-1),
			(tx+8)/128.0f, (ty+16)/256.0f, 2*(pos[0]+8)/vpw[2]-1, -(2*(pos[1]+16)/vpw[3]-1),
			(tx+0)/128.0f, (ty+ 0)/256.0f, 2*(pos[0]+0)/vpw[2]-1, -(2*(pos[1]- 0)/vpw[3]-1),
			(tx+8)/128.0f, (ty+ 0)/256.0f, 2*(pos[0]+8)/vpw[2]-1, -(2*(pos[1]- 0)/vpw[3]-1),
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

		// assert(offset <= total_verts);
	}

	glUseProgram(shader_program); CHECK_GL_ERR;
	glBindTexture(GL_TEXTURE_2D, txt_texture);  CHECK_GL_ERR;
	glEnableVertexAttribArray(0); CHECK_GL_ERR;
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0); CHECK_GL_ERR;
	// TODO: change to instanced draw and just upload characters and locations (character grid)
	glDrawRangeElements(GL_TRIANGLE_STRIP, 0, nstrips*verts_per_strip, total_verts, GL_UNSIGNED_SHORT, idx_buf); CHECK_GL_ERR;
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisableVertexAttribArray(0);CHECK_GL_ERR;
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);CHECK_GL_ERR;
	// glWindowPos2fv(pos); // core since GL 1.5
	// CHECK_GL_ERR;
}

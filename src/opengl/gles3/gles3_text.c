#include "common.h"
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>

#include <GLES3/gl3.h>
#include "terminusIBM.h"

#define CHECK_GL_ERR do { GLint glerr = glGetError(); while(glerr != GL_NO_ERROR) {\
	fprintf(stderr, "%s: In function '%s':\n%s:%d: Warning: %s\n", \
		__FILE__, __func__, __FILE__, __LINE__, gl_error_string(glerr)); \
		glerr = glGetError();\
		}\
	} while(0)

static char const* gl_error_string(GLenum const err)
{
  switch (err)
  {
	// opengl 2 errors (8)
	case GL_NO_ERROR:
	  return "GL_NO_ERROR";

	case GL_INVALID_ENUM:
	  return "GL_INVALID_ENUM";

	case GL_INVALID_VALUE:
	  return "GL_INVALID_VALUE";

	case GL_INVALID_OPERATION:
	  return "GL_INVALID_OPERATION";

	case GL_OUT_OF_MEMORY:
	  return "GL_OUT_OF_MEMORY";

	// opengl 3 errors (1)
	case GL_INVALID_FRAMEBUFFER_OPERATION:
	  return "GL_INVALID_FRAMEBUFFER_OPERATION";

	// gles 2, 3 and gl 4 error are handled by the switch above
	default:
	//   assert(!"unknown error");
	  return NULL;
  }
}

static const char *vtx_shader_src =
	"#version 300 es\n"
	"#ifdef GL_ES\n"
	"precision highp float;\n"
	"#endif\n"
	"layout(location = 0) in vec4 vertex;\n"
	// "layout(location = 0) in vec2 pos;"
	// "layout(location = 1) in vec2 tex;"
	"out vec2 uv;"
	"void main() {\n"
	// "	uv = tex;\n"
	// "	gl_Position = vec4(pos, 0.0 , 1.0 );\n"
	"	uv = vertex.xy;\n"
	"	gl_Position = vec4(vertex.zw, 0. , 1. );\n"
	"}";
static const char *frag_shader_src =
	"#version 300 es\n"
	"#ifdef GL_ES\n"
	"precision highp float;\n"
	"#endif\n"

	// "varying vec2 uv;\n"
	"uniform sampler2D src;\n"
	"in vec2 uv;\n"
	"out vec4 fragColour;\n"
	"void main() {\n"
	// "	fragColour = texture(src, uv);\n"
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
		shader_program = glCreateProgram();
		GLuint vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
		GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

		GLchar *info_log = NULL;
		GLint  log_length;
		GLint compiled = GL_FALSE;

		glShaderSource(vertex_shader, 1, (const GLchar**)&vtx_shader_src, NULL);
		glShaderSource(fragment_shader, 1, (const GLchar**)&frag_shader_src, NULL);

		glCompileShader(vertex_shader);
		glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
		
		glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_length);
		if(log_length != 0) {
			info_log = (GLchar *) malloc(log_length * sizeof(GLchar));
			glGetShaderInfoLog(vertex_shader, log_length, &log_length, info_log);
			printf("%s\n", info_log);
			free(info_log);
			info_log = NULL;
		}

		if(!compiled) {
			printf("vertex shader compile failed");
			exit(1);
		}

		glCompileShader(fragment_shader);
		glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compiled);
		
		glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_length);
		if(log_length != 0) {
			info_log = (GLchar *) malloc(log_length * sizeof(GLchar));
			glGetShaderInfoLog(fragment_shader, log_length, &log_length, info_log);
			printf("%s\n", info_log);
			free(info_log);
			info_log = NULL;
		}

		if(!compiled) {
			printf("fragment shader compile failed");
			exit(1);
		}
		
		// Program keeps shaders alive after they are attached
		glAttachShader(shader_program, vertex_shader); glDeleteShader(vertex_shader);
		glAttachShader(shader_program, fragment_shader); glDeleteShader(fragment_shader);
		glLinkProgram(shader_program);
		GLint linked = GL_FALSE;
		glGetProgramiv(shader_program, GL_LINK_STATUS, &linked);

		glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);
		if(log_length != 0) {
			info_log = (GLchar *) malloc(log_length * sizeof(GLchar));
			glGetProgramInfoLog(shader_program, log_length, &log_length, info_log);
			printf("%s\n", info_log);
			free(info_log);
			info_log = NULL;
		}

		if (!linked) {
			printf("Failed Shader compile:\nvertex:\n%s\nfragment:\n%s\n", vtx_shader_src, frag_shader_src);
			exit(1);
		}

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
		// struct draw_string_quad_ tmp = {  {
		// 	(tx+0)/128.0f, (ty+ 0)/256.0f, -1,  0,
		// 	(tx+8)/128.0f, (ty+ 0)/256.0f,  0,  0,
		// 	(tx+0)/128.0f, (ty+16)/256.0f, -1, -1,
		// 	(tx+8)/128.0f, (ty+16)/256.0f,  0, -1,
		// 	}
		// };
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
#if 0
	static const float verts2[] = {
		0, 0 , -1, -1,
		1, 0 ,  1, -1,
		0, 1 , -1,  1,
		1, 1 ,  1,  1
	};
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts2), verts2, GL_STREAM_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0);CHECK_GL_ERR;
	// glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, verts2);CHECK_GL_ERR;
	// glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, 2);CHECK_GL_ERR; // vertex co-ords
	// glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0);CHECK_GL_ERR; // Texture co-ords
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);CHECK_GL_ERR;
	glBindBuffer(GL_ARRAY_BUFFER, 0);
#else
	// glEnableVertexAttribArray(1); CHECK_GL_ERR;
	// glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (float *)verts + 2);CHECK_GL_ERR; // vertex co-ords
	// glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (float *)verts + 0);CHECK_GL_ERR; // Texture co-ords
	// glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, (float *)verts); CHECK_GL_ERR;
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, 0); CHECK_GL_ERR;
	glDrawRangeElements(GL_TRIANGLE_STRIP, 0, nstrips*verts_per_strip, total_verts, GL_UNSIGNED_SHORT, idx_buf); CHECK_GL_ERR;
	// glDrawElements(GL_TRIANGLE_STRIP, total_verts, GL_UNSIGNED_SHORT, idx_buf); CHECK_GL_ERR;
	// glDisableVertexAttribArray(1);CHECK_GL_ERR;
	glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif
	glDisableVertexAttribArray(0);CHECK_GL_ERR;
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);CHECK_GL_ERR;
	// glWindowPos2fv(pos); // core since GL 1.5
	// CHECK_GL_ERR;
}
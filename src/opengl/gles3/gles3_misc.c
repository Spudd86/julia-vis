#include "common.h"
#include <stdio.h>
#include <string.h>

#include <GLES3/gl3.h>
#include "gles3misc.h"

static void check_shader_msg(GLint shader)
{
	GLint log_length = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length); CHECK_GL_ERR;
	if(log_length != 0)
	{
		GLchar *info_log = malloc(log_length * sizeof(GLchar));
		glGetShaderInfoLog(shader, log_length, &log_length, info_log); CHECK_GL_ERR;
		printf("%s\n", info_log);
		free(info_log);
	}
}

static GLboolean do_shader_compile(GLint shader, int count, const char **source)
{
	CHECK_GL_ERR;
	GLint compiled = GL_FALSE;
	glShaderSource(shader, count, source, NULL); CHECK_GL_ERR;
	glCompileShader(shader); CHECK_GL_ERR;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled); CHECK_GL_ERR;
	check_shader_msg(shader);
	if (!compiled) {
		printf("Compile failed\n");
		return GL_FALSE;
	}
	return GL_TRUE;
}

// VA ARGS?
static GLboolean do_shader_compile_defs(GLint shader, const char *defs, const char *source) {
	if(defs) {
		const char *sources[] = { "#version 300 es\n", defs, source, NULL };
		return do_shader_compile(shader, 3, sources);
	} else {
		const char *sources[] = { "#version 300 es\n#ifdef GL_ES\nprecision highp float;\nprecision highp int;\n#endif\n", source, NULL };
		return do_shader_compile(shader, 2, sources);
	}
}

static GLboolean do_shader_compile_defs_list(GLint shader, const char *defs, const char **source)
{
	int count = 1;
	while(source[count]) count++;

	if(defs) count++;

	const char *srcs[count];

	int i = 0;
	if(defs) {
		srcs[i++] = "#version 300 es\n";
		srcs[i++] = defs;
	} else {
		srcs[i++] = "#version 300 es\n#ifdef GL_ES\nprecision highp float;\nprecision highp int;\n#endif\n";
	}

	//TODO:  add helper for filtered sampling from a texture when the texture sampler won't filter for us

	for(int j=0; i < count; i++, j++) srcs[i] = source[j];
	GLboolean r = do_shader_compile(shader, count, srcs);
	if(!r)
	{
		printf("Shader Failed compile\nSource dump:\n");
		int lineno = 1;
		for(int src_blk=0; srcs[src_blk]; src_blk++) {
			char *src = strdup(srcs[src_blk]);
			const char *cur_line = strtok(src, "\n");
			int ll = 1;
			do {
				printf("%3i:%3i: %s\n", ll, lineno, cur_line);
				lineno++; ll++;
			} while((cur_line = strtok(NULL, "\n")));
			free(src);
		}
		printf("\n\n");
	}
	return r;
}

#if 0
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
	// for(int src_blk=0; shad[src_blk]; src_blk++) {
		char *src = strdup(shad);
		const char *cur_line = strtok(src, "\n");
		int ll = 1;
		do {
			printf("%3i:%3i: %s\n", ll, lineno, cur_line);
			lineno++; ll++;
		} while((cur_line = strtok(NULL, "\n")));
		free(src);
	// }
	printf("\n\n");
}
#else
void dump_shader_src(GLint shader)
{
	GLint length = 0;
	glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &length);

	char *source = calloc(sizeof(*source), length+1);
	glGetShaderSource(shader, length+1, &length, source);

	const char *cur_line = strtok(source, "\n");

	int lineno = 1;
	do {
		printf("%3i: %s\n", lineno, cur_line);
		lineno++;
	} while((cur_line = strtok(NULL, "\n")));
	free(source);
}
#endif

GLuint compile_program_lists(const char *defs, const char **vert_shader, const char **frag_shader)
{
	CHECK_GL_ERR;
	GLuint vert=0, frag=0;

	if(vert_shader != NULL) {
		vert = glCreateShader(GL_VERTEX_SHADER);
		if(!do_shader_compile_defs_list(vert, defs, vert_shader)) {
			// printf("Vertex Shader Failed compile\nSource dump:\n");
			// dump_shader_src(defs, vert_shader);
			dump_shader_src(vert);
			return 0;
		}
	}
	if(frag_shader != NULL) {
		frag =glCreateShader(GL_FRAGMENT_SHADER);
		if(!do_shader_compile_defs_list(frag, defs, frag_shader)) {
			// printf("Fragment Shader Failed compile\nSource dump:\n");
			// dump_shader_src(defs, frag_shader);
			dump_shader_src(frag);
			return 0;
		}
	}

	// delete so that the shaders go away as soon as they are detached from the program
	GLint prog = glCreateProgram(); CHECK_GL_ERR;
	if(vert_shader != NULL) { glAttachShader(prog, vert); glDeleteShader(vert); CHECK_GL_ERR; }
	if(frag_shader != NULL) { glAttachShader(prog, frag); glDeleteShader(frag); CHECK_GL_ERR; }
	glLinkProgram(prog);CHECK_GL_ERR;

	GLint log_length = 0;
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_length); CHECK_GL_ERR;
	if(log_length != 0)
	{
		GLchar *info_log = malloc(log_length * sizeof(GLchar));
		glGetProgramInfoLog(prog, log_length, &log_length, info_log); CHECK_GL_ERR;
		printf("%s\n", info_log);
		free(info_log);
	}

	GLint linked = GL_FALSE;
	glGetProgramiv(prog, GL_LINK_STATUS, &linked);
	if (!linked) {
		printf("Link failed shader dump:\n\n");
		if(vert_shader) {
			printf("Vertex Shader dump:\n");
			// dump_shader_src(defs, vert_shader);
			dump_shader_src(vert);
		}
		if(frag_shader) {
			printf("Fragment Shader dump:\n");
			// dump_shader_src(defs, frag_shader);
			dump_shader_src(frag);
		}
		return 0;
	}
	CHECK_GL_ERR;

	return prog;
}

GLuint compile_program_defs(const char *defs, const char *vert_shader, const char *frag_shader)
{
	// const char *vsources[] = { vert_shader, NULL };
	// const char *fsources[] = { frag_shader, NULL};
	// return compile_program_lists(defs, vsources, fsources);
	CHECK_GL_ERR;
	GLuint vert=0, frag=0;

	if(vert_shader != NULL) {
		vert = glCreateShader(GL_VERTEX_SHADER);
		if(!do_shader_compile_defs(vert, defs, vert_shader)) {
			printf("Vertex Shader Failed compile\nSource dump:\n");
			dump_shader_src(vert);
			// dump_shader_src("#version 300 es\n#ifdef GL_ES\nprecision highp float;\nprecision highp int;\n#endif\n\n", vert_shader);
			exit(1);
		}
	}
	if(frag_shader != NULL) {
		frag =glCreateShader(GL_FRAGMENT_SHADER);
		if(!do_shader_compile_defs(frag, defs, frag_shader)) {
			printf("Fragment Shader Failed compile\nSource dump:\n");
			dump_shader_src(frag);
			// dump_shader_src("#version 300 es\n#ifdef GL_ES\nprecision highp float;\nprecision highp int;\n#endif\n\n", frag_shader);
			exit(1);
		}
	}

	// delete so that the shaders go away as soon as they are detached from the program
	GLint prog = glCreateProgram(); CHECK_GL_ERR;
	if(vert_shader != NULL) { glAttachShader(prog, vert); glDeleteShader(vert); CHECK_GL_ERR; }
	if(frag_shader != NULL) { glAttachShader(prog, frag); glDeleteShader(frag); CHECK_GL_ERR; }
	glLinkProgram(prog);CHECK_GL_ERR;

	GLint log_length = 0;
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_length); CHECK_GL_ERR;
	if(log_length != 0)
	{
		GLchar *info_log = malloc(log_length * sizeof(GLchar));
		glGetProgramInfoLog(prog, log_length, &log_length, info_log); CHECK_GL_ERR;
		printf("%s\n", info_log);
		free(info_log);
	}

	GLint linked = GL_FALSE;
	glGetProgramiv(prog, GL_LINK_STATUS, &linked);
	if (!linked) {
		printf("Link failed shader dump:\n\n");
		if(vert_shader) {
			printf("Vertex Shader dump:\n");
			// dump_shader_src(defs, vert_shader);
			dump_shader_src(vert);
		}
		if(frag_shader) {
			printf("Fragment Shader dump:\n");
			// dump_shader_src(defs, frag_shader);
			dump_shader_src(frag);
		}
		return 0;
	}
	CHECK_GL_ERR;

	return prog;
}


char const* gl_error_string(GLenum const err)
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
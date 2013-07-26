/**
 * glpallet.c
 *
 */

/* TODO:
 *  - rewrite pallet handling so that in GLSL mode we can just let it look
 *     after switching/position and skip the mixing work and active pallet 
 */

#include "common.h"
#include "glmisc.h"
#include "pallet.h"
#include "glpallet.h"

static const uint32_t *active_pal = NULL;
static GLuint pal_tex = 0;
static GLhandleARB pal_prog = 0;

static const char *vtx_shader =
	"#version 110\n"
	"void main() {\n"
	"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"	gl_Position = gl_Vertex;\n"
	"}";

static const char *pal_frag_mix =
	"#version 110\n"
	FLOAT_PACK_FUNCS
	"uniform sampler2D src;\n"
	"uniform sampler1D pal;\n"
	"void main() {\n"
	"	gl_FragColor = texture1D(pal, decode(texture2D(src, gl_TexCoord[0].xy)));\n"
	//"	gl_FragColor = texture1D(pal, texture2D(src, gl_TexCoord[0].xy).x);\n"
	"}";

static const char *pal_frag_shader =
	"#version 110\n"
	"uniform sampler2D src;\n"
	"uniform sampler1D pal;\n"
	"void main() {\n"
	"	gl_FragColor = texture1D(pal, texture2D(src, gl_TexCoord[0].xy).x);\n"
	"}";

static void draw_palleted_glsl(GLuint draw_tex)
{DEBUG_CHECK_GL_ERR;
	static const float verts[] = {
		0, 0 , -1, -1,
		1, 0 ,  1, -1,
		0, 1 , -1,  1,
		1, 1 ,  1,  1
	};

	glPushAttrib(GL_TEXTURE_BIT);
		glUseProgramObjectARB(pal_prog);
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_2D, draw_tex);
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_1D, pal_tex);
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, sizeof(float)*4, verts);
		glVertexPointer(2, GL_FLOAT, sizeof(float)*4, verts + 2);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		
		glUseProgramObjectARB(0);
	glPopAttrib();
	DEBUG_CHECK_GL_ERR;
}

static bool pal_init_glsl(GLboolean float_packed_pixels)
{CHECK_GL_ERR;
	printf("Compiling pallet shader:\n");
	if(!float_packed_pixels)
		pal_prog = compile_program(vtx_shader, pal_frag_shader);
	else
		pal_prog = compile_program(vtx_shader, pal_frag_mix);

	if(!pal_prog) return false;

	glUseProgramObjectARB(pal_prog);
	glUniform1iARB(glGetUniformLocationARB(pal_prog, "src"), 0);
	glUniform1iARB(glGetUniformLocationARB(pal_prog, "pal"), 1);
	glUseProgramObjectARB(0);
	printf("Pallet shader compiled\n");

	glPushAttrib(GL_TEXTURE_BIT);
	glGenTextures(1, &pal_tex);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, active_pal);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib(); CHECK_GL_ERR;
	return true;
}

static GLboolean use_glsl = GL_FALSE;
void pal_init(int width, int height, GLboolean packed_intesity_pixels, GLboolean force_fixed) {	CHECK_GL_ERR;
	pallet_init(0);
	active_pal = get_active_pal();
	use_glsl = GLEE_ARB_shading_language_100 && !force_fixed;
	if(!(use_glsl && pal_init_glsl(packed_intesity_pixels))) {
		use_glsl = false;
		pal_init_fixed(width, height);
	}
	CHECK_GL_ERR;
}

void pal_render(GLuint srctex) { DEBUG_CHECK_GL_ERR;
	if(use_glsl) draw_palleted_glsl(srctex);
	else pal_render_fixed(srctex);
	DEBUG_CHECK_GL_ERR;
}

void pal_pallet_changed(void) {
	if(use_glsl) { // only GLSL version uses a texture for now
		DEBUG_CHECK_GL_ERR;
		glPushAttrib(GL_TEXTURE_BIT);
		glBindTexture(GL_TEXTURE_1D, pal_tex);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, active_pal);
		glPopAttrib();
		DEBUG_CHECK_GL_ERR;
	}
}


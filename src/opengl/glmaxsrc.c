/**
 * gl_maxsrc.c
 *
 */

#include "common.h"
#include "audio/audio.h"
#include "glmisc.h"
#include "glmaxsrc.h"
#include "glscope.h"
#include "getsamp.h"

//TODO: use 2 basis vectors to find p instead of matrix op
// ie, uniform vec3 a,b; 
//     where a = <1,0,0>*R, b = <0, 1, 0>*R
// then vec3 p = (uv.x*a + uv.y*b)*vec3(1,d,d);
// and final co-ords are still R*p, except we can make R a mat3x2 and maybe save some MAD's

static const char *vtx_shader =
	"varying vec2 uv;\n"
	"void main() {\n"
	"	uv = gl_MultiTexCoord0.st;\n"
	"	gl_Position = gl_Vertex;\n"
	"}";

static const char *frag_src =
	"varying vec2 uv;\n"
	"uniform sampler2D prev;\n"
	"uniform mat3 R;\n"
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#endif\n"
	"void main() {\n"
	"	vec2 p;\n"
	"	{\n"
	"		vec3 t = vec3(0.5f);\n"
	"		t.yz = vec2(0.95f*0.5f + (0.05f*0.5f)*length(uv));\n"
	"		t = (uv.x*R[0] + uv.y*R[1])*t;\n"
	"		p = (t*R).xy + 0.5f;\n"
	"	}\n"
	"#ifdef FLOAT_PACK_PIX\n" //TODO: use this formula whenver we have extra prescision in the FBO
	"	gl_FragColor = encode(decode(texture2D(prev, p))*0.978f);\n"
	"#else\n"
//	"	gl_FragColor.r = texture2D(prev, p).r*0.978f;\n"
	"	gl_FragColor = texture2D(prev, p)*0.975f;\n" //TODO: use 0.978 if we have GL_R16 textures...
//	"	vec4 c = texture2D(prev, p);\n"
//	"	gl_FragColor = vec4(c.x - max(2/256.0f, c.x*(1.0f/100)));\n"
//	"	gl_FragColor = (c - max(vec4(2/256.0f), c*0.01f));\n"
	"#endif\n"
	"}\n";

static void bg_vtx(float u, float v, vec2f *restrict txco, const void *cb_data) {
	const float *R = cb_data;
	float d = 0.95f + 0.05f*sqrtf(u*u + v*v);
	float p[] = { // first rotate our frame of reference, then do a zoom along 2 of the 3 axis
		(u*R[0*3+0] + v*R[0*3+1]),
		(u*R[1*3+0] + v*R[1*3+1])*d,
		(u*R[2*3+0] + v*R[2*3+1])*d
	};
	txco->x = (p[0]*R[0*3+0] + p[1]*R[1*3+0] + p[2]*R[2*3+0]+1.0f)*0.5f;
	txco->y = (p[0]*R[0*3+1] + p[1]*R[1*3+1] + p[2]*R[2*3+1]+1.0f)*0.5f;
}

static float tx=0, ty=0, tz=0;
static GLhandleARB shader_prog = 0;
static GLint shad_R_loc = -1;
static int iw, ih;
static GLboolean use_glsl = GL_FALSE;
static Map *fixed_map = NULL;
static struct glscope_ctx *glscope = NULL;
static struct oscr_ctx *offscr = NULL;

GEN_MAP_CB(fixed_map_cb, bg_vtx);

void gl_maxsrc_init(int width, int height, GLboolean packed_intesity_pixels, GLboolean force_fixed)
{CHECK_GL_ERR;
	iw=width, ih=height;
	
	int samp = (int)fminf(fminf(width/2, width/2), 128);
	printf("maxsrc using %i points\n", samp);	
	glscope = gl_scope_init(width, height, samp, force_fixed);
	offscr = offscr_new(width, height, force_fixed, !packed_intesity_pixels);

	if(!force_fixed) {
		printf("Compiling maxsrc shader:\n");
		const char *defs = packed_intesity_pixels?"#version 110\n#define FLOAT_PACK_PIX\n":"#version 110\n";
		shader_prog = compile_program_defs(defs, vtx_shader, frag_src);
		if(shader_prog) { // compile succeed
			printf("maxsrc shader compiled\n");
			glUseProgram(shader_prog);
			glUniform1i(glGetUniformLocation(shader_prog, "prev"), 0);
			shad_R_loc = glGetUniformLocation(shader_prog, "R");
			glUseProgram(0);
			use_glsl = GL_TRUE;
		}
	}
	if(!use_glsl)
		fixed_map = map_new(24, fixed_map_cb);

	CHECK_GL_ERR;
}

// render the old frame and distort it with GL shading language
static void render_bg_glsl(float R[3][3], GLint tex)
{
	const float Rt[9] =
	{
	 R[0][0], R[1][0], R[2][0],
	 R[0][1], R[1][1], R[2][1],
	 R[0][2], R[1][2], R[2][2],
	};
	glUseProgram(shader_prog);
	glUniformMatrix3fv(shad_R_loc, 1, 0, Rt);
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d(-1,-1); glVertex2d(-1, -1);
		glTexCoord2d( 1,-1); glVertex2d( 1, -1);
		glTexCoord2d(-1, 1); glVertex2d(-1,  1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
	glEnd();
	glUseProgram(0);
	DEBUG_CHECK_GL_ERR;
}

static void render_bg_fixed(float R[3][3], GLint tex)
{DEBUG_CHECK_GL_ERR;
	//TODO: try to come up with something that will run faster
	glClearColor(1.0f/256, 1.0f/256,1.0f/256, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0,0,0,1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
	glBlendEquationEXT(GL_FUNC_SUBTRACT_EXT);
	glBlendColor(0, 0, 0, 63.0f/64);
	DEBUG_CHECK_GL_ERR;

	glBindTexture(GL_TEXTURE_2D, tex); DEBUG_CHECK_GL_ERR;
	map_render(fixed_map, R);
}

void gl_maxsrc_update(const float *audio, int audiolen)
{DEBUG_CHECK_GL_ERR;
	static uint32_t lastupdate = 0;
	const uint32_t now = get_ticks();
	const float dt = (now - lastupdate)*24/1000.0f;
	lastupdate = now;

	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	float R[3][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};
	
	GLint src_tex = offscr_start_render(offscr);
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	if(use_glsl) render_bg_glsl(R, src_tex);
	else render_bg_fixed(R, src_tex);
	DEBUG_CHECK_GL_ERR;

	render_scope(glscope, R, audio, audiolen);
	DEBUG_CHECK_GL_ERR;

	glPopAttrib();
	offscr_finish_render(offscr);
	
	tx+=0.02*dt; ty+=0.01*dt; tz-=0.003*dt;

	DEBUG_CHECK_GL_ERR;
}

GLuint gl_maxsrc_get(void) {
	return offscr_get_src_tex(offscr);
}


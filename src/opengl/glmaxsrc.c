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
	"void main() {\n"
	"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"	gl_Position = gl_Vertex;\n"
	"}";

static const char *frag_src =
	"uniform sampler2D prev;\n"
	"uniform mat2x3 R;\n"
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#else\n"
	"#define encode(X) vec4(X)\n#define decode(X) (X)\n"
	"#endif\n"
	"void main() {\n"
	"	vec3 p;\n"
	"	{\n"
	"		vec2 uv = gl_TexCoord[0].st;\n"
	"		vec3 t = vec3(0.5f);\n"
	"		t.yz = vec2(0.95f*0.5f + (0.05f*0.5f)*length(uv));\n"
	"		p = (uv.x*R[0] + uv.y*R[1])*t;\n"
	"	}\n"
	"#ifdef FLOAT_PACK_PIX\n" //TODO: use this formula whenver we have extra prescision in the FBO
	"	gl_FragColor = encode(decode(texture2D(prev, p*R + 0.5f))*0.978f);\n"
	"#else\n"
	//"	gl_FragColor.r = texture2D(prev, p*R + 0.5f).r*0.978f;\n"
	"	vec4 c = texture2D(prev, p*R + 0.5f);\n"
//	"	gl_FragColor = vec4(c.x - max(2/256.0f, c.x*(1.0f/100)));\n"
	"	gl_FragColor = texture2D(prev, p*R + 0.5f)*0.975f;\n"
//	"	const vec4 c = texture2D(prev, p*R + 0.5f);\n"
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
static GLuint max_fbos[] = {0, 0}, fbo_tex[2] = { 0, 0 };
static int iw, ih;
static GLboolean use_glsl = GL_FALSE;
static Map *fixed_map = NULL;
GEN_MAP_CB(fixed_map_cb, bg_vtx);

void gl_maxsrc_init(int width, int height, GLboolean packed_intesity_pixels, GLboolean force_fixed)
{CHECK_GL_ERR;
	iw=width, ih=height;
	
	int samp = (int)fminf(fminf(iw/2,ih/2), 128);
	printf("maxsrc using %i points\n", samp);	
	gl_scope_init(iw, ih, samp, force_fixed);

	if(!force_fixed) {
		printf("Compiling maxsrc shader:\n");
		const char *defs = packed_intesity_pixels?"#version 120\n#define FLOAT_PACK_PIX\n":"#version 120\n";
		shader_prog = compile_program_defs(defs, vtx_shader, frag_src);
		if(shader_prog) { // compile succeed
			printf("maxsrc shader compiled\n");
			glUseProgramObjectARB(shader_prog);
			glUniform1iARB(glGetUniformLocationARB(shader_prog, "prev"), 0);
			shad_R_loc = glGetUniformLocationARB(shader_prog, "R");
			glUseProgramObjectARB(0);
			use_glsl = GL_TRUE;
		}
	}
	if(!use_glsl)
		fixed_map = map_new(24, fixed_map_cb);

	if(GLEE_ARB_texture_rg) {
		printf("glmaxsrc: using R textures\n");
	}

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glGenFramebuffersEXT(2, max_fbos);
	glGenTextures(2, fbo_tex);
	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, fbo_tex[i]);
		if(GLEE_ARB_texture_rg && !packed_intesity_pixels) { //TODO: use RG8 if we're doing float pack stuff
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R16,  width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8,  width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbos[i]);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, fbo_tex[i], 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}
	glPopAttrib();

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
	glUseProgramObjectARB(shader_prog);
	glUniformMatrix2x3fv(shad_R_loc, 1, 0, Rt);
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d(-1,-1); glVertex2d(-1, -1);
		glTexCoord2d( 1,-1); glVertex2d( 1, -1);
		glTexCoord2d(-1, 1); glVertex2d(-1,  1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
	glEnd();
	glUseProgramObjectARB(0);
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

static uint32_t frm = 0;
void gl_maxsrc_update(void)
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

	GLint src_tex = fbo_tex[(frm+1)%2];
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbos[frm%2]);
	setup_viewport(iw, ih);

	if(use_glsl) render_bg_glsl(R, src_tex);
	else render_bg_fixed(R, src_tex);
	DEBUG_CHECK_GL_ERR;

	render_scope(R);
	DEBUG_CHECK_GL_ERR;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopClientAttrib();
	glPopAttrib();
	tx+=0.02*dt; ty+=0.01*dt; tz-=0.003*dt;
	frm++;

	DEBUG_CHECK_GL_ERR;
}

GLuint gl_maxsrc_get(void) {
//	return fbo_tex[(frm+1)%2];
	return fbo_tex[(frm)%2];
}


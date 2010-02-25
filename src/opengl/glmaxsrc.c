/**
 * gl_maxsrc.c
 *
 */

#include "common.h"
#include "audio/audio.h"
#include "glmisc.h"
#include "glmaxsrc.h"

#define USE_MIRRORED_REPEAT

static inline float sqr(float v) {return v*v;}
static uint16_t *setup_point_16(int w, int h)
{
	uint16_t *buf = malloc(w * h * sizeof(*buf));
	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
#ifdef USE_MIRRORED_REPEAT
			float u = 1-((float)x)/(w-1), v = 1-((float)y)/(h-1);
#else
//			float u = (2.0f*x)/(w) - 1, v = (2.0f*y)/(h) - 1;
			float u = (2.0f*x)/(w-1) - 1, v = (2.0f*y)/(h-1) - 1;
#endif
			buf[y*w + x] = (uint16_t)(expf(-4.5f*log2f(u*u+v*v + 1)/2)*(UINT16_MAX));
		}
	}
	return buf;
}

static float tx=0, ty=0, tz=0;
static inline float getsamp(audio_data *d, int i, int w) {
	float res = 0;
	int l = IMAX(i-w, 0);
	int u = IMIN(i+w, d->len);
	for(int i = l; i < u; i++) {
		res += d->data[i];
	}
	res = res / (2*w);
	return res;
}

static const char *pnt_vtx_shader =
		"varying vec4 gl_TexCoord[1];"
		"void main() {\n"
		"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
		"	gl_Position = gl_Vertex;\n"
		"}";

static const char *pnt_shader_src =
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#else\n"
	"#define encode(X) vec4(X)\n"
	"#endif\n"
	"#define uv2flt(uv) 0.5f*(log2(dot((uv),(uv))+1))\n"
//	"#define uv2flt(uv) (dot((uv),(uv)))\n"
	"void main() {\n"
#ifdef USE_MIRRORED_REPEAT
	"	vec2 uv = gl_TexCoord[0].st-1;\n"
//	"	vec2 dx = dFdx(gl_TexCoord[0].st)*0.5f; vec2 dy = dFdy(gl_TexCoord[0].st)*0.5f;\n"
#else
	"	vec2 uv = gl_TexCoord[0].st*2-1;\n"
//	"	vec2 dx = dFdx(gl_TexCoord[0].st); vec2 dy = dFdy(gl_TexCoord[0].st);\n"
#endif
//	"	float v = (1.0f/8)*("
//	"			4*exp(-4.5f*uv2flt(uv))"
//	"			+ exp(-4.5f*uv2flt(uv+dx)) + exp(-4.5f*uv2flt(uv+dy))"
//	"			+ exp(-4.5f*uv2flt(uv-dx)) + exp(-4.5f*uv2flt(uv-dy)));"
//	"	float v = exp((-4.5f/8)*("
//	"			4*uv2flt(uv)"
//	"			+ uv2flt(uv+dx) + uv2flt(uv+dy)"
//	"			+ uv2flt(uv-dx) + uv2flt(uv-dy)));"
//	"	gl_FragColor = encode(v);\n"
	"	gl_FragColor = encode(exp(-4.5f*uv2flt(uv)));\n"
	"}\n";

//TODO: use 2 basis vectors to find p instead of matrix op
// ie, uniform vec3 a,b; 
//     where a = <1,0,0>*R, b = <0, 1, 0>*R
// then vec3 p = (uv.x*a + uv.y*b)*vec3(1,d,d);
// and final co-ords are still R*p, except we can make R a mat3x2 and maybe save some MAD's

static const char *frag_src =
	"uniform sampler2D prev;\n"
	"invariant uniform mat2x3 R;\n"
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#endif\n"
	"void main() {\n"
	"	vec3 p;\n"
	"	{\n"
	"		const vec2 uv = gl_TexCoord[0].st;\n"
	"		vec3 t = vec3(0.5f);\n"
	"		t.yz = vec2(0.95f*0.5f + (0.05f*0.5f)*length(uv));\n"
	"		p = (uv.x*R[0] + uv.y*R[1])*t;\n"
	"	}\n"
	"#ifdef FLOAT_PACK_PIX\n"
	"	gl_FragData[0] = encode(decode(texture2D(prev, p*R + 0.5f))*0.978f);\n"
	"#else"
	"	const vec4 c = texture2D(prev, p*R + 0.5f);\n"
	"	gl_FragData[0] = vec4(c.x - max(2/256.0f, c.x*(1.0f/100)));\n"
//	"	const vec4 c = texture2D(prev, p*R + 0.5f);\n"
//	"	gl_FragData[0] = (c - max(vec4(2/256.0f), c*0.01f));\n"
	"#endif"
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

static GLuint pnt_tex = 0;
static GLhandleARB shader_prog = 0, pnt_shader = 0;
static GLint shad_R_loc = -1;
static GLuint max_fbo = 0, fbo_tex[2] = { 0, 0 };
static int iw, ih;
static int samp = 0;
static float pw = 0, ph = 0;
static float *sco_verts = NULL;
static GLboolean use_glsl = GL_FALSE;
static Map *fixed_map = NULL;
GEN_MAP_CB(fixed_map_cb, bg_vtx);

#define PNT_MIP_LEVELS 4

void gl_maxsrc_init(int width, int height, GLboolean packed_intesity_pixels, GLboolean force_fixed)
{CHECK_GL_ERR;
	iw=width, ih=height;
	pw = 0.5f*fmaxf(1.0f/24, 8.0f/iw), ph = 0.5f*fmaxf(1.0f/24, 8.0f/ih);
	samp = (int)(8*fmaxf(1/pw,1/ph));
//	samp = IMIN(IMAX(iw,ih)/4, 1023);
	sco_verts = malloc(sizeof(float)*samp*5*4);

	printf("maxsrc using %i points\n", samp);
	if(!force_fixed) {
		printf("Compiling maxsrc shader:\n");
		const char *defs = packed_intesity_pixels?"#version 120\n#define FLOAT_PACK_PIX\n":"#version 120\n";
		shader_prog = compile_program_defs(defs, NULL, frag_src);
		if(shader_prog) { // compile succeed
			pnt_shader = compile_program_defs(defs, NULL, pnt_shader_src);
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

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glGenFramebuffersEXT(1, &max_fbo);
	glGenTextures(2, fbo_tex);
	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, fbo_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,  width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	if(!packed_intesity_pixels) {
		glGenTextures(1, &pnt_tex);
		glBindTexture(GL_TEXTURE_2D, pnt_tex);
		const int pnt_tex_size = 1<<PNT_MIP_LEVELS;

		for(int lvl=0; lvl<PNT_MIP_LEVELS; lvl++) {
			int size = pnt_tex_size>>lvl;
			uint16_t *data = setup_point_16(size, size);
//			for(int y=0; y < size; y++) {
//				for(int x=0; x < size; x++, data++)
//					printf("%2X ", ((*data) >> 8)&0xFF);
//				printf("\n");
//			}
			glTexImage2D(GL_TEXTURE_2D, lvl, GL_LUMINANCE, size, size, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, data);
			free(data);
		}
		uint16_t tmp = 0xFFFF;
		glTexImage2D(GL_TEXTURE_2D, PNT_MIP_LEVELS, GL_LUMINANCE, 1, 1, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, &tmp);

#ifdef USE_MIRRORED_REPEAT
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_MIRRORED_REPEAT_ARB);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_MIRRORED_REPEAT_ARB);
#else
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,  GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,  GL_CLAMP);
#endif
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	glPopAttrib();
	CHECK_GL_ERR;
}

// render the old frame and distort it with GL shading language
static void render_bg_glsl(float R[3][3], GLint tex)
{
	float Rt[][3] = {
		{R[0][0], R[1][0], R[2][0]},
		{R[0][1], R[1][1], R[2][1]},
		{R[0][2], R[1][2], R[2][2]}
	};

	glUseProgramObjectARB(shader_prog);
	glUniformMatrix2x3fv(shad_R_loc, 1, 0, (float *)Rt);
	glBindTexture(GL_TEXTURE_2D, tex);
	glBegin(GL_QUADS);
		glTexCoord2d(-1,-1); glVertex2d(-1, -1);
		glTexCoord2d( 1,-1); glVertex2d( 1, -1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
		glTexCoord2d(-1, 1); glVertex2d(-1,  1);
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
//	const float dt = 1;
//	printf("maxsrc: dt = %8f\n", dt);

	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);
	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);

	float R[3][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};

	GLint draw_tex = fbo_tex[frm%2];
	GLint src_tex = fbo_tex[(frm+1)%2];
	// GL_COLOR_BUFFER_BIT // for glBlend/alpha stuff
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	glActiveTexture(GL_TEXTURE0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, max_fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, draw_tex, 0);
	setup_viewport(iw, ih);

	if(use_glsl) render_bg_glsl(R, src_tex);
	else render_bg_fixed(R, src_tex);
	DEBUG_CHECK_GL_ERR;

	// do the rotate/project ourselves because we can send less data to the card
	// also it makes it easier to match the software implementation
	audio_data ad; audio_get_samples(&ad);
	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, i*ad.len/(samp-1), ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i - (samp-1)/2.0f)*(1.0f/(samp-1)), yt = 0.2f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		const float zvd = 1/(z+2);
		x=x*zvd*4/3; y=y*zvd*4/3;

#ifdef USE_MIRRORED_REPEAT
		sco_verts[(i*4+0)*4+0] = 0; sco_verts[(i*4+0)*4+1] = 2; sco_verts[(i*4+0)*4+2] = x-pw; sco_verts[(i*4+0)*4+3] = y-ph;
		sco_verts[(i*4+1)*4+0] = 2; sco_verts[(i*4+1)*4+1] = 2; sco_verts[(i*4+1)*4+2] = x+pw; sco_verts[(i*4+1)*4+3] = y-ph;
		sco_verts[(i*4+2)*4+0] = 2; sco_verts[(i*4+2)*4+1] = 0; sco_verts[(i*4+2)*4+2] = x+pw; sco_verts[(i*4+2)*4+3] = y+ph;
		sco_verts[(i*4+3)*4+0] = 0; sco_verts[(i*4+3)*4+1] = 0; sco_verts[(i*4+3)*4+2] = x-pw; sco_verts[(i*4+3)*4+3] = y+ph;
#else
		sco_verts[(i*4+0)*4+0] = 0; sco_verts[(i*4+0)*4+1] = 1; sco_verts[(i*4+0)*4+2] = x-pw; sco_verts[(i*4+0)*4+3] = y-ph;
		sco_verts[(i*4+1)*4+0] = 1; sco_verts[(i*4+1)*4+1] = 1; sco_verts[(i*4+1)*4+2] = x+pw; sco_verts[(i*4+1)*4+3] = y-ph;
		sco_verts[(i*4+2)*4+0] = 1; sco_verts[(i*4+2)*4+1] = 0; sco_verts[(i*4+2)*4+2] = x+pw; sco_verts[(i*4+2)*4+3] = y+ph;
		sco_verts[(i*4+3)*4+0] = 0; sco_verts[(i*4+3)*4+1] = 0; sco_verts[(i*4+3)*4+2] = x-pw; sco_verts[(i*4+3)*4+3] = y+ph;
#endif
	}
	audio_finish_samples();

	glEnable(GL_BLEND);
	glBlendEquationEXT(GL_MAX_EXT);
	if(!pnt_tex) glUseProgramObjectARB(pnt_shader);
	else glBindTexture(GL_TEXTURE_2D, pnt_tex);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnableClientState(GL_VERTEX_ARRAY); //FIXME
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(float)*4, sco_verts);
	glVertexPointer(2, GL_FLOAT, sizeof(float)*4, sco_verts + 2);
	glDrawArrays(GL_QUADS, 0, samp*4);

	if(!pnt_tex) glUseProgramObjectARB(0);
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

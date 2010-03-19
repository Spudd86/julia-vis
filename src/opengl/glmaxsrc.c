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
	"void main() {\n"
	"#ifdef FLOAT_PACK_PIX\n"
#ifdef USE_MIRRORED_REPEAT
	"	gl_TexCoord[0] = gl_MultiTexCoord0-1;\n"
#else
	"	gl_TexCoord[0] = gl_MultiTexCoord0*2-1;\n"
#endif
	"#else\n"
	"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"#endif\n"
	"	gl_Position = gl_Vertex;\n"
	"}";

static const char *pnt_shader_src =
	"#ifdef FLOAT_PACK_PIX\n"
	"#define uv2flt(uv) 0.5f*(log2(dot((uv),(uv))+1))\n"
	"void main() {\n"
	"	vec2 uv = gl_TexCoord[0].st;\n"
	"	gl_FragColor = vec4(clamp(exp(-4.5f*uv2flt(uv)), 0.0, 1.0-1.0/255.0), 0.0,0.0,0.0);\n"
	"}\n"
	"#else\n"
	"uniform sampler2D tex;\n"
	"void main() {\n"
	"	gl_FragColor = texture2D(tex, gl_TexCoord[0].st);\n"
	"}\n"
	"#endif\n";

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
	"invariant uniform mat2x3 R;\n"
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#endif\n"
	"void main() {\n"
	"	vec3 p;\n"
	"	{\n"
	"		vec2 uv = gl_TexCoord[0].st;\n"
	"		vec3 t = vec3(0.5f);\n"
	"		t.yz = vec2(0.95f*0.5f + (0.05f*0.5f)*length(uv));\n"
	"		p = (uv.x*R[0] + uv.y*R[1])*t;\n"
	"	}\n"
	"#ifdef FLOAT_PACK_PIX\n"
	"	gl_FragColor = encode(decode(texture2D(prev, p*R + 0.5f))*0.978f);\n"
	"#else\n"
	"	vec4 c = texture2D(prev, p*R + 0.5f);\n"
	"	gl_FragColor = vec4(c.x - max(2/256.0f, c.x*(1.0f/100)));\n"
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
//
//		(u*R[0*3+0] + v*R[1*3+0]),
//		(u*R[0*3+1] + v*R[1*3+1])*d,
//		(u*R[0*3+2] + v*R[1*3+2])*d
//	};
//	txco->x = (p[0]*R[0*3+0] + p[1]*R[0*3+1] + p[2]*R[0*3+2]+1.0f)*0.5f;
//	txco->y = (p[0]*R[1*3+0] + p[1]*R[1*3+1] + p[2]*R[1*3+2]+1.0f)*0.5f;
}

static GLuint pnt_tex = 0;
static GLhandleARB shader_prog = 0, pnt_shader = 0;
static GLint shad_R_loc = -1;
static GLuint max_fbo = 0, fbo_tex[2] = { 0, 0 };
static int iw, ih;
static int samp = 0;
static float pw = 0, ph = 0;
static GLfloat sco_verts[128*8*4];
static GLint sco_ind[128*4*3];
static GLboolean use_glsl = GL_FALSE;
static Map *fixed_map = NULL;
GEN_MAP_CB(fixed_map_cb, bg_vtx);

#define USE_VBO
#ifdef USE_VBO
static union {
	GLuint handles[3];
	struct {
		GLuint ind, txco, vtx;
	};
}bufobjs;
static bool use_vbo = false;
#endif

#define PNT_MIP_LEVELS 4

void gl_maxsrc_init(int width, int height, GLboolean packed_intesity_pixels, GLboolean force_fixed)
{CHECK_GL_ERR;
	iw=width, ih=height;
	pw = 0.5f*fmaxf(1.0f/24, 8.0f/iw), ph = 0.5f*fmaxf(1.0f/24, 8.0f/ih);
	samp = (int)fminf(fminf(iw/2,ih/2), 128);

	printf("maxsrc using %i points\n", samp);
	if(!force_fixed) {
		printf("Compiling maxsrc shader:\n");
		const char *defs = packed_intesity_pixels?"#version 120\n#define FLOAT_PACK_PIX\n":"#version 120\n";
		shader_prog = compile_program_defs(defs, vtx_shader, frag_src);
		if(shader_prog) { // compile succeed
			pnt_shader = compile_program_defs(defs, pnt_vtx_shader, pnt_shader_src);
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
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8,  width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
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

		if(pnt_shader) {
			glUseProgramObjectARB(pnt_shader);
			glUniform1iARB(glGetUniformLocationARB(pnt_shader, "tex"), 0);
			glUseProgramObjectARB(0);
			CHECK_GL_ERR;
		}

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

//		int it = i*3;
//		sco_verts[(it*4+0)*4+0] = 0; sco_verts[(it*4+0)*4+1] = 2;
//		sco_verts[(it*4+1)*4+0] = 1; sco_verts[(it*4+1)*4+1] = 2;
//		sco_verts[(it*4+2)*4+0] = 1; sco_verts[(it*4+2)*4+1] = 0;
//		sco_verts[(it*4+3)*4+0] = 0; sco_verts[(it*4+3)*4+1] = 0;
//		it++;
//		sco_verts[(it*4+0)*4+0] = 1; sco_verts[(it*4+0)*4+1] = 2;
//		sco_verts[(it*4+1)*4+0] = 1; sco_verts[(it*4+1)*4+1] = 2;
//		sco_verts[(it*4+2)*4+0] = 1; sco_verts[(it*4+2)*4+1] = 0;
//		sco_verts[(it*4+3)*4+0] = 1; sco_verts[(it*4+3)*4+1] = 0;
//		it++;
//		sco_verts[(it*4+0)*4+0] = 1; sco_verts[(it*4+0)*4+1] = 2;
//		sco_verts[(it*4+1)*4+0] = 2; sco_verts[(it*4+1)*4+1] = 2;
//		sco_verts[(it*4+2)*4+0] = 2; sco_verts[(it*4+2)*4+1] = 0;
//		sco_verts[(it*4+3)*4+0] = 1; sco_verts[(it*4+3)*4+1] = 0;
#ifdef USE_MIRRORED_REPEAT
	for(int i=0; i<samp; i++) {
		sco_verts[(i*8+0)*4+0] = 0; sco_verts[(i*8+0)*4+1] = 2;
		sco_verts[(i*8+1)*4+0] = 0; sco_verts[(i*8+1)*4+1] = 0;
		sco_verts[(i*8+2)*4+0] = 1; sco_verts[(i*8+2)*4+1] = 2;
		sco_verts[(i*8+3)*4+0] = 1; sco_verts[(i*8+3)*4+1] = 0;
		sco_verts[(i*8+4)*4+0] = 1; sco_verts[(i*8+4)*4+1] = 2;
		sco_verts[(i*8+5)*4+0] = 1; sco_verts[(i*8+5)*4+1] = 0;
		sco_verts[(i*8+6)*4+0] = 2; sco_verts[(i*8+6)*4+1] = 2;
		sco_verts[(i*8+7)*4+0] = 2; sco_verts[(i*8+7)*4+1] = 0;
	}
#else
	for(int i=0; i<samp; i++) {
		sco_verts[(i*8+0)*4+0] = 0; sco_verts[(i*8+0)*4+1] = 1;
		sco_verts[(i*8+1)*4+0] = 0; sco_verts[(i*8+1)*4+1] = 0;
		sco_verts[(i*8+2)*4+0] = 0.5f; sco_verts[(i*8+2)*4+1] = 1;
		sco_verts[(i*8+3)*4+0] = 0.5f; sco_verts[(i*8+3)*4+1] = 0;
		sco_verts[(i*8+4)*4+0] = 0.5f; sco_verts[(i*8+4)*4+1] = 1;
		sco_verts[(i*8+5)*4+0] = 0.5f; sco_verts[(i*8+5)*4+1] = 0;
		sco_verts[(i*8+6)*4+0] = 1; sco_verts[(i*8+6)*4+1] = 1;
		sco_verts[(i*8+7)*4+0] = 1; sco_verts[(i*8+7)*4+1] = 0;
	}
#endif
	for(int i=0; i<samp; i++) {
		sco_ind[(i*3+0)*4+0] = i*8+0; sco_ind[(i*3+0)*4+1] = i*8+2; sco_ind[(i*3+0)*4+2] = i*8+3; sco_ind[(i*3+0)*4+3] = i*8+1;
		sco_ind[(i*3+1)*4+0] = i*8+2; sco_ind[(i*3+1)*4+1] = i*8+4; sco_ind[(i*3+1)*4+2] = i*8+5; sco_ind[(i*3+1)*4+3] = i*8+3;
		sco_ind[(i*3+2)*4+0] = i*8+4; sco_ind[(i*3+2)*4+1] = i*8+6; sco_ind[(i*3+2)*4+2] = i*8+7; sco_ind[(i*3+2)*4+3] = i*8+5;
	}

#ifdef USE_VBO
	if(GLEE_ARB_vertex_buffer_object) {
		use_vbo = true;
//		glGenBuffersARB(3, bufobjs.handles);
		glGenBuffersARB(1, &bufobjs.ind);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufobjs.ind);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, sizeof(GLint)*samp*4*3, sco_ind, GL_STATIC_DRAW_ARB);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
//		CHECK_GL_ERR;
//		glBindBufferARB(GL_ARRAY_BUFFER_ARB, bufobjs.vtx);
//		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat)*samp*8*2, NULL, GL_STREAM_DRAW_ARB);
//		glBindBufferARB(GL_ARRAY_BUFFER_ARB, bufobjs.txco);
//		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat)*samp*8*2, NULL, GL_STATIC_DRAW_ARB);
//		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
#endif
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
	float px, py;
	{
		float s = getsamp(&ad, 0, ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);
		float xt = -0.5f, yt = 0.2f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		const float zvd = 1/(z+2);
		px=x*zvd*4/3; py=y*zvd*4/3;
	}

	for(int i=0; i<samp; i++) {
		float s = getsamp(&ad, (i+1)*ad.len/(samp), ad.len/96);
		s=copysignf(log2f(fabsf(s)*3+1)/2, s);

		float xt = (i+1 - (samp)/2.0f)*(1.0f/(samp)), yt = 0.2f*s, zt = 0.0f;
		float x = R[0][0]*xt + R[1][0]*yt + R[2][0]*zt;
		float y = R[0][1]*xt + R[1][1]*yt + R[2][1]*zt;
		float z = R[0][2]*xt + R[1][2]*yt + R[2][2]*zt;
		const float zvd = 1/(z+2);
		x=x*zvd*4/3; y=y*zvd*4/3;

		const float dx=x-px, dy=y-py;
		const float d = 1/sqrt(dx*dx + dy*dy);
		const float tx=dx*d*pw, ty=dy*d*pw;
		const float nx=-dy*d*pw, ny=dx*d*ph;

		sco_verts[(i*8+0)*4+2] = px-nx-tx; sco_verts[(i*8+0)*4+3] = py-ny-ty;
		sco_verts[(i*8+1)*4+2] = px+nx-tx; sco_verts[(i*8+1)*4+3] = py+ny-ty;
		sco_verts[(i*8+2)*4+2] = px-nx   ; sco_verts[(i*8+2)*4+3] = py-ny;
		sco_verts[(i*8+3)*4+2] = px+nx   ; sco_verts[(i*8+3)*4+3] = py+ny;
		sco_verts[(i*8+4)*4+2] =  x-nx   ; sco_verts[(i*8+4)*4+3] =  y-ny;
		sco_verts[(i*8+5)*4+2] =  x+nx   ; sco_verts[(i*8+5)*4+3] =  y+ny;
		sco_verts[(i*8+6)*4+2] =  x-nx+tx; sco_verts[(i*8+6)*4+3] =  y-ny+ty;
		sco_verts[(i*8+7)*4+2] =  x+nx+tx; sco_verts[(i*8+7)*4+3] =  y+ny+ty;
		px=x,py=y;
	}
	audio_finish_samples();

	glEnable(GL_BLEND);
	glBlendEquationEXT(GL_MAX_EXT);
	if(pnt_shader) glUseProgramObjectARB(pnt_shader);
	if(pnt_tex) glBindTexture(GL_TEXTURE_2D, pnt_tex);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(float)*4, sco_verts);
	glVertexPointer(2, GL_FLOAT, sizeof(float)*4, sco_verts + 2);
#ifdef USE_VBO
	if(use_vbo) {
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, bufobjs.ind);
		glDrawElements(GL_QUADS, samp*4*3, GL_UNSIGNED_INT, NULL);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	} else {
		glDrawElements(GL_QUADS, samp*4*3, GL_UNSIGNED_INT, sco_ind);
	}
#else
	glDrawElements(GL_QUADS, samp*4*3, GL_UNSIGNED_INT, sco_ind);
#endif

	if(pnt_shader) glUseProgramObjectARB(0);
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

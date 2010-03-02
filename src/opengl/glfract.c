
#include "common.h"
#include "points.h"
#include "glmisc.h"
#include "glmaxsrc.h"
#include "pallet.h"
#include "audio/audio.h"
#include "glpallet.h"

static const char *map_frag_shader =
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#else\n"
	"#ifdef MAP_SAMP\n"
	"#define encode(X) vec4(X)\n#define decode(X) (X).x\n"
	"#else\n#define encode(X) (X)\n#define decode(X) (X)\n"
	"#endif\n"
	"#endif\n"

	"uniform sampler2D prev;\n"
	"uniform sampler2D maxsrc;\n"
	"invariant uniform vec2 c;\n"

	"#ifdef MAP_SAMP\n"
	"float smap(const in vec2 s) {\n"
	"	vec2 t = s*s;\n"
	"	return max(decode(texture2D(prev, vec2(t.x - t.y, 2*s.x*s.y) + c)), decode(texture2D(maxsrc, gl_TexCoord[0].st)));\n"
	"}\n"
	"void main() {\n"
	"#if MAP_SAMP == 4\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = (254/(4*256.0f))*(\n"
	"			smap(gl_TexCoord[1].st-0.485852f*dx+0.142659f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.485852f*dx-0.142659f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.142659f*dx+0.485852f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.142659f*dx-0.485852f*dy) );\n"
	"#elif MAP_SAMP == 5\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = smap(gl_TexCoord[1].st)*(0.201260f*254/256) + \n"
	"			(smap(gl_TexCoord[1].st+0.23594f*dx+0.50000f*dy) + "
	"			 smap(gl_TexCoord[1].st+0.50000f*dx-0.23594f*dy) +\n"
	"			 smap(gl_TexCoord[1].st-0.23594f*dx-0.50000f*dy) +"
	"			 smap(gl_TexCoord[1].st-0.50000f*dx+0.23594f*dy))*(0.199685f*254/256);\n"
	"#elif MAP_SAMP == 8\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = (253.0f/(8*256.0f))*(\n"
	"			smap(gl_TexCoord[1].st-0.500f*dx+0.143f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.288f*dx+0.500f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.429f*dx+0.288f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.143f*dx+0.429f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.500f*dx-0.143f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.288f*dx-0.500f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.429f*dx-0.288f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.143f*dx-0.429f*dy) );\n"
//	"	const float dx = dFdx(gl_TexCoord[1].s);\n"
//	"	const float r = (253.0f/(8*256.0f))*(\n"
//	"			smap(gl_TexCoord[1].st+vec2(-0.500f,+0.143f)*dx) +\n"
//	"			smap(gl_TexCoord[1].st+vec2(+0.288f,+0.500f)*dx) +\n"
//	"			smap(gl_TexCoord[1].st+vec2(+0.429f,+0.288f)*dx) +\n"
//	"			smap(gl_TexCoord[1].st+vec2(-0.143f,+0.429f)*dx) +\n"
//	"			smap(gl_TexCoord[1].st+vec2(+0.500f,-0.143f)*dx) +\n"
//	"			smap(gl_TexCoord[1].st+vec2(-0.288f,-0.500f)*dx) +\n"
//	"			smap(gl_TexCoord[1].st+vec2(-0.429f,-0.288f)*dx) +\n"
//	"			smap(gl_TexCoord[1].st+vec2(+0.143f,-0.429f)*dx) );\n"
	"#elif MAP_SAMP == 9\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st)*0.5f; vec2 dy = dFdy(gl_TexCoord[1].st)*0.5f;\n"
	"	float r = (253.0f/(9*256.0f))*(smap(gl_TexCoord[1].st) + \n"
	"			smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx) +\n"
	"			smap(gl_TexCoord[1].st+dy+dx) + smap(gl_TexCoord[1].st+dy-dx) +\n"
	"			smap(gl_TexCoord[1].st-dx-dy) + smap(gl_TexCoord[1].st-dy+dx) );\n"
	"#endif\n"
//	"	gl_FragData[0] = r;\n"
	"	gl_FragData[0] = encode(r);\n"
//	"	gl_FragData[0] = encode(max(decode(r), decode(texture2D(maxsrc, gl_TexCoord[0].st))));\n"
	"}\n"
	"#else\n"
	"void main() {\n"
	"	vec2 t = gl_TexCoord[1].st * gl_TexCoord[1].st;\n"
	"	gl_FragData[0] = encode(max("
	"			decode( texture2D(prev, vec2(t.x - t.y, 2*gl_TexCoord[1].x*gl_TexCoord[1].y) + c)*(253/256.0f) ),"
	"			decode( texture2D(maxsrc, gl_TexCoord[0].st) )"
	"	));\n"
	"}\n"
	"#endif\n";
	
static const char *rat_map_frag_shader = 
	"const sampler2D prev;\n"
	"const sampler2D maxsrc;\n"
	"invariant uniform vec4 c;\n"
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#else\n"
	"#ifdef MAP_SAMP\n"
	"#define encode(X) vec4(X)\n#define decode(X) (X).x\n"
	"#else\n"
	"#define encode(X) X\n#define decode(X) X\n"
	"#endif\n"
	"#endif\n"
	"#ifndef MAP_SAMP\n"
	"void main() {\n"
	"	vec2 s = gl_TexCoord[1].st*2.5;\n"
	"	vec2 t = s*s;\n"
	"	float ab = s.x*s.y;\n"
	"	s = vec2(4*ab*(t.x - t.y), t.x*t.x - 6*t.x*t.y + t.y*t.y) + c.xy;\n"
	"	t = vec2(t.x - t.y, 2*ab)+c.zw;\n"
	"	gl_FragColor = encode(max(decode(texture2D(prev,(0.5f/2.5)*vec2(dot(s,t), dot(s,t.yx))/dot(t,t)+0.5f)), decode(texture2D(maxsrc, gl_TexCoord[0].st))));\n"
	"}\n"
	"#else\n"
	"float smap(const in vec2 tmp) {\n"
	"	vec2 s = tmp*2.5;\n"
	"	vec2 t = s*s;\n"
	"	float ab = s.x*s.y;\n"
	"	s = vec2(4*ab*(t.x - t.y), t.x*t.x - 6*t.x*t.y + t.y*t.y) + c.xy;\n"
	"	t = vec2(t.x - t.y, 2*ab)+c.zw;\n"
	"	return max(decode(texture2D(prev,(0.5f/2.5)*vec2(dot(s,t), dot(s,t.yx))/dot(t,t)+0.5f)), decode(texture2D(maxsrc, gl_TexCoord[0].st)));\n"
	"}\n"
	"void main() {\n"
	"#if MAP_SAMP == 4\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = (254/(4*256.0f))*(\n"
	"			smap(gl_TexCoord[1].st-0.485852f*dx+0.142659f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.485852f*dx-0.142659f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.142659f*dx+0.485852f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.142659f*dx-0.485852f*dy) );\n"
	"#elif MAP_SAMP == 5\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = smap(gl_TexCoord[1].st)*(0.201260f*254/(256.0f)) + \n"
	"			(smap(gl_TexCoord[1].st+0.23594f*dx+0.50000f*dy) + "
	"			 smap(gl_TexCoord[1].st+0.50000f*dx-0.23594f*dy) +\n"
	"			 smap(gl_TexCoord[1].st-0.23594f*dx-0.50000f*dy) +"
	"			 smap(gl_TexCoord[1].st-0.50000f*dx+0.23594f*dy))*(0.199685f*254/(256.0f));\n"
	"#elif MAP_SAMP == 8\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
//	"	vec2 dx = vec2(1.0f/1024, 0); const vec2 dy = vec2(0,1.0f/1024);"
	"	float r = (253.0f/(8*256.0f))*(\n"
	"			smap(gl_TexCoord[1].st-0.500f*dx+0.143f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.288f*dx+0.500f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.429f*dx+0.288f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.143f*dx+0.429f*dy) +\n"

	"			smap(gl_TexCoord[1].st+0.500f*dx-0.143f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.288f*dx-0.500f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.429f*dx-0.288f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.143f*dx-0.429f*dy) );\n"
	"#elif MAP_SAMP == 9\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = (253.0f/(9*256.0f))*(smap(gl_TexCoord[1].st) + \n"
	"			(smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx)) +\n"
	"			(smap(gl_TexCoord[1].st+dy+dx) + smap(gl_TexCoord[1].st+dy-dx) +\n"
	"			smap(gl_TexCoord[1].st-dx-dy) + smap(gl_TexCoord[1].st-dy+dx)) );\n"
	"#endif\n"
//	"	gl_FragData[0] = encode(max(decode(r), decode(texture2D(maxsrc, gl_TexCoord[0].st))));\n"
	"	gl_FragData[0] = encode(r);\n"
	"}\n"
	"#endif\n";

static void map_vtx(float u, float v, vec2f *txco, const void *cb_data) {
	const struct point_data *pd = cb_data;
	float c1 = (pd->p[0]-0.5f)*0.25f + 0.5f, c2 = pd->p[1]*0.25f + 0.5f;
	txco->x = (u*u - v*v + c1); txco->y = (2*u*v + c2 );
}
static void rat_map_vtx(float u, float v, vec2f *txco, const void *cb_data) {
	const struct point_data *pd = cb_data;
	static const float xoom = 3.0f, moox = 1.0f/3.0f;
	const float cx0 = pd->p[0], cy0 = pd->p[1], cx1 = pd->p[2]*2, cy1 = pd->p[3]*2;

	float a,b,c,d,sa,sb, cdivt, x, y;
	a=u*xoom; b=v*xoom; sa=a*a; sb=b*b;
	c=sa-sb + cx1; d=2*a*b+cy1;
	b=4*(sa*a*b - a*b*sb) + cy0;  a=sa*sa -6*sa*sb + sb*sb + cx0;
	cdivt = moox/(c*c + d*d);
	x = (a*c + b*d)*cdivt;  y = (a*d + c*b)*cdivt;

	txco->x = (x+1.0f)*0.5f; txco->y = (y+1.0f)*0.5f;
}
GEN_MAP_CB(map_cb, map_vtx);
GEN_MAP_CB(rat_map_cb, rat_map_vtx);

#define NUM_FBO_TEX 2

static GLint map_prog  = -1;
static GLint map_c_loc=-1, map_prev_loc=-1, map_maxsrc_loc=-1;
static GLuint fbo, fbo_tex[NUM_FBO_TEX];
static GLboolean rational_julia = GL_FALSE;

static Map *fixed_map = NULL;
static GLboolean use_glsl = GL_FALSE;
static int im_w = -1, im_h = -1;
static uint32_t frm  = 0;

GLint fract_get_tex(void) {
//	return fbo_tex[(frm+1)%NUM_FBO_TEX];
	return fbo_tex[(frm+2)%NUM_FBO_TEX];
}

void render_fractal(struct point_data *pd)
{	DEBUG_CHECK_GL_ERR;
	GLint draw_tex = fbo_tex[frm%NUM_FBO_TEX];
	GLint src_tex = fbo_tex[(frm+1)%NUM_FBO_TEX];
	
	// GL_COLOR_BUFFER_BIT
	// GL_VIEWPORT_BIT
	// GL_TEXTURE_BIT
	glPushAttrib(GL_ALL_ATTRIB_BITS);
//	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_TEXTURE_BIT);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, draw_tex, 0);
	setup_viewport(im_w, im_h);

	if(use_glsl) {
		glUseProgramObjectARB(map_prog);
		if(!rational_julia) glUniform2fARB(map_c_loc, (pd->p[0]-0.5f)*0.25f + 0.5f, pd->p[1]*0.25f + 0.5f);
		else glUniform4fARB(map_c_loc, pd->p[0], pd->p[1], pd->p[2], pd->p[3]);

		glActiveTextureARB(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_2D, src_tex);
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
		glBegin(GL_QUADS);
			glMultiTexCoord2f(GL_TEXTURE0, 0.0, 1.0);
			glMultiTexCoord2f(GL_TEXTURE1,-1.0, 1.0);
			glVertex2d(-1,  1);
			glMultiTexCoord2f(GL_TEXTURE0, 1.0, 1.0);
			glMultiTexCoord2f(GL_TEXTURE1, 1.0, 1.0);
			glVertex2d( 1,  1);
			glMultiTexCoord2f(GL_TEXTURE0, 1.0, 0.0);
			glMultiTexCoord2f(GL_TEXTURE1, 1.0,-1.0);
			glVertex2d( 1,-1);
			glMultiTexCoord2f(GL_TEXTURE0, 0.0, 0.0);
			glMultiTexCoord2f(GL_TEXTURE1,-1.0,-1.0);
			glVertex2d(-1,-1);
		glEnd();
		glUseProgramObjectARB(0);
	} else {
		glClearColor(1.0f/256, 1.0f/256,1.0f/256, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0,0,0,1);

		glEnable(GL_BLEND);
		glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
		glBlendEquationEXT(GL_FUNC_SUBTRACT_EXT);
		glBlendColor(0, 0, 0, 63.0f/64);
		glBindTexture(GL_TEXTURE_2D, src_tex);
		map_render(fixed_map, pd);

		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBlendEquationEXT(GL_MAX_EXT);
		glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
		glBegin(GL_QUADS);
			glTexCoord2d( 0, 0); glVertex2d(-1, -1);
			glTexCoord2d( 1, 0); glVertex2d( 1, -1);
			glTexCoord2d( 1, 1); glVertex2d( 1,  1);
			glTexCoord2d( 0, 1); glVertex2d(-1,  1);
		glEnd();
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glPopAttrib();
	DEBUG_CHECK_GL_ERR;
	frm++;
}

void fractal_init(const opt_data *opts, int width, int height, GLboolean force_fixed, GLboolean packed_intesity_pixels)
{CHECK_GL_ERR;
	im_w = width; im_h = height;
	rational_julia = opts->rational_julia;

//	glHint(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB, GL_NICEST);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glGenFramebuffersEXT(1, &fbo);
	glGenTextures(NUM_FBO_TEX, fbo_tex);
	for(int i=0; i<NUM_FBO_TEX; i++) {
		glBindTexture(GL_TEXTURE_2D, fbo_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, im_w, im_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		static float foo[] = {0.0f, 0.0f, 0.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, foo);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glPopClientAttrib();
	glPopAttrib();
	CHECK_GL_ERR;

	if(!force_fixed) {
		printf("Compiling map shader:\n");
		const char *map_defs = "#version 120\n";
		//TODO: try to fall back to -q 0 if we fail to compile
		//TODO: might need to die if we don't get the packed pixels shader to compile...
		if(!packed_intesity_pixels) {
			if(opts->quality == 1) map_defs = "#version 120\n#define MAP_SAMP 4\n\n";
			else if(opts->quality == 2) map_defs = "#version 120\n#define MAP_SAMP 5\n";
			else if(opts->quality == 3) map_defs = "#version 120\n#define MAP_SAMP 8\n";
			else if(opts->quality == 4) map_defs = "#version 120\n#define MAP_SAMP 9\n";
		} else {
			map_defs = "#version 120\n#define FLOAT_PACK_PIX\n";
			if(opts->quality == 1) map_defs = "#version 120\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 4\n";
			else if(opts->quality == 2) map_defs = "#version 120\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 5\n";
			else if(opts->quality == 3) map_defs = "#version 120\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 8\n";
			else if(opts->quality == 4) map_defs = "#version 120\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 9\n";
		}
		
		if(rational_julia)
			map_prog = compile_program_defs(map_defs, NULL, rat_map_frag_shader);
		else
			map_prog = compile_program_defs(map_defs, NULL, map_frag_shader);

		if(map_prog) {
			use_glsl = GL_TRUE;
			glUseProgramObjectARB(map_prog);
			map_c_loc = glGetUniformLocationARB(map_prog, "c");
			map_prev_loc = glGetUniformLocationARB(map_prog, "prev");
			map_maxsrc_loc = glGetUniformLocationARB(map_prog, "maxsrc");
			glUniform1iARB(map_maxsrc_loc, 0);
			glUniform1iARB(map_prev_loc, 1);
			glUseProgramObjectARB(0);
			printf("Map shader compiled\n");
		}
	}
	if(!use_glsl) {
		if(!rational_julia)
			fixed_map = map_new(97, map_cb);
		else
			fixed_map = map_new(127, rat_map_cb);
	}
	CHECK_GL_ERR;
}





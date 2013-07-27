#include "common.h"
#include "points.h"
#include "opengl/glmisc.h"
#include "opengl/glmaxsrc.h"
#include "opengl/glfract.h"

static const char *map_defs_list[2][5] =
{
 {
  "#version 110\n",
  "#version 110\n#define MAP_SAMP 4\n\n",
  "#version 110\n#define MAP_SAMP 5\n\n",
  "#version 110\n#define MAP_SAMP 8\n\n",
  "#version 110\n#define MAP_SAMP 9\n\n",
 },
 {
  "#version 110\n#define FLOAT_PACK_PIX\n",
  "#version 110\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 4\n",
  "#version 110\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 5\n",
  "#version 110\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 8\n",
  "#version 110\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 9\n",
 }
};

static const char *vtx_shader =
		"void main() {\n"
		"	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
		"	gl_TexCoord[1] = gl_MultiTexCoord0*2.0f-1.0f;\n"
		"	gl_Position = gl_Vertex;\n"
		"}";
#if 0
// mandelbox http://sites.google.com/site/mandelbox/what-is-a-mandelbox
static const char *map_frag_shader =
	"uniform sampler2D prev;\n"
	"uniform sampler2D maxsrc;\n"
	"uniform vec2 c;\n"
	
	"vec2 boxFold(in vec2 v) {\n"
	"	if(v.x > 1) v.x = 2-v.x;\n"
	"	else if(v.x < -1) v.x = -2-v.x;\n"
	"	if(v.y > 1) v.y = 2-v.y;\n"
	"	else if(v.y < -1) v.y = -2-v.y;\n"
	"	return v;\n"
	"}\n"
	
	"vec2 ballFold(in float r, in vec2 v) {\n"
	"	float m = length(v);\n"
	"	if(m < r) v = v/(r*r);\n"
	"	else if(m<1) v = v/(m*m);\n"
	"	return v;\n"
	"}\n"
	
//	"#define encode(X) vec4(X)\n#define decode(X) (X).x\n"
	"#define encode(X) (X)\n#define decode(X) (X)\n"
	
	"void main() {\n"
	"	vec2 t = (gl_TexCoord[0].st-0.5)*10;\n" //(pd->p[0]-0.5f)*0.25f + 0.5f, pd->p[1]*0.25f + 0.5f
//	"	vec2 v = 1*ballFold(0.5, 2*boxFold(t)) + (c*4+vec2(-1.5, -2.0))*2;\n"
	"	vec2 v = 2.0*ballFold(0.5, 0.5*boxFold(t)) + (c*4+vec2(-1.5, -2.0))*2;\n"
	"	gl_FragData[0] = encode(max(\n"
	"			decode( texture2D(prev, v/10 + 0.5)*(253/256.0f) ),\n"
	"			decode( texture2D(maxsrc, gl_TexCoord[0].st) )\n"
	"	));\n"
	"}\n";
	
/*
  v = s*ballFold(r, f*boxFold(v)) + c
where boxFold(v) means for each axis a:
  if v[a]>1          v[a] =  2-v[a]
  else if v[a]<-1 v[a] =-2-v[a]
and ballFold(r, v) means for v's magnitude m:
  if m<r         m = m/r^2
  else if m<1 m = 1/m^2
*/

#else
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
	"uniform vec2 c;\n"

	"#ifdef MAP_SAMP\n"
	"float smap(const in vec2 s) {\n"
	"	vec2 t = s*s;\n"
	"	return max(decode(texture2D(prev, vec2(t.x - t.y, 2.0f*s.x*s.y) + c)), decode(texture2D(maxsrc, gl_TexCoord[0].st)));\n"
	"}\n"
	"void main() {\n"
	"#if MAP_SAMP == 4\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = (254.0f/(4.0f*256.0f))*(\n"
	"			smap(gl_TexCoord[1].st-0.485852f*dx+0.142659f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.485852f*dx-0.142659f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.142659f*dx+0.485852f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.142659f*dx-0.485852f*dy) );\n"
	"#elif MAP_SAMP == 5\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = smap(gl_TexCoord[1].st)*(0.201260f*254.0f/256.0f) + \n"
	"			(smap(gl_TexCoord[1].st+0.23594f*dx+0.50000f*dy) + "
	"			 smap(gl_TexCoord[1].st+0.50000f*dx-0.23594f*dy) +\n"
	"			 smap(gl_TexCoord[1].st-0.23594f*dx-0.50000f*dy) +"
	"			 smap(gl_TexCoord[1].st-0.50000f*dx+0.23594f*dy))*(0.199685f*254.0f/256.0f);\n"
	"#elif MAP_SAMP == 8\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = (253.0f/(8.0f*256.0f))*(\n"
	"			smap(gl_TexCoord[1].st-0.500f*dx+0.143f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.288f*dx+0.500f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.429f*dx+0.288f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.143f*dx+0.429f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.500f*dx-0.143f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.288f*dx-0.500f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.429f*dx-0.288f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.143f*dx-0.429f*dy) );\n"
	"#elif MAP_SAMP == 9\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st)*0.5f; vec2 dy = dFdy(gl_TexCoord[1].st)*0.5f;\n"
	"	float r = (253.0f/(9.0f*256.0f))*(smap(gl_TexCoord[1].st) + \n"
	"			smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx) +\n"
	"			smap(gl_TexCoord[1].st+dy+dx) + smap(gl_TexCoord[1].st+dy-dx) +\n"
	"			smap(gl_TexCoord[1].st-dx-dy) + smap(gl_TexCoord[1].st-dy+dx) );\n"
	"#endif\n"
	"	gl_FragData[0] = encode(r);\n"
	"}\n"
	"#else\n"
	"void main() {\n"
	"	vec2 t = gl_TexCoord[1].st * gl_TexCoord[1].st;\n"
	"	gl_FragData[0] = encode(max("
	"			decode( texture2D(prev, vec2(t.x - t.y, 2.0f*gl_TexCoord[1].x*gl_TexCoord[1].y) + c)*(253.0f/256.0f) ),"
	"			decode( texture2D(maxsrc, gl_TexCoord[0].st) )"
	"	));\n"
	"}\n"
	"#endif\n";
#endif
static const char *rat_map_frag_shader = 
	"uniform sampler2D prev;\n"
	"uniform sampler2D maxsrc;\n"
	"uniform vec4 c;\n"
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
	"	vec2 s = gl_TexCoord[1].st*2.5f;\n"
	"	vec2 t = s*s;\n"
	"	float ab = s.x*s.y;\n"
	"	s = vec2(4.0f*ab*(t.x - t.y), t.x*t.x - 6.0f*t.x*t.y + t.y*t.y) + c.xy;\n"
	"	t = vec2(t.x - t.y, 2.0f*ab)+c.zw;\n"
	"	gl_FragColor = encode(max(decode(texture2D(prev,(0.5f/2.5f)*vec2(dot(s,t), dot(s,t.yx))/dot(t,t)+0.5f)), decode(texture2D(maxsrc, gl_TexCoord[0].st))));\n"
	"}\n"
	"#else\n"
	"float smap(const in vec2 tmp) {\n"
	"	vec2 s = tmp*2.5;\n"
	"	vec2 t = s*s;\n"
	"	float ab = s.x*s.y;\n"
	"	s = vec2(4.0f*ab*(t.x - t.y), t.x*t.x - 6.0f*t.x*t.y + t.y*t.y) + c.xy;\n"
	"	t = vec2(t.x - t.y, 2.0f*ab)+c.zw;\n"
	"	return max(decode(texture2D(prev,(0.5f/2.5f)*vec2(dot(s,t), dot(s,t.yx))/dot(t,t)+0.5f)), decode(texture2D(maxsrc, gl_TexCoord[0].st)));\n"
	"}\n"
	"void main() {\n"
	"#if MAP_SAMP == 4\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = (254.0f/(4.0f*256.0f))*(\n"
	"			smap(gl_TexCoord[1].st-0.485852f*dx+0.142659f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.485852f*dx-0.142659f*dy) +\n"
	"			smap(gl_TexCoord[1].st+0.142659f*dx+0.485852f*dy) +\n"
	"			smap(gl_TexCoord[1].st-0.142659f*dx-0.485852f*dy) );\n"
	"#elif MAP_SAMP == 5\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = smap(gl_TexCoord[1].st)*(0.201260f*254.0f/(256.0f)) + \n"
	"			(smap(gl_TexCoord[1].st+0.23594f*dx+0.50000f*dy) + "
	"			 smap(gl_TexCoord[1].st+0.50000f*dx-0.23594f*dy) +\n"
	"			 smap(gl_TexCoord[1].st-0.23594f*dx-0.50000f*dy) +"
	"			 smap(gl_TexCoord[1].st-0.50000f*dx+0.23594f*dy))*(0.199685f*254.0f/(256.0f));\n"
	"#elif MAP_SAMP == 8\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
	"	float r = (253.0f/(8.0f*256.0f))*(\n"
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
	"	float r = (253.0f/(9.0f*256.0f))*(smap(gl_TexCoord[1].st) + \n"
	"			(smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			 smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx)) +\n"
	"			(smap(gl_TexCoord[1].st+dy+dx) + smap(gl_TexCoord[1].st+dy-dx) +\n"
	"			 smap(gl_TexCoord[1].st-dx-dy) + smap(gl_TexCoord[1].st-dy+dx)) );\n"
	"#endif\n"
	"	gl_FragData[0] = encode(r);\n"
	"}\n"
	"#endif\n";

struct glsl_ctx {
	struct glfract_ctx pubctx;
	GLint prog;
	GLint c_loc, prev_loc, maxsrc_loc;
	GLboolean rational_julia;
};

static void render_glsl(struct glfract_ctx *ctx, const struct point_data *pd)
{
	struct glsl_ctx *priv = (struct glsl_ctx *)ctx;
	
	GLint src_tex = offscr_start_render(ctx->offscr);
	glPushAttrib(GL_TEXTURE_BIT);
	glUseProgram(priv->prog);
	if(!priv->rational_julia) glUniform2f(priv->c_loc, (pd->p[0]-0.5f)*0.25f + 0.5f, pd->p[1]*0.25f + 0.5f);
	else glUniform4f(priv->c_loc, pd->p[0], pd->p[1], pd->p[2], pd->p[3]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, src_tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d( 0, 0); glVertex2d(-1, -1);
		glTexCoord2d( 1, 0); glVertex2d( 1, -1);
		glTexCoord2d( 0, 1); glVertex2d(-1,  1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
	glEnd();
	glUseProgram(0);
	glPopAttrib();
	offscr_finish_render(ctx->offscr);
}

struct glfract_ctx *fractal_glsl_init(const opt_data *opts, int width, int height, GLboolean packed_intesity_pixels)
{
	struct glsl_ctx *ctx = malloc(sizeof(*ctx));
	ctx->pubctx.render = render_glsl;
	ctx->rational_julia = opts->rational_julia;
		
	int quality = MIN(opts->quality, 4);
	const char *map_defs = map_defs_list[packed_intesity_pixels][quality];
	
	//TODO: try to fall back to -q 0 if we fail to compile
	//TODO: might need to die if we don't get the packed pixels shader to compile...
	printf("Compiling map shader:\n");
	if(ctx->rational_julia)
		ctx->prog = compile_program_defs(map_defs, vtx_shader, rat_map_frag_shader);
	else
		ctx->prog = compile_program_defs(map_defs, vtx_shader, map_frag_shader);

	if(ctx->prog) {
		glUseProgram(ctx->prog);
		ctx->c_loc = glGetUniformLocation(ctx->prog, "c");
		ctx->prev_loc = glGetUniformLocation(ctx->prog, "prev");
		ctx->maxsrc_loc = glGetUniformLocation(ctx->prog, "maxsrc");
		glUniform1i(ctx->maxsrc_loc, 0);
		glUniform1i(ctx->prev_loc, 1);
		glUseProgram(0);
		printf("Map shader compiled\n");
	} else {
		free(ctx);
		return NULL; // allow us to fallback
	}
	CHECK_GL_ERR;
	
	ctx->pubctx.offscr = offscr_new(width, height, false, !packed_intesity_pixels);
	
	return (struct glfract_ctx *)ctx;
}

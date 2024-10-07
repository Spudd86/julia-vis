#include <GLES3/gl3.h>

#include "common.h"
#include "pallet.h"
#include "opengl/glpallet.h"

#define ONE_PAL_TEXT 1

static const char *vtx_shader_src =
	"#version 300 es\n"
	"const vec2 texc[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1));"
	"const vec2 vert[4] = vec2[4](vec2(-1, -1), vec2( 1, -1), vec2(-1,  1), vec2( 1,  1));"
	"out vec2 uv;"
	"void main() {\n"
	// "	uv = vertex.xy;\n"
	// "	gl_Position = vec4(vertex.zw, 0. , 1. );\n"
	"	uv = texc[gl_VertexID];\n"
	"	gl_Position = vec4(vert[gl_VertexID], 0. , 1. );\n"
	"}";
static const char *frag_shader_src =
	"#version 300 es\n"
	"#ifdef GL_ES\n"
	"precision highp float;\n"
	"precision highp int;\n"
	"#endif\n"


	// "vec3 linearize(vec3 c) {\n"
	// "	if(c <= 0.04045f) c = c/12.92f;\n"
	// "	else c = pow((c+0.055f)/1.055f, 2.4f);\n"
	// "	return c;\n"
	// "}\n"
	// "vec3 gamma_curve(vec3 c) {\n"
	// "	return mix(c * 12.92, 1.055*pow(c, vec3(1.0/2.4)) - 0.055, greaterThan(c, vec3(0.0031308)));"
	// "}\n"
	"vec3 rgb2oklab(vec3 c)\n"
	"{\n"
	"	mat3 m1 = mat3(0.4122214708, 0.5363325363, 0.0514459929,\n"
	"	               0.2119034982, 0.6806995451, 0.1073969566,\n"
	"	               0.0883024619, 0.2817188376, 0.6299787005);\n"
	"	mat3 m2 = mat3(0.2104542553, +0.7936177850, -0.0040720468,\n"
	"	               1.9779984951, -2.4285922050, +0.4505937099,\n"
	"	               0.0259040371, +0.7827717662, -0.8086757660);\n"
	"	vec3 lms = m1 * c;"
	"   return m2 * (lms*lms*lms);"
	"}\n"
	"vec3 oklab2rgb(vec3 c)\n"
	"{\n"
	"	mat3 m1 = mat3(1.0,  0.3963377774,  0.2158037573,\n"
	"	               1.0, -0.1055613458, -0.0638541728,\n"
	"	               1.0, -0.0894841775, -1.2914855480);\n"
	"	mat3 m2 = mat3(+4.0767416621, -3.3077115913, +0.2309699292,\n"
	"	               -1.2684380046, +2.6097574011, -0.3413193965,\n"
	"	               -0.0041960863, -0.7034186147, +1.7076147010);\n"
	"   return m2 * pow( m1 * c, vec3(1.0/3.0) );"
	"}\n"

	"in vec2 uv;\n"
	"out vec4 fragColour;\n"
	"uniform mediump usampler2D src;\n"
#ifdef ONE_PAL_TEXT
	"uniform highp sampler2D pal;\n"
	"uniform lowp ivec2 palnums;\n"
#else
	"uniform highp sampler2D pal1;\n"
	"uniform highp sampler2D pal2;\n"
#endif
	"uniform lowp float palpos;\n"

	// "float gradientNoise(in vec2 uv) { return fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715)))); }\n"
	"vec3 ScreenSpaceDither( vec2 vScreenPos )\n"
	"{\n"
		// Iestyn's RGB dither (7 asm instructions) from Portal 2 X360, slightly modified for VR
		"vec3 vDither = vec3(dot( vec2( 171.0, 231.0 ), vScreenPos ));\n"
		"vDither.rgb = fract( vDither.rgb / vec3( 103.0, 71.0, 97.0 ) ) - vec3( 0.5, 0.5, 0.5 );\n"
		"return ( vDither.rgb / 255.0 ) * 0.375;\n"
	"}\n"
	"void main() {\n"
	// On OpenGL 4.0 can use gvec4 textureGather(gsampler sampler,vec texCoord, int comp); to get all four values used for interpolation
	"	uint i = texture(src, uv).r;\n" // May want to do texel fetch for interpolation?
	"	mediump ivec2 idx = ivec2(int(i>>8), 0);\n"
	"	float f  = float(i&0xFFu)/256.0;\n"

	
#ifdef ONE_PAL_TEXT
	"	ivec2 idx2 = ivec2( min(idx.x+1, 255), 0);\n"
	"	ivec2 offset1 = ivec2(0, palnums.x);\n"
	"	ivec2 offset2 = ivec2(0, palnums.y);\n"
	"	vec3 col1 = mix(rgb2oklab(texelFetch(pal, idx + offset1, 0).rgb), rgb2oklab(texelFetch(pal, idx2 + offset1, 0).rgb), f ) ;\n"
	"	vec3 col2 = mix(rgb2oklab(texelFetch(pal, idx + offset2, 0).rgb), rgb2oklab(texelFetch(pal, idx2 + offset2, 0).rgb), f ) ;\n"
#else
	"	vec3 col1 = mix(rgb2oklab(texelFetch(pal1, idx, 0).rgb), rgb2oklab(texelFetch(pal1, min(idx+ivec2(1,0), textureSize(pal1,0)-ivec2(1,0)), 0).rgb), f ) ;\n"
	"	vec3 col2 = mix(rgb2oklab(texelFetch(pal2, idx, 0).rgb), rgb2oklab(texelFetch(pal2, min(idx+ivec2(1,0), textureSize(pal2,0)-ivec2(1,0)), 0).rgb), f ) ;\n"
#endif
	"	vec3 out_col = oklab2rgb( mix(col1, col2, palpos) );\n"
	//TODO: adjust based on actual output bit depth, including 10/12 bit
	// "	float dither_noise = (1.0 / 255.0) * gradientNoise(gl_FragCoord.xy) - (0.5 / 255.0);\n"
	"	vec3 dither_noise = ScreenSpaceDither(gl_FragCoord.xy);\n"
	"	fragColour = vec4(out_col + dither_noise, 1.0);\n"
	"}\n";

struct priv_ctx {
	struct glpal_ctx pubctx;

	GLuint prog;
	GLint palpos_loc;

	int numpal;
	int curpal, nextpal, palpos;
	bool pallet_changing;
#ifdef ONE_PAL_TEXT
	GLint  palnums_loc;
	GLuint pal_tex;
#else
	GLuint pal_tex[];
#endif
};

static void start_switch(struct glpal_ctx *ctx, int next)
{
	struct priv_ctx *self = (struct priv_ctx *)ctx;
	if(next<0) return;
	if(self->pallet_changing) return; // haven't finished the last one
	next = next % self->numpal;
	if(next == self->curpal) next = (next+1) % self->numpal;
	self->nextpal = next;
	self->pallet_changing = true;
	// printf("Start pal switch cur %3d, next %3d\n", self->curpal, self->nextpal);
}

static bool step(struct glpal_ctx *ctx, uint8_t step) {
	struct priv_ctx *self = (struct priv_ctx *)ctx;
	if(!self->pallet_changing) return false;
	self->palpos += step;
	if(self->palpos >= 256) {
		self->palpos = 0;
		self->pallet_changing = false;
		self->curpal = self->nextpal;
	}
	// printf("Step %3d, pos %3d, cur %3d, next %3d\n", step, self->palpos, self->curpal, self->nextpal);
	return true;
}

static bool changing(struct glpal_ctx *ctx) {
	struct priv_ctx *self = (struct priv_ctx *)ctx;
	return self->pallet_changing;
}

static void render(struct glpal_ctx *ctx, GLuint draw_tex)
{
	struct priv_ctx *priv = (struct priv_ctx *)ctx;

	glUseProgram(priv->prog);

	glUniform1f(priv->palpos_loc, priv->palpos/256.0f);

#ifdef ONE_PAL_TEXT
	glUniform2i(priv->palnums_loc, priv->curpal, priv->nextpal);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, draw_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, priv->pal_tex);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
#else
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, draw_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, priv->pal_tex[priv->curpal]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, priv->pal_tex[priv->nextpal]);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
#endif

	glUseProgram(0);
}

struct glpal_ctx * pal_init_gles3(GLboolean float_packed_pixels)
{
	GLuint shader_program = glCreateProgram();
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
		return NULL;	
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
		return NULL;	
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
		return NULL;
	}

	struct pal_lst *pals = pallet_get_palettes();
#ifdef ONE_PAL_TEXT
	struct priv_ctx *priv = malloc(sizeof(*priv));
#else
	struct priv_ctx *priv = malloc(sizeof(*priv) + sizeof(*priv->pal_tex)*pals->numpals);
#endif
	priv->pubctx.render = render;
	priv->pubctx.step = step;
	priv->pubctx.start_switch = start_switch;
	priv->pubctx.changing = changing;
	priv->prog = shader_program;
	priv->numpal = pals->numpals;
	priv->curpal = priv->nextpal = 0;
	priv->palpos = 0;

#ifdef ONE_PAL_TEXT
	glUseProgram(priv->prog);
	glUniform1i(glGetUniformLocation(priv->prog, "src"), 0);
	glUniform1i(glGetUniformLocation(priv->prog, "pal"), 1);
	priv->palpos_loc  = glGetUniformLocation(priv->prog, "palpos");
	priv->palnums_loc = glGetUniformLocation(priv->prog, "palnums");
	glUniform1f(priv->palpos_loc, 0.0f);
	glUniform2i(priv->palnums_loc, priv->curpal, priv->nextpal);
	glUseProgram(0);

	glGenTextures(1, &priv->pal_tex);
	glBindTexture(GL_TEXTURE_2D, priv->pal_tex);

	// We're actually uploading as BGRA
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // want to not get edge pixels for height
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 256, priv->numpal, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	for(int i=0; i<pals->numpals; i++) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, i, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, pals->pallets[i]);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
#else
	glUseProgram(priv->prog);
	glUniform1i(glGetUniformLocation(priv->prog, "src"), 0);
	glUniform1i(glGetUniformLocation(priv->prog, "pal1"), 1);
	glUniform1i(glGetUniformLocation(priv->prog, "pal2"), 2);
	priv->palpos_loc = glGetUniformLocation(priv->prog, "palpos");
	glUniform1f(priv->palpos_loc, 0.0f);
	glUseProgram(0);

	glGenTextures(pals->numpals, priv->pal_tex);
	for(int i=0; i<pals->numpals; i++) {
		glBindTexture(GL_TEXTURE_2D, priv->pal_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pals->pallets[i]);
		
		// We're actually uploading as BGRA
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
		
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // want to not get edge pixels for height
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glBindTexture(GL_TEXTURE_2D, 0);
#endif
	return &priv->pubctx;
}
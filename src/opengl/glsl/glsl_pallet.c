
#include "common.h"
#include "pallet.h"
#include "opengl/glmisc.h"
#include "opengl/glpallet.h"

#if 1
static const char *vtx_shader =
	"#version 110\n"
	"attribute vec4 vertex;\n"
	"varying vec2 uv;\n"
	"void main() {\n"
	"	uv = vertex.xy;\n"
	"	gl_Position = vec4(vertex.zw, 0.0f, 1.0f);\n"
	"}";

static const char *pal_frag_mix =
	"#version 110\n"
	FLOAT_PACK_FUNCS
	"varying vec2 uv;\n"
	"uniform sampler2D src;\n"
	"uniform sampler1D pal1;\n"
	"uniform sampler1D pal2;\n"
	"uniform float palpos;\n"
	"void main() {\n"
	"	float idx = decode(texture2D(src, uv));\n"
	"	gl_FragColor = mix(texture1D(pal1, idx), texture1D(pal2, idx), palpos);\n"
	"}";

static const char *pal_frag_shader =
	"#version 110\n"
	"varying vec2 uv;\n"
	"uniform sampler2D src;\n"
	"uniform sampler1D pal1;\n"
	"uniform sampler1D pal2;\n"
	"uniform float palpos;\n"
	"void main() {\n"
	"	float idx = texture2D(src, uv).x;\n"
	"	gl_FragColor = mix(texture1D(pal1, idx), texture1D(pal2, idx), palpos);\n"
	"}";

struct priv_ctx {
	struct glpal_ctx pubctx;

	GLhandleARB prog;
	GLint palpos_loc;

	int numpal;
	int curpal, nextpal, palpos;
	bool pallet_changing;

	GLuint pal_tex[];
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
	return true;
}

static bool changing(struct glpal_ctx *ctx) {
	struct priv_ctx *self = (struct priv_ctx *)ctx;
	return self->pallet_changing;
}

static void render(struct glpal_ctx *ctx, GLuint draw_tex)
{DEBUG_CHECK_GL_ERR;
	static const float verts[] = {
		0, 0 , -1, -1,
		1, 0 ,  1, -1,
		0, 1 , -1,  1,
		1, 1 ,  1,  1
	};
	struct priv_ctx *priv = (struct priv_ctx *)ctx;

	glPushAttrib(GL_TEXTURE_BIT);
	glUseProgramObjectARB(priv->prog);

	glUniform1fARB(priv->palpos_loc, priv->palpos/255.0f);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, draw_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, priv->pal_tex[priv->curpal]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, priv->pal_tex[priv->nextpal]);

	glEnableVertexAttribArrayARB(0);
	glVertexAttribPointerARB(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, verts);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glUseProgramObjectARB(0);
	glDisableVertexAttribArrayARB(0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glPopAttrib();
	DEBUG_CHECK_GL_ERR;
}

struct glpal_ctx * pal_init_glsl(GLboolean float_packed_pixels)
{CHECK_GL_ERR;
	printf("Compiling pallet shader:\n");
	GLint prog = 0;
	if(!float_packed_pixels)
		prog = compile_program(vtx_shader, pal_frag_shader);
	else
		prog = compile_program(vtx_shader, pal_frag_mix);

	if(!prog) return NULL;

	struct pal_lst *pals = pallet_get_palettes();
	struct priv_ctx *priv = malloc(sizeof(*priv) + sizeof(*priv->pal_tex)*pals->numpals);
	priv->pubctx.render = render;
	priv->pubctx.step = step;
	priv->pubctx.start_switch = start_switch;
	priv->pubctx.changing = changing;
	priv->prog = prog;
	priv->numpal = pals->numpals;
	priv->curpal = priv->nextpal = 0;
	priv->palpos = 0;

	glUseProgramObjectARB(prog);
	glUniform1iARB(glGetUniformLocationARB(prog, "src"), 0);
	glUniform1iARB(glGetUniformLocationARB(prog, "pal1"), 1);
	glUniform1iARB(glGetUniformLocationARB(prog, "pal2"), 2);
	priv->palpos_loc = glGetUniformLocationARB(prog, "palpos");
	glUniform1fARB(priv->palpos_loc, 0.0f);
	glUseProgramObjectARB(0);
	printf("Pallet shader compiled\n");

	glGenTextures(pals->numpals, priv->pal_tex);

	glPushAttrib(GL_TEXTURE_BIT);
	for(int i=0; i<pals->numpals; i++) {
		glBindTexture(GL_TEXTURE_1D, priv->pal_tex[i]);
		glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, pals->pallets[i]);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glPopAttrib(); CHECK_GL_ERR;
	free(pals);
	return (struct glpal_ctx *)priv;
}
#else

static const char *vtx_shader =
	"#version 110\n"
	"attribute vec4 vertex;\n"
	"varying vec2 uv;\n"
	"void main() {\n"
	"	uv = vertex.xy;\n"
	"	gl_Position = vec4(vertex.zw, 0.0f, 1.0f);\n"
	"}";

static const char *pal_frag_mix =
	"#version 110\n"
	FLOAT_PACK_FUNCS
	"varying vec2 uv;\n"
	"uniform sampler2D src;\n"
	"uniform sampler1D pal;\n"
	"void main() {\n"
	"	gl_FragColor = texture1D(pal, decode(texture2D(src, uv)));\n"
	"}";

static const char *pal_frag_shader =
	"#version 110\n"
	"varying vec2 uv;\n"
	"uniform sampler2D src;\n"
	"uniform sampler1D pal;\n"
	"void main() {\n"
	"	gl_FragColor = texture1D(pal, texture2D(src, uv).x);\n"
	"}";

struct priv_ctx {
	struct glpal_ctx pubctx;
	struct pal_ctx *pal;

	int cnt;

	GLuint prog;
	GLuint tex[2];
};

static int pal_step(struct glpal_ctx *ctx, int step) {
	struct priv_ctx *priv = (struct priv_ctx *)ctx;
	return pal_ctx_step(priv->pal, step);
}

static void start_switch(struct glpal_ctx *ctx, int next) {
	struct priv_ctx *priv = (struct priv_ctx *)ctx;
	pal_ctx_start_switch(priv->pal, next);
}

static bool changing(struct glpal_ctx *ctx) {
	struct priv_ctx *priv = (struct priv_ctx *)ctx;
	return pal_ctx_changing(priv->pal);
}

static void render(struct glpal_ctx *ctx, GLuint draw_tex)
{DEBUG_CHECK_GL_ERR;
	static const float verts[] = {
		0, 0 , -1, -1,
		1, 0 ,  1, -1,
		0, 1 , -1,  1,
		1, 1 ,  1,  1
	};
	struct priv_ctx *priv = (struct priv_ctx *)ctx;

	

	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glUseProgramObjectARB(priv->prog);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, draw_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, priv->tex[priv->cnt]);

	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, pal_ctx_get_active(priv->pal));

	priv->cnt = (priv->cnt+1)%2;

	glBindTexture(GL_TEXTURE_1D, priv->tex[priv->cnt]);

	glEnableVertexAttribArrayARB(0);
	glVertexAttribPointerARB(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, verts);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glUseProgramObjectARB(0);
	glDisableVertexAttribArrayARB(0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glPopClientAttrib();

	DEBUG_CHECK_GL_ERR;
}

struct glpal_ctx *pal_init_glsl(GLboolean float_packed_pixels)
{CHECK_GL_ERR;
	printf("Compiling pallet shader:\n");
	GLint prog = 0;
	if(!float_packed_pixels)
		prog = compile_program(vtx_shader, pal_frag_shader);
	else
		prog = compile_program(vtx_shader, pal_frag_mix);

	if(!prog) return NULL;

	glUseProgramObjectARB(prog);
	glUniform1iARB(glGetUniformLocationARB(prog, "src"), 0);
	glUniform1iARB(glGetUniformLocationARB(prog, "pal"), 1);
	glBindAttribLocationARB(prog, 0, "vertex");
	glUseProgramObjectARB(0);
	printf("Pallet shader compiled\n");

	struct priv_ctx *priv = malloc(sizeof(*priv));
	priv->pubctx.render = render;
	priv->pubctx.step = pal_step;
	priv->pubctx.start_switch = start_switch;
	priv->pubctx.changing = changing;
	priv->pal = pal_ctx_new(0);
	priv->prog = prog;
	priv->cnt = 0;

	glGenTextures(2, &priv->tex);
	glBindTexture(GL_TEXTURE_1D, priv->tex[0]);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, pal_ctx_get_active(priv->pal));
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_1D, 0);

	glBindTexture(GL_TEXTURE_1D, priv->tex[1]);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, pal_ctx_get_active(priv->pal));
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_1D, 0);

	CHECK_GL_ERR;
	return (struct glpal_ctx *)priv;
}
#endif


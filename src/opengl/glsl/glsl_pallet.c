
#include "common.h"
#include "pallet.h"
#include "opengl/glmisc.h"
#include "opengl/glpallet.h"

#if 0
static const char *vtx_shader =
	"#version 110\n"
	"varying vec2 uv;\n"
	"void main() {\n"
	"	uv = gl_MultiTexCoord0.xy;\n"
	"	gl_Position = gl_Vertex;\n"
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
	next = next % self->pals->numpals;
	if(next == self->curpal) next = (next+1) % self->pals->numpals;
	self->nextpal = next;
	self->pallet_changing = true;
}

static bool step(struct glpal_ctx *ctx, uint8_t step) {
	struct priv_ctx *self = (struct priv_ctx *)ctx;
	if(!self->pallet_changing) return false;
	self->palpos += step;
	if(self->palpos >=256) {
		self->pallet_changing = self->palpos = 0;
		self->curpal = self->nextpal;
	}
	return true;
}

static void render(struct glpal_ctx *ctx, GLuint draw_tex)
{DEBUG_CHECK_GL_ERR;
	struct priv_ctx *priv = (struct priv_ctx *)ctx;
	
	glPushAttrib(GL_TEXTURE_BIT);
	glUseProgramObjectARB(priv->prog);
	
	glUniform1fARB(priv->palpos_loc, priv->palpos/255.0f);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, draw_tex);
	glActiveTexture(GL_TEXTURE1;
	glBindTexture(GL_TEXTURE_1D, priv->pal_tex[priv->curpal]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, priv->pal_tex[priv->nextpal]);
	
	glBegin(GL_TRIANGLES); //TODO: scale for aspect ration correction
		glTexCoord2d( 0, 0); glVertex2d(-1, -1);
		glTexCoord2d( 1, 0); glVertex2d( 1, -1);
		glTexCoord2d( 0, 1); glVertex2d(-1,  1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
	glEnd();
	glUseProgramObjectARB(0);
	glPopAttrib();
	DEBUG_CHECK_GL_ERR;
}

static struct glpal_ctx * pal_init_glsl(GLboolean float_packed_pixels)
{CHECK_GL_ERR;
	printf("Compiling pallet shader:\n");
	GLint prog = 0;
	if(!float_packed_pixels)
		prog = compile_program(vtx_shader, pal_frag_shader);
	else
		prog = compile_program(vtx_shader, pal_frag_mix);

	if(!priv->prog) return NULL;
	
	struct pal_lst *pals = pallet_get_pallets();
	struct priv_ctx *ctx = malloc(sizeof(*ctx) + sizeof(*ctx->pal_tex)*pals->numpal);
	priv->render = render;
	priv->prog = prog;
	priv->num_pal = pals->numpal;
	priv->curpal = priv->nextpal = 0;
	priv->palpos = 0;

	glUseProgramObjectARB(prog);
	glUniform1iARB(glGetUniformLocationARB(prog, "src"), 0);
	glUniform1iARB(glGetUniformLocationARB(prog, "pal1"), 1);
	glUniform1iARB(glGetUniformLocationARB(prog, "pal2"), 2);
	priv->palpos_loc = glGetUniformLocationARB(prog, "pal2");
	glUniform1fARB(priv->palpos_loc, 0.0f);
	glUseProgramObjectARB(0);
	printf("Pallet shader compiled\n");

	glGenTextures(num_pal, priv->pal_tex);
	
	glPushAttrib(GL_TEXTURE_BIT);
	for(int i=0; i<pals->numpal; i++) {
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

	GLuint prog;
	GLuint texture;
};

static void render(struct glpal_ctx *ctx, GLuint draw_tex)
{DEBUG_CHECK_GL_ERR;
	static const float verts[] = {
		0, 0 , -1, -1,
		1, 0 ,  1, -1,
		0, 1 , -1,  1,
		1, 1 ,  1,  1
	};
	struct priv_ctx *priv = (struct priv_ctx *)ctx;

	glUseProgram(priv->prog);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, draw_tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, priv->texture);
	
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, verts);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, pal_ctx_get_active(ctx->pal));
	
	glUseProgram(0);
	glDisableVertexAttribArray(0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	DEBUG_CHECK_GL_ERR;
}

struct glpal_ctx *pal_init_glsl(GLboolean float_packed_pixels)
{CHECK_GL_ERR;
	pallet_init(0);
	printf("Compiling pallet shader:\n");
	GLint prog = 0;
	if(!float_packed_pixels)
		prog = compile_program(vtx_shader, pal_frag_shader);
	else
		prog = compile_program(vtx_shader, pal_frag_mix);

	if(!prog) return NULL;

	glUseProgram(prog);
	glUniform1i(glGetUniformLocation(prog, "src"), 0);
	glUniform1i(glGetUniformLocation(prog, "pal"), 1);
	glBindAttribLocation(prog, 0, "vertex");
	glUseProgram(0);
	printf("Pallet shader compiled\n");
	
	struct priv_ctx *priv = malloc(sizeof(*priv));
	priv->pubctx.render = render;
	priv->pubctx.pal = pal_ctx_new();
	priv->prog = prog;

	glGenTextures(1, &priv->texture);
	glBindTexture(GL_TEXTURE_1D, priv->texture);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, pal_ctx_get_active(priv->pubctx.pal));
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_1D, 0);
	
	CHECK_GL_ERR;
	return (struct glpal_ctx *)priv;
}
#endif


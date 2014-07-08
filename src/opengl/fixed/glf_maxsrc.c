#include "common.h"
#include "audio/audio.h"
#include "opengl/glmisc.h"
#include "opengl/glmaxsrc.h"
#include "opengl/glscope.h"
#include "glmap.h"
#include "getsamp.h"

struct priv_ctx {
	struct glmaxsrc_ctx pubctx;
	struct glscope_ctx *glscope;
	float tx, ty, tz;
	uint32_t lastupdate;
	Map *map;
};

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

GEN_MAP_CB(fixed_map_cb, bg_vtx);

static void update(struct glmaxsrc_ctx *ctx, const float *audio, int audiolen);

struct glmaxsrc_ctx *maxsrc_new_fixed(int width, int height)
{
	int samp = MIN(MIN(width/2, height/2), 128);
	printf("maxsrc using %i points\n", samp);	
	struct priv_ctx *priv = calloc(sizeof(*priv), 1);
	priv->pubctx.update = update;
	priv->pubctx.offscr = offscr_new(width, height, true, true);
	priv->glscope = gl_scope_init(width, height, samp, true);
	priv->map = map_new(24, fixed_map_cb);
	
	return &priv->pubctx;
}

static void update(struct glmaxsrc_ctx *ctx, const float *audio, int audiolen)
{DEBUG_CHECK_GL_ERR;
	struct priv_ctx *priv = (struct priv_ctx *)ctx;
	const uint32_t now = get_ticks();
	const float dt = (now - priv->lastupdate)*24/1000.0f;
	priv->lastupdate = now;

	float cx=cosf(priv->tx), cy=cosf(priv->ty), cz=cosf(priv->tz);
	float sx=sinf(priv->tx), sy=sinf(priv->ty), sz=sinf(priv->tz);

	float R[3][3] = {
		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},
		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},
		{cx*sy         ,    -sx,  cy*cx}
	};
	
	GLint src_tex = offscr_start_render(ctx->offscr);
	
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glClearColor(1.0f/256, 1.0f/256,1.0f/256, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0,0,0,1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
	glBlendEquation(GL_FUNC_SUBTRACT);
	glBlendColor(0, 0, 0, 63.0f/64);
	DEBUG_CHECK_GL_ERR;

	glBindTexture(GL_TEXTURE_2D, src_tex); DEBUG_CHECK_GL_ERR;
	map_render(priv->map, R);
	glPopAttrib();
	DEBUG_CHECK_GL_ERR;
	
	render_scope(priv->glscope, R, audio, audiolen);
	offscr_finish_render(ctx->offscr);
	
	priv->tx+=0.02f*dt; priv->ty+=0.01f*dt; priv->tz-=0.003f*dt;

	DEBUG_CHECK_GL_ERR;
}


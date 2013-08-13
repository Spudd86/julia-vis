#include "common.h"
#include "points.h"
#include "opengl/glmisc.h"
#include "opengl/glfract.h"
#include "glmap.h"

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

struct fixed_ctx {
	struct glfract_ctx pubctx;
	GLboolean rational_julia;
	Map *map;
};

static void render(struct glfract_ctx *ctx, const struct point_data *pd, GLuint maxsrc_tex)
{
	struct fixed_ctx *priv = (struct fixed_ctx *)ctx;	
	GLint src_tex = offscr_start_render(ctx->offscr);
	
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glClearColor(1.0f/256, 1.0f/256,1.0f/256, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0,0,0,1);

	glEnable(GL_BLEND);
	glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
	glBlendEquation(GL_FUNC_SUBTRACT);
	glBlendColor(0, 0, 0, 63.0f/64);
	glBindTexture(GL_TEXTURE_2D, src_tex);
	map_render(priv->map, pd);

	glBlendEquation(GL_MAX);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, maxsrc_tex);
	glBegin(GL_TRIANGLE_STRIP);
		glTexCoord2d( 0, 0); glVertex2d(-1, -1);
		glTexCoord2d( 1, 0); glVertex2d( 1, -1);
		glTexCoord2d( 0, 1); glVertex2d(-1,  1);
		glTexCoord2d( 1, 1); glVertex2d( 1,  1);
	glEnd();
	glPopAttrib();
	
	offscr_finish_render(ctx->offscr);
}

struct glfract_ctx *fractal_fixed_init(const opt_data *opts, int width, int height)
{
	struct fixed_ctx *ctx = malloc(sizeof(*ctx));
	ctx->pubctx.render = render;
	ctx->rational_julia = opts->rational_julia;
	
	if(!ctx->rational_julia)
		ctx->map = map_new(97, map_cb);
	else
		ctx->map = map_new(127, rat_map_cb);
	
	ctx->pubctx.offscr = offscr_new(width, height, true, true);
	
	return (struct glfract_ctx *)ctx;
}


#include "common.h"
#include "glmisc.h"
#include "audio/audio.h"
#include "glmaxsrc.h"
#include "glscope.h"

static struct glscope_ctx *glscope = NULL;
static int scr_w = 0, scr_h = 0;

void init_gl(const opt_data *opt_data, int width, int height)
{
	scr_w = width; scr_h = height;

	if(!ogl_LoadFunctions()) {
		fprintf(stderr, "ERROR: Failed to load GL extensions\n");
		exit(EXIT_FAILURE);
	}
	CHECK_GL_ERR;
	if(!(ogl_GetMajorVersion() > 1 || ogl_GetMinorVersion() >= 4)) {
		fprintf(stderr, "ERROR: Your OpenGL Implementation is too old\n");
		exit(EXIT_FAILURE);
	}

	setup_viewport(scr_w, scr_h); CHECK_GL_ERR;
	glClear(GL_COLOR_BUFFER_BIT); CHECK_GL_ERR;
	glEnable(GL_TEXTURE_2D); CHECK_GL_ERR;
	glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST); CHECK_GL_ERR;
	
	glscope = gl_scope_init(width, height, 2, false);CHECK_GL_ERR;
}

static int frm = 0;
static float tx=0, ty=0, tz=0;
void render_frame(GLboolean debug_maxsrc, GLboolean debug_pal, GLboolean show_mandel, GLboolean show_fps_hist)
{
/*	float cx=cosf(tx), cy=cosf(ty), cz=cosf(tz);*/
/*	float sx=sinf(tx), sy=sinf(ty), sz=sinf(tz);*/
/*	float R[3][3] = {*/
/*		{cz*cy-sz*sx*sy, -sz*cx, -sy*cz-cy*sz*sx},*/
/*		{sz*cy+cz*sx*sy,  cz*cx, -sy*sz+cy*cz*sx},*/
/*		{cx*sy         ,    -sx,  cy*cx}*/
/*	};*/
	float R[3][3] = {
		{1, 0, 0 },
		{0, 1, 0 },
		{0, 0, 1 }
	};
	
	glClear(GL_COLOR_BUFFER_BIT);	
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glPushMatrix();
	glScalef(2, 2, 1);
	audio_data ad; audio_get_samples(&ad);
	render_scope(glscope, R, ad.data, ad.len); CHECK_GL_ERR;
	audio_finish_samples();
	glPopMatrix();
	glPopClientAttrib();
	glPopAttrib();
	
	glRasterPos2f(-1,1 - 20.0f/(scr_h*0.5f));
	char buf[128];
	sprintf(buf,"Test %i", frm);
	draw_string(buf);
	swap_buffers(); CHECK_GL_ERR;
	frm++;
}

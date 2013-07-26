#include "common.h"
#include "glmisc.h"
#include "audio/audio.h"
#include "glmaxsrc.h"
#include "glscope.h"

static int scr_w = 0, scr_h = 0;

void init_gl(const opt_data *opt_data, int width, int height)
{ CHECK_GL_ERR;
	scr_w = width; scr_h = height;

	setup_viewport(scr_w, scr_h); CHECK_GL_ERR;
	glClear(GL_COLOR_BUFFER_BIT); CHECK_GL_ERR;
	glEnable(GL_TEXTURE_2D); CHECK_GL_ERR;
	glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST); CHECK_GL_ERR;
	
	if(!GLEE_EXT_blend_minmax) {
		printf("missing required gl extension EXT_blend_minmax!\n");
		exit(1);
	}
	
	gl_scope_init(width, height, 8, false);CHECK_GL_ERR;
	gl_maxsrc_init(width, height, false, false); CHECK_GL_ERR;
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
/*	setup_viewport(scr_w, scr_h);*/
	
/*	glColor4f(0.0f, 1.0f, 1.0f, 1.0f);*/
/*	glBegin(GL_TRIANGLE_STRIP);*/
/*		glTexCoord2d( 0, 0); glVertex2d(-1, -1);*/
/*		glTexCoord2d( 1, 0); glVertex2d( 1, -1);*/
/*		glTexCoord2d( 0, 1); glVertex2d(-1,  1);*/
/*		glTexCoord2d( 1, 1); glVertex2d( 1,  1);*/
/*	glEnd();*/
/*	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);*/
/*	*/
/*	gl_maxsrc_update();*/
/*	gl_maxsrc_get();*/
/*	glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());*/
/*	glBegin(GL_TRIANGLE_STRIP);*/
/*		glTexCoord2d(0,0); glVertex2d(-1,  0);*/
/*		glTexCoord2d(1,0); glVertex2d( 0,  0);*/
/*		glTexCoord2d(0,1); glVertex2d(-1,  1);*/
/*		glTexCoord2d(1,1); glVertex2d( 0,  1);*/
/*	glEnd();*/
	
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	glPushMatrix();
	glScalef(2, 2, 1);
	render_scope(R);
	glPopMatrix();
	glPopClientAttrib();
	glPopAttrib();
	
	glRasterPos2f(-1,1 - 20.0f/(scr_h*0.5f));
	char buf[128];
	sprintf(buf,"Test %i", frm);
	draw_string(buf);
	glFlush();
	swap_buffers(); CHECK_GL_ERR;
	frm++;
}

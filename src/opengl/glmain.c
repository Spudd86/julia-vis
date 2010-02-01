/**
 * main.c
 *
 */

#define GL_GLEXT_PROTOTYPES

#include "common.h"

#include <GL/glew.h>
#include <GL/glu.h>
#include <GL/gl.h>

#include "points.h"
#include "sdl-misc.h"
#include "glmisc.h"
#include "glmaxsrc.h"
#include "pallet.h"
#include "audio/audio.h"
#include "glpallet.h"

void init_mandel();
void render_mandel(struct point_data *pd);

static int make_pow2(int x) {
	int t = x, n = 0;
	while(t != 1) { t = t>>1; n++; }
	if(x == 1<<n ) return x;
	else return 1<<(n+1);
}

#define FPS_HIST_LEN 32

static GLboolean packed_intesity_pixels = GL_FALSE;
static int im_w = -1, im_h = -1;
static int scr_w = -1, scr_h = -1;
static struct point_data *pd = NULL;
static const opt_data *opts = NULL;
static uint32_t frametimes[FPS_HIST_LEN];
static uint32_t tick0 = 0;

void fractal_init(opt_data *opts, int width, int height, GLboolean force_fixed, GLboolean packed_intesity_pixels);
void render_fractal(struct point_data *pd);
GLint fract_get_tex(void);

void init_gl(const opt_data *opt_data, int width, int height)
{
	opts = opt_data;
	GLboolean force_fixed = opts->gl_opts != NULL && strstr(opts->gl_opts, "fixed") != NULL;
	GLboolean res_boost = opts->gl_opts != NULL && strstr(opts->gl_opts, "rboost") != NULL;
	packed_intesity_pixels = opts->gl_opts != NULL && strstr(opts->gl_opts, "pintens") != NULL;
	scr_w = width; scr_h = height;
	im_w = IMAX(make_pow2(IMAX(scr_w, scr_h)), 128)<<res_boost; im_h = im_w; //TODO: nearest power of two
	printf("Using internel resolution of %ix%i\n\n", im_h, im_w);

	glewInit();
	setup_viewport(scr_w, scr_h);
	glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, GL_FASTEST);
	glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

	glClear(GL_COLOR_BUFFER_BIT);
	glRasterPos2f(-1,1 - 20.0f/(scr_h*0.5f));
	draw_string("Loading... "); swap_buffers();

	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	if(GLEW_ARB_shading_language_100) printf("GL_SL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("\n\n");

	if(!GLEW_EXT_blend_minmax) {
		printf("missing required gl extension EXT_blend_minmax!\n");
		exit(1);
	}
	if(!GLEW_EXT_framebuffer_object) {
		printf("missing required gl extension ARB_framebuffer_object!\n");
		exit(1);
	}

	if(!GLEW_ARB_shading_language_100)
		printf("No GLSL using all fixed function! (might be slow)\n");
	else if(!glewGetExtension("GL_ARB_shading_language_120"))
		printf("GLSL support not good enough. Using (mostly) fixed function\n");

	if(!GLEW_ARB_shading_language_100 && !GLEW_ARB_pixel_buffer_object)
		printf("Missing GLSL and pixel buffer objects, WILL be slow!\n");

	if(force_fixed)
		printf("Fixed function code forced\n");

	glEnable(GL_TEXTURE_2D);

	init_mandel();
	audio_init(opts);

	draw_string("Done\n"); swap_buffers();
	if(glewGetExtension("GL_ARB_shading_language_120") && !force_fixed) {
		draw_string("Compiling Shaders..."); swap_buffers();
	}

	fractal_init(opts, im_w, im_h, force_fixed, packed_intesity_pixels); CHECK_GL_ERR;
	gl_maxsrc_init(IMAX(im_w/2, 128), IMAX(im_h/2, 128), packed_intesity_pixels, force_fixed); CHECK_GL_ERR;
	pal_init(im_w, im_h, packed_intesity_pixels, force_fixed); CHECK_GL_ERR;
	pd = new_point_data(opts->rational_julia?4:2);

	memset(frametimes, 0, sizeof(frametimes));
	tick0 = get_ticks();
}

void render_frame(GLboolean debug_maxsrc, GLboolean debug_pal, GLboolean show_mandel, GLboolean show_fps_hist)
{
	static uint32_t totframetime = 0;
	static int cnt = 0;
	static uint32_t maxfrms = 0;
	static uint32_t last_beat_time = 0, lastpalstep = 0, fps_oldtime = 0;
	static int beats = 0;

	uint32_t now = get_ticks();

	// rate limit our maxsrc updates, but run at full frame rate if we're close the opts.maxsrc_rate to avoid choppyness
	if(tick0-now + (maxfrms*1000)/opts->maxsrc_rate > + 1000/opts->maxsrc_rate //) {
			|| (totframetime + 10*FPS_HIST_LEN > FPS_HIST_LEN*1000/opts->maxsrc_rate ) ) {
		gl_maxsrc_update();
		maxfrms++;
	}

	render_fractal(pd);

	if(!debug_pal || !debug_maxsrc || !show_mandel) {
		pal_render(fract_get_tex());
	} else {
		glPushAttrib(GL_VIEWPORT_BIT);
		setup_viewport(scr_w/2, scr_h/2);
		pal_render(fract_get_tex());
		glPopAttrib();
	}

	if(show_mandel) render_mandel(pd); //TODO: enable click to change target

	//TODO: figure out what attrib to push to save color
	if(debug_pal || debug_maxsrc) { glPushAttrib(GL_TEXTURE_BIT); if(packed_intesity_pixels) glColor3f(1.0f, 0.0f, 0.0f); }
	if(debug_pal) {
		glBindTexture(GL_TEXTURE_2D, fract_get_tex());
		glBegin(GL_QUADS);
			glTexCoord2d(0,0); glVertex2d( 0, -1);
			glTexCoord2d(1,0); glVertex2d( 1, -1);
			glTexCoord2d(1,1); glVertex2d( 1,  0);
			glTexCoord2d(0,1); glVertex2d( 0,  0);
		glEnd();
	}
	if(debug_maxsrc) {
		glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
		glBegin(GL_QUADS);
			glTexCoord2d(0,0); glVertex2d(-1,  0);
			glTexCoord2d(1,0); glVertex2d( 0,  0);
			glTexCoord2d(1,1); glVertex2d( 0,  1);
			glTexCoord2d(0,1); glVertex2d(-1,  1);
		glEnd();
	}
	if(debug_pal || debug_maxsrc) { glPopAttrib(); if(packed_intesity_pixels) glColor3f(1.0f, 1.0f, 1.0f); }


	if(show_fps_hist) {
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glScalef(0.25, 0.25, 1);
		glTranslatef(-3, 3, 0);
		glBegin(GL_LINES);
		glVertex2f(-1, 0); glVertex2f(0, 0);
		glEnd();
		glColor3f(0.0f, 1.0f, 0.0f);
		glBegin(GL_LINES);
		for(int i=0; i<FPS_HIST_LEN-1; ) {
			int idx = (i + cnt)%FPS_HIST_LEN;
			glVertex2f(-1 + ((float)i)/(FPS_HIST_LEN-1),  4*frametimes[idx]/(float)totframetime);
			i++;idx = (i + cnt)%FPS_HIST_LEN;
			glVertex2f(-1 + ((float)i)/(FPS_HIST_LEN-1),  4*frametimes[idx]/(float)totframetime);
		}
		glEnd();
		glColor3f(1.0f, 1.0f, 1.0f);
		glPopMatrix();
		char buf[64];
		sprintf(buf,"%6.1f FPS %6.1f", FPS_HIST_LEN*1000.0f/totframetime, maxfrms*1000.0f/(now-tick0));
		glRasterPos2f(-1,1 - 20.0f/(scr_h*0.5f));
		draw_string(buf);

	}
	swap_buffers(); CHECK_GL_ERR;

	now = get_ticks();
	if(now - lastpalstep >= 2048/256 && get_pallet_changing()) { // want pallet switch to take ~2 seconds
		if(pallet_step(IMIN((now - lastpalstep)*256/2048, 32)))
			pal_pallet_changed();
		lastpalstep = now;
	}
	int newbeat = beat_get_count();
	if(newbeat != beats) {
		pallet_start_switch(newbeat);
	}
	if(newbeat != beats && now - last_beat_time > 1000) {
		last_beat_time = now;
		update_points(pd, (now - tick0), 1);
	} else update_points(pd, (now - tick0), 0);
	beats = newbeat;

	now = get_ticks();
	int delay =  (tick0 + cnt*1000/opts->draw_rate) - now;
	if(delay > 0) dodelay(delay);
	now = get_ticks();
	totframetime -= frametimes[cnt%FPS_HIST_LEN];
	totframetime += (frametimes[cnt%FPS_HIST_LEN] = now - fps_oldtime);
	fps_oldtime = now;

	cnt++;
}

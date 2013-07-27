/**
 * main.c
 *
 */

#include "common.h"
#include "points.h"
#include "glmisc.h"
#include "glmaxsrc.h"
#include "pallet.h"
#include "audio/audio.h"
#include "glpallet.h"
#include "glfract.h"

/* TODO:
 *  - rewrite pallet handling so that in GLSL mode we can just let it look
 *     after switching/position and skip the mixing work and active pallet 
 */

void init_mandel();
void render_mandel(struct point_data *pd);

static int make_pow2(int x) {
	int t = x, n = 0;
	while(t != 1) { t = t>>1; n++; }
	if(x == 1<<n ) return x;
	else return 1<<(n+1);
}

#define FPS_HIST_LEN 64

static GLboolean packed_intesity_pixels = GL_FALSE;
static int im_w = 0, im_h = 0;
static int scr_w = 0, scr_h = 0;
static struct point_data *pd = NULL;
static const opt_data *opts = NULL;
static int totframetime = 0;
static int frametimes[FPS_HIST_LEN];
static int totworktime = 0;
static int worktimes[FPS_HIST_LEN];

static uint64_t tick0 = 0;

static struct glfract_ctx *glfract = NULL;
static struct glpal_ctx *glpal = NULL;

bool check_res(int w, int h) {
	GLint width = 0;
	glTexImage2D(GL_PROXY_TEXTURE_2D_EXT, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D_EXT, 0, GL_TEXTURE_WIDTH, &width);
	CHECK_GL_ERR; return width != 0;
}

void draw_tex_quad(float sc, float xo, float yo)
{
	float verts[] = {
		0, 0 , -1*sc+xo, -1*sc+yo,
		1, 0 ,  1*sc+xo, -1*sc+yo,
		0, 1 , -1*sc+xo,  1*sc+yo,
		1, 1 ,  1*sc+xo,  1*sc+yo
	};
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(float)*4, verts);
	glVertexPointer(2, GL_FLOAT, sizeof(float)*4, verts + 2);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/*static void APIENTRY gl_debug_callback(enum source,*/
/*                                              enum type,*/
/*                                              uint id,*/
/*                                              enum severity,*/
/*                                              sizei length,*/
/*                                              const char* message,*/
/*                                              const void* userParam)*/
/*{*/
/*	fprintf(stderr, "%s: In function '%s':\n%s:%d: Warning: %s\n",*/
/*	        __FILE__, __func__, __FILE__, __LINE__, gluErrorString(id));*/
/*}*/

void init_gl(const opt_data *opt_data, int width, int height)
{ CHECK_GL_ERR;
	opts = opt_data;
	GLboolean force_fixed = opts->gl_opts != NULL && strstr(opts->gl_opts, "fixed") != NULL;
	GLboolean res_boost = opts->gl_opts != NULL && strstr(opts->gl_opts, "rboost") != NULL;
	packed_intesity_pixels = opts->gl_opts != NULL && strstr(opts->gl_opts, "pintens") != NULL;
	scr_w = width; scr_h = height;
	if(opts->fullscreen) im_w = scr_w, im_h=scr_h;
	else im_w = IMAX(make_pow2(IMAX(scr_w, scr_h)), 128)<<res_boost; im_h = im_w;
	while(!check_res(im_w, im_h)) { // shrink textures until they work
		printf(" %ix%i Too big! Shrink texture\n", im_h, im_w);
		im_w = im_w/2;
		im_h = im_h/2;
	}

	printf("Using internel resolution of %ix%i\n\n", im_h, im_w);

	CHECK_GL_ERR;
	setup_viewport(scr_w, scr_h); CHECK_GL_ERR;
	//glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, GL_FASTEST); CHECK_GL_ERR;
	glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST); CHECK_GL_ERR;
	//glEnable(GL_LINE_SMOOTH); CHECK_GL_ERR;

	glClear(GL_COLOR_BUFFER_BIT); CHECK_GL_ERR;
	glRasterPos2f(-1,1 - 20.0f/(scr_h*0.5f));
	//draw_string("Loading... "); swap_buffers(); CHECK_GL_ERR;

	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	if(GLEE_ARB_shading_language_100) printf("GL_SL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
	printf("\n\n");
	
/*	if(GLEE_ARB_debug_output) {*/
/*		glDebugMessageCallbackARB(gl_debug_callback, NULL);*/
/*		glEnable(GL_DEBUG_OUTPUT);*/
/*		printf("Have ARB_debug_output: registered callback\n");*/
/*	} */

	if(!GLEE_EXT_blend_minmax) {
		printf("missing required gl extension EXT_blend_minmax!\n");
		exit(1);
	}
	if(!GLEE_EXT_framebuffer_object && !GLEE_ARB_framebuffer_object) {
		printf("missing required gl extension EXT_framebuffer_object!\n");
		exit(1);
	}

	if(!GLEE_ARB_shading_language_100)
		printf("No GLSL using all fixed function! (might be slow)\n");

	if(!GLEE_ARB_shading_language_100 && !GLEE_ARB_pixel_buffer_object)
		printf("Missing GLSL and no pixel buffer objects, WILL be slow!\n");

	if(force_fixed) {
		printf("Fixed function code forced\n");
		packed_intesity_pixels = GL_FALSE;
	}
	CHECK_GL_ERR;

	if(packed_intesity_pixels) printf("Packed intensity enabled\n");

	glEnable(GL_TEXTURE_2D); CHECK_GL_ERR;

	init_mandel(); CHECK_GL_ERR;

	if(!force_fixed) glfract = fractal_glsl_init(opts, im_w, im_h, packed_intesity_pixels); 
	if(!glfract) glfract = fractal_fixed_init(opts, im_w, im_h);
	CHECK_GL_ERR;
	
	gl_maxsrc_init(IMAX(im_w>>res_boost, 128), IMAX(im_h>>res_boost, 128), packed_intesity_pixels, force_fixed); CHECK_GL_ERR;
	
	//pal_init(im_w, im_h, packed_intesity_pixels, force_fixed); CHECK_GL_ERR;
	if(!force_fixed) glpal = pal_init_glsl(packed_intesity_pixels);
	if(!glpal) glpal = pal_init_fixed(im_w, im_h);
	CHECK_GL_ERR;
	
	pd = new_point_data(opts->rational_julia?4:2);

	memset(frametimes, 0, sizeof(frametimes));
	totframetime = frametimes[0] = MIN(10000000/opts->draw_rate, 1);
	memset(worktimes, 0, sizeof(worktimes));
	totworktime = worktimes[0] = MIN(10000000/opts->draw_rate, 1);
	tick0 = uget_ticks();
}

void render_frame(GLboolean debug_maxsrc, GLboolean debug_pal, GLboolean show_mandel, GLboolean show_fps_hist)
{
	static int cnt = 0;
	static uint32_t maxfrms = 0;
	static uint32_t last_beat_time = 0, lastpalstep = 0, fps_oldtime = 0;
	static int beats = 0;
	static uint64_t now = 0, workstart = 0;
	
	//TODO: move this up to top
	workstart = now = uget_ticks();
	int delay =  (tick0 + cnt*INT64_C(1000000)/opts->draw_rate) - now;
	if(delay > 0) { udodelay(delay); now = uget_ticks(); }
	

	// rate limit our maxsrc updates, but run at full frame rate if we're close the opts.maxsrc_rate to avoid choppyness
	if((tick0-now)*opts->maxsrc_rate + (maxfrms*INT64_C(1000000)) > INT64_C(1000000) ) {
//			|| (totframetime + 10*FPS_HIST_LEN > FPS_HIST_LEN*1000/opts->maxsrc_rate ) ) {
		audio_data ad; audio_get_samples(&ad);
		gl_maxsrc_update(ad.data, ad.len);
		audio_finish_samples();
		maxfrms++;
	}

	render_fractal(glfract, pd);

	if(!debug_pal || !debug_maxsrc || !show_mandel) {
		gl_pal_render(glpal, fract_get_tex(glfract));
	} else {
		glPushAttrib(GL_VIEWPORT_BIT);
		setup_viewport(scr_w/2, scr_h/2);
		gl_pal_render(glpal, fract_get_tex(glfract));
		glPopAttrib();
	}

	if(show_mandel) render_mandel(pd); //TODO: enable click to change c

	//TODO: figure out what attrib to push to save color
	if(debug_pal || debug_maxsrc) { glPushAttrib(GL_TEXTURE_BIT); if(packed_intesity_pixels) glColor3f(1.0f, 1.0f, 1.0f); }
	if(debug_pal) {
		glBindTexture(GL_TEXTURE_2D, fract_get_tex(glfract));
		draw_tex_quad(0.5f, 0.5f, -0.5f);
	}
	if(debug_maxsrc) {
		glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
		draw_tex_quad(0.5f, -0.5f, 0.5f);
	}
	if(debug_pal || debug_maxsrc) { glPopAttrib(); if(packed_intesity_pixels) glColor3f(1.0f, 1.0f, 1.0f); }
	
	if(show_fps_hist) { DEBUG_CHECK_GL_ERR;
		glPushMatrix();
		glScalef(0.5, 0.25, 1);
		glTranslatef(-2, 3, 0);
		draw_hist_array(cnt, FPS_HIST_LEN/(8.0f*totframetime), frametimes, FPS_HIST_LEN);
		glPopMatrix();
		glPushMatrix();
		glScalef(0.5, 0.25, 1);
		glTranslatef(1, 3, 0);
		draw_hist_array(cnt, FPS_HIST_LEN/(8.0f*totworktime), worktimes, FPS_HIST_LEN);
		glPopMatrix();
		glColor3f(1.0f, 1.0f, 1.0f);
		char buf[128];
		glRasterPos2f(-1,1 - 20.0f/(scr_h*0.5f));
		sprintf(buf,"%6.1f FPS %6.1f", FPS_HIST_LEN*1000000.0f/totframetime, maxfrms*1000000.0f/(now-tick0));
		draw_string(buf); DEBUG_CHECK_GL_ERR;
		glRasterPos2f(-1,0.75-20.0f/(scr_h*0.5f));
		sprintf(buf,"%7.1fns frametime\n%7.1fns worktime\n", totframetime/((float)FPS_HIST_LEN), totworktime/((float)FPS_HIST_LEN));
		draw_string(buf); DEBUG_CHECK_GL_ERR;
	} else {
		glRasterPos2f(-1,0.75-20.0f/(scr_h*0.5f));
	}

	render_debug_overlay();
	
	swap_buffers(); CHECK_GL_ERR;

	now = uget_ticks();
	if(now - lastpalstep >= 1000*2048/256 && gl_pal_changing(glpal)) { // want pallet switch to take ~2 seconds
		if(gl_pal_step(glpal, IMIN((now - lastpalstep)*256/(2048*1000), 32)))
		lastpalstep = now;
	}
	int newbeat = beat_get_count();
	if(newbeat != beats) {
		gl_pal_start_switch(glpal, newbeat);
	}
	if(newbeat != beats && now - last_beat_time > 1000000) {
		last_beat_time = now;
		update_points(pd, (now - tick0)/1000, 1);
	} else update_points(pd, (now - tick0)/1000, 0);
	beats = newbeat;

	
	now = uget_ticks();
	totframetime -= frametimes[cnt%FPS_HIST_LEN];
	totframetime += (frametimes[cnt%FPS_HIST_LEN] = now - fps_oldtime);
	fps_oldtime = now;
	
	totworktime -= worktimes[cnt%FPS_HIST_LEN];
	totworktime += (worktimes[cnt%FPS_HIST_LEN] = now - workstart);

	cnt++;
}

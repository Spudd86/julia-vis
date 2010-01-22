/**
 * main.c
 *
 */

//TODO: depth textures? (more bits!) GL_ARB_depth_texture (also use the depth texture as a shadow texture)

#define GL_GLEXT_PROTOTYPES

#include "common.h"

#include <SDL.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "points.h"
#include "sdl-misc.h"
#include "glmisc.h"
#include "gl_maxsrc.h"
#include "pixmisc.h"
#include "audio/audio.h"
#include "glpallet.h"


#define IM_SIZE (512)

//TODO: ARB_fragment_program version
//TODO: write code to mix'n'match the map() functions with different mains (ie different # of samples per pixel)
static GLhandleARB map_prog;
static GLint map_c_loc=0, map_prev_loc=0, map_maxsrc_loc=0;
static GLhandleARB rat_map_prog;
static const char *map_frag_shader =

	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#else\n"
	"#define encode(X) (X)\n"
	"#define decode(X) (X)\n"
	"#endif\n"

	"uniform sampler2D prev;\n"
	"uniform sampler2D maxsrc;\n"
	"invariant uniform vec2 c;\n"
	"#ifdef MAP_SAMP\n"
	"vec4 smap(vec2 s) {\n"
	"	vec2 t = s*s;\n"
	"	return texture2D(prev, vec2(t.x - t.y, 2*s.x*s.y) + c);\n"
	"}\n"
	"void main() {\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st)*0.5f; vec2 dy = dFdy(gl_TexCoord[1].st)*0.5f;\n"
	"#if MAP_SAMP == 5\n"
	"	vec4 r = (253/(8*256.0f))*(smap(gl_TexCoord[1].st)*4 + \n"
	"			smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx) );\n"
	"#elif MAP_SAMP == 7\n"
	"	vec4 r = (253.0f/4096)*(smap(gl_TexCoord[1].st)*4 + \n"
	"			(smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx))*2 +\n"
	"			(smap(gl_TexCoord[1].st+dy+dx) + smap(gl_TexCoord[1].st+dy-dx) +\n"
	"			smap(gl_TexCoord[1].st-dx-dy) + smap(gl_TexCoord[1].st-dy+dx)) );\n"
	"#endif\n"
	"	gl_FragData[0] = encode(max(decode(r), decode(texture2D(maxsrc, gl_TexCoord[0].st))));\n"
	"}\n"
	"#else\n"
	"void main() {\n"
	"	vec2 t = gl_TexCoord[1].st * gl_TexCoord[1].st;\n"
	"	gl_FragData[0] = encode(max("
	"			decode( texture2D(prev, vec2(t.x - t.y, 2*gl_TexCoord[1].x*gl_TexCoord[1].y) + c)*(253/256.0f) ),"
	"			decode( texture2D(maxsrc, gl_TexCoord[0].st) )"
	"	));\n"
	"}\n"
	"#endif\n";
	
static const char *rat_map_frag_shader = 
	"uniform sampler2D prev;\n"
	"uniform sampler2D maxsrc;\n"
	"invariant uniform vec4 c;\n"
	"#ifdef FLOAT_PACK_PIX\n"
	FLOAT_PACK_FUNCS
	"#else\n"
	"#define encode(X) X\n#define decode(X) X\n"
	"#endif\n"
	"vec4 smap(vec2 s) {\n"
	"	s = s*2.5;\n"
	"	vec2 t = s*s;\n"
	"	float ab = s.x*s.y;\n"
	"	s = vec2(4*ab*(t.x - t.y), t.x*t.x - 6*t.x*t.y + t.y*t.y) + c.xy;\n"
	"	t = vec2(t.x - t.y, 2*ab)+c.zw;\n"
	"	return texture2D(prev,(0.5f/2.5)*vec2(dot(s,t), dot(s,t.yx))/dot(t,t)+0.5f);\n"
	"}\n"
	"void main() {\n"
	"#ifdef MAP_SAMP\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st)*0.5f; vec2 dy = dFdy(gl_TexCoord[1].st)*0.5f;\n"
	"#if MAP_SAMP == 5\n"
	"	vec4 r = (254/(8*256.0f))*(smap(gl_TexCoord[1].st)*4 + \n"
	"			smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx) );\n"
	"#elif MAP_SAMP == 7\n"
	"	vec4 r = (253.0f/4096)*(smap(gl_TexCoord[1].st)*4 + \n"
	"			(smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx))*2 +\n"
	"			(smap(gl_TexCoord[1].st+dy+dx) + smap(gl_TexCoord[1].st+dy-dx) +\n"
	"			smap(gl_TexCoord[1].st-dx-dy) + smap(gl_TexCoord[1].st-dy+dx)) );\n"
	"#endif\n"
	"	gl_FragData[0] = encode(max(decode(r), decode(texture2D(maxsrc, gl_TexCoord[0].st))));\n"
	"#else\n"
	"	gl_FragData[0] = encode(max((253.0f/256.0f)*decode(smap(gl_TexCoord[1].st)), decode(texture2D(maxsrc, gl_TexCoord[0].st))));\n"
	"#endif\n"
	"}\n"
//	"#endif\n\n"
	;


GLuint disp_texture;

static void setup_opengl(int width, int height);
static opt_data opts;

GLuint map_fbo, map_fbo_tex[2];

static void setup_map_fbo(int width, int height) {
	glGenFramebuffersEXT(1, &map_fbo);
	glGenTextures(2, map_fbo_tex);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, map_fbo_tex[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,  width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		GLint redsize; glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &redsize);
		GLint greensize; glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &greensize);
		GLint bluesize; glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &bluesize);
		GLint alphasize; glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE, &alphasize);
		printf("Got format with RGBA bits %i,%i,%i %i\n", redsize, greensize, bluesize, alphasize);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		static float foo[] = {0.0, 0.0, 0.0 };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, foo);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	glPopClientAttrib();
	glPopAttrib();
}

static void set_viewport(int w, int h) {
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

static void do_frame(int src_tex, int draw_tex, int im_w, int im_h, struct point_data *pd) {
	glPushAttrib(GL_ALL_ATTRIB_BITS);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, map_fbo);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, map_fbo_tex[draw_tex], 0);
		set_viewport(im_w, im_h);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, map_fbo_tex[src_tex]);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());

		if(pd->dim == 2) {
			glUseProgramObjectARB(map_prog);
			//glUniform2fARB(map_c_loc, pd->p[0]*0.5f, pd->p[1]*0.5f);
			glUniform2fARB(map_c_loc, (pd->p[0]-0.5f)*0.25f + 0.5f, pd->p[1]*0.25f + 0.5f);
			glUniform1iARB(map_maxsrc_loc, 0);
			glUniform1iARB(map_prev_loc, 1);
		} else if(pd->dim == 4) {
			glUseProgramObjectARB(rat_map_prog);
			glUniform4fARB(glGetUniformLocationARB(rat_map_prog, "c"), pd->p[0], pd->p[1], pd->p[2], pd->p[3]);
			glUniform1iARB(glGetUniformLocationARB(rat_map_prog, "maxsrc"), 0);
			glUniform1iARB(glGetUniformLocationARB(rat_map_prog, "prev"), 1);
		}
		glBegin(GL_QUADS);
			glMultiTexCoord2f(GL_TEXTURE0, 0.0, 1.0);
			glMultiTexCoord2f(GL_TEXTURE1,-1.0, 1.0);
			glVertex2d(-1,  1);
			glMultiTexCoord2f(GL_TEXTURE0, 1.0, 1.0);
			glMultiTexCoord2f(GL_TEXTURE1, 1.0, 1.0);
			glVertex2d( 1,  1);
			glMultiTexCoord2f(GL_TEXTURE0, 1.0, 0.0);
			glMultiTexCoord2f(GL_TEXTURE1, 1.0,-1.0);
			glVertex2d( 1,-1);
			glMultiTexCoord2f(GL_TEXTURE0, 0.0, 0.0);
			glMultiTexCoord2f(GL_TEXTURE1,-1.0,-1.0);
			glVertex2d(-1,-1);
		glEnd();
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glUseProgramObjectARB(0);
	glPopAttrib();

	pal_render(src_tex);
}

void init_mandel();
void render_mandel(struct point_data *pd);

static int make_pow2(int x) {
	int t = x, n = 0;
	while(t != 1) { t = t>>1; n++; }
	if(x == 1<<n ) return x;
	else return 1<<(n+1);
}

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup_gl(&opts, IM_SIZE);
	int im_w = IMAX(make_pow2(IMAX(screen->w, screen->h)), 128), im_h = im_w; //TODO: nearest power of two
	glewInit();

	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	if(GLEW_ARB_shading_language_100) printf("GL_SL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("\n\n");

	if(!glewGetExtension("GL_ARB_shading_language_120")) {
		printf("GLSL support not good enough, or missing\n");
		exit(1);
	}
	
	if(GLEW_EXT_blend_minmax) {
		printf("Have blend_minmax!\n");
	} else {
		printf("missing GL_EXE_blend_minmax!\n");
		exit(1);
	}

	init_mandel();
	audio_init(&opts);
	setup_opengl(screen->w, screen->h);
	gl_maxsrc_init(IMAX(im_w/2, 128), IMAX(im_h/2, 128), opts.quality == 3);
	setup_map_fbo(im_w, im_h); 

	printf("Compiling map shader:\n");
	const char *map_defs = "#version 120\n";
	if(opts.quality == 1) map_defs = "#version 120\n#define MAP_SAMP 5\n\n";
	else if(opts.quality == 2) map_defs = "#version 120\n#define MAP_SAMP 7\n";
	else if(opts.quality == 3) map_defs = "#version 120\n#define FLOAT_PACK_PIX\n";
	else if(opts.quality == 4) map_defs = "#version 120\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 5\n";
	else if(opts.quality == 5) map_defs = "#version 120\n#define FLOAT_PACK_PIX\n#define MAP_SAMP 7\n";
	if(opts.rational_julia) {
		rat_map_prog = compile_program_defs(map_defs, NULL, rat_map_frag_shader);
	} else {
		map_prog = compile_program_defs(map_defs, NULL, map_frag_shader);
		map_c_loc = glGetUniformLocationARB(map_prog, "c");
		map_prev_loc = glGetUniformLocationARB(map_prog, "prev");
		map_maxsrc_loc = glGetUniformLocationARB(map_prog, "maxsrc");
	}
	printf("Map shader compiled\n");

	struct point_data *pd = new_point_data(opts.rational_julia?4:2);

	pal_init(im_w, im_h, map_fbo_tex, opts.quality == 3);

	glPixelZoom(1.0, -1.0);
	glRasterPos2f(-1.0, 1.0);
	SDL_Event	event;
	Uint32 tick0, fps_oldtime;
	Uint32 now = fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int cnt = 0;
	int beats = beat_get_count();
	Uint32 last_beat_time = tick0, lastpalstep = tick0;
//	Uint32  maxfrms = 0;

	Uint32 src_tex = 0, dst_tex = 1;

	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0;
	int lastframe_key = 0;

	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)) { if(!lastframe_key) { debug_maxsrc = !debug_maxsrc; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2)) { if(!lastframe_key) { debug_pal = !debug_pal; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3)) { if(!lastframe_key) { show_mandel = !show_mandel; } lastframe_key = 1; }
		else lastframe_key = 0;

		gl_maxsrc_update(now);
		do_frame(src_tex, dst_tex, im_w, im_h, pd);
		if(show_mandel) render_mandel(pd); //TODO: enable click to change target

		//TODO: figure out what attrib to push to save color
		if(debug_pal || debug_maxsrc) { glPushAttrib(GL_TEXTURE_BIT); glColor3f(1.0f, 0.0f, 0.0f); }
		if(debug_pal) {
			glBindTexture(GL_TEXTURE_2D, map_fbo_tex[0]);
			glBegin(GL_QUADS);
				glTexCoord2d(0.0,1.0); glVertex2d( 1, -1);
				glTexCoord2d(1.0,1.0); glVertex2d( 0, -1);
				glTexCoord2d(1.0,0.0); glVertex2d( 0,  0);
				glTexCoord2d(0.0,0.0); glVertex2d( 1,  0);
			glEnd();
		}
		if(debug_maxsrc) { glColor3f(1.0f, 1.0f, 1.0f);
			glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
			glBegin(GL_QUADS);
				glTexCoord2d(0.0,1.0); glVertex2d( 0,  0);
				glTexCoord2d(1.0,1.0); glVertex2d(-1,  0);
				glTexCoord2d(1.0,0.0); glVertex2d(-1,  1);
				glTexCoord2d(0.0,0.0); glVertex2d( 0,  1);
			glEnd();
		}
		if(debug_pal || debug_maxsrc) { glPopAttrib(); glColor3f(1.0f, 1.0f, 1.0f); }

		char buf[64];
		sprintf(buf,"%6.1f FPS %6.1f UPS", 1000.0f / frametime, maxfrms*1000.0f/(now-tick0));
		glRasterPos2f(-1,1 - 20.0f/(screen->h*0.5f));
		draw_string(buf);

		SDL_GL_SwapBuffers();
		GLint gl_err = glGetError();
		if(gl_err != GL_NO_ERROR) {
			const GLubyte *errstr = gluErrorString(gl_err);
			printf("GL ERROR: %s\n", errstr);
		}

		Uint32 tex_tmp = src_tex; src_tex = dst_tex; dst_tex = tex_tmp;

		now = SDL_GetTicks();
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

		now = SDL_GetTicks();
		if(now - fps_oldtime < 10) SDL_Delay(10 - (now-fps_oldtime)); // stay below ~125 FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
		cnt++;
	}


	SDL_Quit();
	return 0;
}

static void setup_opengl(int width, int height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glClearColor(0, 0 ,0 , 0);

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);
//	glEnable(GL_DITHER);
	glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, GL_FASTEST);
}

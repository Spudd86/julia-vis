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

#define IM_SIZE (512)

uint32_t *get_active_pal(void);

static uint32_t *active_pal;
static GLuint pal_tex;

static void init_pal_tex(void) {
	glPushAttrib(GL_TEXTURE_BIT);
	active_pal = get_active_pal();
	glGenTextures(1, &pal_tex);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, active_pal);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib();
}

static void update_pal_tex(void) {
	glPushAttrib(GL_TEXTURE_BIT);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
//	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, 256, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, active_pal);
	glPopAttrib();
}

//TODO: ARB_fragment_program version
static GLhandleARB pal_prog;
static GLhandleARB map_prog;
static const char *map_frag_shader =
	"uniform sampler2D prev;"
	"uniform sampler2D maxsrc;"
	"uniform vec2 c;"
	"void main() {"
	"	vec2 t = gl_TexCoord[1].st * gl_TexCoord[1].st;"
	"	gl_FragData[0] = max("
	"			texture2D(prev, vec2(t.x - t.y, 2*gl_TexCoord[1].x*gl_TexCoord[1].y) + c),"
	"			texture2D(maxsrc, gl_TexCoord[0].st));"
	"}";

static const char *pal_frag_shader =
	"uniform sampler2D src;"
	"uniform sampler1D pal;"
	"void main() {"
	"	gl_FragColor = texture1D(pal, float(texture2D(src, vec2(gl_TexCoord[0]))));"
	"}";

GLuint disp_texture;

static void setup_opengl(int width, int height);
static opt_data opts;

GLuint map_fbo, map_fbo_render_buf, map_fbo_tex[2];

static void setup_map_fbo(int width, int height) {
	glGenFramebuffersEXT(1, &map_fbo);
	glGenTextures(2, map_fbo_tex);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	for(int i=0; i<2; i++) {
		glBindTexture(GL_TEXTURE_2D, map_fbo_tex[i]);
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F_ARB,  width, height, 0, GL_RGB, GL_HALF_FLOAT_ARB, NULL);
//		if(glGetError() != GL_NONE) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB,  width, height, 0, GL_RGB, GL_FLOAT, NULL);
//		if(glGetError() != GL_NONE) glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,  width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		if(GLEW_ARB_half_float_pixel && GLEW_ARB_texture_float) { printf("using half float pixels in map fbo\n");
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F_ARB,  width, height, 0, GL_RGB, GL_HALF_FLOAT_ARB, NULL);
		} else if(GLEW_ARB_color_buffer_float && GLEW_ARB_texture_float) { printf("float pixels in map fbo\n");
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F_ARB,  width, height, 0, GL_RGB, GL_FLOAT, NULL);
		} else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,  width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		static float foo[] = {0.0, 0.0, 0.0 };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, foo);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
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

		glUseProgramObjectARB(map_prog);
		glUniform2fARB(glGetUniformLocationARB(map_prog, "c"), (pd->p[0]-0.5f)*0.25f + 0.5f, pd->p[1]*0.25f + 0.5f);
		glUniform1iARB(glGetUniformLocationARB(map_prog, "maxsrc"), 0);
		glUniform1iARB(glGetUniformLocationARB(map_prog, "prev"), 1);
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
	glPopAttrib();

	glPushAttrib(GL_ALL_ATTRIB_BITS);
		glUseProgramObjectARB(pal_prog);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, map_fbo_tex[draw_tex]);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, pal_tex);
		glUniform1iARB(glGetUniformLocationARB(pal_prog, "src"), 0);
		glUniform1iARB(glGetUniformLocationARB(pal_prog, "pal"), 1);

		glBegin(GL_QUADS);
			glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
			glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
			glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
			glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
		glEnd();
		glUseProgramObjectARB(0);
	glPopAttrib();
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
	int im_w = make_pow2(IMAX(screen->w, screen->h)), im_h = im_w; //TODO: nearest power of two
	glewInit();

	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	printf("GL_SL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
//	printf("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
	printf("\n\n");


	//	if (glewIsSupported("GL_VERSION_2_0"))
	//			printf("Ready for OpenGL 2.0\n");
	//	else
	if(GLEW_ARB_half_float_pixel) {
		printf("Have half float pixels!\n");
	}
	if(GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader) {
		printf("Ready for GLSL\n");
	} else {
		printf("No GLSL support\n");
		exit(1);
	}

	pallet_init(1);
	init_pal_tex();
	init_mandel();
	audio_init(&opts);
	setup_opengl(screen->w, screen->h);
	gl_maxsrc_init(IMIN(im_w/2, 512), IMIN(im_h/2, 512));
	setup_map_fbo(im_w, im_h);
	map_prog = compile_program(NULL, map_frag_shader);
	pal_prog = compile_program(NULL, pal_frag_shader);

	struct point_data *pd = new_point_data(2);

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

	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)) debug_maxsrc = !debug_maxsrc;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2)) debug_pal = !debug_pal;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3)) show_mandel = !show_mandel;

		gl_maxsrc_update(now);
		do_frame(src_tex, dst_tex, im_w, im_h, pd);
		if(show_mandel) render_mandel(pd); //TODO: enable click to change target

		if(debug_pal) {
			glPushAttrib(GL_TEXTURE_BIT);
			glBindTexture(GL_TEXTURE_2D, map_fbo_tex[0]);
			glBegin(GL_QUADS);
				glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
				glTexCoord2d(1.0,1.0); glVertex2d( 0, -1);
				glTexCoord2d(1.0,0.0); glVertex2d( 0,  0);
				glTexCoord2d(0.0,0.0); glVertex2d(-1,  0);
			glEnd();
			glPopAttrib();
		}
		if(debug_maxsrc) {
			glPushAttrib(GL_TEXTURE_BIT);
			glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
			glBegin(GL_QUADS);
				glTexCoord2d(0.0,1.0); glVertex2d( 0,  0);
				glTexCoord2d(1.0,1.0); glVertex2d( 1,  0);
				glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
				glTexCoord2d(0.0,0.0); glVertex2d( 0,  1);
			glEnd();
			glPopAttrib();
		}
		if(debug_maxsrc && debug_pal) {
			glPushAttrib(GL_TEXTURE_BIT);
			glUseProgramObjectARB(pal_prog);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, map_fbo_tex[dst_tex]);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_1D, pal_tex);
			glBegin(GL_QUADS);
				glTexCoord2d(0.0,1.0); glVertex2d(-1,  0);
				glTexCoord2d(1.0,1.0); glVertex2d( 0,  0);
				glTexCoord2d(1.0,0.0); glVertex2d( 0,  1);
				glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
			glEnd();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
			glBegin(GL_QUADS);
				glTexCoord2d(0.0,1.0); glVertex2d( 0, -1);
				glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
				glTexCoord2d(1.0,0.0); glVertex2d( 1,  0);
				glTexCoord2d(0.0,0.0); glVertex2d( 0,  0);
			glEnd();
			glUseProgramObjectARB(0);
			glPopAttrib();
		}
		SDL_GL_SwapBuffers();
		Uint32 tex_tmp = src_tex; src_tex = dst_tex; dst_tex = tex_tmp;

		now = SDL_GetTicks();
		if(now - lastpalstep >= 2048/256 && get_pallet_changing()) { // want pallet switch to take ~2 seconds
			pallet_step(IMIN((now - lastpalstep)*256/2048, 32));
			update_pal_tex();
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
		if(now - fps_oldtime < 2) SDL_Delay(2); // stay below 500FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
		cnt++;
	}
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

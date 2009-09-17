/**
 * main.c
 *
 */

#include "common.h"

#include <SDL.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include "points.h"
#include "sdl-misc.h"
#include "glmisc.h"
#include "pixmisc.h"
#include "audio/audio.h"

#define IM_SIZE (512)

#define MAX_LISTS 4
#define DISP_TEXS 2

static Pixbuf *setup_point(int w, int h)
{
	Pixbuf *surf = malloc(sizeof(Pixbuf));
//	uint32_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 32;
	uint16_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 16;
//	uint8_t *buf = surf->data = malloc(w * h * sizeof(*buf)); surf->bpp  = 8;
	surf->w = w; surf->h = h;
	surf->pitch = surf->w*sizeof(*buf);
	memset(buf, 0, w*h*sizeof(*buf));

	for(int y=0; y < h; y++)  {
		for(int x=0; x < w; x++) {
			float u = (2.0f*x)/w - 1, v = (2.0f*y)/h - 1;
			float t = expf(-4.5f*(u*u+v*v));
			buf[y*w + x] = (uint16_t)(t*(UINT16_MAX));
//			buf[y*w + x] = (uint8_t)(t*(UINT8_MAX));
//			uint8_t tmp = (uint8_t)(t*(UINT8_MAX));
//			buf[y*w + x] = (tmp<<24)|(tmp<<16)|(tmp<<8)|255;
		}
	}
	return surf;
}

static void setup_texture(Pixbuf *src, GLuint *tex) {
	glPushAttrib(GL_TEXTURE_BIT);
	glGenTextures(1, tex);
	glBindTexture(GL_TEXTURE_2D, *tex);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src->w, src->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, src->data);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, src->w, src->h, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, src->data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	static float foo[] = {0.0, 0.0, 0.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, foo);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glPopAttrib();
}

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
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
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

static GLhandleARB pal_prog;
static GLhandleARB map_prog;
static const char *map_frag_shader =
	"uniform sampler2D prev;"
	"uniform sampler2D maxsrc;"
	"uniform vec2 c;"
	"void main() {"
	"	float u = gl_TexCoord[0].s*2-1, v = gl_TexCoord[0].t*2-1;"
//	"	float u = gl_TexCoord[0].s, v = gl_TexCoord[0].t;"
//	"	gl_FragColor = max("
	"	gl_FragData[0] = max("
	"			texture2D(prev, vec2(u*u - v*v + c.x, 2*u*v + c.y)),"
//	"			texture2D(prev, vec2(u+0.1, v)),"
	"			texture2D(maxsrc, vec2(gl_TexCoord[0])));"
	"}";

static const char *pal_frag_shader =
	"uniform sampler2D src;"
	"uniform sampler1D pal;"
	"void main() {"
//	"	float v = clamp(float(texture2D(src, vec2(gl_TexCoord[0]))), 0, 1- 1/256.0);"
//	"	gl_FragColor = texture1D(pal, v);"
	"	gl_FragColor = texture1D(pal,float(texture2D(src, vec2(gl_TexCoord[0]))));"
//	"	gl_FragColor = texture2D(src, vec2(gl_TexCoord[0]));"
	"}";

GLuint disp_texture;

static GLuint make_disp_list() {
	GLuint list = glGenLists(MAX_LISTS);
	glNewList(list, GL_COMPILE);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

//		glRectf(1.0, 1.0, -1.0, -1.0);
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
	glEndList();
	return list;
//	return 0;
}

static void setup_opengl(int width, int height);
static opt_data opts;

GLuint map_fbos[DISP_TEXS], map_fbo_render_buf, map_fbo_tex[DISP_TEXS];
GLuint map_fbo;

static void setup_map_fbo(int width, int height) {
	glGenFramebuffersEXT(1, &map_fbo);
	glGenFramebuffersEXT(DISP_TEXS, map_fbos);
	glGenTextures(DISP_TEXS, map_fbo_tex);

	glPushAttrib(GL_VIEWPORT_BIT | GL_TEXTURE_BIT | GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, map_fbo);
	for(int i=0; i<DISP_TEXS; i++) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, map_fbos[i]);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, map_fbo_tex[i], 0);
		glBindTexture(GL_TEXTURE_2D, map_fbo_tex[i]);
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,  width, height, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, NULL);
//		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,  width, height, 0, GL_RGB, GL_UNSIGNED_SHORT, NULL);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,  width, height, 0, GL_RGB, GL_FLOAT, NULL);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		static float foo[] = {0.0, 0.0, 0.0 };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, foo);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, map_fbo_tex[i], 0);
		glViewport(0,0,width, height);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
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

static void do_frame(int src_tex, int draw_tex, int im_w, int im_h, struct point_data *pd, GLuint disp_list) {
	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_ENABLE_BIT);
	glEnable(GL_TEXTURE_2D);
//	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, map_fbo);
//	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, map_fbo_tex[draw_tex], 0);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, map_fbos[draw_tex]);
//		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, map_fbo_tex[draw_tex], 0);
	set_viewport(im_w, im_h);
//	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, map_fbo_tex[draw_tex], 0);

//	glEnable(GL_TEXTURE1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, map_fbo_tex[src_tex]);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, disp_texture);
//	glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());

	glUseProgramObjectARB(map_prog);
	glUniform2fARB(glGetUniformLocationARB(map_prog, "c"), pd->p[0]*0.25f + 0.5f, pd->p[1]*0.25f + 0.5f);
	glUniform1iARB(glGetUniformLocationARB(map_prog, "maxsrc"), 0);
	glUniform1iARB(glGetUniformLocationARB(map_prog, "prev"), 1);
	glCallList(disp_list);
	glPopAttrib();

	glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	set_viewport(im_w, im_h);

	glUseProgramObjectARB(pal_prog);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, map_fbo_tex[src_tex]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_1D, pal_tex);
	glUniform1iARB(glGetUniformLocationARB(map_prog, "src"), 0);
	glUniform1iARB(glGetUniformLocationARB(map_prog, "pal"), 1);

//	glRectf(0.0, 0.0, 1.0, 1.0);
	glBegin(GL_QUADS);
		glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
		glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
		glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
		glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
	glEnd();
	glUseProgramObjectARB(0);
	glPopAttrib();
//	SDL_GL_SwapBuffers();
}

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup_gl(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%16, im_h = screen->h - screen->h%8;
	glewInit();
	//	if (glewIsSupported("GL_VERSION_2_0"))
	//			printf("Ready for OpenGL 2.0\n");
	//	else
	if(GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader) {
		if(!GLEW_ARB_vertex_shader) printf("no vertex shaders\n");
		if(!GLEW_ARB_fragment_shader) printf("no frag shaders\n");
		printf("Ready for GLSL\n");
	} else {
		printf("No GLSL support\n");
		exit(1);
	}

	pallet_init(1);
	init_pal_tex();
	audio_init(&opts);

	setup_opengl(im_w, im_h);
	gl_maxsrc_init(im_w/2, im_h/2);

	setup_map_fbo(im_w, im_h);

	struct point_data *pd = new_point_data(2);
	Pixbuf *disp_surf = setup_point(256, 256);

	setup_texture(disp_surf, &disp_texture);
	map_prog = compile_program(NULL, map_frag_shader);
	pal_prog = compile_program(NULL, pal_frag_shader);

	GLuint disp_list = make_disp_list();

	glPixelZoom(1.0, -1.0);
	glRasterPos2f(-1.0, 1.0);
	SDL_Event	event;
	Uint32 tick0, fps_oldtime;
	Uint32 now = fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int cnt = 0;
	int beats = beat_get_count();
	Uint32 last_beat_time = tick0, lastpalstep = tick0;

	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;

		do_frame(cnt%2,(cnt+1)%2, im_w, im_h, pd, disp_list);

//		glPushAttrib(GL_ALL_ATTRIB_BITS);
//			glUseProgramObjectARB(pal_prog);
//			glActiveTexture(GL_TEXTURE0);
//			glBindTexture(GL_TEXTURE_2D, gl_maxsrc_get());
//			glActiveTexture(GL_TEXTURE1);
//			glBindTexture(GL_TEXTURE_1D, pal_tex);
//			glUniform1iARB(glGetUniformLocationARB(map_prog, "src"), 0);
//			glUniform1iARB(glGetUniformLocationARB(map_prog, "pal"), 1);
//			glBegin(GL_QUADS);
//				glTexCoord2d(0.0,1.0); glVertex2d(0, 0);
//				glTexCoord2d(1.0,1.0); glVertex2d( 1, 0);
//				glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
//				glTexCoord2d(0.0,0.0); glVertex2d(0,  1);
//			glEnd();
//			glUseProgramObjectARB(0);
//		glPopAttrib();
//		gl_maxsrc_update();
		SDL_GL_SwapBuffers();

		now = SDL_GetTicks();
		if(now - lastpalstep >= 2048/256) { // want pallet switch to take ~2 seconds
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
		if(now - fps_oldtime < 1) SDL_Delay(1); // stay below 1000FPS
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

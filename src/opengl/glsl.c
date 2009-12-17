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

#include "terminusIBM.h"


static void draw_string(const char *str)
{
	glPushClientAttrib( GL_CLIENT_PIXEL_STORE_BIT );
	glPixelStorei( GL_UNPACK_SWAP_BYTES,  GL_FALSE );
	glPixelStorei( GL_UNPACK_LSB_FIRST,   GL_FALSE );
	glPixelStorei( GL_UNPACK_ROW_LENGTH,  0        );
	glPixelStorei( GL_UNPACK_SKIP_ROWS,   0        );
	glPixelStorei( GL_UNPACK_SKIP_PIXELS, 0        );
	glPixelStorei( GL_UNPACK_ALIGNMENT,   1        );

	const char *c = str;
	while(*c) {
			//TODO: fix the font

			uint8_t tmp[16]; const uint8_t *src = terminusIBM + 16 * *c;
			for(int i = 0; i<16; i++) { //FIXME draws upsidedown on ATI's gl on windows
				tmp[i] = src[15-i];
			}

			glBitmap(8, 16, 0,0, 8, 0, tmp);
			c++;
		}
	glPopClientAttrib();
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
//TODO: write code to mix'n'match the map() functions with different mains (ie different # of samples per pixel)
static GLhandleARB pal_prog;
static GLhandleARB map_prog;
static GLint map_c_loc=0, map_prev_loc=0, map_maxsrc_loc=0;
static GLhandleARB rat_map_prog;
static const char *map_frag_shader = 
	"#version 120\n"
	"uniform sampler2D prev;"
	"uniform sampler2D maxsrc;"
	"invariant uniform vec2 c;"
	"void main() {"
	"	vec2 t = gl_TexCoord[1].st * gl_TexCoord[1].st;"
//	"	vec4 c1 = texture2D(prev, vec2(t.x - t.y, 2*gl_TexCoord[1].x*gl_TexCoord[1].y) + c);"
//	"	vec4 c2 = texture2D(maxsrc, gl_TexCoord[0].st);"
//	"	float cl = max( dot(vec2(c1), vec2(255,1 - 1.0f/256)), dot(vec2(c2), vec2(255,1 - 1.0f/256)) );"
//	"	gl_FragData[0].xy = vec2(cl/255, fract(cl));"
	"	gl_FragData[0] = max("
	"			texture2D(prev, vec2(t.x - t.y, 2*gl_TexCoord[1].x*gl_TexCoord[1].y) + c)*(253/256.0f),"
	"			texture2D(maxsrc, gl_TexCoord[0].st));"
	"}";
	

	
static const char *rat_map_frag_shader = 
	"#version 120\n"
	"uniform sampler2D prev;\n"
	"uniform sampler2D maxsrc;\n"
	"invariant uniform vec4 c;\n"
	
	"vec4 smap(vec2 s) {\n"
	"	s = s*2.5;\n"
	"	vec2 t = s*s;\n"
	"	float ab = s.x*s.y;\n"
	"	s = vec2(4*ab*(t.x - t.y), t.x*t.x - 6*t.x*t.y + t.y*t.y) + c.xy;\n"
	"	t = vec2(t.x - t.y, 2*ab)+c.zw;\n"
	"	return texture2D(prev,(0.5f/2.5)*vec2(dot(s,t), dot(s,t.yx))/dot(t,t)+0.5f);\n"
	"}\n"

	"void main() {\n"
	"	vec2 dx = dFdx(gl_TexCoord[1].st); vec2 dy = dFdy(gl_TexCoord[1].st);\n"
/*	"	vec4 r = (254/(8*256.0f))*(smap(gl_TexCoord[1].st)*4 + \n"
	"			smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx) );\n"
/**/"	vec4 r = (253.0f/4096)*(smap(gl_TexCoord[1].st)*4 + \n"
	"			(smap(gl_TexCoord[1].st+dy) + smap(gl_TexCoord[1].st+dx) +\n"
	"			smap(gl_TexCoord[1].st-dy) + smap(gl_TexCoord[1].st-dx))*2 +\n"
	"			(smap(gl_TexCoord[1].st+dy+dx) + smap(gl_TexCoord[1].st+dy-dx) +\n"
	"			smap(gl_TexCoord[1].st-dx-dy) + smap(gl_TexCoord[1].st-dy+dx)) );\n"
/**/"	gl_FragData[0] = max(r, texture2D(maxsrc, gl_TexCoord[0].st));\n"
//	"	gl_FragData[0] = max(\n"
//	"		texture2D(prev, map(gl_TexCoord[1].xy))*(253/256.0f), texture2D(maxsrc, gl_TexCoord[0].st));\n"
	"}\n";

static const char *pal_frag_shader =
	"#version 120\n"
	"uniform sampler2D src;"
	"uniform sampler1D pal;"
	"void main() {"
	"	gl_FragColor = texture1D(pal, texture2D(src, gl_TexCoord[0].xy).x);"
	"}";
//	"	vec2 c = texture2D(src, vec2(gl_TexCoord[0])).xy;"
//	"	gl_FragColor = texture1D(pal, dot(vec2(c), vec2(255,1-1.0f/256)/255)-0.5/256);"
//	"}";


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
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2,  width, height, 0, GL_RGBA, GL_UNSIGNED_SHORT, NULL);
			//TODO: make sure that this works
		}

		//GL_RGB10_EXT
		//TODO: accum buffer supports 16 bit per component.... can I use this instead of colour buffer?
		// perhaps use depth textures?

		//TODO: check for errors to be sure we can actually use GL_RGB10 here (possibly also testing that we can render to it)

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
	int im_w = IMAX(make_pow2(IMAX(screen->w, screen->h)), 128), im_h = im_w; //TODO: nearest power of two
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
	
	if(GLEW_EXT_blend_minmax) {
		printf("Have blend_minmax!\n");
	} else {
		printf("missing GL_EXE_blend_minmax!\n");
		exit(1);
	}

	pallet_init(1);
	init_pal_tex();
	init_mandel();
	audio_init(&opts);
	setup_opengl(screen->w, screen->h);
	gl_maxsrc_init(IMAX(im_w/2, 128), IMAX(im_h/2, 128));
	setup_map_fbo(im_w, im_h); 
	map_prog = compile_program(NULL, map_frag_shader);
	map_c_loc = glGetUniformLocationARB(map_prog, "c");
	map_prev_loc = glGetUniformLocationARB(map_prog, "prev");
	map_maxsrc_loc = glGetUniformLocationARB(map_prog, "maxsrc");
	
	pal_prog = compile_program(NULL, pal_frag_shader);
	
	rat_map_prog = compile_program(NULL, rat_map_frag_shader);

	struct point_data *pd = new_point_data(opts.rational_julia?4:2);

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

		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		glRasterPos2f(-1,1 - 20.0f/(screen->h*0.5f));
		draw_string(buf);

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

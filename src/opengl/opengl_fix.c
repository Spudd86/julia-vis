/**
 * opengl_fix.c
 *
 */

//TODO: make this like glsl, then slowly convert to fixed function

#include "../common.h"
#include <stdio.h>
#include <SDL.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include <mm_malloc.h>

#include "map.h"
#include "sdl-misc.h"
#include "pixmisc.h"
#include "audio/audio.h"
#include "glmisc.h"
#include "glfixed.h"

#define MAP soft_map_interp

#define IM_SIZE (512)
#define MAX_LISTS (3)

static opt_data opts;

static void setup_opengl(int width, int height);

GLuint disp_texture, maxsrc_tex;

static void draw_bg() {
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glBindTexture(GL_TEXTURE_2D, disp_texture);
	glBegin(GL_QUADS);
		glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
		glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
		glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
		glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
	glEnd();
	glPopAttrib();
}

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup_gl(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%16, im_h = screen->h - screen->h%8;
	printf("running with %dx%d bufs\n", im_w, im_h);

	glewInit();

	audio_init(&opts);

	maxsrc_setup(im_w, im_h);
	pallet_init(1);
	setup_opengl(im_w, im_h);
	scope_init(im_w, im_h);

	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
//	printf("GL_SL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	printf("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
	printf("\n\n");


	int cnt = 0;

	struct point_data *pd = new_point_data(opts.rational_julia?4:2);

	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	Uint32 last_beat_time = tick0;
	Uint32 lastpalstep = tick0;
	Uint32 now = tick0;
	Uint32  maxfrms = 0;

	GLboolean have_pbo = GL_FALSE;
	GLuint pbos[2];
	Pixbuf *disp_surf = malloc(sizeof(Pixbuf));
	disp_surf->bpp  = 32; disp_surf->w = im_w; disp_surf->h = im_h;
	disp_surf->pitch = disp_surf->w*sizeof(uint32_t);
//	if(GLEW_ARB_pixel_buffer_object) {
	if(0) {
		have_pbo = GL_TRUE;
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glGenBuffersARB(2, pbos);
		for(int i=0; i<2; i++) {
			glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[i]);
			glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, disp_surf->w * disp_surf->h * sizeof(uint32_t), 0, GL_STREAM_DRAW_ARB);
		}
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		glPopAttrib();
	} else {
		disp_surf->data = _mm_malloc(im_w * im_h * sizeof(uint32_t), 32);
		pixbuf_to_texture(disp_surf, &disp_texture, GL_CLAMP_TO_EDGE, GL_TRUE);
	}

	MapMesh *map_m = new_map_mesh(64);

	glGenTextures(1, &maxsrc_tex);
	glBindTexture(GL_TEXTURE_2D, maxsrc_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, im_w, im_h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	SDL_Event	event;
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;

		//TODO: check for vertex shaders and use if available
		vec2f *map_co = map_mesh_begin_mod_texco(map_m);
		for(int y = 0; y < map_m->res; y++) {
			const float x0 = pd->p[0]*0.25f + 0.5f, y0=pd->p[1]*0.25f + 0.5f;
			for(int x = 0; x < map_m->res; x++) {
				float u = 2*((float)x)/(map_m->res-1) - 1;
				float v = 2*((float)y)/(map_m->res-1) - 1;
				float xf = u*u - v*v + x0, yf = 2*u*v + y0;

				map_co[y*map_m->res + x].x = xf;//(xf + 1)*0.5f;
				map_co[y*map_m->res + x].y = yf;//(yf + 1)*0.5f;
			}
		}
		map_mesh_end_mod_texco(map_m);

		render_map_mesh(map_m, maxsrc_tex);
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glEnable(GL_BLEND);
		glBlendEquation(GL_MAX);
		draw_bg();
		glPopAttrib();
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glBindTexture(GL_TEXTURE_2D, maxsrc_tex);
		//opengl 1.1 TODO: check gl version and use right thing
//		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, im_w, im_h, 0);
		// opengl 1.3
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, im_w, im_h);
		glPopAttrib();

		if(now - lastpalstep >= 2048/256) { // want pallet switch to take ~2 seconds
			pallet_step(IMIN((now - lastpalstep)*256/2048, 32));
			lastpalstep = now;
		}

		now = SDL_GetTicks();
		int newbeat = beat_get_count();
		if(newbeat != beats) {
			pallet_start_switch(newbeat);
		}
		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
			update_points(pd, (now - tick0), 1);
		} else update_points(pd, (now - tick0), 0);
		beats = newbeat;

//		render_scope();

		SDL_GL_SwapBuffers();

		//TODO: move to top of loop?
		if((tick0+(maxfrms*1000)/opts.maxsrc_rate) - now > 1000/opts.maxsrc_rate) {
			maxsrc_update();
			glPushAttrib(GL_TEXTURE_BIT);
			if(have_pbo) {
				glBindTexture(GL_TEXTURE_2D, disp_texture);
				glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[maxfrms%2]);
				// copy pixels from PBO to texture object
				// TODO: make this a PBO so we can blit right to it
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, disp_surf->w, disp_surf->h, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, 0);
				glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, pbos[(maxfrms+1)%2]);
				glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_ARB, disp_surf->w * disp_surf->h * sizeof(uint32_t), 0, GL_STREAM_DRAW_ARB);
				disp_surf->data = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);
				if(disp_surf->data) {
					pallet_blit_Pixbuf(disp_surf, maxsrc_get(), disp_surf->w, disp_surf->h);
					glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB);
				}
				glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			} else {
				pallet_blit_Pixbuf(disp_surf, maxsrc_get(), disp_surf->w, disp_surf->h);
				glBindTexture(GL_TEXTURE_2D, disp_texture);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, disp_surf->w, disp_surf->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, disp_surf->data);
			}
			glPopAttrib();
			maxfrms++;
		}

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
	glDisable(GL_CULL_FACE);
	glDisable(GL_DITHER);
//	glDisable( GL_BLEND );
}

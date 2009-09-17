/**
 * opengl_fix.c
 *
 */


#include "../common.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <mm_malloc.h>

#include "../map.h"
#include "../sdl-misc.h"
#include "../pixmisc.h"
#include "../audio/audio.h"

#define MAP soft_map_interp

#define IM_SIZE (512)
#define MAX_LISTS (3)

static opt_data opts;

static void setup_opengl(int width, int height);

GLuint disp_texture;

static void bind_tex(GLuint tex, Pixbuf *src) {
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src->w, src->h, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, src->data);

}

static void setup_textures(Pixbuf *src) {
	glGenTextures(1, &disp_texture);
	bind_tex(disp_texture, src);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

static GLuint gl_gen_back_list() {
	GLuint list = glGenLists(MAX_LISTS);

	glNewList(list, GL_COMPILE);
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glBindTexture(GL_TEXTURE_2D, disp_texture);
		glBegin(GL_QUADS);
			glTexCoord2d(0.0,1.0); glVertex2d(-1, -1);
			glTexCoord2d(1.0,1.0); glVertex2d( 1, -1);
			glTexCoord2d(1.0,0.0); glVertex2d( 1,  1);
			glTexCoord2d(0.0,0.0); glVertex2d(-1,  1);
		glEnd();
		glPopAttrib();
	glEndList();
	return list;
}

static GLuint gen_map_disp_list(float **texarray, GLuint tex, int res)
{
	GLuint list = glGenLists(MAX_LISTS);

	//TODO: allocate space for texture co-ord array
	float *texcoord = *texarray = malloc(sizeof(float));

	glNewList(list, GL_COMPILE);
		glPushAttrib(GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
		glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
		glEnableClientState(GL_VERTEX_ARRAY);   //We want a vertex array
		glEnableClientState(GL_COLOR_ARRAY);    //and a color array
		glEnable(GL_BLEND);
		glBlendEquation(GL_MAX);
		glBindTexture(GL_TEXTURE_2D, tex);

		//TODO: generate vertices in here


		glPopClientAttrib();
		glPopAttrib();
	glEndList();

	return list;
}


int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	SDL_Surface *screen = sdl_setup_gl(&opts, IM_SIZE);
	int im_w = screen->w - screen->w%16, im_h = screen->h - screen->h%8;
	printf("running with %dx%d bufs\n", im_w, im_h);

	audio_init(&opts);

	maxsrc_setup(im_w, im_h);
	pallet_init(1);
	setup_opengl(im_w, im_h);

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

	Pixbuf *disp_surf = malloc(sizeof(Pixbuf));
	disp_surf->data = _mm_malloc(im_w * im_h * sizeof(uint32_t), 32);
	disp_surf->bpp  = 32;
	disp_surf->w = disp_surf->h = im_h;
	disp_surf->pitch = disp_surf->w*sizeof(uint32_t);
	setup_textures(disp_surf);

	GLuint back_list = gl_gen_back_list();

	glPixelZoom(1.0, -1.0);
	glRasterPos2f(-1.0, 1.0);
	SDL_Event	event;
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;

		glCallList(back_list);
//		gl_gen_back_list();

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

		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);

		SDL_Surface *text_surface = DrawTextGL(buf);
		if(text_surface != NULL) {
			uint8_t *data = malloc(text_surface->w*text_surface->h*4);
			uint8_t *src_data = text_surface->pixels;
			for(int y=0; y<text_surface->h; y++) {
				for(int x=0; x<text_surface->w; x++) {
					uint8_t alpha = src_data[y*text_surface->pitch + x];
					int i = (y*text_surface->w + x)*4;
					data[i+3] = data[i + 1] = data[i + 2] = 255; data[i] = alpha;
				}
			}
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, 0);
			glDrawPixels(text_surface->w, text_surface->h, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, data);
			glDisable(GL_ALPHA_TEST);

			SDL_FreeSurface(text_surface);
		}
		SDL_GL_SwapBuffers();

		if((tick0+(maxfrms*1000)/opts.maxsrc_rate) - now > 1000/opts.maxsrc_rate) {
			maxsrc_update();
			pallet_blit_Pixbuf(disp_surf, maxsrc_get(), im_w, im_h);
			bind_tex(disp_texture, disp_surf);
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

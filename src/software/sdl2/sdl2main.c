#include "common.h"
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
// #include <SDL2/SDL_opengl.h>

#include <GLES3/gl3.h>

#include "opengl/gles3/gles3misc.h"

#include "pallet.h"

#include "audio/audio.h"

#include "software/pixmisc.h"
#include "software/maxsrc.h"
#include "software/map.h"

#include "opengl/glpallet.h"


// #define USE_SDL_RENDERER 1
#define USE_GL_UPLOAD 1
#define GL_ACCEL_PAL 1

void gl_draw_string(const char *str);
SDL_Surface* render_string(const char *str);
void DrawText(SDL_Renderer* renderer, const char* text);
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal);

struct glpal_ctx * pal_init_gles3(GLboolean float_packed_pixels);

static const char *vtx_shader_src =
	"const vec2 texc[4] = vec2[4](vec2(0, 0), vec2(1, 0), vec2(0, 1), vec2(1, 1));"
	"const vec2 vert[4] = vec2[4](vec2(-1, -1), vec2( 1, -1), vec2(-1,  1), vec2( 1,  1));"
	"out vec2 uv;"
	"void main() {\n"
	"	uv = texc[gl_VertexID];\n"
	"	gl_Position = vec4(vert[gl_VertexID], 0. , 1. );\n"
	"}";
static const char *frag_shader_src =
	"uniform sampler2D src;\n"
	"in vec2 uv;\n"
	"out vec4 fragColour;\n"
	"void main() {\n"
	"	fragColour = texture(src, uv);\n"
	"}";

//TODO: switch to using the julia_vis_pixel_format based pallet init function

#define IM_SIZE (512)

static opt_data opts;

static soft_map_func map_func = soft_map_interp;

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	if(strcmp(opts.map_name, "rational") == 0) {
		if(opts.quality == 0) map_func = soft_map_rational_interp;
		else if(opts.quality >= 1)  map_func = soft_map_rational;
	}
	else if(strcmp(opts.map_name, "butterfly") == 0) map_func = soft_map_butterfly;
	else if(opts.quality >= 1)  map_func = soft_map;
	if(audio_init(&opts) < 0) exit(1);

	// SDL_SetHint(SDL_HINT_APP_NAME, "Julia Set Audio Visualizer");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION , "1");

	if(opts.w < 0 && opts.h < 0) opts.w = opts.h = IM_SIZE;
	else if(opts.w < 0) opts.w = opts.h;
	else if(opts.h < 0) opts.h = opts.w;

	int win_w = opts.w ;
	int win_h = opts.h ;

	// TODO: option!
	opts.w *= 2;
	opts.h *= 2;

	int im_w = opts.w - opts.w%16, im_h = opts.h - opts.h%16;



	printf("running with %dx%d bufs\n", im_w, im_h);


	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS);

#if defined(USE_SDL_RENDERER) || defined(USE_GL_UPLOAD)
	// SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	// SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	// SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	// SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	// SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
	SDL_Window *window = SDL_CreateWindow("Julia Set Audio Visualizer",
	                                      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                                      win_w, win_h,
	                                      SDL_WINDOW_OPENGL );
	if(!window)
	{
		printf("Window creation failed\n");
		return 1;
	}

	int window_sdl_pixformat = SDL_GetWindowPixelFormat(window);

#ifdef USE_SDL_RENDERER
	int renderCount = SDL_GetNumRenderDrivers();
	printf("Total render drivers: %d\n", renderCount);
	for(int i = 0; i < renderCount; i++) {
		printf("Render driver: %d\n", i);
		SDL_RendererInfo info;
		memset(&info, 0, sizeof(info));
		if(!SDL_GetRenderDriverInfo(i, &info)) {
			printf("\tName: %s\n", info.name);
			printf("\tAccelerated: %s\n", info.flags & SDL_RENDERER_ACCELERATED ? "YES" : "NO");

			printf("\tTexture Formats: ");
			for(int i=0; i < info.num_texture_formats; i++)
			{
				printf("%s, ", SDL_GetPixelFormatName(info.texture_formats[i]));
			}
			printf("\n");
		} else {
			printf("No driver info available\n");
		}
	}

	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if(!renderer)
	{
		printf("Renderer creation failed\n");
		return 1;
	}

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

	julia_vis_pixel_format texture_format = SOFT_PIX_FMT_xRGB8888;
	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, opts.w, opts.h);

#elif defined(USE_GL_UPLOAD)
	//TODO: use GL pallet code
	SDL_GLContext context = SDL_GL_CreateContext(window);
#ifdef GL_ACCEL_PAL
	struct glpal_ctx *glpal = pal_init_gles3(false);
#else
	GLuint shader_program = compile_program_defs(NULL, vtx_shader_src, frag_shader_src);
	if(!shader_program) exit(1);

	glUseProgram(shader_program); CHECK_GL_ERR;
	glUniform1i(glGetUniformLocation(shader_program, "src"), 0); CHECK_GL_ERR;
	glUseProgram(0); CHECK_GL_ERR;

	uint8_t *fallback_pixel_buffer = aligned_alloc(512, 512 + im_w * im_h * sizeof(uint32_t));

	GLuint pbo;
	glGenBuffers(1, &pbo); CHECK_GL_ERR;
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo); CHECK_GL_ERR;
	glBufferData(GL_PIXEL_UNPACK_BUFFER, opts.w * opts.h * sizeof(uint32_t), NULL, GL_STREAM_DRAW); CHECK_GL_ERR;
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); CHECK_GL_ERR;
#endif

	GLuint upload_tex = 0;
	glGenTextures(1, &upload_tex);
	glBindTexture(GL_TEXTURE_2D, upload_tex);

	julia_vis_pixel_format texture_format = SOFT_PIX_FMT_RGBx8888;
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef GL_ACCEL_PAL
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
#else
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#endif
	glBindTexture(GL_TEXTURE_2D, 0);
	CHECK_GL_ERR;

	SDL_GL_SetSwapInterval(0);

	glEnable(GL_FRAMEBUFFER_SRGB_EXT);
	glDisable(GL_DITHER);

	CHECK_GL_ERR;
#else
	julia_vis_pixel_format texture_format = SOFT_PIX_FMT_BGRx8888;
#endif


	uint16_t *map_surf[2];
	map_surf[0] = aligned_alloc(512, 512 + im_w * im_h * sizeof(uint16_t));
	memset(map_surf[0], 0, im_w * im_h * sizeof(uint16_t));
	map_surf[1] = aligned_alloc(512, 512 + im_w * im_h * sizeof(uint16_t));
	memset(map_surf[0], 0, im_w * im_h * sizeof(uint16_t));

	int m = 0, cnt = 0;

	struct maxsrc *maxsrc = maxsrc_new(im_w, im_h);
	struct pal_ctx *pal_ctx = pal_ctx_pix_format_new(texture_format);
	struct point_data *pd = new_point_data(opts.rational_julia?4:2);

	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	uint32_t last_beat_time = tick0;
	uint32_t lastpalstep = tick0;
	uint32_t now = tick0;
	uint32_t maxfrms = 0;

	uint32_t target_frames = 0;

	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;
	int limit_fps = 1;
	int lastframe_key = 0;

	SDL_Event event;
	memset(&event, 0, sizeof(event));
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)) { if(!lastframe_key) { debug_maxsrc = !debug_maxsrc; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2)) { if(!lastframe_key) { debug_pal = !debug_pal; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3)) { if(!lastframe_key) { show_mandel = !show_mandel; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F4)) { if(!lastframe_key) { show_fps_hist = !show_fps_hist; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F6)) { if(!lastframe_key) { limit_fps = !limit_fps; } lastframe_key = 1; }
		else lastframe_key = 0;

		m = (m+1)&0x1;

		if(!opts.rational_julia) {
			map_func(map_surf[m], map_surf[(m+1)&0x1], im_w, im_h, pd);
			maxblend(map_surf[m], maxsrc_get(maxsrc), im_w, im_h);
		}

		if(opts.rational_julia) {
			maxblend(map_surf[(m+1)&0x1], maxsrc_get(maxsrc), im_w, im_h);
			map_func(map_surf[m], map_surf[(m+1)&0x1], im_w, im_h,  pd);
		}

		const uint16_t *src_buf = (debug_maxsrc) ? maxsrc_get(maxsrc) : map_surf[m] ;

#ifdef USE_SDL_RENDERER
		int dst_pitch;
		void *dst_buf;
		SDL_LockTexture(texture, NULL, &dst_buf, &dst_pitch);
		pallet_blit_raw(dst_buf, texture_format, dst_pitch, src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));
		SDL_UnlockTexture(texture);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, NULL, NULL);

		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		DrawText(renderer, buf);

		SDL_RenderPresent(renderer);
#elif defined(USE_GL_UPLOAD)

		glViewport(0, 0, win_w, win_h); CHECK_GL_ERR; ;

#ifndef GL_ACCEL_PAL
		glBindTexture(GL_TEXTURE_2D, upload_tex); CHECK_GL_ERR;

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, im_w * im_h * sizeof(uint32_t), NULL, GL_STREAM_DRAW);
		void *dst_buf = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, im_w * im_h * sizeof(uint32_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);// | GL_MAP_UNSYNCHRONIZED_BIT);
		if(dst_buf) {
			pallet_blit_raw(dst_buf, texture_format, im_w * sizeof(uint32_t), src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, im_w, im_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}
		else {
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			pallet_blit_raw(fallback_pixel_buffer, texture_format, im_w * sizeof(uint32_t), src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));
			glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, im_w, im_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback_pixel_buffer); CHECK_GL_ERR;
			printf("Map failed\n");
		}

		glUseProgram(shader_program); CHECK_GL_ERR;
		glBindTexture(GL_TEXTURE_2D, upload_tex); CHECK_GL_ERR;
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);CHECK_GL_ERR;
		glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERR;
		glUseProgram(0);CHECK_GL_ERR;
#else
		glBindTexture(GL_TEXTURE_2D, upload_tex); CHECK_GL_ERR;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, im_w, im_h, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, src_buf); CHECK_GL_ERR;
		gl_pal_render(glpal, upload_tex);
		glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERR;
#endif
		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		gl_draw_string(buf);

		SDL_GL_SwapWindow(window);CHECK_GL_ERR;
		// printf("%6.1f FPS\n", 1000.0f / frametime);
#else
		SDL_Surface *screen = SDL_GetWindowSurface(window);
		pallet_blit_SDL(screen, src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));
		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		SDL_Surface *text_surface = render_string(buf);
		SDL_BlitSurface(text_surface, NULL, screen, &screen->clip_rect);
		SDL_UpdateWindowSurface(window);
		SDL_FreeSurface(text_surface);
#endif

		now = SDL_GetTicks();
		int newbeat = beat_get_count();

#ifdef GL_ACCEL_PAL
		if(newbeat != beats) gl_pal_start_switch(glpal, newbeat);
#else
		if(newbeat != beats) pal_ctx_start_switch(pal_ctx, newbeat);
#endif

		if((now - lastpalstep)*256/2048 > 0) { // want pallet switch to take ~2 seconds
#ifdef GL_ACCEL_PAL
			gl_pal_step(glpal, IMIN((now - lastpalstep)*256/2048, 32));
#else
			pal_ctx_step(pal_ctx, IMIN((now - lastpalstep)*256/2048, 32));
#endif
			lastpalstep = now;
		}

		if(newbeat != beats && now - last_beat_time > 1000) {
			last_beat_time = now;
			update_points(pd, (now - tick0), 1);
		}
		else update_points(pd, (now - tick0), 0);
		beats = newbeat;

		if(tick0+(maxfrms*1000)/opts.maxsrc_rate - now > 1000/opts.maxsrc_rate) {
			audio_data ad; audio_get_samples(&ad);
			maxsrc_update(maxsrc, ad.data, ad.len);
			audio_finish_samples();

			//tick0 + (maxfrms*1000)/opts.maxsrc_rate - now > 1000/opts.maxsrc_rate
			//tick0 - now + (maxfrms*1000 + 1000)/opts.maxsrc_rate > 0

			// maxfrms++;
			while((tick0+(maxfrms*1000)/opts.maxsrc_rate) - now > 1000/opts.maxsrc_rate) maxfrms++; // skip frames until we catch up so we don't race if we fall behind
		}

		now = SDL_GetTicks();

		const uint64_t target_frame_rate = 100;
		if(limit_fps) {
			if(now - fps_oldtime < 5) SDL_Delay(5 - (now - fps_oldtime)); // stay below 1000FPS

			now = SDL_GetTicks();
		}

		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = now;
		cnt++;
	}

	pal_ctx_delete(pal_ctx);
	maxsrc_delete(maxsrc);
	audio_shutdown();
	SDL_Quit();
	return 0;
}

#if NO_PARATASK
void pallet_blit_SDL(SDL_Surface *dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	const unsigned int src_stride = w;
	w = IMIN(w, dst->w);
	h = IMIN(h, dst->h);

	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	if(dst->format->BitsPerPixel == 32) pallet_blit32(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 16) pallet_blit565(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	else if(dst->format->BitsPerPixel == 15) pallet_blit555(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}
#else

#include "paratask/paratask.h"

struct pallet_blit_work_args {
	void (*task_fn)(uint8_t * restrict dest, unsigned int dst_stride,
	                const uint16_t *restrict src, unsigned int src_stride,
	                unsigned int w, unsigned int h,
	                const uint32_t *restrict pal);
	SDL_Surface *dst;
	const uint16_t* restrict src;
	int w, h;
	const uint32_t *restrict pal;
	size_t span;
};
static void paratask_func(size_t work_item_id, void *arg_)
{
	struct pallet_blit_work_args *a = arg_;

	const int ystart = work_item_id * a->span;
	const int yend   = IMIN(ystart + a->span, (unsigned int)a->h);
	a->task_fn((char *)a->dst->pixels + ystart*a->dst->pitch, a->dst->pitch, a->src + ystart*a->w, a->w, a->w, yend - ystart, a->pal);
}

void pallet_blit_SDL(SDL_Surface *dst, const uint16_t* restrict src, int w, int h, const uint32_t *restrict pal)
{
	static int first_time = 1;
	const unsigned int src_stride = w;
	int span = 2;

	if((SDL_MUSTLOCK(dst) && SDL_LockSurface(dst) < 0) || w < 0 || h < 0) return;
	if(first_time) { // the cpu dispatch code is not thread safe so first blit is done single threaded
		//TODO: do the first time thing per format?
		if(dst->format->BitsPerPixel == 32) pallet_blit32(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
		else if(dst->format->BitsPerPixel == 16) pallet_blit565(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
		else if(dst->format->BitsPerPixel == 15) pallet_blit555(dst->pixels, dst->pitch, src, src_stride, w, h, pal);
		first_time = 0;
	} else if(dst->format->BitsPerPixel != 8) {
		struct pallet_blit_work_args args = {
			NULL, dst, src, w, h, pal, span
		};
		if(dst->format->BitsPerPixel == 32) args.task_fn = pallet_blit32;
		else if(dst->format->BitsPerPixel == 16) args.task_fn = pallet_blit565;
		else if(dst->format->BitsPerPixel == 15) args.task_fn = pallet_blit555;

		paratask_call(paratask_default_instance(), 0, h/span, paratask_func, &args);
	}
	if(SDL_MUSTLOCK(dst)) SDL_UnlockSurface(dst);
}
#endif

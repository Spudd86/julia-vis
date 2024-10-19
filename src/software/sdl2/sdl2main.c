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

#include "software/softcore.h"

#include "opengl/glpallet.h"

#define USE_GL_UPLOAD 1
#define GL_ACCEL_PAL 1

void gl_draw_string(const char *str);
SDL_Surface* render_string(const char *str);

#if defined(GL_ACCEL_PAL)
struct glpal_ctx * pal_init_gles3(GLboolean float_packed_pixels);
#elif defined(USE_GL_UPLOAD)
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
#endif

int main(int argc, char **argv)
{
	opt_data opts;
	optproc(argc, argv, &opts);

	simple_soft_map_func map_func = SOFT_MAP_FUNC_NORMAL_INTERP;
	if(strcmp(opts.map_name, "rational") == 0) {
		if(opts.quality == 0) map_func = SOFT_MAP_FUNC_RATIONAL_INTERP;
		else if(opts.quality >= 1)  map_func = SOFT_MAP_FUNC_RATIONAL;
	}
	else if(strcmp(opts.map_name, "butterfly") == 0) map_func = SOFT_MAP_FUNC_BUTTERFLY;
	else if(opts.quality >= 1)  map_func = SOFT_MAP_FUNC_NORMAL;

	if(audio_init(&opts) < 0) exit(1);

	// SDL_SetHint(SDL_HINT_APP_NAME, "Julia Set Audio Visualizer");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION , "1");

	if(opts.w < 0 && opts.h < 0) opts.w = opts.h = 768;
	else if(opts.w < 0) opts.w = opts.h;
	else if(opts.h < 0) opts.h = opts.w;

	int win_w = opts.w ;
	int win_h = opts.h ;

	// TODO: option!
	// opts.w *= 2;
	// opts.h *= 2;

	struct softcore_ctx * core = softcore_init(opts.w, opts.h, map_func);

	int im_w, im_h;
	softcore_get_buffer_dims(core, &im_w, &im_h);
	printf("running with %dx%d bufs\n", im_w, im_h);

	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS);

#if defined(USE_GL_UPLOAD)
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

#if defined(USE_GL_UPLOAD)
	SDL_GLContext context = SDL_GL_CreateContext(window);
#ifdef GL_ACCEL_PAL
	struct glpal_ctx *glpal = pal_init_gles3(false);

	GLuint upload_tex = 0;
	glGenTextures(1, &upload_tex);
	glBindTexture(GL_TEXTURE_2D, upload_tex);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, im_w, im_h);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
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
	glBufferData(GL_PIXEL_UNPACK_BUFFER, im_w * im_h * sizeof(uint32_t), NULL, GL_STREAM_DRAW); CHECK_GL_ERR;
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0); CHECK_GL_ERR;

	GLuint upload_tex = 0;
	glGenTextures(1, &upload_tex);
	glBindTexture(GL_TEXTURE_2D, upload_tex);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, im_w, im_h);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
	// glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);
	glBindTexture(GL_TEXTURE_2D, 0);
	julia_vis_pixel_format texture_format = SOFT_PIX_FMT_RGBx8888;
	struct pal_ctx *pal_ctx = pal_ctx_pix_format_new(texture_format);
#endif

	CHECK_GL_ERR;

	SDL_GL_SetSwapInterval(0);

	glEnable(GL_FRAMEBUFFER_SRGB_EXT);
	glDisable(GL_DITHER);

	CHECK_GL_ERR;
#else
	// TODO: convert pixel format
	int window_sdl_pixformat = SDL_GetWindowPixelFormat(window);
	julia_vis_pixel_format texture_format = SOFT_PIX_FMT_BGRx8888;
	struct pal_ctx *pal_ctx = pal_ctx_pix_format_new(texture_format);
#endif

	int cnt = 0;
	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	uint32_t lastpalstep = tick0;
	uint32_t now = tick0;

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

		audio_data ad; audio_get_samples(&ad);
		const uint16_t *src_buf = softcore_render(core, now, tick0, beat_get_count(), ad.data, ad.len);
		audio_finish_samples();
		if(debug_maxsrc) src_buf = get_last_maxsrc_buffer(core);

#if defined(USE_GL_UPLOAD)
		glViewport(0, 0, win_w, win_h); CHECK_GL_ERR; ;
#ifndef GL_ACCEL_PAL
		glBindTexture(GL_TEXTURE_2D, upload_tex); CHECK_GL_ERR;

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
		void *dst_buf = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, im_w * im_h * sizeof(uint32_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		if(dst_buf) {
			pallet_blit_raw(dst_buf, texture_format, im_w * sizeof(uint32_t), src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im_w, im_h, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}
		else {
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			pallet_blit_raw(fallback_pixel_buffer, texture_format, im_w * sizeof(uint32_t), src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im_w, im_h, GL_RGBA, GL_UNSIGNED_BYTE, fallback_pixel_buffer); CHECK_GL_ERR;
			printf("Map failed\n");
		}

		glUseProgram(shader_program); CHECK_GL_ERR;
		glBindTexture(GL_TEXTURE_2D, upload_tex); CHECK_GL_ERR;
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);CHECK_GL_ERR;
		glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERR;
		glUseProgram(0);CHECK_GL_ERR;
#else
		glBindTexture(GL_TEXTURE_2D, upload_tex); CHECK_GL_ERR;
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im_w, im_h, GL_RED_INTEGER, GL_UNSIGNED_SHORT, src_buf); CHECK_GL_ERR;
		gl_pal_render(glpal, upload_tex);
		glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERR;
#endif
		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		gl_draw_string(buf);

		SDL_GL_SwapWindow(window);CHECK_GL_ERR;
#else
		SDL_Surface *screen = SDL_GetWindowSurface(window);
		if(!(SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0)) {
			pallet_blit_raw(screen->pixels, texture_format, screen->pitch, src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));
			if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		}
		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		SDL_Surface *text_surface = render_string(buf);
		SDL_BlitSurface(text_surface, NULL, screen, &screen->clip_rect);
		SDL_UpdateWindowSurface(window);
		SDL_FreeSurface(text_surface);
#endif
		int newbeat = beat_get_count();
#ifdef GL_ACCEL_PAL
		if(newbeat != beats) gl_pal_start_switch(glpal, newbeat);
#else
		if(newbeat != beats) pal_ctx_start_switch(pal_ctx, newbeat);
#endif
		beats = newbeat;

		if((now - lastpalstep)*256/2048 > 0) { // want pallet switch to take ~2 seconds
#ifdef GL_ACCEL_PAL
			gl_pal_step(glpal, IMIN((now - lastpalstep)*256/2048, 32));
#else
			pal_ctx_step(pal_ctx, IMIN((now - lastpalstep)*256/2048, 32));
#endif
			lastpalstep = now;
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
#ifndef GL_ACCEL_PAL
	pal_ctx_delete(pal_ctx);
#endif
	softcore_destroy(core);
	audio_shutdown();
	SDL_Quit();
	return 0;
}


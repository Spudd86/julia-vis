#include "common.h"
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
// #include <SDL2/SDL_opengl.h>

#include <GLES3/gl3.h>
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

#define CHECK_GL_ERR do { GLint glerr = glGetError(); while(glerr != GL_NO_ERROR) {\
	fprintf(stderr, "%s: In function '%s':\n%s:%d: Warning: %s\n", \
		__FILE__, __func__, __FILE__, __LINE__, gl_error_string(glerr)); \
		glerr = glGetError();\
		}\
	} while(0)

static char const* gl_error_string(GLenum const err)
{
  switch (err)
  {
    // opengl 2 errors (8)
    case GL_NO_ERROR:
      return "GL_NO_ERROR";

    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";

    case GL_INVALID_VALUE:
      return "GL_INVALID_VALUE";

    case GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION";

    case GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY";

    // opengl 3 errors (1)
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "GL_INVALID_FRAMEBUFFER_OPERATION";

    // gles 2, 3 and gl 4 error are handled by the switch above
    default:
    //   assert(!"unknown error");
      return NULL;
  }
}

struct glpal_ctx * pal_init_gles3(GLboolean float_packed_pixels);

static const char *vtx_shader_src =
	"#version 300 es\n"
	// "attribute vec2 vertex;\n"
	"layout(location = 0) in vec4 vertex;"
	"out vec2 uv;"
	"void main() {\n"
	"	uv = vertex.xy;\n"
	"	gl_Position = vec4(vertex.zw, 0. , 1. );\n"
	// "	gl_Position = vec4((vertex.zw*0.5+0.5)*gl_viewportDimensions.xy, 0.0f, 1.0f);\n"
	// "	gl_Position = vec4((vertex*0.5+0.5)*viewportDimensions.xy, 0.0f, 1.0f);\n"
	"}";
static const char *frag_shader_src =
	"#version 300 es\n"
	"#ifdef GL_ES\n"
	"precision highp float;\n"
	"#endif\n"

	// "varying vec2 uv;\n"
	"uniform sampler2D src;\n"
	"in vec2 uv;\n"
	"out vec4 fragColour;\n"
	"vec4 gamma_curve(vec4 c) {\n"
	"	return mix(c * 12.92, 1.055*pow(c, vec4(1.0/2.4)) - 0.055, greaterThan(c, vec4(0.0031308)));"
	"}\n"
	"void main() {\n"
	// "	fragColour = texture(src, uv) + vec4(uv*0.5, 0, 1);\n"
	// "	fragColour = gamma_curve(texture(src, uv));\n"
	"	fragColour = texture(src, uv);\n"
	// "	fragColour = vec4(uv, 0, 1);\n"
	// "	gl_FragColor = texture(src, gl_FragCoord.xy / viewportDimensions.xy);\n"
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
	                                      opts.w, opts.h,
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

	// SDL_GLContext glcontext = SDL_GL_CreateContext(window);
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if(!renderer)
	{
		printf("Renderer creation failed\n");
		return 1;
	}

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

	SDL_GL_SetSwapInterval(-1);

	// julia_vis_pixel_format texture_format = SOFT_PIX_FMT_xRGB8888;

	julia_vis_pixel_format texture_format = SOFT_PIX_FMT_xRGB8888;
	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_STREAMING, opts.w, opts.h);

	struct Pixbuf screen = {
		.w = opts.w, .h = opts.h,
		.pitch = opts.w * sizeof(uint32_t), .bpp = 32,
		.format = texture_format,
		// .data = aligned_alloc(512, 512 + opts.w * opts.h * sizeof(uint32_t))
		.data = NULL
	};
#elif defined(USE_GL_UPLOAD)
	//TODO: use GL pallet code
	SDL_GLContext context = SDL_GL_CreateContext(window);
#ifdef GL_ACCEL_PAL
	struct glpal_ctx *glpal = pal_init_gles3(false);
#else
	GLuint shader_program = glCreateProgram();
	GLuint vertex_shader   = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	GLchar *info_log = NULL;
	GLint  log_length;
	GLint compiled = GL_FALSE;

	glShaderSource(vertex_shader, 1, (const GLchar**)&vtx_shader_src, NULL);
	glShaderSource(fragment_shader, 1, (const GLchar**)&frag_shader_src, NULL);

	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
	
	glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &log_length);
	if(log_length != 0) {
		info_log = (GLchar *) malloc(log_length * sizeof(GLchar));
		glGetShaderInfoLog(vertex_shader, log_length, &log_length, info_log);
		printf("%s\n", info_log);
		free(info_log);
		info_log = NULL;
	}

	if(!compiled) {
		printf("vertex shader compile failed");
		return 1;	
	}

	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compiled);
	
	glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &log_length);
	if(log_length != 0) {
		info_log = (GLchar *) malloc(log_length * sizeof(GLchar));
		glGetShaderInfoLog(fragment_shader, log_length, &log_length, info_log);
		printf("%s\n", info_log);
		free(info_log);
		info_log = NULL;
	}

	if(!compiled) {
		printf("fragment shader compile failed");
		return 1;	
	}
	
	// Program keeps shaders alive after they are attached
	glAttachShader(shader_program, vertex_shader); glDeleteShader(vertex_shader);
	glAttachShader(shader_program, fragment_shader); glDeleteShader(fragment_shader);
	glLinkProgram(shader_program);
	GLint linked = GL_FALSE;
	glGetProgramiv(shader_program, GL_LINK_STATUS, &linked);

	glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &log_length);
	if(log_length != 0) {
		info_log = (GLchar *) malloc(log_length * sizeof(GLchar));
		glGetProgramInfoLog(shader_program, log_length, &log_length, info_log);
		printf("%s\n", info_log);
		free(info_log);
		info_log = NULL;
	}

	if (!linked) {
		printf("Failed Shader compile:\nvertex:\n%s\nfragment:\n%s\n", vtx_shader_src, frag_shader_src);
		return 1;
	}

	glUseProgram(shader_program);
	glUniform1i(glGetUniformLocation(shader_program, "src"), 0);
	glUseProgram(0);

	uint8_t *fallback_pixel_buffer = aligned_alloc(512, 512 + im_w * im_h * sizeof(uint32_t));


	
	GLuint pbos[2];

	glGenBuffers(2, pbos);

	for(int i=0; i<2; i++)
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[i]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, opts.w * opts.h * sizeof(uint32_t), NULL, GL_STREAM_DRAW);
	}
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	CHECK_GL_ERR;
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

	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;
	int lastframe_key = 0;

	SDL_Event event;
	memset(&event, 0, sizeof(event));
	while(SDL_PollEvent(&event) >= 0) {
		if(event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) break;
		if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)) { if(!lastframe_key) { debug_maxsrc = !debug_maxsrc; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2)) { if(!lastframe_key) { debug_pal = !debug_pal; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3)) { if(!lastframe_key) { show_mandel = !show_mandel; } lastframe_key = 1; }
		else if((event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F4)) { if(!lastframe_key) { show_fps_hist = !show_fps_hist; } lastframe_key = 1; }
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

		glViewport(0, 0, im_w, im_h); CHECK_GL_ERR; ;

#ifndef GL_ACCEL_PAL
		glBindTexture(GL_TEXTURE_2D, upload_tex); CHECK_GL_ERR;

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[(m+1)&0x1]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, im_w * im_h * sizeof(uint32_t), NULL, GL_STREAM_DRAW);
		void *dst_buf = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, im_w * im_h * sizeof(uint32_t), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);// | GL_MAP_UNSYNCHRONIZED_BIT);
		if(dst_buf) {
			pallet_blit_raw(dst_buf, texture_format, im_w * sizeof(uint32_t), src_buf, im_w, im_h, pal_ctx_get_active(pal_ctx));
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[m]);
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

		static const float verts[] = {
			0, 0 , -1, -1,
			1, 0 ,  1, -1,
			0, 1 , -1,  1,
			1, 1 ,  1,  1
		};
		glUseProgram(shader_program); CHECK_GL_ERR;
		glBindTexture(GL_TEXTURE_2D, upload_tex); CHECK_GL_ERR;
		glEnableVertexAttribArray(0); CHECK_GL_ERR;
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float)*4, verts);CHECK_GL_ERR;
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);CHECK_GL_ERR;
		glDisableVertexAttribArray(0);CHECK_GL_ERR;
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

		if((tick0+(maxfrms*1000)/opts.maxsrc_rate) - now > 1000/opts.maxsrc_rate) {
			audio_data ad; audio_get_samples(&ad);
			maxsrc_update(maxsrc, ad.data, ad.len);
			audio_finish_samples();
			maxfrms++;
		}

		now = SDL_GetTicks();
		//if(now - fps_oldtime < 10) SDL_Delay(10 - (now - fps_oldtime)); // stay below 1000FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = SDL_GetTicks();
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
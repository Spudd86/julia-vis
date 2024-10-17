#include "common.h"
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles2.h>
// #include <SDL2/SDL_opengl.h>

#include <GLES3/gl3.h>

#include "points.h"

#include "audio/audio.h"

#include "opengl/gles3/gles3misc.h"
#include "opengl/glpallet.h"
#include "opengl/glmaxsrc.h"
#include "opengl/gles3/gles_fract.h"

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
	"vec4 linearize(vec4 c) {\n"
	// "	if(c <= 0.04045f) c = c/12.92f;\n"
	// "	else c = pow((c+0.055)/1.055, 2.4);\n"
	"	return mix(c/12.92f, pow((c+0.055)/1.055, vec4(2.4)), greaterThan(c, vec4(0.04045)));\n"
	"}\n"
	"void main() {\n"
	"	fragColour = linearize(texture(src, uv));\n"
	"}";


void gl_draw_string(const char *str);
SDL_Surface* render_string(const char *str);

struct glpal_ctx * pal_init_gles3(GLboolean float_packed_pixels);
struct glmaxsrc_ctx *maxsrc_new_gles3(int width, int height);

#define IM_SIZE (512)

static opt_data opts;

int main(int argc, char **argv)
{
	optproc(argc, argv, &opts);
	// if(strcmp(opts.map_name, "rational") == 0) {
	// 	if(opts.quality == 0) map_func = soft_map_rational_interp;
	// 	else if(opts.quality >= 1)  map_func = soft_map_rational;
	// }
	// else if(strcmp(opts.map_name, "butterfly") == 0) map_func = soft_map_butterfly;
	// else if(opts.quality >= 1)  map_func = soft_map;
	if(audio_init(&opts) < 0) exit(1);

	// SDL_SetHint(SDL_HINT_APP_NAME, "Julia Set Audio Visualizer");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION , "1");

	if(opts.w < 0 && opts.h < 0) opts.w = opts.h = IM_SIZE;
	else if(opts.w < 0) opts.w = opts.h;
	else if(opts.h < 0) opts.h = opts.w;

	int im_w = opts.w - opts.w%16, im_h = opts.h - opts.h%16;
	printf("running with %dx%d bufs\n", im_w, im_h);


	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS);


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

	SDL_Window *window = SDL_CreateWindow("Julia Set Audio Visualizer",
	                                      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
	                                      opts.w, opts.h,
	                                      SDL_WINDOW_OPENGL );
	if(!window)
	{
		printf("Window creation failed\n");
		return 1;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);

	printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));
	printf("GL_RENDERER: %s\n", glGetString(GL_RENDERER));
	printf("GL_VERSION: %s\n", glGetString(GL_VERSION));
	printf("GL_SL_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("GL_EXTENSIONS: %s\n", glGetString(GL_EXTENSIONS));
	printf("\n\n");

	struct glfract_ctx *glfract = fractal_gles_init(GLES_MAP_FUNC_NORMAL, im_w, im_h);
	struct glmaxsrc_ctx *glmaxsrc = maxsrc_new_gles3(im_w, im_h);
	struct glpal_ctx *glpal = pal_init_gles3(false);
	struct point_data *pd = new_point_data(4);

	SDL_GL_SetSwapInterval(0);

	GLuint shader_program = compile_program_defs(NULL, vtx_shader_src, frag_shader_src);
	glUseProgram(shader_program); CHECK_GL_ERR;
	glUniform1i(glGetUniformLocation(shader_program, "src"), 0); CHECK_GL_ERR;
	glUseProgram(0); CHECK_GL_ERR;

	glEnable(GL_FRAMEBUFFER_SRGB_EXT);
	glDisable(GL_DITHER);
	CHECK_GL_ERR;

	Uint32 tick0, fps_oldtime;
	fps_oldtime = tick0 = SDL_GetTicks();
	float frametime = 100;
	int beats = beat_get_count();
	uint32_t last_beat_time = tick0;
	uint32_t lastpalstep = tick0;
	uint32_t now = tick0;
	uint32_t maxfrms = 0;
	uint64_t cnt = 0;

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

		CHECK_GL_ERR;

		render_fractal(glfract, pd, maxsrc_get_tex(glmaxsrc));
		gl_pal_render(glpal, fract_get_tex(glfract));

		// gl_pal_render(glpal, maxsrc_get_tex(glmaxsrc));

		// glUseProgram(shader_program); CHECK_GL_ERR;
		// glBindTexture(GL_TEXTURE_2D, maxsrc_get_tex(glmaxsrc)); CHECK_GL_ERR;
		// // glBindTexture(GL_TEXTURE_2D, fract_get_tex(glfract)); CHECK_GL_ERR;
		// glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);CHECK_GL_ERR;
		// glBindTexture(GL_TEXTURE_2D, 0); CHECK_GL_ERR;
		// glUseProgram(0);CHECK_GL_ERR;

		char buf[32];
		sprintf(buf,"%6.1f FPS", 1000.0f / frametime);
		gl_draw_string(buf);

		SDL_GL_SwapWindow(window);

		CHECK_GL_ERR;

		now = SDL_GetTicks();
		int newbeat = beat_get_count();

		if(newbeat != beats) gl_pal_start_switch(glpal, newbeat);

		if((now - lastpalstep)*256/2048 > 0) { // want pallet switch to take ~2 seconds
			gl_pal_step(glpal, IMIN((now - lastpalstep)*256/2048, 32));
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
			maxsrc_update(glmaxsrc, ad.data, ad.len);
			audio_finish_samples();
			maxfrms++;
		}

		now = SDL_GetTicks();
		//if(now - fps_oldtime < 10) SDL_Delay(10 - (now - fps_oldtime)); // stay below 1000FPS
		frametime = 0.02f * (now - fps_oldtime) + (1.0f - 0.02f) * frametime;
		fps_oldtime = SDL_GetTicks();
		cnt++;
	}

	audio_shutdown();
	SDL_Quit();
	return 0;
}

uint32_t get_ticks(void) {
	return SDL_GetTicks();
}

void dodelay(uint32_t ms) {
	SDL_Delay(ms);
}

uint64_t uget_ticks(void) {
	return (uint64_t)SDL_GetTicks() * 1000;
}

void udodelay(uint64_t us) {
	SDL_Delay(us/1000);
}

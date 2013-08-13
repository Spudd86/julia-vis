/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Based on a egl cube test app originally written by Arvin Schnell */

#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <poll.h>
#include <termios.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <EGL/egl.h>

#include "opengl/glmisc.h"
#include "audio/audio.h"
#include "opengl/fpsservo/fpsservo.h"

void render_frame(GLboolean debug_maxsrc, GLboolean debug_pal, GLboolean show_mandel, GLboolean show_fps_hist);
void init_gl(const opt_data *opt_data, int width, int height);

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


static struct {
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
	EGLSurface surface;
} gl;

static struct {
	struct gbm_device *dev;
	struct gbm_surface *surface;
} gbm;

static struct {
	int fd;
	drmModeModeInfo *mode;
	drmModeCrtc *saved_crtc;
	uint32_t crtc_id;
	uint32_t connector_id;
} drm;

struct drm_fb {
	struct gbm_bo *bo;
	uint32_t fb_id;
};

static int init_drm(opt_data *opts, int im_size)
{
	printf("Initializing DRM.\n");
	static const char *modules[] = {
			"i915", "radeon", "nouveau", "vmwgfx", "omapdrm", "exynos"
	};
	drmModeRes *resources;
	drmModeConnector *connector = NULL;
	drmModeEncoder *encoder = NULL;
	int i, area;

	for (i = 0; i < ARRAY_SIZE(modules); i++) {
		printf("trying to load module %s...", modules[i]);
		drm.fd = drmOpen(modules[i], NULL);
		if (drm.fd < 0) {
			printf("failed.\n");
		} else {
			printf("success.\n");
			break;
		}
	}

	if (drm.fd < 0) {
		printf("could not open drm device\n");
		return -1;
	}

	resources = drmModeGetResources(drm.fd);
	if (!resources) {
		printf("drmModeGetResources failed: %s\n", strerror(errno));
		return -1;
	}

	/* find a connected connector: */
	for (i = 0; i < resources->count_connectors; i++) {
		connector = drmModeGetConnector(drm.fd, resources->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			/* it's connected, let's use this! */
			break;
		}
		drmModeFreeConnector(connector);
		connector = NULL;
	}

	if (!connector) {
		/* we could be fancy and listen for hotplug events and wait for
		 * a connector..
		 */
		printf("no connected connector!\n");
		return -1;
	}
	
	// find a mode to use
	//TODO: figure out how to set a better mode
	if(opts->w < 0 && opts->h < 0) opts->h = im_size;
/*	drmModeModeInfo *modes = connector->modes;*/
/*	for (int i=0; i < connector->count_modes; i++) {*/
/*		printf("  %d x %d\n", modes[i].hdisplay, modes[i].vdisplay);*/
/*		if(modes[i].hdisplay >= opts->w && modes[i].vdisplay >= opts->h)*/
/*			drm.mode = modes + i;*/
/*	}*/
	if(connector->count_modes > 0) // just pick the first mode for now
		drm.mode = connector->modes;
	if (!drm.mode) {
		printf("could not find mode!\n");
		return -1;
	}
	
	printf("Picked Mode:\nclock: %" PRId32 "\n", drm.mode->clock);
	printf("\th pixels/sync start+end/total/skew %" PRId16 " %" PRId16 " %" PRId16 " %" PRId16" %" PRId16 "\n",
	       drm.mode->hdisplay, drm.mode->hsync_start, drm.mode->hsync_end, drm.mode->htotal, drm.mode->hskew);
	printf("\tv pixels/sync start+end/total/scan %" PRId16 " %" PRId16 " %" PRId16 " %" PRId16" %" PRId16 "\n",
	       drm.mode->vdisplay, drm.mode->vsync_start, drm.mode->vsync_end, drm.mode->vtotal, drm.mode->vscan);
	printf("\tvrefresh: %" PRId32 "\n", drm.mode->vrefresh);
	printf("\tswap freq = %" PRId32 "/%" PRId32 "\n\n", drm.mode->clock*1000, (uint32_t)drm.mode->htotal*drm.mode->vtotal);
	// fpsservo wants (struct fps_period){ .n = drm.mode->clock/1000, .d = (uint32_t)drm.mode->htotal*drm.mode->vtotal };

	/* find encoder: */
	for (i = 0; i < resources->count_encoders; i++) {
		encoder = drmModeGetEncoder(drm.fd, resources->encoders[i]);
		if (encoder->encoder_id == connector->encoder_id)
			break;
		drmModeFreeEncoder(encoder);
		encoder = NULL;
	}

	if (!encoder) {
		printf("no encoder!\n");
		return -1;
	}
	
	// save setup so we can restore it when we are done
	drm.saved_crtc = drmModeGetCrtc(drm.fd, encoder->crtc_id);

	drm.crtc_id = encoder->crtc_id;
	drm.connector_id = connector->connector_id;

	return 0;
}

static int init_gbm(void)
{
	gbm.dev = gbm_create_device(drm.fd);

	gbm.surface = gbm_surface_create(gbm.dev,
			drm.mode->hdisplay, drm.mode->vdisplay,
			GBM_FORMAT_XRGB8888,
			GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	if (!gbm.surface) {
		printf("failed to create gbm surface\n");
		return -1;
	}

	return 0;
}

static int init_egl(void)
{
	EGLint major, minor, n;
	GLint ret;

	static const EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_ALPHA_SIZE, 0,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_NONE
	};

	gl.display = eglGetDisplay(gbm.dev);

	if (!eglInitialize(gl.display, &major, &minor)) {
		printf("failed to initialize\n");
		return -1;
	}

	printf("Using display %p with EGL version %d.%d\n",
			gl.display, major, minor);

	printf("EGL Version \"%s\"\n", eglQueryString(gl.display, EGL_VERSION));
	printf("EGL Vendor \"%s\"\n", eglQueryString(gl.display, EGL_VENDOR));
	printf("EGL Extensions \"%s\"\n", eglQueryString(gl.display, EGL_EXTENSIONS));

	if (!eglBindAPI(EGL_OPENGL_API)) {
		printf("failed to bind api EGL_OPENGL_API\n");
		return -1;
	}

	if (!eglChooseConfig(gl.display, config_attribs, &gl.config, 1, &n) || n != 1) {
		printf("failed to choose config: %d\n", n);
		return -1;
	}

	gl.context = eglCreateContext(gl.display, gl.config, EGL_NO_CONTEXT, NULL);
	if (gl.context == NULL) {
		printf("failed to create context\n");
		return -1;
	}

	gl.surface = eglCreateWindowSurface(gl.display, gl.config, gbm.surface, NULL);
	if (gl.surface == EGL_NO_SURFACE) {
		printf("failed to create egl surface\n");
		return -1;
	}

	/* connect the context to the surface */
	eglMakeCurrent(gl.display, gl.surface, gl.surface, gl.context);

	return 0;
}

static void
drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
	struct drm_fb *fb = data;
	struct gbm_device *gbm = gbm_bo_get_device(bo);

	if (fb->fb_id)
		drmModeRmFB(drm.fd, fb->fb_id);

	free(fb);
}

static struct drm_fb * drm_fb_get_from_bo(struct gbm_bo *bo)
{
	struct drm_fb *fb = gbm_bo_get_user_data(bo);
	uint32_t width, height, stride, handle;
	int ret;

	if (fb)
		return fb;

	fb = calloc(1, sizeof *fb);
	fb->bo = bo;

	width = gbm_bo_get_width(bo);
	height = gbm_bo_get_height(bo);
	stride = gbm_bo_get_stride(bo);
	handle = gbm_bo_get_handle(bo).u32;

	ret = drmModeAddFB(drm.fd, width, height, 24, 32, stride, handle, &fb->fb_id);
	if (ret) {
		printf("failed to create fb: %s\n", strerror(errno));
		free(fb);
		return NULL;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);

	return fb;
}

static void vblank_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	//printf("vblank: %d %d.%06d\n", frame, sec, usec);
	
	drmVBlank vbl;
	vbl.request.type = DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT;
	vbl.request.sequence = 1;
	vbl.request.signal = (unsigned long)data;
	drmWaitVBlank(drm.fd, &vbl);
	
	//looks like we can do stuff with frame == msc here
}

static void page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	//printf("swap:   %d %d.%06d\n", frame, sec, usec);

	int *waiting_for_flip = data;
	*waiting_for_flip = 0;
}

static struct gbm_bo *next_bo = NULL;
static struct drm_fb *fb = NULL;
static int waiting_for_flip;
static struct termios save_term;

static drmEventContext evctx = {
		.version = DRM_EVENT_CONTEXT_VERSION,
		//TODO: look this up, see if we can use the data passed into it to run the fpsservo code
		.vblank_handler = vblank_handler,
		.page_flip_handler = page_flip_handler, 
};

static void shutdown_cleanup(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &save_term);
	
	fprintf(stderr, "wait for pending page-flip to complete...\n");
	while(waiting_for_flip) {
		if(drmHandleEvent(drm.fd, &evctx)) break;
	}
	
	drmModeSetCrtc(drm.fd,
		       drm.saved_crtc->crtc_id,
		       drm.saved_crtc->buffer_id,
		       drm.saved_crtc->x,
		       drm.saved_crtc->y,
		       &drm.connector_id,
		       1,
		       &drm.saved_crtc->mode);
	drmModeFreeCrtc(drm.saved_crtc);
}

int main(int argc, char *argv[])
{
	struct opt_data opts; optproc(argc, argv, &opts);
	if(audio_init(&opts) < 0) exit(1);
	if(opts.w < 0 && opts.h < 0) opts.w = opts.h = 512;
	else if(opts.w < 0) opts.w = opts.h;
	else if(opts.h < 0) opts.h = opts.w;
	
	int ret;

	ret = init_drm(&opts, 512);
	if (ret) {
		printf("failed to initialize DRM\n");
		return ret;
	}

	ret = init_gbm();
	if (ret) {
		printf("failed to initialize GBM\n");
		return ret;
	}

	ret = init_egl();
	if (ret) {
		printf("failed to initialize EGL\n");
		return ret;
	}
	
	// call init_gl
	//init_gl(&opts, drm.mode->hdisplay, drm.mode->vdisplay);
	init_gl(&opts, opts.w, opts.h); //TODO: better dealing with our great big screen
	
	eglSwapBuffers(gl.display, gl.surface);
	struct gbm_bo *bo = gbm_surface_lock_front_buffer(gbm.surface);
	fb = drm_fb_get_from_bo(bo);

	/* set mode: */
	ret = drmModeSetCrtc(drm.fd, drm.crtc_id, fb->fb_id, 0, 0,
			&drm.connector_id, 1, drm.mode);
	if (ret) {
		printf("failed to set mode: %s\n", strerror(errno));
		return ret;
	}
	
	// turn off line buffering on input and echoing of characters
	struct termios term;
	tcgetattr(STDIN_FILENO, &save_term);
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON|ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    
    atexit(shutdown_cleanup);
	
	drmVBlank vbl;
	/* Get current count first hopefully also sync up with blank too*/
	vbl.request.type = DRM_VBLANK_RELATIVE;
	vbl.request.sequence = 1;
	ret = drmWaitVBlank(drm.fd, &vbl);
	if (ret != 0) {
		printf("drmWaitVBlank (relative, event) failed ret: %i\n", ret);
		return -1;
	}
	printf("starting msc: %d\n", vbl.request.sequence);
	
	/* Queue an event for frame + 1 */
	vbl.request.type = DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT;
	vbl.request.sequence = 1;
	vbl.request.signal = NULL;
	ret = drmWaitVBlank(drm.fd, &vbl);
	if (ret != 0) {
		printf("drmWaitVBlank (relative, event) failed ret: %i\n", ret);
		return -1;
	}
	
	struct pollfd pfds[] = {
		{drm.fd, POLLIN | POLLPRI, 0 },
		{STDIN_FILENO, POLLIN, 0 },
	};

	int debug_maxsrc = 0, debug_pal = 0, show_mandel = 0, show_fps_hist = 0;
	bool done = false;
	while (!done) {

		render_frame(debug_maxsrc, debug_pal, show_mandel, show_fps_hist);

		while (waiting_for_flip) { // TODO: input handling			
			ret = poll(pfds, 2, -1);
			if (ret < 0) {
				printf("poll err: %s\n", strerror(errno));
				return ret;
			} else if (ret == 0) {
				printf("poll timeout!\n");
				done = true;
				break;
			} 
			
			if (pfds[1].revents) {
				char buf[128];
				int cnt = read(STDIN_FILENO, buf, sizeof(buf));
				if(buf[0] == 27) done = true;
				else if(buf[0] == '1') debug_maxsrc = !debug_maxsrc;
				else if(buf[0] == '2') debug_pal = !debug_pal;
				else if(buf[0] == '3') show_mandel = !show_mandel;
				else if(buf[0] == '4') show_fps_hist = !show_fps_hist;
				//continue;
			}
			
			if(pfds[0].revents)
				drmHandleEvent(drm.fd, &evctx);
		}

		/* release last buffer to render on again: */
		gbm_surface_release_buffer(gbm.surface, bo);
		bo = next_bo;
	}
	
	audio_shutdown();

	return ret;
}

void swap_buffers(void) 
{
	eglSwapBuffers(gl.display, gl.surface);
	next_bo = gbm_surface_lock_front_buffer(gbm.surface);
	fb = drm_fb_get_from_bo(next_bo);
	
	waiting_for_flip = 1;
	
	int ret = drmModePageFlip(drm.fd, drm.crtc_id, fb->fb_id,
			DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
	if(ret) {
		printf("failed to queue page flip: %s\n", strerror(errno));
		exit(-1);
	}
}

void render_debug_overlay(void) {
	
}

static int timeval_subtract(struct timeval *result, struct timeval x, struct timeval y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if(x.tv_usec < y.tv_usec) {
		int nsec = (y.tv_usec - x.tv_usec) / 1000000 + 1;
		y.tv_usec -= 1000000 * nsec;
		y.tv_sec += nsec;
	}
	if(x.tv_usec - y.tv_usec > 1000000) {
		int nsec = (x.tv_usec - y.tv_usec) / 1000000;
		y.tv_usec += 1000000 * nsec;
		y.tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	tv_usec is certainly positive. */
	result->tv_sec = x.tv_sec - y.tv_sec;
	result->tv_usec = x.tv_usec - y.tv_usec;

	/* Return 1 if result is negative. */
	return x.tv_sec < y.tv_sec;
}

static struct timeval starttime = {0, 0};

uint64_t uget_ticks(void) {
	if(!starttime.tv_sec) {
		gettimeofday(&starttime, NULL);
		return 0;
	}

	struct timeval now, tv;
	gettimeofday(&now, NULL);
	timeval_subtract(&tv, now, starttime);
	return ((uint64_t)tv.tv_sec*1000*1000 + (uint64_t)tv.tv_usec);
}

void udodelay(uint64_t us) {
	usleep(us);
}

uint32_t get_ticks(void) {
	return uget_ticks() / 1000;
	//return (uint32_t)(tv.tv_sec*1000 + tv.tv_usec/1000);
}

void dodelay(uint32_t ms) {
	usleep(ms*1000);
}


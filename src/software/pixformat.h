#ifndef JULIA_VIS_PIXEL_FORMAT_H__
#define JULIA_VIS_PIXEL_FORMAT_H__ 1

// refers to order inside a uint32_t
typedef enum {
	SOFT_PIX_FMT_RGBx101010, // FIXME: add support for this
	SOFT_PIX_FMT_BGRx101010, // FIXME: add support for this

	SOFT_PIX_FMT_RGBx8888,
	SOFT_PIX_FMT_BGRx8888,
	SOFT_PIX_FMT_xRGB8888,
	SOFT_PIX_FMT_xBGR8888,

	SOFT_PIX_FMT_RGB565,
	SOFT_PIX_FMT_BGR565,
	SOFT_PIX_FMT_RGB555,
	SOFT_PIX_FMT_BGR555,

	SOFT_PIX_FMT_8_xRGB_PAL,
	SOFT_PIX_FMT_8_xBGR_PAL,
	SOFT_PIX_FMT_8_RGBx_PAL,
	SOFT_PIX_FMT_8_BGRx_PAL,

	SOFT_PIX_FMT_NONE = -1

} julia_vis_pixel_format;

typedef struct Pixbuf {
	uint16_t w, h;
	int pitch;
	int bpp;
	julia_vis_pixel_format format;
	void *data;
} Pixbuf;

struct pal_ctx *pal_ctx_pix_format_new(julia_vis_pixel_format format);

#endif
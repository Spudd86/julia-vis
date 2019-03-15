#include "common.h"

#include "element2.h"

#include "software/simple_main.h"

// gst-launch-1.0.exe -v filesrc location=<filename> ! decodebin ! audioconvert ! audioresample ! tee name=t ! queue ! autoaudiosink
// t. ! queue ! audioconvert  ! julia_vis ! video/x-raw,width=512,height=512,framerate=30/1 ! queue ! autovideosink


//TODO: take a look at visualization plugins in gstreamer 1.6
// particularly wavescope/spacescope and see how hard it would be
// to avoid using GstAudioVisualizer2

//TODO: work out if playbin is doing something to end up with a more efficient
// pipeline, if so see if we can replicate it via gst-launch

//TODO: write a simple player using GstPlayer and it's visualisation support?

enum
{
  PROP_0,
  PROP_MAP_FUNC
};


GST_DEBUG_CATEGORY_EXTERN (julia_vis_debug);
#define GST_CAT_DEFAULT (julia_vis_debug)

#if 0
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (" { "
#if G_BYTE_ORDER == G_BIG_ENDIAN
		"\"xRGB\", "
#else
		"\"BGRx\", "
#endif
		"\"RGB16\", "
		"\"RGB15\" } "))
);
#else
static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
	GST_PAD_SRC,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (" { "
		"\"xRGB\", "
		"\"xBGR\", "
		"\"RGBx\", "
		"\"BGRx\", "
		"\"RGB16\", "
		"\"RGB15\" "
		// "\"RGB8P\", " //palleted 
		" } "
		))
);
#endif

//TODO: add support for RGB and BGR and pass flags to our pallet init depending on which one is in use
static GstStaticPadTemplate sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
	GST_PAD_SINK,
	GST_PAD_ALWAYS,
	GST_STATIC_CAPS ("audio/x-raw, "
		"format = (string) " GST_AUDIO_NE(F32) ", "
		"layout = (string) interleaved, "
		"rate = (int) [ 8000, 96000 ], "
		"channels = (int) { 1, 2 } ")
	);

static GstElementClass *parent_class = NULL;

static void gst_julia_vis_init (GstJuliaVis * visual);
static void gst_julia_vis_finalize (GObject * object);

static gboolean gst_julia_vis_setup (GstAudioVisualizer2 * bscope);
static gboolean gst_julia_vis_add_audio(GstAudioVisualizer2 *bscope, GstBuffer * audio);
static gboolean gst_julia_vis_render (GstAudioVisualizer2 * bscope, GstVideoFrame * video);
static void gst_julia_vis_frame_dropped (GstAudioVisualizer2 * bscope);

#define GST_JULIA_VIS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GST_TYPE_JULIA_VIS, GstJuliaVisPrivate))

struct _GstJuliaVisPrivate
{
	struct simple_soft_ctx *julia_vis_ctx;

	uint64_t frame_count;
	int64_t frame_duration;

	GstJuliaVisMapFunc map_func;
};

static void
gst_julia_vis_init (GstJuliaVis *self)
{
	self->priv = GST_JULIA_VIS_GET_PRIVATE (self);
	self->priv->frame_count = 0;
	self->priv->frame_duration = 0;
	self->priv->julia_vis_ctx = NULL;
	self->priv->map_func = GST_JULIA_VIS_MAP_FUNC_NORMAL_INTERP;
}

static void
gst_julia_vis_finalize (GObject * object)
{
	GstJuliaVis *self = GST_JULIA_VIS(object);
	if(self->priv->julia_vis_ctx) simple_soft_destroy(self->priv->julia_vis_ctx);

	GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

// formats we can support

//little endian
// GST_VIDEO_FORMAT_RGBx sparse rgb packed into 32 bit, space last
// GST_VIDEO_FORMAT_BGRx sparse reverse rgb packed into 32 bit, space last

// big endian
// GST_VIDEO_FORMAT_xRGB sparse rgb packed into 32 bit, space first
// GST_VIDEO_FORMAT_xBGR sparse reverse rgb packed into 32 bit, space first

// both
// GST_VIDEO_FORMAT_RGB16 rgb 5-6-5 bits per component
// GST_VIDEO_FORMAT_BGR16 reverse rgb 5-6-5 bits per component
// GST_VIDEO_FORMAT_RGB15 rgb 5-5-5 bits per component
// GST_VIDEO_FORMAT_BGR15 reverse rgb 5-5-5 bits per component

// TODO: on this one
// GST_VIDEO_FORMAT_RGB8P 8-bit paletted RGB

static julia_vis_pixel_format get_vis_format(GstVideoFormat gst_format)
{
	//TODO: possibly examine vinfo.format to determine this based on shifts
	julia_vis_pixel_format vis_format;
#if G_BYTE_ORDER == G_BIG_ENDIAN
	switch(gst_format) {
		case GST_VIDEO_FORMAT_xRGB:
			break;
		case GST_VIDEO_FORMAT_xBGR:
			bswap = 1;
			break;

		case GST_VIDEO_FORMAT_RGB16:
		case GST_VIDEO_FORMAT_BGR16:

		case GST_VIDEO_FORMAT_RGB15:
		case GST_VIDEO_FORMAT_BGR15:
		default:
			vis_format = SOFT_PIX_FMT_NONE;
			break;
	}
#else
	switch(gst_format) {
		case GST_VIDEO_FORMAT_RGBx:
			vis_format = SOFT_PIX_FMT_RGBx8888;
			break;
		case GST_VIDEO_FORMAT_BGRx:
			vis_format = SOFT_PIX_FMT_BGRx8888;
			break;
		case GST_VIDEO_FORMAT_xRGB:
			vis_format = SOFT_PIX_FMT_xRGB8888;
			break;
		case GST_VIDEO_FORMAT_xBGR:
			vis_format = SOFT_PIX_FMT_xBGR8888;
			break;
		case GST_VIDEO_FORMAT_RGB16:
			vis_format = SOFT_PIX_FMT_RGB565;
			break;
		case GST_VIDEO_FORMAT_BGR16:
			vis_format = SOFT_PIX_FMT_BGR565;
			break;
		case GST_VIDEO_FORMAT_RGB15:
			vis_format = SOFT_PIX_FMT_RGB555;
			break;
		case GST_VIDEO_FORMAT_BGR15:
			vis_format = SOFT_PIX_FMT_BGR555;
			break;
		default:
			vis_format = SOFT_PIX_FMT_NONE;
			break;
	}
#endif
	return vis_format;
}

static gboolean
gst_julia_vis_setup(GstAudioVisualizer2 * bscope)
{
	GstJuliaVis *self = GST_JULIA_VIS(bscope);

	int w, h, sr, channels;
	w = GST_VIDEO_INFO_WIDTH(&bscope->vinfo);
	h = GST_VIDEO_INFO_HEIGHT(&bscope->vinfo);
	sr = GST_AUDIO_INFO_RATE(&bscope->ainfo);
	channels = GST_AUDIO_INFO_CHANNELS (&bscope->ainfo);
	self->priv->frame_duration = gst_util_uint64_scale_int (GST_SECOND, GST_VIDEO_INFO_FPS_D (&bscope->vinfo), GST_VIDEO_INFO_FPS_N (&bscope->vinfo));

	julia_vis_pixel_format vis_format = get_vis_format(bscope->vinfo.finfo->format);
	if(vis_format == SOFT_PIX_FMT_NONE) {
		GST_ELEMENT_ERROR(self, STREAM, FORMAT, (NULL), ("Unsupported video format!"));
		return false;
	}

	GST_DEBUG_OBJECT(self, "WxH: %dx%d", w, h);
	GST_DEBUG_OBJECT(self, "Audio samplerate: %d", sr);
	GST_DEBUG_OBJECT(self, "Audio bpf: %d", bscope->ainfo.bpf);

	// TODO: only reset if we really need to
	// TODO: need to tell it about the channel order so it can properly
	// swap it around in the pallets
	if(self->priv->julia_vis_ctx) simple_soft_destroy(self->priv->julia_vis_ctx);

	// reset
	self->priv->julia_vis_ctx = simple_soft_init(w, h, self->priv->map_func, sr, channels, vis_format);
	self->priv->frame_count = 0;

	return true;
}

static gboolean gst_julia_vis_add_audio(GstAudioVisualizer2 *bscope, GstBuffer * audio)
{
	GstJuliaVis *self = GST_JULIA_VIS(bscope);
	
	if(audio == NULL) {
		GST_ELEMENT_ERROR(self, STREAM, FORMAT, (NULL), ("No audio buffer!"));
		return TRUE;
	}

	GstMapInfo amap;
	gst_buffer_map(audio, &amap, GST_MAP_READ);

	simple_soft_add_audio(self->priv->julia_vis_ctx, (const float *)amap.data, amap.size/bscope->ainfo.bpf);

	gst_buffer_unmap(audio, &amap);

	if(GST_BUFFER_PTS_IS_VALID(audio)) {
		GST_DEBUG_OBJECT(self, "audio pts %" GST_TIME_FORMAT, GST_TIME_ARGS(audio->pts));
	}
	if(GST_BUFFER_DTS_IS_VALID(audio)) {
		GST_DEBUG_OBJECT(self, "audio dts %" GST_TIME_FORMAT, GST_TIME_ARGS(audio->dts));
	}

	return true;
}

static void
gst_julia_vis_frame_dropped(GstAudioVisualizer2 * bscope)
{
	GstJuliaVis *self = GST_JULIA_VIS(bscope);

	self->priv->frame_count++;
}

static gboolean
gst_julia_vis_render (GstAudioVisualizer2 * bscope, GstVideoFrame * video)
{
	GstJuliaVis *self = GST_JULIA_VIS(bscope);
	
	if(video == NULL) {
		GST_ELEMENT_ERROR(self, STREAM, FORMAT, (NULL), ("No video buffer!"));
		return TRUE;
	}
	
	if(video->info.finfo == NULL) {
		GST_ELEMENT_ERROR(self, STREAM, FORMAT, (NULL), ("No video format!"));
		return TRUE;
	}

	const GstVideoFormat format = GST_VIDEO_FRAME_FORMAT(video);

	struct Pixbuf pb;

	switch(format) {
		case GST_VIDEO_FORMAT_RGBx:
		case GST_VIDEO_FORMAT_BGRx:
		case GST_VIDEO_FORMAT_xRGB:
		case GST_VIDEO_FORMAT_xBGR:
			pb.bpp = 32;
			break;
		case GST_VIDEO_FORMAT_RGB16:
		case GST_VIDEO_FORMAT_BGR16:
			pb.bpp = 16;
			break;
		case GST_VIDEO_FORMAT_RGB15:
		case GST_VIDEO_FORMAT_BGR15:
			pb.bpp = 15;
			break;
#if 0
		case GST_VIDEO_FORMAT_RGB8P:
			pb.bpp = 8;
			break;
#endif
		default:
			GST_ELEMENT_ERROR(self, STREAM, FORMAT, (NULL), ("Unsupported video format"));
			return false;
	}
	pb.format = get_vis_format(format);
	pb.w = GST_VIDEO_FRAME_WIDTH(video);
	pb.h = GST_VIDEO_FRAME_HEIGHT(video);
	pb.pitch = GST_VIDEO_FRAME_PLANE_STRIDE(video, 0);
	pb.data = GST_VIDEO_FRAME_PLANE_DATA(video, 0);

	GstClockTime start_time = GST_ELEMENT_START_TIME(self);
	GstClock *clock = gst_element_get_clock(GST_ELEMENT(self));
	if(clock && start_time != GST_CLOCK_TIME_NONE) {

		GST_DEBUG_OBJECT(self, "tick0 %" GST_TIME_FORMAT, GST_TIME_ARGS(start_time));
		GST_DEBUG_OBJECT(self, "streamtime %" GST_TIME_FORMAT, GST_TIME_ARGS(gst_clock_get_time(clock)));
#if 1
		if(GST_BUFFER_PTS_IS_VALID(video->buffer)) {
			GST_DEBUG_OBJECT(self, "video pts %" GST_TIME_FORMAT, GST_TIME_ARGS(video->buffer->pts));
		}
		if(GST_BUFFER_DTS_IS_VALID(video->buffer)) {
			GST_DEBUG_OBJECT(self, "video dts %" GST_TIME_FORMAT, GST_TIME_ARGS(video->buffer->dts));
		}
#endif
	}

	simple_soft_render(
		self->priv->julia_vis_ctx,
		&pb,
		GST_TIME_AS_MSECONDS(self->priv->frame_count * self->priv->frame_duration),
		0);

	self->priv->frame_count++;

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GType stuff
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void
gst_julia_vis_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
	GstJuliaVis *self = GST_JULIA_VIS (object);

	switch (prop_id) {
		case PROP_MAP_FUNC:
			self->priv->map_func = g_value_get_enum (value);
			GST_DEBUG_OBJECT(self, "mapfunc set %d", self->priv->map_func);
			if(self->priv->julia_vis_ctx)
				simple_soft_change_map_func(self->priv->julia_vis_ctx, self->priv->map_func);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gst_julia_vis_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
	GstJuliaVis *self = GST_JULIA_VIS (object);

	switch (prop_id) {
		case PROP_MAP_FUNC:
			g_value_set_enum (value, self->priv->map_func);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gst_julia_vis_class_init (gpointer g_class, gpointer class_data)
{
	GObjectClass *gobject_class = (GObjectClass *) g_class;
	GstElementClass *element_class = (GstElementClass *) g_class;
	GstAudioVisualizer2Class *scope_class = (GstAudioVisualizer2Class *) g_class;

	g_type_class_add_private (g_class, sizeof (GstJuliaVisPrivate));

	parent_class = g_type_class_peek_parent(g_class);

	GST_DEBUG_CATEGORY_INIT(julia_vis_debug,
      "julia_vis", 
      0,
      "Julia Vis fractal audio visualization");

    gobject_class->set_property = gst_julia_vis_set_property;
	gobject_class->get_property = gst_julia_vis_get_property;

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&src_template));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&sink_template));

	gst_element_class_set_static_metadata(
		element_class,
	    "Julia Vis fractal audio visualization",
	    "Visualization",
	    "Audio visualizer based around the Julia set fractal.",
	    "W Thomas Jones <thomas.jones@utoronto.ca>");

	gobject_class->finalize = gst_julia_vis_finalize;
	scope_class->add_audio = GST_DEBUG_FUNCPTR(gst_julia_vis_add_audio);
	scope_class->setup = GST_DEBUG_FUNCPTR(gst_julia_vis_setup);
	scope_class->render = GST_DEBUG_FUNCPTR(gst_julia_vis_render);
	scope_class->frame_dropped = GST_DEBUG_FUNCPTR(gst_julia_vis_frame_dropped);

	g_object_class_install_property (gobject_class, PROP_MAP_FUNC,
      g_param_spec_enum ("map-func", "fractal function",
          "The function used to compute the fractal",
          GST_TYPE_JULIA_VIS_MAP_FUNC, GST_JULIA_VIS_MAP_FUNC_NORMAL_INTERP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

GType
gst_julia_vis_get_type (void)
{
	static volatile gsize type = 0;

	if (g_once_init_enter(&type)) {
		static const GTypeInfo info = {
			sizeof (GstJuliaVisClass),
			NULL,
			NULL,
			gst_julia_vis_class_init,
			NULL,
			NULL,
			sizeof (GstJuliaVis),
			0,
			(GInstanceInitFunc) gst_julia_vis_init
		};
		GType _type;
		_type = g_type_register_static(GST_TYPE_AUDIO_VISUALIZER2, "GstJuliaVis", &info, 0);
		g_once_init_leave(&type, _type);
	}
	return type;
}

// For the property 
GType
gst_julia_vis_map_func_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;
  if (g_once_init_enter (&g_define_type_id__volatile)) {
    static const GEnumValue values[] = {
      { GST_JULIA_VIS_MAP_FUNC_NORMAL, "GST_JULIA_VIS_MAP_FUNC_NORMAL", "normal" },
      { GST_JULIA_VIS_MAP_FUNC_NORMAL_INTERP, "GST_JULIA_VIS_MAP_FUNC_NORMAL_INTERP", "normal-interp" },
      { GST_JULIA_VIS_MAP_FUNC_RATIONAL, "GST_JULIA_VIS_MAP_FUNC_RATIONAL", "rational" },
      { GST_JULIA_VIS_MAP_FUNC_RATIONAL_INTERP, "GST_JULIA_VIS_MAP_FUNC_RATIONAL_INTERP", "rational-interp" },
      { GST_JULIA_VIS_MAP_FUNC_BUTTERFLY, "GST_JULIA_VIS_MAP_FUNC_BUTTERFLY", "butterfly" },
      { GST_JULIA_VIS_MAP_FUNC_BUTTERFLY_INTERP, "GST_JULIA_VIS_MAP_FUNC_BUTTERFLY_INTERP", "butterfly-interp" },
      { 0, NULL, NULL }
    };
    GType g_define_type_id = g_enum_register_static ("GstJuliaVisMapFunc", values);
    g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
  }
  return g_define_type_id__volatile;
}
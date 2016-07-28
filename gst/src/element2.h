
#ifndef JULIA_VIS_GST_ELEMENT_H__
#define JULIA_VIS_GST_ELEMENT_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/video/video.h>
#include <gst/video/gstvideopool.h>
#include <gst/audio/audio.h>

//#include <gst/pbutils/gstaudiovisualizer.h>
#include "gstaudiovisualizer2.h"

G_BEGIN_DECLS

#define GST_TYPE_JULIA_VIS (gst_julia_vis_get_type())
#define GST_IS_JULIA_VIS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_JULIA_VIS))
#define GST_JULIA_VIS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_JULIA_VIS,GstJuliaVis))
#define GST_IS_JULIA_VIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_JULIA_VIS))
#define GST_JULIA_VIS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_JULIA_VIS,GstJuliaVisClass))
#define GST_JULIA_VIS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_JULIA_VIS, GstJuliaVisClass))

typedef struct _GstJuliaVis GstJuliaVis;
typedef struct _GstJuliaVisClass GstJuliaVisClass;
typedef struct _GstJuliaVisPrivate GstJuliaVisPrivate;

typedef enum {
  GST_JULIA_VIS_MAP_FUNC_NORMAL,
  GST_JULIA_VIS_MAP_FUNC_NORMAL_INTERP,
  GST_JULIA_VIS_MAP_FUNC_RATIONAL,
  GST_JULIA_VIS_MAP_FUNC_RATIONAL_INTERP,
  GST_JULIA_VIS_MAP_FUNC_BUTTERFLY,
  GST_JULIA_VIS_MAP_FUNC_BUTTERFLY_INTERP
} GstJuliaVisMapFunc;

struct _GstJuliaVis
{
	GstAudioVisualizer element;

	GstJuliaVisPrivate *priv;
};

struct _GstJuliaVisClass
{
	GstAudioVisualizerClass parent_class;
};

GType gst_julia_vis_get_type (void);

GType gst_julia_vis_map_func_get_type (void);
#define GST_TYPE_JULIA_VIS_MAP_FUNC (gst_julia_vis_map_func_get_type())

G_END_DECLS

#endif

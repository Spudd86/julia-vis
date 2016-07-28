/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) <2015> Luis de Bethencourt <luis@debethencourt.com>
 *
 * gstaudiovisualizer.c: base class for audio visualisation elements
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_AUDIO_VISUALIZER_H__
#define __GST_AUDIO_VISUALIZER_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS
#define GST_TYPE_AUDIO_VISUALIZER            (gst_audio_visualizer_get_type())
#define GST_AUDIO_VISUALIZER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_VISUALIZER,GstAudioVisualizer))
#define GST_AUDIO_VISUALIZER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIO_VISUALIZER,GstAudioVisualizerClass))
#define GST_AUDIO_VISUALIZER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_AUDIO_VISUALIZER,GstAudioVisualizerClass))
#define GST_IS_SYNAESTHESIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIO_VISUALIZER))
#define GST_IS_SYNAESTHESIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIO_VISUALIZER))
typedef struct _GstAudioVisualizer GstAudioVisualizer;
typedef struct _GstAudioVisualizerClass GstAudioVisualizerClass;
typedef struct _GstAudioVisualizerPrivate GstAudioVisualizerPrivate;

struct _GstAudioVisualizer
{
	GstElement parent;

	guint req_spf;                /* min samples per frame wanted by the subclass */

	/* video state */
	GstVideoInfo vinfo;

	/* audio state */
	GstAudioInfo ainfo;

	/* <private> */
	GstAudioVisualizerPrivate *priv;
};

struct _GstAudioVisualizerClass
{
	GstElementClass parent_class;

	/* virtual function, called whenever the format changes */
	gboolean (*setup) (GstAudioVisualizer * scope);

	/* virtual function for accepting audio */
	gboolean (*add_audio) (GstAudioVisualizer * scope, GstBuffer * audio);

	/* virtual function called whenever we skip render due to being behind */
	void (*frame_dropped) (GstAudioVisualizer * scope);

	/* virtual function for rendering a frame */
	gboolean (*render) (GstAudioVisualizer * scope, GstVideoFrame * video);

	gboolean (*decide_allocation)   (GstAudioVisualizer * scope, GstQuery *query);
};

GType gst_audio_visualizer_get_type (void);

G_END_DECLS
#endif /* __GST_AUDIO_VISUALIZER_H__ */


#include "common.h"

#include "element2.h"

GST_DEBUG_CATEGORY (julia_vis_debug);
#define GST_CAT_DEFAULT (julia_vis_debug)

static gboolean
plugin_init (GstPlugin * plugin)
{
	GST_DEBUG_CATEGORY_INIT (julia_vis_debug, "julia_vis", 0, "Julia vis");
	return gst_element_register(plugin, "julia_vis", GST_RANK_NONE, GST_TYPE_JULIA_VIS);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    julia_vis,
    "julia_vis visualization plugin",
    plugin_init, "0.0.1", "GPL", "Julia Vis", "https://github.com/Spudd86/julia-vis")

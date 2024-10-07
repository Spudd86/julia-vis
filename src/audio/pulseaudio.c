#include "common.h"

#ifdef HAVE_PULSE

#include "audio.h"
#include "audio-private.h"

#include <pulse/pulseaudio.h>

static pa_threaded_mainloop *pulse_ml = NULL;
static pa_context *context = NULL;
static pa_stream *stream = NULL;

void pulse_shutdown(void) {
    if(pulse_ml) {
        pa_threaded_mainloop_stop(pulse_ml);

        if(stream)
            pa_stream_unref(stream);

        if(context) {
            pa_context_disconnect(context);
            pa_context_unref(context);
        }

    	pa_threaded_mainloop_free(pulse_ml);
    }
    pulse_ml = NULL;
    context = NULL;
    stream = NULL;
}

//TODO: listen for when the default sink changes and try to grab it's
// monitor instead of the one we already have

//TODO: clean this up to deal with errors better


static void stream_read_callback(pa_stream *s, size_t length, void *userdata)
{ (void)userdata;
	const void *data = NULL;
    if (pa_stream_peek(s, &data, &length) < 0) {
        fprintf(stderr, "pa_stream_peek() failed: %s\n", pa_strerror(pa_context_errno(context)));
//        pulse_shutdown();
        return;
    }
    
	audio_update(data, length/sizeof(float));
    pa_stream_drop(s);
}

/* This routine is called whenever the stream state changes */
static void stream_state_callback(pa_stream *s, void *userdata)
{
    volatile int *stream_ready = userdata;
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
            break;
        case PA_STREAM_READY:
            *stream_ready = 1;
            pa_threaded_mainloop_signal(pulse_ml, 0);
            break;
        case PA_STREAM_TERMINATED:
            *stream_ready = 2;
            pa_threaded_mainloop_signal(pulse_ml, 0);
            break;
        case PA_STREAM_FAILED:
        default:
            *stream_ready = 2;
            fprintf(stderr, "pulseaudio: Stream errror: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            pa_threaded_mainloop_signal(pulse_ml, 0);
    }
}

static void connect_sink_info_callback(pa_context  *c, const pa_sink_info  *i, int eol, void *userdata)
{(void)c;(void)eol;
    if(i != NULL) {
        volatile const pa_sink_info **info = userdata;
        *info = i;
        pa_threaded_mainloop_signal(pulse_ml, 1);
    }
}

static void connect_server_info_callback(pa_context *c, const pa_server_info *i, void *userdata)
{(void)c;
    if(i != NULL) {
        volatile const pa_server_info **server_info = userdata;
        *server_info = i;
        pa_threaded_mainloop_signal(pulse_ml, 1);
    }
}

static volatile int pa_ready = 0;
/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata)
{(void)c;(void)userdata;
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            pa_ready = 1;
            pa_threaded_mainloop_signal(pulse_ml, 0);
            break;

        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
        default:
            pa_ready = 2;
            fprintf(stderr, "pulseaudio: Connection failure: %s\n", pa_strerror(pa_context_errno(c)));
            pa_threaded_mainloop_signal(pulse_ml, 0);
    }
}

static const pa_sample_spec sample_spec = { .format = PA_SAMPLE_FLOAT32NE, .rate = 44100, .channels = 1 };
static pa_buffer_attr buf_attr = { 
    .maxlength = -1, 
    .tlength = -1, 
    .prebuf = -1, 
    .minreq = -1, 
    .fragsize = 512*sizeof(float)
};

static char source_name[512];
static char default_sink_name[512];

int pulse_setup(const opt_data *od)
{
	setenv("PULSE_PROP_application.name", "Julia Set Fractal Visualizer", 1);

    printf("Starting pulseaudio\n");
	pulse_ml = pa_threaded_mainloop_new();
	if (pa_threaded_mainloop_start(pulse_ml) < 0) {
        fprintf(stderr, "pa_threaded_mainloop_run() failed.\n");
		return 1;
    }

	pa_threaded_mainloop_lock(pulse_ml);

	pa_mainloop_api *mainloop_api = pa_threaded_mainloop_get_api(pulse_ml);
	if (!(context = pa_context_new(mainloop_api, "fractal_test"))) {
        fprintf(stderr, "pa_context_new() failed.\n");
        goto quit;
    }

	pa_context_set_state_callback(context, context_state_callback, NULL);
    if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
        fprintf(stderr, "pa_context_connect() failed: %s", pa_strerror(pa_context_errno(context)));
        goto quit;
    }

    while(pa_ready == 0) pa_threaded_mainloop_wait(pulse_ml);
    if(pa_ready != 1) goto quit;
    
    printf("Connection established.\n");

    
    if(od->audio_opts != NULL) {
        strncpy(source_name, od->audio_opts, 512);
    } else {
        printf("Getting sever info\n");
        const pa_server_info *server_info = NULL;
        pa_operation *o = pa_context_get_server_info(context, connect_server_info_callback, &server_info);
        while(server_info == NULL)
            pa_threaded_mainloop_wait(pulse_ml);
        pa_operation_unref(o);
        strncpy(default_sink_name, server_info->default_sink_name, 512);
        pa_threaded_mainloop_accept(pulse_ml);

        printf("Attempting to find monitor for %s\n", default_sink_name);

        const pa_sink_info *sink_info = NULL;
        o = pa_context_get_sink_info_by_name(context, default_sink_name, connect_sink_info_callback, &sink_info);
        while(sink_info == NULL)
            pa_threaded_mainloop_wait(pulse_ml);
        pa_operation_unref(o);
        strncpy(source_name, sink_info->monitor_source_name, 512);
        pa_threaded_mainloop_accept(pulse_ml);
    }

    int stream_ready = 0;
    stream = pa_stream_new(context, "foo", &sample_spec, NULL);
    pa_stream_set_state_callback(stream, stream_state_callback, &stream_ready);

    printf("pulseaudio: Connecting to source '%s'\n", source_name);
    pa_stream_connect_record(stream, source_name, &buf_attr, PA_STREAM_FIX_RATE | PA_STREAM_ADJUST_LATENCY);
    // we probably don't want PA_STREAM_ADJUST_LATENCY since it tries to fiddle with device latency, which we don't actually 
    // care too much about as long as it isn't huge...

    while(stream_ready == 0) pa_threaded_mainloop_wait(pulse_ml);
    if(stream_ready != 1) goto quit;

    const pa_sample_spec *stream_sample_spec = pa_stream_get_sample_spec(stream);
    audio_setup(stream_sample_spec->rate);

    pa_stream_set_read_callback(stream, stream_read_callback, NULL);

    pa_threaded_mainloop_unlock(pulse_ml);

    printf("Pulsaudio running.\n");

	return 0;

quit:
    //TODO: do we need to unlock the main loop before we delete it?
	if(stream)
        pa_stream_unref(stream);

    if(context)
        pa_context_unref(context);

    if(pulse_ml) {
    	pa_threaded_mainloop_stop(pulse_ml);
    	pa_threaded_mainloop_free(pulse_ml);
    }
    
    pulse_ml = NULL;
    mainloop_api = NULL;
    context = NULL;
    stream = NULL;

    fprintf(stderr, "Pulsaudio startup failed!\n");
	return 1;
}

#else
#warning NO PULSE!
#endif

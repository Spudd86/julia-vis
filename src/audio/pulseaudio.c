#include "common.h"

#ifdef HAVE_PULSE

#include "audio.h"
#include "audio-private.h"

//TODO: make this whole thing suck less
#include <pulse/pulseaudio.h>

#include <pthread.h>


//static pa_threaded_mainloop *pulse_ml = NULL;
static pa_mainloop *pulse_ml = NULL;
static pa_mainloop_api *mainloop_api = NULL;
static pa_context *context = NULL;
static pa_stream *stream = NULL;
static void quit(int ret) { mainloop_api->quit(mainloop_api, ret); }
//static void quit(int ret) { pa_threaded_mainloop_stop(pulse_ml); pa_threaded_mainloop_free(pulse_ml); }


void pulse_shutdown() {
	quit(0);
}


static void stream_read_callback(pa_stream *s, size_t length, void *userdata) { (void)userdata;
	const void *data = NULL;
    if (pa_stream_peek(s, &data, &length) < 0) {
        fprintf(stderr, "pa_stream_peek() failed: %s\n", pa_strerror(pa_context_errno(context)));
        quit(1);
        return;
    }
	audio_update(data, length/sizeof(float));
    pa_stream_drop(s);
}

/* This routine is called whenever the stream state changes */
static void stream_state_callback(pa_stream *s, void *userdata)
{ (void)userdata;
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
        case PA_STREAM_TERMINATED:
        case PA_STREAM_READY:
        	pa_stream_set_read_callback(stream, stream_read_callback, NULL);
            break;
        case PA_STREAM_FAILED:
        default:
            fprintf(stderr, "Stream errror: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            quit(1);
    }
}

static const pa_sample_spec sample_spec = { .format = PA_SAMPLE_FLOAT32NE, .rate = 44100, .channels = 1 };
static const pa_buffer_attr buf_attr = { -1, -1, -1, -1, 1024*4 };

static void connect_sink_info_callback(pa_context  *c, const pa_sink_info  *i, int eol, void *userdata) {(void)userdata;(void)eol;
	stream = pa_stream_new(c, "foo", &sample_spec, NULL);
    pa_stream_set_state_callback(stream, stream_state_callback, NULL);
//    pa_stream_set_read_callback(stream, stream_read_callback, NULL);
	// TODO use PA_STREAM_FIX_RATE
	if(i != NULL) {
		printf("Connecting to source '%s'\n", i->monitor_source_name);
		pa_stream_connect_record(stream, i->monitor_source_name, &buf_attr, PA_STREAM_ADJUST_LATENCY);
	} else {
//		printf("Didn't get default sink, not using monitor, try default source instead!\n");
//		pa_stream_connect_record(stream, NULL, &buf_attr, PA_STREAM_ADJUST_LATENCY);
	}
}

static void connect_server_info_callback(pa_context *c, const pa_server_info *i, void *userdata) { (void)userdata;
	if(i->default_sink_name) printf("Default sink is '%s'\n", i->default_sink_name);
	pa_context_get_sink_info_by_name(c, i->default_sink_name, connect_sink_info_callback, NULL);
	//TODO: try just appending '.monitor' to name?
}

/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata) { opt_data *od = userdata;
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {
            fprintf(stderr, "Connection established.\n");
            if(od->audio_opts != NULL) {
            	stream = pa_stream_new(c, "foo", &sample_spec, NULL);
				pa_stream_set_state_callback(stream, stream_state_callback, NULL);
				printf("Connecting to source '%s'\n", od->audio_opts);
				pa_stream_connect_record(stream, od->audio_opts, &buf_attr, PA_STREAM_ADJUST_LATENCY);
            } else {
            	printf("Getting sever info\n");
				pa_context_get_server_info(c, connect_server_info_callback, NULL);
            }
            break;
        }

        case PA_CONTEXT_TERMINATED:
            quit(0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            fprintf(stderr, "Connection failure: %s\n", pa_strerror(pa_context_errno(c)));
            quit(1);
    }
}


static int pulse_run()
{
	int ret; if (pa_mainloop_run(pulse_ml, &ret) < 0) {
        fprintf(stderr, "pa_mainloop_run() failed.\n");
		abort();
    }
	return ret;
}
static pthread_t pulse_thread;

int pulse_setup(const opt_data *od)
{
	setenv("PULSE_PROP_application.name", "Fractal Visualizer", 1);

	audio_setup(44100);
	pulse_ml = pa_mainloop_new();
//	pulse_ml = pa_threaded_mainloop_new();
	mainloop_api = pa_mainloop_get_api(pulse_ml);

	if (!(context = pa_context_new(mainloop_api, "fractal_test"))) {
        fprintf(stderr, "pa_context_new() failed.\n");
        goto quit;
    }
	pa_context_set_state_callback(context, context_state_callback, (void *)od);

    if (pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0) {
        fprintf(stderr, "pa_context_connect() failed: %s", pa_strerror(pa_context_errno(context)));
        goto quit;
    }

//	pa_threaded_mainloop_start(pulse_ml);
	pthread_create(&pulse_thread, NULL, (void*)&pulse_run, NULL);

	return 0;

quit:
	if (stream)
        pa_stream_unref(stream);

    if (context)
        pa_context_unref(context);

    if (pulse_ml) {
//    	pa_threaded_mainloop_stop(pulse_ml);
//    	pa_threaded_mainloop_free(pulse_ml);
        pa_signal_done();
        pa_mainloop_free(pulse_ml);
    }
	return 1;
}

#else
#warning NO PULSE!
#endif

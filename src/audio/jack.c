#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <jack/jack.h>

#include "audio.h"


static jack_port_t *in_port;


// jack callback
static int process (jack_nframes_t nframes, void *arg)
{
	static int pos = 0;
	
	jack_default_audio_sample_t *in = (jack_default_audio_sample_t *) jack_port_get_buffer (in_port, nframes);
	
	//FIXME:
	audio_update(in, nframes);
	// process samplses?

	return 0;
}

static jack_client_t *client;

int jack_setup()
{
	jack_status_t status;
	client = jack_client_open("test", 0, &status);
	if (client == NULL) {
		fprintf (stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			fprintf (stderr, "Unable to connect to JACK server\n");
		}
		exit (1);
	}

	jack_set_process_callback (client, process, 0);
	in_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		exit(1);
	}
}

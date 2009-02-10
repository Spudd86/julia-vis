#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <jack/jack.h>

#include "audio.h"

static jack_port_t *in_port;


/*TODO: 
 * proper shutdown
 * addd buffer change callback
 */ 

// jack callback
static int process (jack_nframes_t nframes, void *arg)
{
	float *in = (float *) jack_port_get_buffer (in_port, nframes);
	
	audio_update(in, nframes);

	return 0;
}

static jack_client_t *client;

static void shutdown(void) {
	jack_client_close (client);
}

int jack_setup(opt_data *od)
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
	
	audio_setup(jack_get_sample_rate(client));

	jack_set_process_callback (client, process, 0);
	in_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		exit(1);
	}
	atexit(&shutdown);
	
	if(od->jack_opt !=NULL) { // connect to some ports!
		const char **ports = jack_get_ports(client, od->jack_opt, NULL, JackPortIsOutput);
		if(ports == NULL) return 0;
		for(int i=0; ports[i]!=NULL; i++) 
			jack_connect(client, ports[i], jack_port_name(in_port));
		free(ports);
	}

	return 0;
}

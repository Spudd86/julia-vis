#include "common.h"
#include <stdio.h>
#include <getopt.h>

#ifdef HAVE_ORC
#include <orc/orc.h>
#endif

static const char *helpstr =
"Usage: %s [-w width] [-h height] [-s screen updates/second] [-fdpu] [-j<pattern>]\n"
"\t-r use rational map\n"
"\t-p try to set an 8bpp mode (hardware pallets)\n"
"\t-f go fullscreen\n"
"\t-a oscolliscope update rate [default 12]\n"
"\t-s screen update rate [default 30, threaded only]\n"
"\t-d try to enable double buffering\n"

"\t-i <driver>[:opts] select audio input driver\n"

"\t-z [num] select audio device\n"
#ifdef HAVE_JACK
"\t-j use jack optionally specify a pattern of ports to connect to\n"
#endif
#ifdef HAVE_PULSE
"\t-u use pulseaudio for audio input"
#endif
;
//TODO: change audio input selection to be something like -i <driver>:<driver_opts>

//TODO: use getopt_long
void optproc(int argc, char **argv, opt_data *res)
{
#ifdef HAVE_ORC
	orc_init();
#endif

	int opt;

	res->w = res->h = -1;
	res->fullscreen = 0;
	res->draw_rate = 30;
	res->maxsrc_rate = 12;
	res->doublebuf = 0;
	res->hw_pallet = 0;
	res->use_jack = 0;
	res->use_pulse = 0;
	res->rational_julia = 0;
	res->jack_opt = NULL;
	res->audiodev = -1;

	res->audio_driver = AUDIO_PORTAUDIO;
	res->audio_opts = NULL;

//	while((opt = getopt(argc, argv, "w:h:s:a:i:rftpd")) != -1) {
	while((opt = getopt(argc, argv, "w:h:s:a:z:urftpdj::")) != -1) {
		switch(opt) {
			case 'w':
				res->w = atoi(optarg);
				break;
			case 'h':
				res->h = atoi(optarg);
				break;
			case 'r':
				res->rational_julia = 1;
				break;
			case 'f':
				res->fullscreen = 1;
				break;
			case 'd':
				res->doublebuf = 1;
				break;
			case 'p':
				res->hw_pallet = 1;
				break;
//			case 'i': {
//				char *drvstr = strdup(optarg);
//				char *drvopt = strchr(drvstr, ':');
//				if(drvopt != NULL) { *drvopt = '\0'; drvopt++;}
//				res->audio_opts = drvopt;
//				if(!strcmp(drvstr, "portaudio")) ;
//#ifdef HAVE_PULSE
//				else if(!strcmp(drvstr, "pulse")) res->audio_driver = AUDIO_PULSE;
//#endif
//#ifdef HAVE_JACK
//				else if(!strcmp(drvstr, "jack")) res->audio_driver = AUDIO_JACK;
//#endif
//				else fprintf(stderr, "Bad audio driver name %s, using portaudio\n", drvstr);
//			} break;
			#ifdef HAVE_JACK
			case 'j':
				res->use_jack = 1;
				res->jack_opt = optarg;
				break;
			#endif
			#ifdef HAVE_PULSE
			case 'u':
				res->use_pulse = 1;
				break;
			#endif
			case 'z':
				res->audiodev = atoi(optarg);
				break;
			case 's':
				res->draw_rate = atoi(optarg);
				break;
			case 'a':
				res->maxsrc_rate = atoi(optarg);
				break;
			case 't': // will be threads on/off
				//break;
			default:
				fprintf(stderr, helpstr, argv[0]);
				exit(EXIT_FAILURE);
		}
	}
}

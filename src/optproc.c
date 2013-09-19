#include "common.h"
#include <getopt.h>
#ifdef HAVE_ORC
#include <orc/orc.h>
#endif

static const char *helpstr =
"Usage: %s [-w width] [-h height] [-s screen updates/second] [-fdpu]\n"
"\t-r use rational map\n"
"\t-f go fullscreen\n"
"\t-a oscolliscope update rate [default 24]\n"
"\t-s screen update rate [default 60]\n"
#ifndef USE_GL
"\t-p try to set an 8bpp mode (hardware pallets, software versions only)\n"
"\t-d try to enable double buffering (opengl version always double buffers)\n"
#endif
"\t-m <name>\n"
"\t\twhere name is one of default,rational,butterfly\n"
#ifdef USE_GL
"\t\t Note: only default and rational are implemented for opengl\n\n"
#endif

#ifdef USE_GL
"\t-q quality\n"
"\t\tglsl: control # of samples\n"
"\t\t  0 take 1 sample (default)\n"
"\t\t  1 take 4 samples\n"
"\t\t  2 take 5 samples\n"
"\t\t  3 take 8 samples\n"
"\t\t  4 take 9 samples\n"
"\t\tfixed function gl: currently no effect\n"
"\t-g opt1:opt2:...\n"
"\t\tgeneric opengl options\n"
"\t\t\tfixed\tforce use fixed function GL\n"
"\t\t\tpintens\tuse packed intensity pixel values (precision boost)\n"
"\t\t\trboos\tdouble internal resolution\n\n"
#else
"\t-q control map quality\n"
"\t\t  0 interpolate map function across 8x8 squares\n"
"\t\t  1 calculate map at every pixel\n\n"
#endif

"\t-i <driver>[:opts] select audio input driver\n"
"\t\tdrivers:\n"
#ifdef HAVE_PORTAUDIO
"\t\t  portaudio: optionally specify a device number (they are listed at startup)\n"
#endif
#ifdef HAVE_JACK
"\t\t  jack: optionally specify a pattern of ports to connect to\n"
#endif
#ifdef HAVE_PULSE
"\t\t  pulse: use pulseaudio, takes a source name\n"
#endif
#ifdef HAVE_SNDFILE
"\t\t  file: use libsndfile, takes file name to read from\n"
#endif
;

//TODO: use getopt_long
void optproc(int argc, char **argv, opt_data *res)
{
#ifdef HAVE_ORC
	orc_init();
#endif

	int opt;
	
	res->w = res->h = -1;
	res->fullscreen = 0;
	res->draw_rate = 60;
	res->maxsrc_rate = 24;
	res->doublebuf = 0;
	res->hw_pallet = 0;
	res->rational_julia = 0;

	res->quality = 0;

#if HAVE_PULSE
	res->audio_driver = AUDIO_PULSE;
#elif HAVE_PORTAUDIO
	res->audio_driver = AUDIO_PORTAUDIO;
#else
	res->audio_driver = AUDIO_NONE
#endif
	res->audio_opts = NULL;
	res->gl_opts = NULL;
	res->backend_opts = NULL;

	res->map_name = "default";

	while((opt = getopt(argc, argv, "w:h:s:a:i:q:g:m:b:rftpd")) != -1) {
		switch(opt) {
			case 'w':
				res->w = atoi(optarg);
				break;
			case 'h':
				res->h = atoi(optarg);
				break;
			case 'q':
				res->quality = atoi(optarg);
				if(res->quality < 0) {
					fprintf(stderr, "Invalid quality arg, clamping to 0\n");
					res->quality = 0;
				}
				break;
			case 'r':
				res->rational_julia = 1;
				break;
			case 'm':
				res->map_name = optarg;
				if(strcmp("rational", res->map_name) == 0)
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
			case 'b':
				res->backend_opts = strdup(optarg);
				break;
#ifdef USE_GL
			case 'g':
				res->gl_opts = strdup(optarg);
				break;
#endif
			case 'i': {
				char *drvstr = strdup(optarg);
				char *drvopt = strchr(drvstr, ':');
				if(drvopt != NULL) { *drvopt = '\0'; drvopt++;}
				res->audio_opts = drvopt;
				if(0) ;
#ifdef HAVE_PORTAUDIO
				else if(!strcmp(drvstr, "portaudio")) res->audio_driver = AUDIO_PORTAUDIO;
#endif
#ifdef HAVE_PULSE
				else if(!strcmp(drvstr, "pulse")) res->audio_driver = AUDIO_PULSE;
#endif
#ifdef HAVE_JACK
				else if(!strcmp(drvstr, "jack")) res->audio_driver = AUDIO_JACK;
#endif
#ifdef HAVE_SNDFILE
				else if(!strcmp(drvstr, "file")) res->audio_driver = AUDIO_SNDFILE;
#endif
				else {
					res->audio_opts = NULL;
					fprintf(stderr, "Bad audio driver name %s, using default\n", drvstr);
				}
			} break;
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

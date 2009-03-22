
#include "common.h"
#include <stdio.h>
#include <getopt.h>

static const char *helpstr =
"Usage: %s [-w width] [-h height] [-s screen updates/second] [-fdp] [-j<pattern>]\n"
"\t-p try to set an 8bpp mode (hardware pallets)\n"
"\t-f go fullscreen\n"
"\t-a oscolliscope update rate [default 12]\n"
"\t-s screen update rate [default 30, threaded only]\n"
"\t-d try to enable double buffering\n"
"\t-j use jack optionally specify a pattern of ports to connect to\n"
;


//TODO: use getopt_long 
void optproc(int argc, char **argv, opt_data *res)
{
	int opt;
	
	res->w = res->h = -1;
	res->fullscreen = 0;
	res->draw_rate = 30;
	res->maxsrc_rate = 12;
	res->doublebuf = 0;
	res->hw_pallet = 0;
	res->use_jack = 0;
	res->jack_opt = NULL;
	
	while((opt = getopt(argc, argv, "w:h:s:a:ftpdj::")) != -1) {
		switch(opt) {
			case 'w':
				res->w = atoi(optarg);
				break;
			case 'h':
				res->h = atoi(optarg);
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
			case 'j':
				res->use_jack = 1;
				res->jack_opt = optarg;
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

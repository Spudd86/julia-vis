#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


#include "common.h"


//TODO: use getopt_long 
void optproc(int argc, char **argv, opt_data *res)
{
	int opt;
	
	res->w = res->h = -1;
	res->fullscreen = 0;
	res->draw_rate = 30;
	res->doublebuf = 0;
	res->use_jack = 0;
	res->jack_opt = NULL;
	
	while((opt = getopt(argc, argv, "w:h:s:ftdj::")) != -1) {
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
			case 'j':
				res->use_jack = 1;
				res->jack_opt = optarg;
				break;
			case 's':
				res->draw_rate = atoi(optarg);
				break;
			case 't': // will be threads on/off
				//break;
			default:
				fprintf(stderr, "Usage: %s [-w width] [-h height] [-s screen updates/second] [-f]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}
}

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>


#include "common.h"

void optproc(int argc, char **argv, opt_data *res)
{
	int opt;
	
	res->w = res->h = -1;
	res->fullscreen = 0;
	res->draw_rate = 30;
	
	while((opt = getopt(argc, argv, "w:h:s:ft")) != -1) {
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

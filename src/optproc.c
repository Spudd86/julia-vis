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
	
	while((opt = getopt(argc, argv, "w:h:ft")) != -1) {
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
			case 't': // will be threads on/off
				//break;
			default:
				fprintf(stderr, "Usage: %s [-w width] [-h height] [-f]\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}
}

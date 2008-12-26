#include <unistd.h>
#include <glib.h>

#include "audio.h"

int main() {
	setup_audio();
	
	while(!sleep(1)) {
		g_print("%i\n", get_beats());
	}
}

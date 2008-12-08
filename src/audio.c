#include <glib.h>

#include <portaudio.h>

#include <fftw3.h>


static int audio_callback( const void *input,
                           void *output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
	// Portaudio sound callback
}


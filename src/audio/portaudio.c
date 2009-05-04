#include "../common.h"
#include <stdio.h>

#include <portaudio.h>

#include "audio.h"

static PaStream *stream;

static int callback(const void *input,
					void *output,
					unsigned long frameCount,
					const PaStreamCallbackTimeInfo* timeInfo,
					PaStreamCallbackFlags statusFlags,
					void *userData )
{
	(void)output; (void)timeInfo; (void)statusFlags; (void)userData; // shut compiler up
	audio_update(input, frameCount);
	return paContinue;
}

void audio_stop_pa(void);

int audio_setup_pa()
{
	printf("Using PortAudio\n");

	PaError err = Pa_Initialize();
	if( err != paNoError ) { fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err)); exit(1); }

	const PaDeviceInfo *inf = Pa_GetDeviceInfo(Pa_GetDefaultInputDevice());

	int numdev = Pa_GetDeviceCount();
	printf("Portaudio devices:\n");
	for(int i=0; i<numdev; i++) {
		const PaDeviceInfo *di = Pa_GetDeviceInfo(i);
		if(i==Pa_GetDefaultInputDevice())printf("*");
		printf("%i\t%s\n", i, di->name);
	}

	/* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &stream,
                                1,          /* 1 input channel */
                                0,          /* no output */
                                paFloat32,
                                inf->defaultSampleRate,
                                paFramesPerBufferUnspecified,
                                &callback,
                                NULL );
    if( err != paNoError ) goto error;

	const PaStreamInfo *si = Pa_GetStreamInfo (stream);
	audio_setup(si->sampleRate);

	err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;

	atexit(audio_stop_pa);
	return 0;

error:
	fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
	err = Pa_Terminate();
	if( err != paNoError )
		fprintf(stderr, "Couldn't terminate: PortAudio error: %s\n", Pa_GetErrorText(err));
	return -1;
}

void audio_stop_pa(void)
{
	PaError err;
	err = Pa_StopStream( stream ); if( err != paNoError ) goto error;
	err = Pa_CloseStream( stream ); if( err != paNoError ) goto error;
	err = Pa_Terminate(); if( err != paNoError ) goto error;
	return;
error:
	fprintf(stderr, "Couldn't terminate: PortAudio error: %s\n", Pa_GetErrorText(err));
	exit(1);
}

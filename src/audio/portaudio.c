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

int audio_setup_pa(const opt_data *od)
{
	printf("Using PortAudio\n");

	PaError err = Pa_Initialize();
	if( err != paNoError ) { fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err)); exit(1); }

	int usedev = (!od->audio_opts)?Pa_GetDefaultInputDevice():atoi(od->audio_opts);
	if(usedev < 0 || usedev >= Pa_GetDeviceCount()) {
		fprintf(stderr, "bad device number %i\n", usedev);
		return -1;
	}

	int numdev = Pa_GetDeviceCount();
	printf("Portaudio devices:\n");
	for(int i=0; i<numdev; i++) {
		const PaDeviceInfo *di = Pa_GetDeviceInfo(i);
		if(i==usedev) printf("*");
		printf("%i\t%s\n", i, di->name);
	}

	const PaDeviceInfo *inf = Pa_GetDeviceInfo(usedev);
	PaStreamParameters parms = { usedev, 1, paFloat32, inf->defaultLowInputLatency, NULL};
	err = Pa_OpenStream(&stream, &parms, NULL, inf->defaultSampleRate,
						paFramesPerBufferUnspecified,
						paClipOff | paDitherOff,
						&callback, NULL);
    if( err != paNoError ) goto error;

	const PaStreamInfo *si = Pa_GetStreamInfo (stream);
	audio_setup(si->sampleRate);

	err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
	return 0;

error:
	fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
	err = Pa_Terminate();
	if( err != paNoError )
		fprintf(stderr, "Couldn't terminate: PortAudio error: %s\n", Pa_GetErrorText(err));
	return -1;
}

void audio_stop_pa()
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

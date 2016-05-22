#include "common.h"

#include "audio.h"
#include "audio-private.h"

#include <portaudio.h>

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
		const PaHostApiInfo *hapi = Pa_GetHostApiInfo(di->hostApi);
		PaStreamParameters parms = { i, 1, paFloat32, di->defaultLowInputLatency, NULL};
		if(i==usedev) printf("*");
		printf("%i\t%s\n", i, di->name);
		printf("\t\thost API:     %s\n", hapi->name);
		printf("\t\tmax channels: %10i\n", di->maxInputChannels);
		printf("\t\tdefault rate: %8.1f\n", di->defaultSampleRate);

		err = Pa_IsFormatSupported(&parms, NULL, di->defaultSampleRate);
		printf("\t\tstatus      : %s\n", Pa_GetErrorText(err));
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

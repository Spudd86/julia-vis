#include "../common.h"
#include <stdio.h>

#include <portaudio.h>

#include "audio.h"

#define SAMPLE_RATE (44100)

static PaStream *stream;

static int callback(const void *input,
					void *output,
					unsigned long frameCount,
					const PaStreamCallbackTimeInfo* timeInfo,
					PaStreamCallbackFlags statusFlags,
					void *userData )
{
	audio_update(input, frameCount);
	return paContinue;
}

void audio_stop_pa(void);

int audio_setup_pa()
{
	printf("Using PortAudio\n");
	
	PaError err = Pa_Initialize();
	if( err != paNoError ) { printf(  "PortAudio error: %s\n", Pa_GetErrorText(err)); exit(1); }
	
	const PaDeviceInfo *inf = Pa_GetDeviceInfo(Pa_GetDefaultInputDevice());
	
	/* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &stream,
                                1,          /* 1 input channel */
                                0,          /* no output */
                                paFloat32,  /* 32 bit floating point output */
                                inf->defaultSampleRate,
                                1024,        /* frames per buffer, i.e. the number
                                                   of sample frames that PortAudio will
                                                   request from the callback. Many apps
                                                   may want to use
                                                   paFramesPerBufferUnspecified, which
                                                   tells PortAudio to pick the best,
                                                   possibly changing, buffer size.*/
                                &callback, /* this is your callback function */
                                NULL ); /*This is a pointer that will be passed to
                                                   your callback*/
    if( err != paNoError ) goto error;
	
	const PaStreamInfo *si = Pa_GetStreamInfo (stream);
	audio_setup(si->sampleRate);
	
	err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
	
	atexit(audio_stop_pa);
	return 0;
	
error:
	printf("PortAudio error: %s\n", Pa_GetErrorText(err));
	err = Pa_Terminate();
	if( err != paNoError )
		printf("Couldn't terminate: PortAudio error: %s\n", Pa_GetErrorText(err));
	return -1;
}

void audio_stop_pa(void)
{
	PaError err;
	err = Pa_StopStream( stream ); if( err != paNoError ) goto error;
	err = Pa_CloseStream( stream ); if( err != paNoError ) goto error;
	err = Pa_Terminate(); if( err != paNoError ) goto error;
error:
	printf("Couldn't terminate: PortAudio error: %s\n", Pa_GetErrorText(err));
	exit(1);
}

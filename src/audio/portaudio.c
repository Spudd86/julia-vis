#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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


int audio_setup_pa()
{
	printf("Using PortAudio\n");
	
	PaError err = Pa_Initialize();
	if( err != paNoError ) { printf(  "PortAudio error: %s\n", Pa_GetErrorText(err)); exit(1); }
	
	
	/* Open an audio I/O stream. */
    err = Pa_OpenDefaultStream( &stream,
                                1,          /* no input channels */
                                0,          /* stereo output */
                                paFloat32,  /* 32 bit floating point output */
                                SAMPLE_RATE,
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

	audio_setup(SAMPLE_RATE);
	
	err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
	
	return 0;
	
error:
	printf("PortAudio error: %s\n", Pa_GetErrorText(err));
	err = Pa_Terminate();
	if( err != paNoError )
		printf("Couldn't terminate: PortAudio error: %s\n", Pa_GetErrorText(err));
	return -1;
}

int audio_stop_pa()
{
	PaError err;
	err = Pa_StopStream( stream ); if( err != paNoError ) goto error;
	err = Pa_CloseStream( stream ); if( err != paNoError ) goto error;
	err = Pa_Terminate(); if( err != paNoError ) goto error;
	return 0;
error:
	printf("Couldn't terminate: PortAudio error: %s\n", Pa_GetErrorText(err));
	exit(1);
	return -1;
}

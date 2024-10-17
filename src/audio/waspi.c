#include "common.h"

#include "audio.h"
#include "audio-private.h"

#include <mmreg.h>

#define COBJMACROS
#include <mmdeviceapi.h>
#include <audioclient.h>

/**
 * Based on:
 * 
 * https://learn.microsoft.com/en-ca/windows/win32/coreaudio/loopback-recording
 * 
 * https://habr.com/en/articles/663352/
 *   https://github.com/stsaz/audio-api-quick-start-guide/blob/master/wasapi-record.c
 *   https://github.com/stsaz/ffaudio/blob/master/ffaudio/wasapi.c
 * 
 * https://matthewvaneerde.wordpress.com/2014/11/05/draining-the-wasapi-capture-buffer-fully/
 *    https://github.com/mvaneerde/blog/blob/develop/loopback-capture/loopback-capture/loopback-capture.cpp
 *    https://github.com/mvaneerde/blog/blob/develop/loopback-capture/loopback-capture/main.cpp
 */


// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto error; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

static const GUID _CLSID_MMDeviceEnumerator = {0xbcde0395, 0xe52f, 0x467c, {0x8e,0x3d, 0xc4,0x57,0x92,0x91,0x69,0x2e}};
static const GUID _IID_IMMDeviceEnumerator = {0xa95664d2, 0x9614, 0x4f35, {0xa7,0x46, 0xde,0x8d,0xb6,0x36,0x17,0xe6}};
static const GUID _IID_IAudioRenderClient = {0xf294acfc, 0x3146, 0x4483, {0xa7,0xbf, 0xad,0xdc,0xa7,0xc2,0x60,0xe2}};
static const GUID _IID_IAudioCaptureClient = {0xc8adbd64, 0xe71e, 0x48a0, {0xa4,0xde, 0x18,0x5c,0x39,0x5c,0xd3,0x17}};
static const GUID _IID_IAudioClient = {0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1,0x78, 0xc2,0xf5,0x68,0xa7,0x03,0xb2}};
static const PROPERTYKEY _PKEY_Device_FriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14}; // DEVPROP_TYPE_STRING
static const PROPERTYKEY _PKEY_AudioEngine_DeviceFormat = {{0xf19f064d, 0x082c, 0x4e27, {0xbc,0x73,0x68,0x82,0xa1,0xbb,0x8e,0x4c}}, 0};

struct capture_thread_context
{
	IMMDevice *pMMDevice;
	HANDLE hStartedEvent;
	HANDLE hStopEvent;
	HRESULT hr;

};

// Everything needs to be done in an external thread, can't use any of the COM objects outside the thread they were created in
DWORD WINAPI capture_thread(LPVOID pContext)
{
	struct capture_thread_context *ctx = pContext;
	CoInitializeEx(NULL, 0);

	HRESULT hr;
	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME hnsActualDuration;
	UINT32 bufferFrameCount;
	UINT32 numFramesAvailable;
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	IAudioCaptureClient *pCaptureClient = NULL;
	WAVEFORMATEX *pwfx = NULL;
	UINT32 packetLength = 0;
	BOOL bDone = FALSE;
	BYTE *pData;
	DWORD flags;

	hr = CoCreateInstance(
	       CLSID_MMDeviceEnumerator, NULL,
	       CLSCTX_ALL, IID_IMMDeviceEnumerator,
	       (void**)&pEnumerator);
	EXIT_ON_ERROR(hr)

	//TODO: allow specifying a device

	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	EXIT_ON_ERROR(hr)

	hr = pDevice->Activate(
	                IID_IAudioClient, CLSCTX_ALL,
	                NULL, (void**)&pAudioClient);
	EXIT_ON_ERROR(hr)

	// TODO: want to co-erce to float format

	// get the default device format
	hr = pAudioClient->GetMixFormat(&pwfx);
	EXIT_ON_ERROR(hr)

	// TODO: include co-ercing downmix to mono
	switch (pwfx->wFormatTag)
	{
		case WAVE_FORMAT_PCM:
			pwfx->wFormatTag = WAVE_FORMAT_IEEE_FLOAT ;
			pwfx->wBitsPerSample = 32;
			pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
			pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
			break;
		case WAVE_FORMAT_EXTENSIBLE:
			PWAVEFORMATEXTENSIBLE pEx = (PWAVEFORMATEXTENSIBLE)pwfx ;
			// TODO
			if (IsEqualGUID(KSDATAFORMAT_SUBTYPE_IEEE_PCM, pEx->SubFormat))
			{
				pEx->SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
				pEx->Samples.wValidBitsPerSample = 32;
				pwfx->wBitsPerSample = 32;
				pwfx->nBlockAlign = pwfx->nChannels * pwfx->wBitsPerSample / 8;
				pwfx->nAvgBytesPerSec = pwfx->nBlockAlign * pwfx->nSamplesPerSec;
			}
			else {
				ERR(L"%s", L"Don't know how to coerce mix format");
				goto error;
			}
			break;
	}

	hr = pAudioClient->Initialize(
	                     AUDCLNT_SHAREMODE_SHARED,
	                     AUDCLNT_STREAMFLAGS_LOOPBACK,
	                     hnsRequestedDuration,
	                     0,
	                     pwfx,
	                     NULL);
	EXIT_ON_ERROR(hr)

	// Get the size of the allocated buffer.
	hr = pAudioClient->GetBufferSize(&bufferFrameCount);
	EXIT_ON_ERROR(hr)

	hr = pAudioClient->GetService(
	                     IID_IAudioCaptureClient,
	                     (void**)&pCaptureClient);
	EXIT_ON_ERROR(hr)

	// Notify the audio sink which format to use.
	hr = pMySink->SetFormat(pwfx);
	EXIT_ON_ERROR(hr)

	// Calculate the actual duration of the allocated buffer.
	hnsActualDuration = (double)REFTIMES_PER_SEC * bufferFrameCount / pwfx->nSamplesPerSec;

	hr = pAudioClient->Start();  // Start recording.
	EXIT_ON_ERROR(hr)

	// Want to wake up every 5ms to mesh well with the desired 1024/48000 = 21.333333333333333ms update rate of the audio subsystem

	// Each loop fills about half of the shared buffer.
	while (bDone == FALSE)
	{
		//TODO: fix duration so that we get < number of samples we want in main audio code
		Sleep(5);

		hr = pCaptureClient->GetNextPacketSize(&packetLength);
		EXIT_ON_ERROR(hr)

		while (packetLength != 0)
		{
			// Get the available data in the shared buffer.
			hr = pCaptureClient->GetBuffer(
			                       &pData,
			                       &numFramesAvailable,
			                       &flags, NULL, NULL);
			EXIT_ON_ERROR(hr)

			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
			{
			    pData = NULL;  // Tell CopyData to write silence.
			}

			// Copy the available capture data to the audio sink.
			hr = pMySink->CopyData(
			                  pData, numFramesAvailable, &bDone);
			EXIT_ON_ERROR(hr)

			hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
			EXIT_ON_ERROR(hr)

			hr = pCaptureClient->GetNextPacketSize(&packetLength);
			EXIT_ON_ERROR(hr)
		}
	}

	hr = pAudioClient->Stop();  // Stop recording.
	EXIT_ON_ERROR(hr)

	ctx->hr = hr ;
	return 0;

error:
	fprintf(stderr, "WASPI error: %s\n", Pa_GetErrorText(err));

	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pDevice)
	SAFE_RELEASE(pAudioClient)
	SAFE_RELEASE(pCaptureClient)

	ctx->hr = hr ;
	return -1;
}



static struct capture_thread_context context;

int audio_setup_waspi(const opt_data *od)
{
	printf("Using WASPI\n");

	context.

	// create a capture thread finished starting event
	HANDLE hStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == hStartedEvent) {
		ERR(L"CreateEvent failed: last error is %u", GetLastError());
		return -__LINE__;
	}

	// create a stop capture thread event
	HANDLE hStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (NULL == hStartedEvent) {
		ERR(L"CreateEvent failed: last error is %u", GetLastError());
		return -__LINE__;
	}

	context.hr            = E_UNEXPECTED;
	context.hStartedEvent = hStartedEvent;
	context.hStopEvent    = hStopEvent;

	HANDLE hThread = CreateThread(
		    NULL, 0,
		    LoopbackCaptureThreadFunction, &threadArgs,
		    0, NULL
		);
	if (NULL == hThread) {
		ERR(L"CreateThread failed: last error is %u", GetLastError());
		return -__LINE__;
	}
	CloseHandleOnExit closeThread(hThread); // FIXME: De-c++

	// wait for either capture to start or the thread to end
	HANDLE waitArray[2] = { hStartedEvent, hThread };



}

void audio_stop_waspi(void)
{
}
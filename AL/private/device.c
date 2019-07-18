#include "../aldc.h"
#include "errors.h"
#include <stdio.h>
#include <dc/sound/stream.h>

#define DREAMCAST_AUDIO_DEVICE_NAME "Yamaha AICA Stereo Device"

static snd_stream_hnd_t STREAM_HANDLE = 0;
static SDL_AudioSpec SPEC;
static Uint8* BUFFER = NULL;
static SDL_AudioStatus STATUS = SDL_AUDIO_STOPPED;

static volatile SDL_bool RUNNING = SDL_TRUE;
static kthread_t* THREAD = NULL;


int SDL_GetNumAudioDevices(int iscapture) {
    if(iscapture) {
        return 0;
    } else {
        return 1;
    }
}

const char *SDL_GetAudioDeviceName(int index, int iscapture) {
    if(index == 0 && !iscapture) {
        return DREAMCAST_AUDIO_DEVICE_NAME;
    } else {
        return NULL;
    }
}

SDL_AudioStatus SDL_GetAudioDeviceStatus(SDL_AudioDeviceID dev) {
    assert(dev == 1);
    return STATUS;
}

void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {
    assert(dev == 1);
    STATUS = (pause_on) ? SDL_AUDIO_PAUSED : SDL_AUDIO_PLAYING;
}

void SDL_LockAudioDevice(SDL_AudioDeviceID dev) {

}

void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) {

}

int stream_thread(void *arg) {
	while(RUNNING) {
		snd_stream_poll(STREAM_HANDLE);
		thd_pass();
	}
	return 0;
}

void* stream_callback(snd_stream_hnd_t hnd, int smp_req, int *smp_recv) {

    // smp_req == spec.size because that's what we passed
    // when initalizing the KOS stream
    assert(STREAM_HANDLE != SND_STREAM_INVALID);

    SDL_LockAudioDevice(1);
    if(STATUS == SDL_AUDIO_PAUSED || !SPEC.callback) {
        // Fill the buffer with silence
        SDL_memset(BUFFER, SPEC.silence, SPEC.size);
    } else {
        assert(hnd == STREAM_HANDLE);
        // Fill the buffer from SDL
        SPEC.callback(SPEC.userdata, BUFFER, SPEC.size);
    }
    SDL_UnlockAudioDevice(1);

    // We always send the full amount of data back to KOS
    // some of it might be silence though
    *smp_recv = SPEC.size;

    // Return it to KOS
    return BUFFER;
}

void SDL_CalculateAudioSpec(SDL_AudioSpec * spec) {
    switch (spec->format) {
    case AUDIO_U8:
        spec->silence = 0x80;
        break;
    default:
        spec->silence = 0x00;
        break;
    }
    spec->size = SDL_AUDIO_BITSIZE(spec->format) / 8;
    spec->size *= spec->channels;
    spec->size *= spec->samples;
}

SDL_AudioDeviceID SDL_OpenAudioDevice(
    const char*          device,
    int                  iscapture,
    const SDL_AudioSpec* desired,
    SDL_AudioSpec*       obtained,
    int                  allowed_changes) {

    if(!device || strcmp(device, '\0') == 0) {
        device = DREAMCAST_AUDIO_DEVICE_NAME;
    }

    if(strcmp(device, DREAMCAST_AUDIO_DEVICE_NAME) != 0) {
        SDL_SetError("Couldn't find matching audio device:");
        fprintf(stderr, device);
        fprintf(stderr, "\n");
        return 0;
    }

    if(iscapture) {
        SDL_SetError("No capture devices supported\n");
        return 0;
    }

    if(STREAM_HANDLE) {
        return 0;
    }

    // Populate the size + silence params of the spec
    *obtained = *desired;
    SDL_CalculateAudioSpec(obtained);
    SPEC = *obtained;

    assert(!BUFFER);

    BUFFER = (Uint8*) malloc(SPEC.size);

    // FIXME: Move to SDL_init();
    snd_stream_init();

    STREAM_HANDLE = snd_stream_alloc(&stream_callback, SPEC.size);
    assert(STREAM_HANDLE != SND_STREAM_INVALID);

    THREAD = thd_create(1, (void*)stream_thread, NULL);
    snd_stream_start(STREAM_HANDLE, SPEC.samples, 1);

    return 1;
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev) {
    if(dev != 1) {
        return;
    }

    // FIXME: Should be atomic
    RUNNING = SDL_FALSE;
    thd_join(THREAD, NULL);

    snd_stream_stop(STREAM_HANDLE);
    snd_stream_destroy(STREAM_HANDLE);
    snd_stream_shutdown();
}

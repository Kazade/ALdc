#include "../aldc.h"
#include "errors.h"
#include <stdio.h>

#ifdef _arch_dreamcast
#include <dc/sound/stream.h>
#endif

#define DREAMCAST_AUDIO_DEVICE_NAME "Yamaha AICA Stereo Device"

#ifndef _arch_dreamcast
typedef uint32_t snd_stream_hnd_t;
const snd_stream_hnd_t SND_STREAM_INVALID = -1;
#else
static kthread_t* THREAD = NULL;
#endif

static SDL_AudioSpec SPEC;
static Uint8* BUFFER = NULL;
static SDL_AudioStatus STATUS = SDL_AUDIO_STOPPED;
static snd_stream_hnd_t STREAM_HANDLE = SND_STREAM_INVALID;
static volatile SDL_bool RUNNING = SDL_TRUE;

/* MojoAL submits stream data as floats, we leverage
 * the SDL coversion stuff to convert it into little endian
 * bytes */
static SDL_AudioStream* STREAM = NULL;

static const SDL_AudioDeviceID DEVICE_ID = 2;

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
    assert(dev == DEVICE_ID);
    return STATUS;
}

void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {
    assert(dev == DEVICE_ID);
    STATUS = (pause_on) ? SDL_AUDIO_PAUSED : SDL_AUDIO_PLAYING;
}

void SDL_LockAudioDevice(SDL_AudioDeviceID dev) {

}

void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) {

}

int stream_thread(void *arg) {
	while(RUNNING) {
#ifdef _arch_dreamcast
		snd_stream_poll(STREAM_HANDLE);
		thd_pass();
#endif
	}
	return 0;
}

#ifdef _arch_dreamcast
void* stream_callback(snd_stream_hnd_t hnd, int smp_req, int *smp_recv) {
    static Uint8* CONVERTED_BUFFER = NULL;

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

    int rc = SDL_AudioStreamPut(STREAM, BUFFER, SPEC.size);
    assert(rc != -1);

    SDL_AudioStreamFlush(STREAM);

    int available = SDL_AudioStreamAvailable(STREAM);

    if(CONVERTED_BUFFER) {
        free(CONVERTED_BUFFER);
    }

    CONVERTED_BUFFER = (Uint8*) malloc(available);

    int gotten = SDL_AudioStreamGet(STREAM, CONVERTED_BUFFER, sizeof(CONVERTED_BUFFER));
    assert(gotten != -1);

    *smp_recv = gotten;

    // Return it to KOS
    return CONVERTED_BUFFER;
}
#endif

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

    if(device && strlen(device) && strcmp(device, DREAMCAST_AUDIO_DEVICE_NAME) != 0) {
        SDL_SetError("Couldn't find matching audio device:");
        fprintf(stderr, device);
        fprintf(stderr, "\n");
        return 0;
    }

    if(iscapture) {
        SDL_SetError("No capture devices supported\n");
        return 0;
    }

    assert(STREAM_HANDLE == SND_STREAM_INVALID);

    if(STREAM_HANDLE != SND_STREAM_INVALID) {
        return 0;
    }

    memcpy(&SPEC, desired, sizeof(SDL_AudioSpec));
    SDL_CalculateAudioSpec(&SPEC);

    if(obtained) {
        memcpy(obtained, &SPEC, sizeof(SDL_AudioSpec));
    }

    // This is what mojoAL submits
    assert(desired->format == AUDIO_F32LSB);
    assert(SPEC.format == AUDIO_F32LSB);

    assert(!BUFFER);

    BUFFER = (Uint8*) malloc(SPEC.size);

    STREAM = SDL_NewAudioStream(
        SPEC.format, SPEC.channels, SPEC.freq,
        AUDIO_S8, SPEC.channels, SPEC.freq
    );

    assert(STREAM);

    // All things start paused!
    STATUS = SDL_AUDIO_PAUSED;

#ifdef _arch_dreamcast
    // FIXME: Move to SDL_init();
    snd_stream_init();

    STREAM_HANDLE = snd_stream_alloc(&stream_callback, SPEC.size);
    assert(STREAM_HANDLE != SND_STREAM_INVALID);

    THREAD = thd_create(1, (void*)stream_thread, NULL);
    snd_stream_start(STREAM_HANDLE, SPEC.samples, 1);
#endif
    return DEVICE_ID;
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev) {
    if(dev != DEVICE_ID) {
        return;
    }

    // FIXME: Should be atomic
    RUNNING = SDL_FALSE;

#ifdef _arch_dreamcast
    thd_join(THREAD, NULL);

    snd_stream_stop(STREAM_HANDLE);
    snd_stream_destroy(STREAM_HANDLE);
    snd_stream_shutdown();
#endif

    SDL_FreeAudioStream(STREAM);
}

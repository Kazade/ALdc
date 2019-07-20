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

// This is the spec that MojoAL asked for
static SDL_AudioSpec DESIRED_SPEC;

// This is the spec that we're passing to KOS
static SDL_AudioSpec OBTAINED_SPEC;

// We then convert the format into this dest buffer
static Uint8* DEST_BUFFER = NULL;

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
    if(!pause_on) // printf("Unpausing audio\n");
    STATUS = (pause_on) ? SDL_AUDIO_PAUSED : SDL_AUDIO_PLAYING;
}

void SDL_LockAudioDevice(SDL_AudioDeviceID dev) {

}

void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) {

}

#ifdef _arch_dreamcast
void* stream_callback(snd_stream_hnd_t hnd, int smp_req, int *smp_recv) {
    static const int SAMPLES = 4096;
    static Uint8 BUFFER[4096];

    // printf("KOS requested %d bytes\n", smp_req);

    // smp_req == spec.size because that's what we passed
    // when initalizing the KOS stream
    assert(STREAM_HANDLE != SND_STREAM_INVALID);

    SDL_LockAudioDevice(1);

    assert(hnd == STREAM_HANDLE);

    // Fill the buffer from SDL
    while (SDL_AudioStreamAvailable(STREAM) < smp_req) {
        if(STATUS == SDL_AUDIO_PAUSED) {
            // printf("Playing silence...(stream is paused)\n");
            SDL_memset(BUFFER, DESIRED_SPEC.silence, SAMPLES);
        } else {
            // printf("Gathering data...\n");
            OBTAINED_SPEC.callback(DESIRED_SPEC.userdata, BUFFER, SAMPLES);
        }
        int rc = SDL_AudioStreamPut(STREAM, BUFFER, SAMPLES);
        assert(rc != -1);
    }

    SDL_UnlockAudioDevice(1);

    // printf("Converting buffer of size %u...\n", (unsigned int) DESIRED_SPEC.size);
    // printf("%d bytes available...\n", available);
    int gotten = SDL_AudioStreamGet(STREAM, DEST_BUFFER, smp_req);
    assert(gotten != -1);

    *smp_recv = gotten;

    // printf("Returning buffer to KOS...\n");
    // Return it to KOS
    return DEST_BUFFER;
}
#endif

int stream_thread(void *arg) {
    STREAM_HANDLE = snd_stream_alloc(&stream_callback, OBTAINED_SPEC.size);
    assert(STREAM_HANDLE != SND_STREAM_INVALID);
    snd_stream_start(STREAM_HANDLE, OBTAINED_SPEC.freq, OBTAINED_SPEC.channels == 2);

	while(RUNNING) {
#ifdef _arch_dreamcast
        int ret = snd_stream_poll(STREAM_HANDLE);
        assert(ret == 0);

        thd_pass();
#endif
	}

    snd_stream_stop(STREAM_HANDLE);
    snd_stream_destroy(STREAM_HANDLE);
    snd_stream_shutdown();

    // printf("Thread exit!\n");
	return 0;
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

    // We should check device here, but, mojoAL passes NULL
    // for the const char* and for some reason checking for
    // NULL doesn't work and the world ends horrifically, so
    // we don't check device here.

    if(iscapture) {
        SDL_SetError("No capture devices supported\n");
        return 0;
    }

    assert(STREAM_HANDLE == SND_STREAM_INVALID);

    if(STREAM_HANDLE != SND_STREAM_INVALID) {
        return 0;
    }

    memcpy(&DESIRED_SPEC, desired, sizeof(SDL_AudioSpec));
    memcpy(&OBTAINED_SPEC, desired, sizeof(SDL_AudioSpec));

    OBTAINED_SPEC.format = AUDIO_S16LSB;
    OBTAINED_SPEC.channels = 2;

    SDL_CalculateAudioSpec(&DESIRED_SPEC);
    SDL_CalculateAudioSpec(&OBTAINED_SPEC);

    if(obtained) {
        memcpy(obtained, &OBTAINED_SPEC, sizeof(SDL_AudioSpec));
    }

    // This is what mojoAL submits
    assert(desired->format == AUDIO_F32LSB);

    DEST_BUFFER = (Uint8*) malloc(OBTAINED_SPEC.size);

    STREAM = SDL_NewAudioStream(
        DESIRED_SPEC.format, DESIRED_SPEC.channels, DESIRED_SPEC.freq,
        OBTAINED_SPEC.format, OBTAINED_SPEC.channels, OBTAINED_SPEC.freq
    );

    assert(STREAM);

    // All things start paused!
    STATUS = SDL_AUDIO_PAUSED;

#ifdef _arch_dreamcast
    // FIXME: Move to SDL_init();
    int rc = snd_stream_init();
    assert(rc == 0);

    assert(OBTAINED_SPEC.size < SND_STREAM_BUFFER_MAX);

    // printf("Desired Frequency is: %d\n", DESIRED_SPEC.freq);
    // printf("Desired Channels: %d\n", DESIRED_SPEC.channels);
    // printf("Desired Samples: %d\n", DESIRED_SPEC.samples);

    // printf("Starting stream...\n");
    // printf("Frequency is: %d\n", OBTAINED_SPEC.freq);
    // printf("Channels: %d\n", OBTAINED_SPEC.channels);

    THREAD = thd_create(1, (void*)stream_thread, NULL);
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
    // printf("Destroying stream!\n");
    thd_join(THREAD, NULL);
#endif

    SDL_FreeAudioStream(STREAM);
}

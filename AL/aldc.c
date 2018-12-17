#include <stdint.h>
#include <kos/mutex.h>
#include "aldc.h"

// FIXME: How to implement this on SH4?
#define PAUSE_INSTRUCTION()

#define SDL_Delay thd_sleep

static SDL_SpinLock locks[32];

static inline void enterLock(void *a) {
    uintptr_t index = ((((uintptr_t)a) >> 3) & 0x1f);
    SDL_AtomicLock(&locks[index]);
}

static inline void leaveLock(void *a) {
    uintptr_t index = ((((uintptr_t)a) >> 3) & 0x1f);
    SDL_AtomicUnlock(&locks[index]);
}

SDL_bool SDL_AtomicTryLock(SDL_SpinLock *lock)
{
    /* Terrible terrible damage */
    static mutex_t *_spinlock_mutex;

    if (!_spinlock_mutex) {
        _spinlock_mutex = (mutex_t*) malloc(sizeof(mutex_t));
        /* Race condition on first lock... */
        mutex_init(_spinlock_mutex, MUTEX_TYPE_NORMAL);
    }

    mutex_lock(_spinlock_mutex);

    if (*lock == 0) {
        *lock = 1;
        mutex_unlock(_spinlock_mutex);
        return SDL_TRUE;
    } else {
        mutex_unlock(_spinlock_mutex);
        return SDL_FALSE;
    }
}

void SDL_AtomicLock(SDL_SpinLock *lock) {
    int iterations = 0;
    /* FIXME: Should we have an eventual timeout? */
    while (!SDL_AtomicTryLock(lock)) {
        if (iterations < 32) {
            iterations++;
            PAUSE_INSTRUCTION();
        } else {
            /* !!! FIXME: this doesn't definitely give up the current timeslice, it does different things on various platforms. */
            SDL_Delay(0);
        }
    }
}

void SDL_AtomicUnlock(SDL_SpinLock *lock) {
    *lock = 0;
}

SDL_bool SDL_AtomicCAS(SDL_atomic_t *a, int oldval, int newval) {
    SDL_bool retval = SDL_FALSE;

    enterLock(a);
    if (a->value == oldval) {
        a->value = newval;
        retval = SDL_TRUE;
    }
    leaveLock(a);

    return retval;
}

SDL_bool SDL_AtomicCASPtr(void* *a, void *oldval, void *newval) {
    SDL_bool retval = SDL_FALSE;

    enterLock(a);
    if (*a == oldval) {
        *a = newval;
        retval = SDL_TRUE;
    }
    leaveLock(a);

    return retval;
}

int SDLCALL SDL_AtomicSet(SDL_atomic_t *a, int v) {
    int value;
    do {
        value = a->value;
    } while (!SDL_AtomicCAS(a, value, v));
    return value;
}

int SDLCALL SDL_AtomicAdd(SDL_atomic_t *a, int v)
{
    int value;
    do {
        value = a->value;
    } while (!SDL_AtomicCAS(a, value, (value + v)));
    return value;
}

void* SDL_AtomicSetPtr(void* *a, void* v)
{
    void* value;
    do {
        value = *a;
    } while (!SDL_AtomicCASPtr(a, value, v));
    return value;
}

void* SDL_AtomicGetPtr(void* *a)
{
    void* value = *a;
    SDL_CompilerBarrier();
    return value;
}

int SDL_AtomicGet(SDL_atomic_t *a)
{
    int value = a->value;
    SDL_CompilerBarrier();
    return value;
}

int SDL_InitSubSystem(Uint32 flags) {
    return 0;
}

void SDL_QuitSubSystem(Uint32 flags) {

}

SDL_AudioDeviceID SDL_OpenAudioDevice(
    const char*          device,
    int                  iscapture,
    const SDL_AudioSpec* desired,
    SDL_AudioSpec*       obtained,
    int                  allowed_changes
        ) {

}

void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID dev) {

}

SDL_AudioStream * SDLCALL SDL_NewAudioStream(
    const SDL_AudioFormat src_format,
    const Uint8 src_channels,
    const int src_rate,
    const SDL_AudioFormat dst_format,
    const Uint8 dst_channels,
    const int dst_rate
        ) {

}

int SDLCALL SDL_AudioStreamPut(SDL_AudioStream *stream, const void *buf, int len) {

}

int SDLCALL SDL_AudioStreamGet(SDL_AudioStream *stream, void *buf, int len) {

}

int SDLCALL SDL_AudioStreamAvailable(SDL_AudioStream *stream) {

}

SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID dev) {

}

void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {

}

void SDLCALL SDL_LockAudioDevice(SDL_AudioDeviceID dev) {

}

void SDLCALL SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) {

}

void SDLCALL SDL_FreeAudioStream(SDL_AudioStream *stream) {

}

int SDLCALL SDL_GetNumAudioDevices(int iscapture) {

}

const char *SDLCALL SDL_GetAudioDeviceName(int index, int iscapture) {

}

int SDLCALL SDL_BuildAudioCVT(
    SDL_AudioCVT * cvt,
    SDL_AudioFormat src_format,
    Uint8 src_channels,
    int src_rate,
    SDL_AudioFormat dst_format,
    Uint8 dst_channels,
    int dst_rate
        ) {

}

int SDLCALL SDL_ConvertAudio(SDL_AudioCVT * cvt) {

}

#include "aldc.h"

void SDL_AtomicLock(SDL_SpinLock *lock) {

}

void SDL_AtomicUnlock(SDL_SpinLock *lock) {

}

SDL_bool SDL_AtomicCAS(SDL_atomic_t *a, int oldval, int newval) {

}

SDL_bool SDL_AtomicCASPtr(void* *a, void *oldval, void *newval) {

}

void SDLCALL SDL_MemoryBarrierRelease(void) {

}

void SDLCALL SDL_MemoryBarrierAcquire(void) {

}

int SDLCALL SDL_AtomicSet(SDL_atomic_t *a, int v) {

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

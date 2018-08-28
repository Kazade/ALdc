#include <stdlib.h>
#include <string.h>

#include "private.h"
#include "../containers/named_array.h"
#include "../include/AL/al.h"
#include "../include/AL/alc.h"


struct ALCdevice_struct {
    ALCchar deviceName[64];
    ALboolean isOpen;

    NamedArray contexts;
};


/* DC only supports a single device (for now, I mean I guess we could support the VMU
   beeps or something but that would be weird... maybe microphone capture?)
*/

static ALCdevice DEVICE;
static ALCcontext* CURRENT_CONTEXT = NULL;

ALCdevice* ALC_APIENTRY alcOpenDevice(const ALCchar *devicename) {
    if(devicename == NULL || strcmp(devicename, DREAMCAST_AUDIO_DEVICE) == 0) {
        DEVICE.isOpen = AL_TRUE;
        named_array_init(&DEVICE.contexts, sizeof(ALCcontext), MAX_DEVICE_CONTEXTS);
        return &DEVICE;
    }

    return NULL;
}

ALCboolean ALC_APIENTRY alcCloseDevice(ALCdevice *device) {
    device->isOpen = AL_FALSE;
    return AL_TRUE;
}

ALCcontext* ALC_APIENTRY alcCreateContext(ALCdevice *device, const ALCint* attrlist) {
    ALuint name;
    ALCcontext* context = (ALCcontext*) named_array_alloc(&device->contexts, &name);
    context->name = name;
    context->device = device;
    return context;
}

ALCboolean  ALC_APIENTRY alcMakeContextCurrent(ALCcontext *context) {
    CURRENT_CONTEXT = context;
}

void ALC_APIENTRY alcProcessContext(ALCcontext *context) {

}

void ALC_APIENTRY alcSuspendContext(ALCcontext *context) {
    context->state = CONTEXT_STATE_SUSPENDED;
}

void ALC_APIENTRY alcDestroyContext(ALCcontext *context) {
    named_array_release(&context->device->contexts, context->name);
}

ALCcontext* ALC_APIENTRY alcGetCurrentContext(void) {
    return CURRENT_CONTEXT;
}

ALCdevice*  ALC_APIENTRY alcGetContextsDevice(ALCcontext *context) {
    return context->device;
}

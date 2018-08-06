#include <stdlib.h>

#include "private.h"
#include "../include/AL/alc.h"

ALCboolean ALC_APIENTRY alcIsExtensionPresent(ALCdevice *device, const ALCchar *extname) {
    if(strcmp(extname, "ALC_ENUMERATION_EXT") == 0) {
        return AL_TRUE;
    }

    return AL_FALSE;
}

void* ALC_APIENTRY alcGetProcAddress(ALCdevice *device, const ALCchar *funcname) {
    return NULL;
}

ALCenum ALC_APIENTRY alcGetEnumValue(ALCdevice *device, const ALCchar *enumname) {

}

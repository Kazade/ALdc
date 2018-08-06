#include "private.h"
#include "../include/AL/alc.h"


const ALCchar* ALC_APIENTRY alcGetString(ALCdevice *device, ALCenum param) {
    switch(param) {
        case ALC_DEVICE_SPECIFIER:
            return "Dreamcast Audio\0\0";
        break;
        default: {
            _alThrowError(ALC_INVALID_ENUM, __func__);
            return;
        }
    }
}

void ALC_APIENTRY alcGetIntegerv(ALCdevice *device, ALCenum param, ALCsizei size, ALCint *values) {

}

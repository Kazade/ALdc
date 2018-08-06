#include <stdio.h>
#include "../include/AL/al.h"

static ALenum CURRENT_ERROR = AL_NO_ERROR;


const char* _alErrorToString(ALenum error) {
    switch(error) {
        case AL_NO_ERROR: return "AL_NO_ERROR";
        case AL_INVALID_ENUM: return "AL_INVALID_ENUM";
        case AL_INVALID_VALUE: return "AL_INVALID_VALUE";
        case AL_INVALID_NAME: return "AL_INVALID_NAME";
        case AL_INVALID_OPERATION: return "AL_INVALID_OPERATION";
        return "UNKNOWN_ERROR";
    }
}

void _alThrowError(ALenum type, const char* funcName) {
    if(CURRENT_ERROR == AL_NO_ERROR) {
        CURRENT_ERROR = type;
        fprintf(stderr, _alErrorToString(type));
    }
}

ALenum AL_APIENTRY alGetError(void) {
    ALenum ret = CURRENT_ERROR;
    CURRENT_ERROR = AL_NO_ERROR;
    return ret;
}

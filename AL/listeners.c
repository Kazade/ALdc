#include "private.h"
#include "../include/AL/alc.h"

void AL_APIENTRY alListenerf(ALenum param, ALfloat value) {
    ALCcontext* current = alcGetCurrentContext();

    switch(param) {
        case AL_GAIN:
            current->listener.gain = value;
        break;
        default:
            _alThrowError(AL_INVALID_ENUM, __func__);
    }
}

void AL_APIENTRY alListener3f(ALenum param, ALfloat value1, ALfloat value2, ALfloat value3) {
    ALCcontext* current = alcGetCurrentContext();

    switch(param) {
        case AL_POSITION: {
            current->listener.position[0] = value1;
            current->listener.position[1] = value2;
            current->listener.position[2] = value3;
        }
        break;
        case AL_VELOCITY: {
            current->listener.velocity[0] = value1;
            current->listener.velocity[1] = value2;
            current->listener.velocity[2] = value3;
        }
        break;
        default:
            _alThrowError(AL_INVALID_ENUM, __func__);
    }
}

void AL_APIENTRY alListenerfv(ALenum param, const ALfloat *values) {
    ALCcontext* current = alcGetCurrentContext();

    switch(param) {
        case AL_POSITION:
        case AL_VELOCITY: {
            alListener3f(param, values[0], values[1], values[2]);
        }
        break;
        default:
            _alThrowError(AL_INVALID_ENUM, __func__);
    }
}

void AL_APIENTRY alListeneri(ALenum param, ALint value) {

}

void AL_APIENTRY alListener3i(ALenum param, ALint value1, ALint value2, ALint value3) {

}

void AL_APIENTRY alListeneriv(ALenum param, const ALint *values) {

}

void AL_APIENTRY alGetListenerf(ALenum param, ALfloat *value) {

}

void AL_APIENTRY alGetListener3f(ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3) {

}

void AL_APIENTRY alGetListenerfv(ALenum param, ALfloat *values) {

}

void AL_APIENTRY alGetListeneri(ALenum param, ALint *value) {

}

void AL_APIENTRY alGetListener3i(ALenum param, ALint *value1, ALint *value2, ALint *value3) {

}

void AL_APIENTRY alGetListeneriv(ALenum param, ALint *values) {

}

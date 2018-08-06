#pragma once

#include "../include/AL/al.h"

#define MAX_DEVICE_CONTEXTS 2
#define DREAMCAST_AUDIO_DEVICE "Dreamcast Audio"

void _alThrowError(ALenum type, const char* funcName);


typedef struct {
    ALfloat gain;
    ALfloat position[3];
    ALfloat velocity[3];
    ALfloat up[3];
    ALfloat forward[3];
} Listener;

struct ALCcontext_struct {
    ALuint name;
    ALCdevice* device;
    enum ContextState state;

    ALint frequency;
    ALint monoSources;
    ALint stereoSources;
    ALint refresh;
    ALint sync;

    Listener listener;
};

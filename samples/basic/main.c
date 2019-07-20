/*
 * OpenAL example
 *
 * Copyright(C) Florian Fainelli <f.fainelli@gmail.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>

#include <AL/al.h>
#include <AL/alc.h>

#ifdef _arch_dreamcast
extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);
#endif

static void list_audio_devices(const ALCchar *devices)
{
	const ALCchar *device = devices, *next = devices + 1;
	size_t len = 0;

	fprintf(stdout, "Devices list:\n");
	fprintf(stdout, "----------\n");
	while (device && *device != '\0' && next && *next != '\0') {
		fprintf(stdout, "%s\n", device);
		len = strlen(device);
		device += (len + 1);
		next += (len + 2);
	}
	fprintf(stdout, "----------\n");
}

#define TEST_ERROR(_msg)		\
	error = alGetError();		\
	if (error != AL_NO_ERROR) {	\
        fprintf(stderr, "ERROR: "); \
		fprintf(stderr, _msg "\n");	\
		return -1;		\
	}

static inline ALenum to_al_format(short channels, short samples)
{
	bool stereo = (channels > 1);

	switch (samples) {
	case 16:
		if (stereo)
			return AL_FORMAT_STEREO16;
		else
			return AL_FORMAT_MONO16;
	case 8:
		if (stereo)
			return AL_FORMAT_STEREO8;
		else
			return AL_FORMAT_MONO8;
	default:
		return -1;
	}
}

static bool is_big_endian() {
    int a = 1;
    return !((char*)&a)[0];
}

int convert_to_int(char* buffer, int len) {
    int i = 0;
    int a = 0;
    if (!is_big_endian())
        for (; i<len; i++)
            ((char*)&a)[i] = buffer[i];
    else
        for (; i<len; i++)
            ((char*)&a)[3 - i] = buffer[i];
    return a;
}

ALboolean LoadWAVFile(const char* filename, ALenum* format, ALvoid** data, ALsizei* size, ALsizei* freq) {
    char buffer[4];

    FILE* in = fopen(filename, "rb");
    if(!in) {
        fprintf(stderr, "Couldn't open file\n");
        return AL_FALSE;
    }

    fread(buffer, 4, sizeof(char), in);

    if (strncmp(buffer, "RIFF", 4) != 0) {
        fprintf(stderr, "Not a valid wave file\n");
        return AL_FALSE;
    }

    fread(buffer, 4, sizeof(char), in);
    fread(buffer, 4, sizeof(char), in);          //WAVE
    fread(buffer, 4, sizeof(char), in);          //fmt
    fread(buffer, 4, sizeof(char), in);      //16
    fread(buffer, 2, sizeof(char), in);      //1
    fread(buffer, 2, sizeof(char), in);

    int chan = convert_to_int(buffer, 2);
    fread(buffer, 4, sizeof(char), in);
    *freq = convert_to_int(buffer, 4);
    fread(buffer, 4, sizeof(char), in);
    fread(buffer, 2, sizeof(char), in);
    fread(buffer, 2, sizeof(char), in);
    int bps = convert_to_int(buffer, 2);
    fread(buffer, 4, sizeof(char), in);      //data
    fread(buffer, 4, sizeof(char), in);
    *size = (ALsizei) convert_to_int(buffer, 4);
    *data = (ALvoid*) malloc(*size * sizeof(char));
    fread(*data, *size, sizeof(char), in);

    if(chan == 1) {
        *format = (bps == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    } else {
        *format = (bps == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
    }

    fclose(in);

    printf("Loaded WAV file!\n");

    return AL_TRUE;
}

int main(int argc, char **argv)
{
	ALboolean enumeration;
	const ALCchar *defaultDeviceName = argv[1];
	ALCdevice *device;
	ALvoid *data;
	ALCcontext *context;
	ALsizei size, freq;
	ALenum format;
	ALuint buffer, source;
	ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
	ALCenum error;
	ALint source_state;

	enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
	if (enumeration == AL_FALSE)
		fprintf(stderr, "enumeration extension not available\n");

	list_audio_devices(alcGetString(NULL, ALC_DEVICE_SPECIFIER));

	if (!defaultDeviceName)
		defaultDeviceName = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

	device = alcOpenDevice(defaultDeviceName);
	if (!device) {
		fprintf(stderr, "unable to open default device\n");
		return -1;
	}

    // fprintf(stdout, "Device: %s\n", alcGetString(device, ALC_DEVICE_SPECIFIER));

	alGetError();

	context = alcCreateContext(device, NULL);
	if (!alcMakeContextCurrent(context)) {
		fprintf(stderr, "failed to make default context\n");
		return -1;
	}
	TEST_ERROR("make default context");

	/* set orientation */
	alListener3f(AL_POSITION, 0, 0, 1.0f);
	TEST_ERROR("listener position");
    	alListener3f(AL_VELOCITY, 0, 0, 0);
	TEST_ERROR("listener velocity");
	alListenerfv(AL_ORIENTATION, listenerOri);
	TEST_ERROR("listener orientation");

	alGenSources((ALuint)1, &source);
	TEST_ERROR("source generation");

	alSourcef(source, AL_PITCH, 1);
	TEST_ERROR("source pitch");
	alSourcef(source, AL_GAIN, 1);
	TEST_ERROR("source gain");
	alSource3f(source, AL_POSITION, 0, 0, 0);
	TEST_ERROR("source position");
	alSource3f(source, AL_VELOCITY, 0, 0, 0);
	TEST_ERROR("source velocity");
	alSourcei(source, AL_LOOPING, AL_FALSE);
	TEST_ERROR("source looping");

	alGenBuffers(1, &buffer);
	TEST_ERROR("buffer generation");

#ifdef _arch_dreamcast
    if(!LoadWAVFile("/rd/test.wav", &format, &data, &size, &freq)) {
        return -1;
    }
#else
    if(!LoadWAVFile("test.wav", &format, &data, &size, &freq)) {
        return -1;
    }
#endif

	TEST_ERROR("loading wav file");

	alBufferData(buffer, format, data, size, freq);
	TEST_ERROR("buffer copy");

	alSourcei(source, AL_BUFFER, buffer);
	TEST_ERROR("buffer binding");

    alSourcei(source, AL_LOOPING, 1);
	alSourcePlay(source);
	TEST_ERROR("source playing");

	alGetSourcei(source, AL_SOURCE_STATE, &source_state);
	TEST_ERROR("source state get");
	while (source_state == AL_PLAYING) {
		alGetSourcei(source, AL_SOURCE_STATE, &source_state);
		TEST_ERROR("source state get");
        thd_pass();
	}

	/* exit context */
	alDeleteSources(1, &source);
	alDeleteBuffers(1, &buffer);
	device = alcGetContextsDevice(context);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);

	return 0;
}

/*
 * Streaming OpenAL Sample
 *
 * Copyright (c) Florian Fainelli <f.fainelli@gmail.com>
 * Copyright (c) 2019 HaydenKow
 * Copyright (c) 2019 Luke Benstead
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

extern uint8 romdisk[];
KOS_INIT_ROMDISK(romdisk);

static void list_audio_devices(const ALCchar *devices)
{
	const ALCchar *device = devices, *next = devices + 1;
	size_t len = 0;

	fprintf(stdout, "Devices list:\n");
	fprintf(stdout, "----------\n");
	while (device && *device != '\0' && next && *next != '\0')
	{
		fprintf(stdout, "%s\n", device);
		len = strlen(device);
		device += (len + 1);
		next += (len + 2);
	}
	fprintf(stdout, "----------\n");
}

#define TEST_ERROR(_msg)            \
	error = alGetError();           \
	if (error != AL_NO_ERROR)       \
	{                               \
		fprintf(stderr, "ERROR: "); \
		fprintf(stderr, _msg "\n"); \
		return -1;                  \
	}

static inline ALenum to_al_format(short channels, short samples)
{
	bool stereo = (channels > 1);

	switch (samples)
	{
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

static bool is_big_endian()
{
	int a = 1;
	return !((char *)&a)[0];
}

int convert_to_int(char *buffer, int len)
{
	int i = 0;
	int a = 0;
	if (!is_big_endian())
		for (; i < len; i++)
			((char *)&a)[i] = buffer[i];
	else
		for (; i < len; i++)
			((char *)&a)[3 - i] = buffer[i];
	return a;
}

static FILE *in;
#define DATA_CHUNK_SIZE (1024 * 64)

static const int WAV_data_start = 44;
static int WAV_size;
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static void WAVE_buffer(void *data)
{

	int spare = WAV_size - ftell(in);
	int read = fread(data, 1, MIN(DATA_CHUNK_SIZE, spare), in);
	printf("Reading WAV: read: %d spare:%d\n", read, spare);
	if (read < DATA_CHUNK_SIZE)
	{
		printf("Writing silence\n");
		fseek(in, WAV_data_start, SEEK_SET);
		memset(&((char *)data)[read], 0, DATA_CHUNK_SIZE - read);
		//fread(&((char*)data)[read], DATA_CHUNK_SIZE-read, 1, in);
	}
}

ALboolean LoadWAVFile(const char *filename, ALenum *format, ALsizei *size, ALsizei *freq)
{
	char buffer[4];

	in = fopen(filename, "rb");
	if (!in)
	{
		fprintf(stderr, "Couldn't open file\n");
		return AL_FALSE;
	}
	fseek(in, 0, SEEK_END);
	WAV_size = ftell(in);
	fseek(in, 0, SEEK_SET);

	fread(buffer, 4, sizeof(char), in);

	if (strncmp(buffer, "RIFF", 4) != 0)
	{
		fprintf(stderr, "Not a valid wave file\n");
		return AL_FALSE;
	}

	fread(buffer, 4, sizeof(char), in);
	fread(buffer, 4, sizeof(char), in); //WAVE
	fread(buffer, 4, sizeof(char), in); //fmt
	fread(buffer, 4, sizeof(char), in); //16
	fread(buffer, 2, sizeof(char), in); //1
	fread(buffer, 2, sizeof(char), in);

	int chan = convert_to_int(buffer, 2);
	fread(buffer, 4, sizeof(char), in);
	*freq = convert_to_int(buffer, 4);
	fread(buffer, 4, sizeof(char), in);
	fread(buffer, 2, sizeof(char), in);
	fread(buffer, 2, sizeof(char), in);
	int bps = convert_to_int(buffer, 2);
	fread(buffer, 4, sizeof(char), in); //data
	fread(buffer, 4, sizeof(char), in);
	*size = (ALsizei)convert_to_int(buffer, 4);
	//*data = (ALvoid*) malloc(DATA_CHUNK_SIZE * sizeof(char));
	//fread(*data, DATA_CHUNK_SIZE, sizeof(char), in);

	if (chan == 1)
	{
		*format = (bps == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
	}
	else
	{
		*format = (bps == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
	}

	//fclose(in);

	printf("Loaded WAV file!\n");

	return AL_TRUE;
}

ALCenum error;

int setupHelicopter(void)
{
	ALuint source, buffer;
	ALsizei size, freq;
	ALenum format;
	ALvoid *data;

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

	if (!LoadWAVFile("/rd/test.wav", &format, &size, &freq)) {
		return -1;
	}

	data = (ALvoid *)malloc(size);
	fread(data, size, sizeof(char), in);
	fclose(in);
	TEST_ERROR("loading wav file");

	alBufferData(buffer, format, data, size, freq);
	TEST_ERROR("buffer copy");
	free(data);
	alSourcei(source, AL_BUFFER, buffer);
	TEST_ERROR("buffer binding");

	alSourcei(source, AL_LOOPING, 1);
	alSourcePlay(source);
	TEST_ERROR("source playing");

	return 0;
}
int main(int argc, char **argv)
{
	ALboolean enumeration;
	const ALCchar *defaultDeviceName = argv[1];
	ALCdevice *device;
	ALCcontext *context;
	ALsizei size, freq;
	ALenum format;
	ALuint source;
	ALfloat listenerOri[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
	ALint source_state;

	enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
	if (enumeration == AL_FALSE)
		fprintf(stderr, "enumeration extension not available\n");

	list_audio_devices(alcGetString(NULL, ALC_DEVICE_SPECIFIER));

	if (!defaultDeviceName)
		defaultDeviceName = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

	device = alcOpenDevice(defaultDeviceName);
	if (!device)
	{
		fprintf(stderr, "unable to open default device\n");
		return -1;
	}

	// fprintf(stdout, "Device: %s\n", alcGetString(device, ALC_DEVICE_SPECIFIER));

	alGetError();

	context = alcCreateContext(device, NULL);
	if (!alcMakeContextCurrent(context))
	{
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

	setupHelicopter();

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
	//alSourcei(source, AL_LOOPING, AL_FALSE);
	TEST_ERROR("source looping");

	// This example uses 4 buffers and 1 source
	static ALuint buffer[4];

	alGenSources((ALsizei)1, &source);
	alGenBuffers((ALsizei)4, buffer);
	TEST_ERROR("buffer generation");

	if (!LoadWAVFile("/rd/file_1.wav", &format, &size, &freq))
	{
		return -1;
	}

	ALvoid *data;

	int i;
	// Fill all the buffers with audio data from the wave file
	for (i = 0; i < 4; i++)
	{
		data = malloc(DATA_CHUNK_SIZE);
		WAVE_buffer(data);
		alBufferData(buffer[i], format, data, DATA_CHUNK_SIZE, freq);
		free(data);
		alSourceQueueBuffers(source, 1, &buffer[i]);
	}
	TEST_ERROR("loading wav file");

	ALint iBuffersProcessed;
	ALint iTotalBuffersProcessed;
	ALuint uiBuffer;
	// Buffer queuing loop must operate in a new thread
	iBuffersProcessed = 0;

	alSourcePlay(source);
	TEST_ERROR("source playing");

	while (1)
	{
		alGetSourcei(source, AL_SOURCE_STATE, &source_state);
		TEST_ERROR("source state get");
		while (source_state == AL_PLAYING)
		{
			alGetSourcei(source, AL_BUFFERS_PROCESSED, &iBuffersProcessed);

			// Buffer queuing loop must operate in a new thread
			iBuffersProcessed = 0;
			alGetSourcei(source, AL_BUFFERS_PROCESSED, &iBuffersProcessed);

			iTotalBuffersProcessed += iBuffersProcessed;

			// For each processed buffer, remove it from the source queue, read the next chunk of
			// audio data from the file, fill the buffer with new data, and add it to the source queue
			while (iBuffersProcessed)
			{
				// Remove the buffer from the queue (uiBuffer contains the buffer ID for the dequeued buffer)
				uiBuffer = 0;
				alSourceUnqueueBuffers(source, 1, &uiBuffer);

				// Read more pData audio data (if there is any)
				data = malloc(DATA_CHUNK_SIZE);
				WAVE_buffer(data);
				// Copy audio data to buffer
				alBufferData(uiBuffer, format, data, DATA_CHUNK_SIZE, freq);
				free(data);
				// Insert the audio buffer to the source queue
				alSourceQueueBuffers(source, 1, &uiBuffer);

				iBuffersProcessed--;
			}

			alGetSourcei(source, AL_SOURCE_STATE, &source_state);
			TEST_ERROR("source state get");
			thd_pass();
		}
	}
	fclose(in);

	/* exit context */
	alDeleteSources(1, &source);
	alDeleteBuffers(4, &buffer[0]);
	device = alcGetContextsDevice(context);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);

	return 0;
}

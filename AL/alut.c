#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/AL/alut.h"


static ALenum ALUT_ERROR = ALUT_ERROR_NO_ERROR;

AL_API ALboolean AL_APIENTRY alutInit(int *argcp, char **argv) {
    return AL_TRUE;
}

AL_API ALboolean AL_APIENTRY alutInitWithoutContext(int *argcp, char **argv) {
    return AL_TRUE;
}

AL_API ALboolean AL_APIENTRY alutExit(void) {
    return AL_TRUE;
}

AL_API ALenum AL_APIENTRY alutGetError() {
    ALenum result = ALUT_ERROR;
    ALUT_ERROR = ALUT_ERROR_NO_ERROR;
    return result;
}

AL_API const ALchar* AL_APIENTRY alutGetErrorString(ALenum error) {
    switch(error) {
        case ALUT_ERROR_NO_ERROR: return "No Error";
        /* FIXME */
        default:
            return "Undescribed error";
    }
}

static inline ALenum to_al_format(short channels, short samples)
{
	ALboolean stereo = (channels > 1);

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

static ALboolean is_big_endian() {
    int a = 1;
    return !((char*)&a)[0];
}

static int convert_to_int(char* buffer, int len) {
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


AL_API void AL_APIENTRY alutLoadWAVFile (ALbyte* filename, ALenum* format, ALvoid** data, ALsizei* size, ALsizei* freq) {
    char buffer[4];

    FILE* in = fopen((char*) filename, "rb");
    if(!in) {
        fprintf(stderr, "Couldn't open file\n");
        ALUT_ERROR = ALUT_ERROR_OPEN_DEVICE;
        return;
    }

    fread(buffer, 4, sizeof(char), in);

    if (strncmp(buffer, "RIFF", 4) != 0) {
        fprintf(stderr, "Not a valid wave file\n");
        ALUT_ERROR = ALUT_ERROR_CORRUPT_OR_TRUNCATED_DATA;
        fclose(in);
        return;
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
}


AL_API void AL_APIENTRY alutUnloadWAV (ALenum format, ALvoid *data, ALsizei size, ALsizei frequency) {
    free(data);
}

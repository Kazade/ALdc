#include "../include/AL/alut.h"


static ALenum ALUT_ERROR = ALUT_ERROR_NO_ERROR;

AL_API ALboolean AL_APIENTRY alutInit(int *argcp, char **argv) {

}

AL_API ALboolean AL_APIENTRY alutInitWithoutContext(int *argcp, char **argv) {

}

AL_API ALboolean AL_APIENTRY alutExit(void) {

}

AL_API ALenum AL_APIENTRY alutGetError() {
    ALenum result = ALUT_ERROR;
    ALUT_ERROR = ALUT_ERROR_NO_ERROR;
    return result;
}

AL_API const ALchar* AL_APIENTRY alutGetErrorString(ALenum error) {
    switch(error) {
        case ALUT_ERROR_NO_ERROR: "No Error";
        /* FIXME */
        default:
            return "Undescribed error";
    }
}

AL_API void AL_APIENTRY alutLoadWAVFile (ALbyte *filename,
                      ALenum *format,
                      void **data,
                      ALsizei *size,
                      ALsizei *frequency,
                      ALboolean *loop) {

}

AL_API void AL_APIENTRY alutUnloadWAV (ALenum format, ALvoid *data, ALsizei size, ALsizei frequency) {

}

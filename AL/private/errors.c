#include "stdio.h"
#include "errors.h"

static SDL_errorcode error = SDL_NO_ERROR;

/* This doesn't behave list SDL_Error, but more like GL errors */
int SDL_Error(SDL_errorcode code) {
    if(error != SDL_NO_ERROR) {
        error = code;
    }

    return -1;
}


/* Lazy hack */

int SDL_SetError(const char* msg, ...) {
    fprintf(stderr, msg);
    SDL_Error(SDL_UNKNOWN_ERROR);
    return -1;
}

typedef enum
{
    SDL_NO_ERROR,
    SDL_ENOMEM,
    SDL_EFREAD,
    SDL_EFWRITE,
    SDL_EFSEEK,
    SDL_UNSUPPORTED,
    SDL_LASTERROR,
    SDL_UNKNOWN_ERROR
} SDL_errorcode;

#define SDL_OutOfMemory()   SDL_Error(SDL_ENOMEM)
#define SDL_InvalidParamError(param)   SDL_SetError("Parameter '%s' is invalid", (param))

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

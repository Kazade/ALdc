#pragma once

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

int SDL_Error(SDL_errorcode code);
int SDL_SetError(const char* msg, ...);

#define SDL_OutOfMemory()   SDL_Error(SDL_ENOMEM)
#define SDL_InvalidParamError(param)   SDL_SetError("Parameter '%s' is invalid", (param))

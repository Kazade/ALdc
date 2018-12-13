#pragma once

/* Internal support funtions for MojoAL <> DC compatibility */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

typedef uint8_t Uint8;
typedef uint32_t Uint32;

typedef enum {
    SDL_FALSE = 0,
    SDL_TRUE  = 1
} SDL_bool;

#define SDL_memcpy(dst, src, len) memcpy(dst, src, len)
#define SDL_malloc	malloc
#define SDL_calloc	calloc
#define SDL_free	free
#define SDL_memset      memset
#define SDL_cosf cos
#define SDL_sinf sin

#define SDL_min(x, y)	(((x) < (y)) ? (x) : (y))
#define SDL_max(x, y)	(((x) > (y)) ? (x) : (y))
#define SDL_abs		abs

#define SDL_sqrtf sqrtf
#define SDL_powf powf
#define SDL_acosf acosf

#define SDL_strdup strdup
#define SDL_strlcpy strlcpy
#define SDL_strcasecmp strcasecmp
#define SDL_strlen strlen
#define SDL_assert assert
#define SDL_strcmp      strcmp
#define SDL_zero(x)   SDL_memset(&(x), 0, sizeof((x)))

typedef struct { int value; } SDL_atomic_t;
typedef int SDL_SpinLock;

extern void SDL_AtomicLock(SDL_SpinLock *lock);
extern void SDL_AtomicUnlock(SDL_SpinLock *lock);

#define SDL_CompilerBarrier()   \
    { SDL_SpinLock _tmp = 0; SDL_AtomicLock(&_tmp); SDL_AtomicUnlock(&_tmp); }

/* this is opaque to the outside world. */
struct _SDL_AudioStream;
typedef struct _SDL_AudioStream SDL_AudioStream;
typedef Uint32 SDL_AudioDeviceID;

extern SDL_bool SDL_AtomicCAS(SDL_atomic_t *a, int oldval, int newval);
extern SDL_bool SDL_AtomicCASPtr(void* *a, void *oldval, void *newval);

#ifndef SDL_AtomicAdd
inline int SDL_AtomicAdd(SDL_atomic_t *a, int v)
{
    int value;
    do {
        value = a->value;
    } while (!SDL_AtomicCAS(a, value, (value + v)));
    return value;
}
#endif

#ifndef SDL_AtomicGet
inline int SDL_AtomicGet(SDL_atomic_t *a)
{
    int value = a->value;
    SDL_CompilerBarrier();
    return value;
}
#endif

#ifndef SDL_AtomicDecRef
#define SDL_AtomicDecRef(a)    (SDL_AtomicAdd(a, -1) == 1)
#endif


#ifndef SDL_AtomicSetPtr
inline void* SDL_AtomicSetPtr(void* *a, void* v)
{
    void* value;
    do {
        value = *a;
    } while (!SDL_AtomicCASPtr(a, value, v));
    return value;
}
#endif

#ifndef SDL_AtomicGetPtr
inline void* SDL_AtomicGetPtr(void* *a)
{
    void* value = *a;
    SDL_CompilerBarrier();
    return value;
}
#endif

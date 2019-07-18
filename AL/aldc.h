#pragma once

/* Internal support funtions for MojoAL <> DC compatibility */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define SDL_LIL_ENDIAN 1234
#define SDL_BYTEORDER SDL_LIL_ENDIAN
#define SDL_MAX_SINT32 ((Sint32)0x7FFFFFFF)
#if (defined(__GNUC__) && (__GNUC__ <= 2)) || defined(__CC_ARM) || defined(__cplusplus)
#define SDL_VARIABLE_LENGTH_ARRAY 1
#else
#define SDL_VARIABLE_LENGTH_ARRAY
#endif

#define SDL_zerop(x) SDL_memset((x), 0, sizeof(*(x)))

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;

#define SDL_MAX_SINT32 ((Sint32)0x7FFFFFFF)

typedef enum {
    SDL_FALSE = 0,
    SDL_TRUE  = 1
} SDL_bool;

extern SDL_bool SDL_HasSSE();

#define SDLCALL
#define DECLSPEC __attribute__ ((visibility("default")))

#define SDL_memcpy(dst, src, len) memcpy(dst, src, len)
#define SDL_malloc	malloc
#define SDL_calloc	calloc
#define SDL_realloc realloc
#define SDL_free	free
#define SDL_memset      memset
#define SDL_memmove memmove
#define SDL_cosf cos
#define SDL_sinf sin
#define SDL_sqrt sqrt
#define SDL_pow pow
#define SDL_ceil ceil

#define SDL_min(x, y)	(((x) < (y)) ? (x) : (y))
#define SDL_max(x, y)	(((x) > (y)) ? (x) : (y))
#define SDL_abs		abs

#define SDL_sqrtf sqrtf
#define SDL_powf powf
#define SDL_acosf acosf

#define SDL_strdup strdup
#define SDL_strcasecmp strcasecmp
#define SDL_strlen strlen
#define SDL_assert assert
#define SDL_strcmp      strcmp
#define SDL_zero(x)   SDL_memset(&(x), 0, sizeof((x)))

static inline size_t SDL_strlcpy(char *dst, const char *src, size_t maxlen) {
    size_t srclen = SDL_strlen(src);
    if (maxlen > 0) {
        size_t len = SDL_min(srclen, maxlen - 1);
        SDL_memcpy(dst, src, len);
        dst[len] = '\0';
    }

    return srclen;
}

#define SDL_arraysize(array)   (sizeof(array)/sizeof(array[0]))

#define SDL_static_cast(type, expression) ((type)(expression))
#define SDL_FORCE_INLINE __attribute__((always_inline)) static __inline__

// --------------- ATOMIC -------------------

typedef struct { int value; } SDL_atomic_t;
typedef int SDL_SpinLock;

extern void SDL_AtomicLock(SDL_SpinLock *lock);
extern void SDL_AtomicUnlock(SDL_SpinLock *lock);

#define SDL_CompilerBarrier()   \
    { SDL_SpinLock _tmp = 0; SDL_AtomicLock(&_tmp); SDL_AtomicUnlock(&_tmp); }

extern SDL_bool SDL_AtomicCAS(SDL_atomic_t *a, int oldval, int newval);
extern SDL_bool SDL_AtomicCASPtr(void* *a, void *oldval, void *newval);

#define SDL_MemoryBarrierRelease() __asm__ __volatile__ ("" : : : "memory")
#define SDL_MemoryBarrierAcquire() __asm__ __volatile__ ("" : : : "memory")

extern DECLSPEC int SDLCALL SDL_AtomicGet(SDL_atomic_t *a);
extern DECLSPEC int SDLCALL SDL_AtomicSet(SDL_atomic_t *a, int v);
extern DECLSPEC int SDLCALL SDL_AtomicAdd(SDL_atomic_t *a, int v);
extern DECLSPEC void* SDLCALL SDL_AtomicSetPtr(void **a, void* v);
extern DECLSPEC void* SDLCALL SDL_AtomicGetPtr(void **a);

#ifndef SDL_AtomicIncRef
#define SDL_AtomicIncRef(a)    SDL_AtomicAdd(a, 1)
#endif

#ifndef SDL_AtomicDecRef
#define SDL_AtomicDecRef(a)    (SDL_AtomicAdd(a, -1) == 1)
#endif




// --------------- AUDIO -------------------

#define SDL_AUDIO_MASK_BITSIZE       (0xFF)
#define SDL_AUDIO_MASK_DATATYPE      (1<<8)
#define SDL_AUDIO_MASK_ENDIAN        (1<<12)
#define SDL_AUDIO_MASK_SIGNED        (1<<15)
#define SDL_AUDIO_BITSIZE(x)         (x & SDL_AUDIO_MASK_BITSIZE)
#define SDL_AUDIO_ISFLOAT(x)         (x & SDL_AUDIO_MASK_DATATYPE)
#define SDL_AUDIO_ISBIGENDIAN(x)     (x & SDL_AUDIO_MASK_ENDIAN)
#define SDL_AUDIO_ISSIGNED(x)        (x & SDL_AUDIO_MASK_SIGNED)
#define SDL_AUDIO_ISINT(x)           (!SDL_AUDIO_ISFLOAT(x))
#define SDL_AUDIO_ISLITTLEENDIAN(x)  (!SDL_AUDIO_ISBIGENDIAN(x))
#define SDL_AUDIO_ISUNSIGNED(x) (!SDL_AUDIO_ISSIGNED(x))

#define SDL_INIT_AUDIO   0x00000010

#define AUDIO_U8        0x0008  /**< Unsigned 8-bit samples */
#define AUDIO_S8        0x8008  /**< Signed 8-bit samples */
#define AUDIO_U16LSB    0x0010  /**< Unsigned 16-bit samples */
#define AUDIO_S16LSB    0x8010  /**< Signed 16-bit samples */
#define AUDIO_U16MSB    0x1010  /**< As above, but big-endian byte order */
#define AUDIO_S16MSB    0x9010  /**< As above, but big-endian byte order */
#define AUDIO_U16       AUDIO_U16LSB
#define AUDIO_S16 AUDIO_S16LSB

#define AUDIO_S32LSB    0x8020  /**< 32-bit integer samples */
#define AUDIO_S32MSB    0x9020  /**< As above, but big-endian byte order */
#define AUDIO_S32 AUDIO_S32LSB

#define AUDIO_F32LSB    0x8120  /**< 32-bit floating point samples */
#define AUDIO_F32MSB    0x9120  /**< As above, but big-endian byte order */
#define AUDIO_F32 AUDIO_F32LSB

/* Little endian */
#define AUDIO_U16SYS    AUDIO_U16LSB
#define AUDIO_S16SYS    AUDIO_S16LSB
#define AUDIO_S32SYS    AUDIO_S32LSB
#define AUDIO_F32SYS AUDIO_F32LSB

/* this is opaque to the outside world. */
struct _SDL_AudioStream;
typedef struct _SDL_AudioStream SDL_AudioStream;
typedef Uint32 SDL_AudioDeviceID;
typedef Uint16 SDL_AudioFormat;


typedef struct{
    int freq;
    Uint16 format;
    Uint8 channels;
    Uint8 silence;
    Uint16 samples;
    Uint32 size;
    void (*callback)(void *userdata, Uint8 *stream, int len);
    void *userdata;
} SDL_AudioSpec;

typedef enum
{
    SDL_AUDIO_STOPPED = 0,
    SDL_AUDIO_PLAYING,
    SDL_AUDIO_PAUSED
} SDL_AudioStatus;

struct SDL_AudioCVT;
typedef void (SDLCALL * SDL_AudioFilter) (struct SDL_AudioCVT * cvt, SDL_AudioFormat format);

#define SDL_AUDIOCVT_MAX_FILTERS 9

typedef struct SDL_AudioCVT
{
    int needed;                 /**< Set to 1 if conversion possible */
    SDL_AudioFormat src_format; /**< Source audio format */
    SDL_AudioFormat dst_format; /**< Target audio format */
    double rate_incr;           /**< Rate conversion increment */
    Uint8 *buf;                 /**< Buffer to hold entire audio data */
    int len;                    /**< Length of original audio buffer */
    int len_cvt;                /**< Length of converted audio buffer */
    int len_mult;               /**< buffer must be len*len_mult big */
    double len_ratio;           /**< Given len, final size is len*len_ratio */
    SDL_AudioFilter filters[SDL_AUDIOCVT_MAX_FILTERS + 1]; /**< NULL-terminated list of filter functions */
    int filter_index;           /**< Current audio conversion function */
} SDL_AudioCVT;

extern int SDL_InitSubSystem(Uint32 flags);
extern void SDL_QuitSubSystem(Uint32 flags);

extern SDL_AudioDeviceID SDL_OpenAudioDevice(
    const char*          device,
    int                  iscapture,
    const SDL_AudioSpec* desired,
    SDL_AudioSpec*       obtained,
    int                  allowed_changes
);

extern DECLSPEC void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID dev);

extern DECLSPEC SDL_AudioStream * SDLCALL SDL_NewAudioStream(
    const SDL_AudioFormat src_format,
    const Uint8 src_channels,
    const int src_rate,
    const SDL_AudioFormat dst_format,
    const Uint8 dst_channels,
    const int dst_rate
);

extern DECLSPEC int SDLCALL SDL_AudioStreamPut(SDL_AudioStream *stream, const void *buf, int len);
extern DECLSPEC int SDLCALL SDL_AudioStreamGet(SDL_AudioStream *stream, void *buf, int len);
extern DECLSPEC int SDLCALL SDL_AudioStreamAvailable(SDL_AudioStream *stream);
extern DECLSPEC int SDLCALL SDL_AudioStreamFlush(SDL_AudioStream *stream);
extern DECLSPEC SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID dev);
extern DECLSPEC void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on);
extern DECLSPEC void SDLCALL SDL_LockAudioDevice(SDL_AudioDeviceID dev);
extern DECLSPEC void SDLCALL SDL_UnlockAudioDevice(SDL_AudioDeviceID dev);
extern DECLSPEC void SDLCALL SDL_FreeAudioStream(SDL_AudioStream *stream);
extern DECLSPEC int SDLCALL SDL_GetNumAudioDevices(int iscapture);
extern DECLSPEC const char *SDLCALL SDL_GetAudioDeviceName(int index, int iscapture);
extern DECLSPEC int SDLCALL SDL_BuildAudioCVT(
    SDL_AudioCVT * cvt,
    SDL_AudioFormat src_format,
    Uint8 src_channels,
    int src_rate,
    SDL_AudioFormat dst_format,
    Uint8 dst_channels,
    int dst_rate
);
extern DECLSPEC int SDLCALL SDL_ConvertAudio(SDL_AudioCVT * cvt);




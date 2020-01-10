

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

#ifdef __STRICT_ANSI__
char *strdup(const char *);
#endif

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

#define SDL_Delay thd_sleep

int SDL_InitSubSystem(Uint32 flags) {
    return 0;
}

void SDL_QuitSubSystem(Uint32 flags) {

}

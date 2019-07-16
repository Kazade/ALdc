/*
 * This file is essentially a copy-paste of the minimum code from SDL required to power MojoAL, with the exception
 * of the audio device code which is Dreamcast specific.
 *
 * It's quick and dirty, but it's also the fastest way to get some kind of OpenAL implementation on the DC without
 * doing it from scratch
 */

#include <dc/spu.h>

#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#include <kos/mutex.h>
#include "aldc.h"

#include "./private/data_queue.h"
#include "./private/errors.h"
#include "./private/endian.h"
#include "./private/converters.h"

// FIXME: How to implement this on SH4?
#define PAUSE_INSTRUCTION()

#define DREAMCAST_AUDIO_DEVICE_NAME "Yamaha AICA Stereo Device"

#define SDL_Delay thd_sleep

static SDL_SpinLock locks[32];

static inline void enterLock(void *a) {
    uintptr_t index = ((((uintptr_t)a) >> 3) & 0x1f);
    SDL_AtomicLock(&locks[index]);
}

static inline void leaveLock(void *a) {
    uintptr_t index = ((((uintptr_t)a) >> 3) & 0x1f);
    SDL_AtomicUnlock(&locks[index]);
}

SDL_bool SDL_AtomicTryLock(SDL_SpinLock *lock)
{
    /* Terrible terrible damage */
    static mutex_t *_spinlock_mutex;

    if (!_spinlock_mutex) {
        _spinlock_mutex = (mutex_t*) malloc(sizeof(mutex_t));
        /* Race condition on first lock... */
        mutex_init(_spinlock_mutex, MUTEX_TYPE_NORMAL);
    }

    mutex_lock(_spinlock_mutex);

    if (*lock == 0) {
        *lock = 1;
        mutex_unlock(_spinlock_mutex);
        return SDL_TRUE;
    } else {
        mutex_unlock(_spinlock_mutex);
        return SDL_FALSE;
    }
}

void SDL_AtomicLock(SDL_SpinLock *lock) {
    int iterations = 0;
    /* FIXME: Should we have an eventual timeout? */
    while (!SDL_AtomicTryLock(lock)) {
        if (iterations < 32) {
            iterations++;
            PAUSE_INSTRUCTION();
        } else {
            /* !!! FIXME: this doesn't definitely give up the current timeslice, it does different things on various platforms. */
            SDL_Delay(0);
        }
    }
}

void SDL_AtomicUnlock(SDL_SpinLock *lock) {
    *lock = 0;
}

SDL_bool SDL_AtomicCAS(SDL_atomic_t *a, int oldval, int newval) {
    SDL_bool retval = SDL_FALSE;

    enterLock(a);
    if (a->value == oldval) {
        a->value = newval;
        retval = SDL_TRUE;
    }
    leaveLock(a);

    return retval;
}

SDL_bool SDL_AtomicCASPtr(void* *a, void *oldval, void *newval) {
    SDL_bool retval = SDL_FALSE;

    enterLock(a);
    if (*a == oldval) {
        *a = newval;
        retval = SDL_TRUE;
    }
    leaveLock(a);

    return retval;
}

int SDL_AtomicSet(SDL_atomic_t *a, int v) {
    int value;
    do {
        value = a->value;
    } while (!SDL_AtomicCAS(a, value, v));
    return value;
}

int SDL_AtomicAdd(SDL_atomic_t *a, int v)
{
    int value;
    do {
        value = a->value;
    } while (!SDL_AtomicCAS(a, value, (value + v)));
    return value;
}

void* SDL_AtomicSetPtr(void* *a, void* v)
{
    void* value;
    do {
        value = *a;
    } while (!SDL_AtomicCASPtr(a, value, v));
    return value;
}

void* SDL_AtomicGetPtr(void* *a)
{
    void* value = *a;
    SDL_CompilerBarrier();
    return value;
}

int SDL_AtomicGet(SDL_atomic_t *a)
{
    int value = a->value;
    SDL_CompilerBarrier();
    return value;
}

int SDL_InitSubSystem(Uint32 flags) {
    return 0;
}

void SDL_QuitSubSystem(Uint32 flags) {

}

// ----------------- AUDIO --------------------

/* SDL's resampler uses a "bandlimited interpolation" algorithm:
     https://ccrma.stanford.edu/~jos/resample/ */

#define RESAMPLER_ZERO_CROSSINGS 5
#define RESAMPLER_BITS_PER_SAMPLE 16
#define RESAMPLER_SAMPLES_PER_ZERO_CROSSING  (1 << ((RESAMPLER_BITS_PER_SAMPLE / 2) + 1))
#define RESAMPLER_FILTER_SIZE ((RESAMPLER_SAMPLES_PER_ZERO_CROSSING * RESAMPLER_ZERO_CROSSINGS) + 1)

static int ResamplerPadding(const int inrate, const int outrate) {
    if (inrate == outrate) {
        return 0;
    } else if (inrate > outrate) {
        return (int) SDL_ceil(((float) (RESAMPLER_SAMPLES_PER_ZERO_CROSSING * inrate) / ((float) outrate)));
    }
    return RESAMPLER_SAMPLES_PER_ZERO_CROSSING;
}

typedef int (*SDL_ResampleAudioStreamFunc)(SDL_AudioStream *stream, const void *inbuf, const int inbuflen, void *outbuf, const int outbuflen);
typedef void (*SDL_ResetAudioStreamResamplerFunc)(SDL_AudioStream *stream);
typedef void (*SDL_CleanupAudioStreamResamplerFunc)(SDL_AudioStream *stream);


struct _SDL_AudioStream
{
    SDL_AudioCVT cvt_before_resampling;
    SDL_AudioCVT cvt_after_resampling;
    SDL_DataQueue *queue;
    SDL_bool first_run;
    Uint8 *staging_buffer;
    int staging_buffer_size;
    int staging_buffer_filled;
    Uint8 *work_buffer_base;  /* maybe unaligned pointer from SDL_realloc(). */
    int work_buffer_len;
    int src_sample_frame_size;
    SDL_AudioFormat src_format;
    Uint8 src_channels;
    int src_rate;
    int dst_sample_frame_size;
    SDL_AudioFormat dst_format;
    Uint8 dst_channels;
    int dst_rate;
    double rate_incr;
    Uint8 pre_resample_channels;
    int packetlen;
    int resampler_padding_samples;
    float *resampler_padding;
    void *resampler_state;
    SDL_ResampleAudioStreamFunc resampler_func;
    SDL_ResetAudioStreamResamplerFunc reset_resampler_func;
    SDL_CleanupAudioStreamResamplerFunc cleanup_resampler_func;
};

/* This is a "modified" bessel function, so you can't use POSIX j0() */
static double bessel(const double x)
{
    const double xdiv2 = x / 2.0;
    double i0 = 1.0f;
    double f = 1.0f;
    int i = 1;

    while (SDL_TRUE) {
        const double diff = SDL_pow(xdiv2, i * 2) / SDL_pow(f, 2);
        if (diff < 1.0e-21f) {
            break;
        }
        i0 += diff;
        i++;
        f *= (double) i;
    }

    return i0;
}

/* build kaiser table with cardinal sine applied to it, and array of differences between elements. */
static void kaiser_and_sinc(float *table, float *diffs, const int tablelen, const double beta)
{
    const int lenm1 = tablelen - 1;
    const int lenm1div2 = lenm1 / 2;
    int i;

    table[0] = 1.0f;
    for (i = 1; i < tablelen; i++) {
        const double kaiser = bessel(beta * SDL_sqrt(1.0 - SDL_pow(((i - lenm1) / 2.0) / lenm1div2, 2.0))) / bessel(beta);
        table[tablelen - i] = (float) kaiser;
    }

    for (i = 1; i < tablelen; i++) {
        const float x = (((float) i) / ((float) RESAMPLER_SAMPLES_PER_ZERO_CROSSING)) * ((float) M_PI);
        table[i] *= SDL_sinf(x) / x;
        diffs[i - 1] = table[i] - table[i - 1];
    }
    diffs[lenm1] = 0.0f;
}

static SDL_SpinLock ResampleFilterSpinlock = 0;
static float *ResamplerFilter = NULL;
static float *ResamplerFilterDifference = NULL;

int SDL_PrepareResampleFilter(void) {
    SDL_AtomicLock(&ResampleFilterSpinlock);
    if (!ResamplerFilter) {
        /* if dB > 50, beta=(0.1102 * (dB - 8.7)), according to Matlab. */
        const double dB = 80.0;
        const double beta = 0.1102 * (dB - 8.7);
        const size_t alloclen = RESAMPLER_FILTER_SIZE * sizeof (float);

        ResamplerFilter = (float *) SDL_malloc(alloclen);
        if (!ResamplerFilter) {
            SDL_AtomicUnlock(&ResampleFilterSpinlock);
            return SDL_OutOfMemory();
        }

        ResamplerFilterDifference = (float *) SDL_malloc(alloclen);
        if (!ResamplerFilterDifference) {
            SDL_free(ResamplerFilter);
            ResamplerFilter = NULL;
            SDL_AtomicUnlock(&ResampleFilterSpinlock);
            return SDL_OutOfMemory();
        }
        kaiser_and_sinc(ResamplerFilter, ResamplerFilterDifference, RESAMPLER_FILTER_SIZE, beta);
    }
    SDL_AtomicUnlock(&ResampleFilterSpinlock);
    return 0;
}

static void SDL_ResetAudioStreamResampler(SDL_AudioStream *stream)
{
    /* set all the padding to silence. */
    const int len = stream->resampler_padding_samples;
    SDL_memset(stream->resampler_state, '\0', len * sizeof (float));
}

static void SDL_CleanupAudioStreamResampler(SDL_AudioStream *stream)
{
    SDL_free(stream->resampler_state);
}

/* lpadding and rpadding are expected to be buffers of (ResamplePadding(inrate, outrate) * chans * sizeof (float)) bytes. */
static int
SDL_ResampleAudio(const int chans, const int inrate, const int outrate,
                        const float *lpadding, const float *rpadding,
                        const float *inbuf, const int inbuflen,
                        float *outbuf, const int outbuflen)
{
    const double finrate = (double) inrate;
    const double outtimeincr = 1.0 / ((float) outrate);
    const double  ratio = ((float) outrate) / ((float) inrate);
    const int paddinglen = ResamplerPadding(inrate, outrate);
    const int framelen = chans * (int)sizeof (float);
    const int inframes = inbuflen / framelen;
    const int wantedoutframes = (int) ((inbuflen / framelen) * ratio);  /* outbuflen isn't total to write, it's total available. */
    const int maxoutframes = outbuflen / framelen;
    const int outframes = SDL_min(wantedoutframes, maxoutframes);
    float *dst = outbuf;
    double outtime = 0.0;
    int i, j, chan;

    for (i = 0; i < outframes; i++) {
        const int srcindex = (int) (outtime * inrate);
        const double intime = ((double) srcindex) / finrate;
        const double innexttime = ((double) (srcindex + 1)) / finrate;
        const double interpolation1 = 1.0 - ((innexttime - outtime) / (innexttime - intime));
        const int filterindex1 = (int) (interpolation1 * RESAMPLER_SAMPLES_PER_ZERO_CROSSING);
        const double interpolation2 = 1.0 - interpolation1;
        const int filterindex2 = (int) (interpolation2 * RESAMPLER_SAMPLES_PER_ZERO_CROSSING);

        for (chan = 0; chan < chans; chan++) {
            float outsample = 0.0f;

            /* do this twice to calculate the sample, once for the "left wing" and then same for the right. */
            /* !!! FIXME: do both wings in one loop */
            for (j = 0; (filterindex1 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)) < RESAMPLER_FILTER_SIZE; j++) {
                const int srcframe = srcindex - j;
                /* !!! FIXME: we can bubble this conditional out of here by doing a pre loop. */
                const float insample = (srcframe < 0) ? lpadding[((paddinglen + srcframe) * chans) + chan] : inbuf[(srcframe * chans) + chan];
                outsample += (float)(insample * (ResamplerFilter[filterindex1 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)] + (interpolation1 * ResamplerFilterDifference[filterindex1 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)])));
            }

            for (j = 0; (filterindex2 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)) < RESAMPLER_FILTER_SIZE; j++) {
                const int srcframe = srcindex + 1 + j;
                /* !!! FIXME: we can bubble this conditional out of here by doing a post loop. */
                const float insample = (srcframe >= inframes) ? rpadding[((srcframe - inframes) * chans) + chan] : inbuf[(srcframe * chans) + chan];
                outsample += (float)(insample * (ResamplerFilter[filterindex2 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)] + (interpolation2 * ResamplerFilterDifference[filterindex2 + (j * RESAMPLER_SAMPLES_PER_ZERO_CROSSING)])));
            }
            *(dst++) = outsample;
        }

        outtime += outtimeincr;
    }

    return outframes * chans * sizeof (float);
}

static int SDL_ResampleAudioStream(SDL_AudioStream *stream, const void *_inbuf, const int inbuflen, void *_outbuf, const int outbuflen)
{
    const Uint8 *inbufend = ((const Uint8 *) _inbuf) + inbuflen;
    const float *inbuf = (const float *) _inbuf;
    float *outbuf = (float *) _outbuf;
    const int chans = (int) stream->pre_resample_channels;
    const int inrate = stream->src_rate;
    const int outrate = stream->dst_rate;
    const int paddingsamples = stream->resampler_padding_samples;
    const int paddingbytes = paddingsamples * sizeof (float);
    float *lpadding = (float *) stream->resampler_state;
    const float *rpadding = (const float *) inbufend; /* we set this up so there are valid padding samples at the end of the input buffer. */
    const int cpy = SDL_min(inbuflen, paddingbytes);
    int retval;

    SDL_assert(inbuf != ((const float *) outbuf));  /* SDL_AudioStreamPut() shouldn't allow in-place resamples. */

    retval = SDL_ResampleAudio(chans, inrate, outrate, lpadding, rpadding, inbuf, inbuflen, outbuf, outbuflen);

    /* update our left padding with end of current input, for next run. */
    SDL_memcpy((lpadding + paddingsamples) - (cpy / sizeof (float)), inbufend - cpy, cpy);
    return retval;
}

static uint8_t DEVICE_INITIALIZED = 0;

SDL_AudioDeviceID SDL_OpenAudioDevice(
    const char*          device,
    int                  iscapture,
    const SDL_AudioSpec* desired,
    SDL_AudioSpec*       obtained,
    int                  allowed_changes
        ) {

    if(device && strcmp(device, DREAMCAST_AUDIO_DEVICE_NAME) != 0) {
        SDL_SetError("Couldn't find matching audio device");
        return 0;
    }

    if(iscapture) {
        SDL_SetError("No capture devices supported");
        return 0;
    }

    if(DEVICE_INITIALIZED) {
        return 0;
    }

    switch(desired->format) {
        case AUDIO_S8:
        case AUDIO_U8:
            obtained->format = AUDIO_S8;
        case AUDIO_S16:
        case AUDIO_U16:
            obtained->format = AUDIO_U16LSB;
        default:
            SDL_SetError("Unsupported audio format");
            return 0;
    }

    //FIXME: Fill out the obtained details, do all the right checks here

    obtained->channels = 2; // Always 2 channels for now

    spu_init();

    DEVICE_INITIALIZED = 1;

    return 1;
}

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev) {
    if(dev != 1) {
        return;
    }

    // SDL_DC_aica_stop(0);
    DEVICE_INITIALIZED = 0;
}

SDL_AudioStream * SDL_NewAudioStream(
    const SDL_AudioFormat src_format,
    const Uint8 src_channels,
    const int src_rate,
    const SDL_AudioFormat dst_format,
    const Uint8 dst_channels,
    const int dst_rate
        ) {

    const int packetlen = 4096;  /* !!! FIXME: good enough for now. */
    Uint8 pre_resample_channels;
    SDL_AudioStream *retval;

    retval = (SDL_AudioStream *) SDL_calloc(1, sizeof (SDL_AudioStream));
    if (!retval) {
        return NULL;
    }

    /* If increasing channels, do it after resampling, since we'd just
       do more work to resample duplicate channels. If we're decreasing, do
       it first so we resample the interpolated data instead of interpolating
       the resampled data (!!! FIXME: decide if that works in practice, though!). */
    pre_resample_channels = SDL_min(src_channels, dst_channels);

    retval->first_run = SDL_TRUE;
    retval->src_sample_frame_size = (SDL_AUDIO_BITSIZE(src_format) / 8) * src_channels;
    retval->src_format = src_format;
    retval->src_channels = src_channels;
    retval->src_rate = src_rate;
    retval->dst_sample_frame_size = (SDL_AUDIO_BITSIZE(dst_format) / 8) * dst_channels;
    retval->dst_format = dst_format;
    retval->dst_channels = dst_channels;
    retval->dst_rate = dst_rate;
    retval->pre_resample_channels = pre_resample_channels;
    retval->packetlen = packetlen;
    retval->rate_incr = ((double) dst_rate) / ((double) src_rate);
    retval->resampler_padding_samples = ResamplerPadding(retval->src_rate, retval->dst_rate) * pre_resample_channels;
    retval->resampler_padding = (float *) SDL_calloc(retval->resampler_padding_samples ? retval->resampler_padding_samples : 1, sizeof (float));

    if (retval->resampler_padding == NULL) {
        SDL_FreeAudioStream(retval);
        SDL_OutOfMemory();
        return NULL;
    }

    retval->staging_buffer_size = ((retval->resampler_padding_samples / retval->pre_resample_channels) * retval->src_sample_frame_size);
    if (retval->staging_buffer_size > 0) {
        retval->staging_buffer = (Uint8 *) SDL_malloc(retval->staging_buffer_size);
        if (retval->staging_buffer == NULL) {
            SDL_FreeAudioStream(retval);
            SDL_OutOfMemory();
            return NULL;
        }
    }

    /* Not resampling? It's an easy conversion (and maybe not even that!) */
    if (src_rate == dst_rate) {
        retval->cvt_before_resampling.needed = SDL_FALSE;
        if (SDL_BuildAudioCVT(&retval->cvt_after_resampling, src_format, src_channels, dst_rate, dst_format, dst_channels, dst_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL;  /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }
    } else {
        /* Don't resample at first. Just get us to Float32 format. */
        /* !!! FIXME: convert to int32 on devices without hardware float. */
        if (SDL_BuildAudioCVT(&retval->cvt_before_resampling, src_format, src_channels, src_rate, AUDIO_F32SYS, pre_resample_channels, src_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL;  /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }

        if (!retval->resampler_func) {
            retval->resampler_state = SDL_calloc(retval->resampler_padding_samples, sizeof (float));
            if (!retval->resampler_state) {
                SDL_FreeAudioStream(retval);
                SDL_OutOfMemory();
                return NULL;
            }

            if (SDL_PrepareResampleFilter() < 0) {
                SDL_free(retval->resampler_state);
                retval->resampler_state = NULL;
                SDL_FreeAudioStream(retval);
                return NULL;
            }

            retval->resampler_func = SDL_ResampleAudioStream;
            retval->reset_resampler_func = SDL_ResetAudioStreamResampler;
            retval->cleanup_resampler_func = SDL_CleanupAudioStreamResampler;
        }

        /* Convert us to the final format after resampling. */
        if (SDL_BuildAudioCVT(&retval->cvt_after_resampling, AUDIO_F32SYS, pre_resample_channels, dst_rate, dst_format, dst_channels, dst_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL;  /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }
    }

    retval->queue = SDL_NewDataQueue(packetlen, packetlen * 2);
    if (!retval->queue) {
        SDL_FreeAudioStream(retval);
        return NULL;  /* SDL_NewDataQueue should have called SDL_SetError. */
    }

    return retval;
}

int SDL_AudioStreamPut(SDL_AudioStream *stream, const void *buf, int len) {

}

int SDL_AudioStreamGet(SDL_AudioStream *stream, void *buf, int len) {

}

int SDL_AudioStreamAvailable(SDL_AudioStream *stream) {

}

SDL_AudioStatus SDL_GetAudioDeviceStatus(SDL_AudioDeviceID dev) {

}

void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {

}

void SDL_LockAudioDevice(SDL_AudioDeviceID dev) {

}

void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) {

}

void SDL_FreeAudioStream(SDL_AudioStream *stream) {

}

int SDL_GetNumAudioDevices(int iscapture) {
    if(iscapture) {
        return 0;
    } else {
        return 1;
    }
}

const char *SDL_GetAudioDeviceName(int index, int iscapture) {
    if(index == 0 && !iscapture) {
        return DREAMCAST_AUDIO_DEVICE_NAME;
    } else {
        return NULL;
    }
}

static SDL_bool SDL_SupportedAudioFormat(const SDL_AudioFormat fmt) {
    switch (fmt) {
        case AUDIO_U8:
        case AUDIO_S8:
        case AUDIO_U16LSB:
        case AUDIO_S16LSB:
        case AUDIO_U16MSB:
        case AUDIO_S16MSB:
        case AUDIO_S32LSB:
        case AUDIO_S32MSB:
        case AUDIO_F32LSB:
        case AUDIO_F32MSB:
            return SDL_TRUE;  /* supported. */

        default:
            break;
    }

    return SDL_FALSE;  /* unsupported. */
}

static SDL_bool SDL_SupportedChannelCount(const int channels) {
    switch (channels) {
        case 1:  /* mono */
        case 2:  /* stereo */
        case 4:  /* quad */
        case 6:  /* 5.1 */
        case 8:  /* 7.1 */
          return SDL_TRUE;  /* supported. */

        default:
            break;
    }

    return SDL_FALSE;  /* unsupported. */
}

static int SDL_AddAudioCVTFilter(SDL_AudioCVT *cvt, const SDL_AudioFilter filter) {
    if (cvt->filter_index >= SDL_AUDIOCVT_MAX_FILTERS) {
        return SDL_SetError("Too many filters needed for conversion, exceeded maximum of %d", SDL_AUDIOCVT_MAX_FILTERS);
    }
    if (filter == NULL) {
        return SDL_SetError("Audio filter pointer is NULL");
    }
    cvt->filters[cvt->filter_index++] = filter;
    cvt->filters[cvt->filter_index] = NULL; /* Moving terminator */
    return 0;
}

static void SDLCALL SDL_Convert_Byteswap(SDL_AudioCVT *cvt, SDL_AudioFormat format) {
    switch (SDL_AUDIO_BITSIZE(format)) {
        #define CASESWAP(b) \
            case b: { \
                Uint##b *ptr = (Uint##b *) cvt->buf; \
                int i; \
                for (i = cvt->len_cvt / sizeof (*ptr); i; --i, ++ptr) { \
                    *ptr = SDL_Swap##b(*ptr); \
                } \
                break; \
            }

        CASESWAP(16);
        CASESWAP(32);
        CASESWAP(64);

        #undef CASESWAP

        default: SDL_assert(!"unhandled byteswap datatype!"); break;
    }

    if (cvt->filters[++cvt->filter_index]) {
        /* flip endian flag for data. */
        if (format & SDL_AUDIO_MASK_ENDIAN) {
            format &= ~SDL_AUDIO_MASK_ENDIAN;
        } else {
            format |= SDL_AUDIO_MASK_ENDIAN;
        }
        cvt->filters[cvt->filter_index](cvt, format);
    }
}

static int SDL_BuildAudioTypeCVTToFloat(SDL_AudioCVT *cvt, const SDL_AudioFormat src_fmt) {
    int retval = 0;  /* 0 == no conversion necessary. */

    if ((SDL_AUDIO_ISBIGENDIAN(src_fmt) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN)) {
        if (SDL_AddAudioCVTFilter(cvt, SDL_Convert_Byteswap) < 0) {
            return -1;
        }
        retval = 1;  /* added a converter. */
    }

    if (!SDL_AUDIO_ISFLOAT(src_fmt)) {
        const Uint16 src_bitsize = SDL_AUDIO_BITSIZE(src_fmt);
        const Uint16 dst_bitsize = 32;
        SDL_AudioFilter filter = NULL;

        switch (src_fmt & ~SDL_AUDIO_MASK_ENDIAN) {
            case AUDIO_S8: filter = SDL_Convert_S8_to_F32; break;
            case AUDIO_U8: filter = SDL_Convert_U8_to_F32; break;
            case AUDIO_S16: filter = SDL_Convert_S16_to_F32; break;
            case AUDIO_U16: filter = SDL_Convert_U16_to_F32; break;
            case AUDIO_S32: filter = SDL_Convert_S32_to_F32; break;
            default: SDL_assert(!"Unexpected audio format!"); break;
        }

        if (!filter) {
            return SDL_SetError("No conversion from source format to float available");
        }

        if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
            return -1;
        }
        if (src_bitsize < dst_bitsize) {
            const int mult = (dst_bitsize / src_bitsize);
            cvt->len_mult *= mult;
            cvt->len_ratio *= mult;
        } else if (src_bitsize > dst_bitsize) {
            cvt->len_ratio /= (src_bitsize / dst_bitsize);
        }

        retval = 1;  /* added a converter. */
    }

    return retval;
}

/* Convert from stereo to mono. Average left and right. */
static void SDLCALL
SDL_ConvertStereoToMono(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / 8; i; --i, src += 2) {
        *(dst++) = (src[0] + src[1]) * 0.5f;
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Convert from 5.1 to stereo. Average left and right, distribute center, discard LFE. */
static void SDLCALL
SDL_Convert51ToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    SDL_assert(format == AUDIO_F32SYS);

    /* SDL's 5.1 layout: FL+FR+FC+LFE+BL+BR */
    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6, dst += 2) {
        const float front_center_distributed = src[2] * 0.5f;
        dst[0] = (src[0] + front_center_distributed + src[4]) / 2.5f;  /* left */
        dst[1] = (src[1] + front_center_distributed + src[5]) / 2.5f;  /* right */
    }

    cvt->len_cvt /= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Convert from quad to stereo. Average left and right. */
static void SDLCALL
SDL_ConvertQuadToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 4); i; --i, src += 4, dst += 2) {
        dst[0] = (src[0] + src[2]) * 0.5f; /* left */
        dst[1] = (src[1] + src[3]) * 0.5f; /* right */
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Convert from 7.1 to 5.1. Distribute sides across front and back. */
static void SDLCALL
SDL_Convert71To51(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof (float) * 8); i; --i, src += 8, dst += 6) {
        const float surround_left_distributed = src[6] * 0.5f;
        const float surround_right_distributed = src[7] * 0.5f;
        dst[0] = (src[0] + surround_left_distributed) / 1.5f;  /* FL */
        dst[1] = (src[1] + surround_right_distributed) / 1.5f;  /* FR */
        dst[2] = src[2] / 1.5f; /* CC */
        dst[3] = src[3] / 1.5f; /* LFE */
        dst[4] = (src[4] + surround_left_distributed) / 1.5f;  /* BL */
        dst[5] = (src[5] + surround_right_distributed) / 1.5f;  /* BR */
    }

    cvt->len_cvt /= 8;
    cvt->len_cvt *= 6;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Convert from 5.1 to quad. Distribute center across front, discard LFE. */
static void SDLCALL
SDL_Convert51ToQuad(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float *dst = (float *) cvt->buf;
    const float *src = dst;
    int i;

    SDL_assert(format == AUDIO_F32SYS);

    /* SDL's 4.0 layout: FL+FR+BL+BR */
    /* SDL's 5.1 layout: FL+FR+FC+LFE+BL+BR */
    for (i = cvt->len_cvt / (sizeof (float) * 6); i; --i, src += 6, dst += 4) {
        const float front_center_distributed = src[2] * 0.5f;
        dst[0] = (src[0] + front_center_distributed) / 1.5f;  /* FL */
        dst[1] = (src[1] + front_center_distributed) / 1.5f;  /* FR */
        dst[2] = src[4] / 1.5f;  /* BL */
        dst[3] = src[5] / 1.5f;  /* BR */
    }

    cvt->len_cvt /= 6;
    cvt->len_cvt *= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Upmix mono to stereo (by duplication) */
static void SDLCALL
SDL_ConvertMonoToStereo(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 2);
    int i;

    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / sizeof (float); i; --i) {
        src--;
        dst -= 2;
        dst[0] = dst[1] = *src;
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Upmix stereo to a pseudo-5.1 stream */
static void SDLCALL
SDL_ConvertStereoTo51(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    int i;
    float lf, rf, ce;
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 3);

    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof(float) * 2); i; --i) {
        dst -= 6;
        src -= 2;
        lf = src[0];
        rf = src[1];
        ce = (lf + rf) * 0.5f;
        /* !!! FIXME: FL and FR may clip */
        dst[0] = lf + (lf - ce);  /* FL */
        dst[1] = rf + (rf - ce);  /* FR */
        dst[2] = ce;  /* FC */
        dst[3] = 0;   /* LFE (only meant for special LFE effects) */
        dst[4] = lf;  /* BL */
        dst[5] = rf;  /* BR */
    }

    cvt->len_cvt *= 3;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Upmix quad to a pseudo-5.1 stream */
static void SDLCALL
SDL_ConvertQuadTo51(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    int i;
    float lf, rf, lb, rb, ce;
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 3 / 2);

    SDL_assert(format == AUDIO_F32SYS);
    SDL_assert(cvt->len_cvt % (sizeof(float) * 4) == 0);

    for (i = cvt->len_cvt / (sizeof(float) * 4); i; --i) {
        dst -= 6;
        src -= 4;
        lf = src[0];
        rf = src[1];
        lb = src[2];
        rb = src[3];
        ce = (lf + rf) * 0.5f;
        /* !!! FIXME: FL and FR may clip */
        dst[0] = lf + (lf - ce);  /* FL */
        dst[1] = rf + (rf - ce);  /* FR */
        dst[2] = ce;  /* FC */
        dst[3] = 0;   /* LFE (only meant for special LFE effects) */
        dst[4] = lb;  /* BL */
        dst[5] = rb;  /* BR */
    }

    cvt->len_cvt = cvt->len_cvt * 3 / 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Upmix stereo to a pseudo-4.0 stream (by duplication) */
static void SDLCALL
SDL_ConvertStereoToQuad(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 2);
    float lf, rf;
    int i;

    SDL_assert(format == AUDIO_F32SYS);

    for (i = cvt->len_cvt / (sizeof(float) * 2); i; --i) {
        dst -= 4;
        src -= 2;
        lf = src[0];
        rf = src[1];
        dst[0] = lf;  /* FL */
        dst[1] = rf;  /* FR */
        dst[2] = lf;  /* BL */
        dst[3] = rf;  /* BR */
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}


/* Upmix 5.1 to 7.1 */
static void SDLCALL
SDL_Convert51To71(SDL_AudioCVT * cvt, SDL_AudioFormat format)
{
    float lf, rf, lb, rb, ls, rs;
    int i;
    const float *src = (const float *) (cvt->buf + cvt->len_cvt);
    float *dst = (float *) (cvt->buf + cvt->len_cvt * 4 / 3);

    SDL_assert(format == AUDIO_F32SYS);
    SDL_assert(cvt->len_cvt % (sizeof(float) * 6) == 0);

    for (i = cvt->len_cvt / (sizeof(float) * 6); i; --i) {
        dst -= 8;
        src -= 6;
        lf = src[0];
        rf = src[1];
        lb = src[4];
        rb = src[5];
        ls = (lf + lb) * 0.5f;
        rs = (rf + rb) * 0.5f;
        /* !!! FIXME: these four may clip */
        lf += lf - ls;
        rf += rf - ls;
        lb += lb - ls;
        rb += rb - ls;
        dst[3] = src[3];  /* LFE */
        dst[2] = src[2];  /* FC */
        dst[7] = rs; /* SR */
        dst[6] = ls; /* SL */
        dst[5] = rb;  /* BR */
        dst[4] = lb;  /* BL */
        dst[1] = rf;  /* FR */
        dst[0] = lf;  /* FL */
    }

    cvt->len_cvt = cvt->len_cvt * 4 / 3;

    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index] (cvt, format);
    }
}

static int
SDL_BuildAudioTypeCVTFromFloat(SDL_AudioCVT *cvt, const SDL_AudioFormat dst_fmt)
{
    int retval = 0;  /* 0 == no conversion necessary. */

    if (!SDL_AUDIO_ISFLOAT(dst_fmt)) {
        const Uint16 dst_bitsize = SDL_AUDIO_BITSIZE(dst_fmt);
        const Uint16 src_bitsize = 32;
        SDL_AudioFilter filter = NULL;
        switch (dst_fmt & ~SDL_AUDIO_MASK_ENDIAN) {
            case AUDIO_S8: filter = SDL_Convert_F32_to_S8; break;
            case AUDIO_U8: filter = SDL_Convert_F32_to_U8; break;
            case AUDIO_S16: filter = SDL_Convert_F32_to_S16; break;
            case AUDIO_U16: filter = SDL_Convert_F32_to_U16; break;
            case AUDIO_S32: filter = SDL_Convert_F32_to_S32; break;
            default: SDL_assert(!"Unexpected audio format!"); break;
        }

        if (!filter) {
            return SDL_SetError("No conversion from float to destination format available");
        }

        if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
            return -1;
        }
        if (src_bitsize < dst_bitsize) {
            const int mult = (dst_bitsize / src_bitsize);
            cvt->len_mult *= mult;
            cvt->len_ratio *= mult;
        } else if (src_bitsize > dst_bitsize) {
            cvt->len_ratio /= (src_bitsize / dst_bitsize);
        }
        retval = 1;  /* added a converter. */
    }

    if ((SDL_AUDIO_ISBIGENDIAN(dst_fmt) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN)) {
        if (SDL_AddAudioCVTFilter(cvt, SDL_Convert_Byteswap) < 0) {
            return -1;
        }
        retval = 1;  /* added a converter. */
    }

    return retval;
}

static void SDL_ResampleCVT(SDL_AudioCVT *cvt, const int chans, const SDL_AudioFormat format) {
    /* !!! FIXME in 2.1: there are ten slots in the filter list, and the theoretical maximum we use is six (seven with NULL terminator).
       !!! FIXME in 2.1:   We need to store data for this resampler, because the cvt structure doesn't store the original sample rates,
       !!! FIXME in 2.1:   so we steal the ninth and tenth slot.  :( */
    const int inrate = (int) (size_t) cvt->filters[SDL_AUDIOCVT_MAX_FILTERS-1];
    const int outrate = (int) (size_t) cvt->filters[SDL_AUDIOCVT_MAX_FILTERS];
    const float *src = (const float *) cvt->buf;
    const int srclen = cvt->len_cvt;
    /*float *dst = (float *) cvt->buf;
    const int dstlen = (cvt->len * cvt->len_mult);*/
    /* !!! FIXME: remove this if we can get the resampler to work in-place again. */
    float *dst = (float *) (cvt->buf + srclen);
    const int dstlen = (cvt->len * cvt->len_mult) - srclen;
    const int requestedpadding = ResamplerPadding(inrate, outrate);
    int paddingsamples;
    float *padding;

    if (requestedpadding < SDL_MAX_SINT32 / chans) {
        paddingsamples = requestedpadding * chans;
    } else {
        paddingsamples = 0;
    }
    SDL_assert(format == AUDIO_F32SYS);

    /* we keep no streaming state here, so pad with silence on both ends. */
    padding = (float *) SDL_calloc(paddingsamples ? paddingsamples : 1, sizeof (float));
    if (!padding) {
        SDL_OutOfMemory();
        return;
    }

    cvt->len_cvt = SDL_ResampleAudio(chans, inrate, outrate, padding, padding, src, srclen, dst, dstlen);

    SDL_free(padding);

    SDL_memmove(cvt->buf, dst, cvt->len_cvt);  /* !!! FIXME: remove this if we can get the resampler to work in-place again. */

    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, format);
    }
}

/* !!! FIXME: We only have this macro salsa because SDL_AudioCVT doesn't
   !!! FIXME:  store channel info, so we have to have function entry
   !!! FIXME:  points for each supported channel count and multiple
   !!! FIXME:  vs arbitrary. When we rev the ABI, clean this up. */
#define RESAMPLER_FUNCS(chans) \
    static void SDLCALL \
    SDL_ResampleCVT_c##chans(SDL_AudioCVT *cvt, SDL_AudioFormat format) { \
        SDL_ResampleCVT(cvt, chans, format); \
    }
RESAMPLER_FUNCS(1)
RESAMPLER_FUNCS(2)
RESAMPLER_FUNCS(4)
RESAMPLER_FUNCS(6)
RESAMPLER_FUNCS(8)
#undef RESAMPLER_FUNCS

static SDL_AudioFilter ChooseCVTResampler(const int dst_channels) {
    switch (dst_channels) {
        case 1: return SDL_ResampleCVT_c1;
        case 2: return SDL_ResampleCVT_c2;
        case 4: return SDL_ResampleCVT_c4;
        case 6: return SDL_ResampleCVT_c6;
        case 8: return SDL_ResampleCVT_c8;
        default: break;
    }

    return NULL;
}

static int
SDL_BuildAudioResampleCVT(SDL_AudioCVT * cvt, const int dst_channels,
                          const int src_rate, const int dst_rate)
{
    SDL_AudioFilter filter;

    if (src_rate == dst_rate) {
        return 0;  /* no conversion necessary. */
    }

    filter = ChooseCVTResampler(dst_channels);
    if (filter == NULL) {
        return SDL_SetError("No conversion available for these rates");
    }

    if (SDL_PrepareResampleFilter() < 0) {
        return -1;
    }

    /* Update (cvt) with filter details... */
    if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
        return -1;
    }

    /* !!! FIXME in 2.1: there are ten slots in the filter list, and the theoretical maximum we use is six (seven with NULL terminator).
       !!! FIXME in 2.1:   We need to store data for this resampler, because the cvt structure doesn't store the original sample rates,
       !!! FIXME in 2.1:   so we steal the ninth and tenth slot.  :( */
    if (cvt->filter_index >= (SDL_AUDIOCVT_MAX_FILTERS-2)) {
        return SDL_SetError("Too many filters needed for conversion, exceeded maximum of %d", SDL_AUDIOCVT_MAX_FILTERS-2);
    }
    cvt->filters[SDL_AUDIOCVT_MAX_FILTERS-1] = (SDL_AudioFilter) (size_t) src_rate;
    cvt->filters[SDL_AUDIOCVT_MAX_FILTERS] = (SDL_AudioFilter) (size_t) dst_rate;

    if (src_rate < dst_rate) {
        const double mult = ((double) dst_rate) / ((double) src_rate);
        cvt->len_mult *= (int) SDL_ceil(mult);
        cvt->len_ratio *= mult;
    } else {
        cvt->len_ratio /= ((double) src_rate) / ((double) dst_rate);
    }

    /* !!! FIXME: remove this if we can get the resampler to work in-place again. */
    /* the buffer is big enough to hold the destination now, but
       we need it large enough to hold a separate scratch buffer. */
    cvt->len_mult *= 2;

    return 1;               /* added a converter. */
}

int SDL_BuildAudioCVT(SDL_AudioCVT * cvt, SDL_AudioFormat src_fmt, Uint8 src_channels, int src_rate,
    SDL_AudioFormat dst_fmt, Uint8 dst_channels, int dst_rate) {

    /* Sanity check target pointer */
    if (cvt == NULL) {
        return SDL_InvalidParamError("cvt");
    }

    /* Make sure we zero out the audio conversion before error checking */
    SDL_zerop(cvt);

    if (!SDL_SupportedAudioFormat(src_fmt)) {
        return SDL_SetError("Invalid source format");
    } else if (!SDL_SupportedAudioFormat(dst_fmt)) {
        return SDL_SetError("Invalid destination format");
    } else if (!SDL_SupportedChannelCount(src_channels)) {
        return SDL_SetError("Invalid source channels");
    } else if (!SDL_SupportedChannelCount(dst_channels)) {
        return SDL_SetError("Invalid destination channels");
    } else if (src_rate == 0) {
        return SDL_SetError("Source rate is zero");
    } else if (dst_rate == 0) {
        return SDL_SetError("Destination rate is zero");
    }

    /* Start off with no conversion necessary */
    cvt->src_format = src_fmt;
    cvt->dst_format = dst_fmt;
    cvt->needed = 0;
    cvt->filter_index = 0;
    SDL_zero(cvt->filters);
    cvt->len_mult = 1;
    cvt->len_ratio = 1.0;
    cvt->rate_incr = ((double) dst_rate) / ((double) src_rate);

    /* Type conversion goes like this now:
        - byteswap to CPU native format first if necessary.
        - convert to native Float32 if necessary.
        - resample and change channel count if necessary.
        - convert back to native format.
        - byteswap back to foreign format if necessary.
       The expectation is we can process data faster in float32
       (possibly with SIMD), and making several passes over the same
       buffer is likely to be CPU cache-friendly, avoiding the
       biggest performance hit in modern times. Previously we had
       (script-generated) custom converters for every data type and
       it was a bloat on SDL compile times and final library size. */

    /* see if we can skip float conversion entirely. */
    if (src_rate == dst_rate && src_channels == dst_channels) {
        if (src_fmt == dst_fmt) {
            return 0;
        }

        /* just a byteswap needed? */
        if ((src_fmt & ~SDL_AUDIO_MASK_ENDIAN) == (dst_fmt & ~SDL_AUDIO_MASK_ENDIAN)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_Convert_Byteswap) < 0) {
                return -1;
            }
            cvt->needed = 1;
            return 1;
        }
    }

    /* Convert data types, if necessary. Updates (cvt). */
    if (SDL_BuildAudioTypeCVTToFloat(cvt, src_fmt) < 0) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    /* Channel conversion */
    if (src_channels < dst_channels) {
        /* Upmixing */
        /* Mono -> Stereo [-> ...] */
        if ((src_channels == 1) && (dst_channels > 1)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_ConvertMonoToStereo) < 0) {
                return -1;
            }
            cvt->len_mult *= 2;
            src_channels = 2;
            cvt->len_ratio *= 2;
        }
        /* [Mono ->] Stereo -> 5.1 [-> 7.1] */
        if ((src_channels == 2) && (dst_channels >= 6)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_ConvertStereoTo51) < 0) {
                return -1;
            }
            src_channels = 6;
            cvt->len_mult *= 3;
            cvt->len_ratio *= 3;
        }
        /* Quad -> 5.1 [-> 7.1] */
        if ((src_channels == 4) && (dst_channels >= 6)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_ConvertQuadTo51) < 0) {
                return -1;
            }
            src_channels = 6;
            cvt->len_mult = (cvt->len_mult * 3 + 1) / 2;
            cvt->len_ratio *= 1.5;
        }
        /* [[Mono ->] Stereo ->] 5.1 -> 7.1 */
        if ((src_channels == 6) && (dst_channels == 8)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_Convert51To71) < 0) {
                return -1;
            }
            src_channels = 8;
            cvt->len_mult = (cvt->len_mult * 4 + 2) / 3;
            /* Should be numerically exact with every valid input to this
               function */
            cvt->len_ratio = cvt->len_ratio * 4 / 3;
        }
        /* [Mono ->] Stereo -> Quad */
        if ((src_channels == 2) && (dst_channels == 4)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_ConvertStereoToQuad) < 0) {
                return -1;
            }
            src_channels = 4;
            cvt->len_mult *= 2;
            cvt->len_ratio *= 2;
        }
    } else if (src_channels > dst_channels) {
        /* Downmixing */
        /* 7.1 -> 5.1 [-> Stereo [-> Mono]] */
        /* 7.1 -> 5.1 [-> Quad] */
        if ((src_channels == 8) && (dst_channels <= 6)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_Convert71To51) < 0) {
                return -1;
            }
            src_channels = 6;
            cvt->len_ratio *= 0.75;
        }
        /* [7.1 ->] 5.1 -> Stereo [-> Mono] */
        if ((src_channels == 6) && (dst_channels <= 2)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_Convert51ToStereo) < 0) {
                return -1;
            }
            src_channels = 2;
            cvt->len_ratio /= 3;
        }
        /* 5.1 -> Quad */
        if ((src_channels == 6) && (dst_channels == 4)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_Convert51ToQuad) < 0) {
                return -1;
            }
            src_channels = 4;
            cvt->len_ratio = cvt->len_ratio * 2 / 3;
        }
        /* Quad -> Stereo [-> Mono] */
        if ((src_channels == 4) && (dst_channels <= 2)) {
            if (SDL_AddAudioCVTFilter(cvt, SDL_ConvertQuadToStereo) < 0) {
                return -1;
            }
            src_channels = 2;
            cvt->len_ratio /= 2;
        }
        /* [... ->] Stereo -> Mono */
        if ((src_channels == 2) && (dst_channels == 1)) {
            SDL_AudioFilter filter = NULL;

            filter = SDL_ConvertStereoToMono;

            if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
                return -1;
            }

            src_channels = 1;
            cvt->len_ratio /= 2;
        }
    }

    if (src_channels != dst_channels) {
        /* All combinations of supported channel counts should have been
           handled by now, but let's be defensive */
      return SDL_SetError("Invalid channel combination");
    }

    /* Do rate conversion, if necessary. Updates (cvt). */
    if (SDL_BuildAudioResampleCVT(cvt, dst_channels, src_rate, dst_rate) < 0) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    /* Move to final data type. */
    if (SDL_BuildAudioTypeCVTFromFloat(cvt, dst_fmt) < 0) {
        return -1;              /* shouldn't happen, but just in case... */
    }

    cvt->needed = (cvt->filter_index != 0);
    return (cvt->needed);
}

int SDL_ConvertAudio(SDL_AudioCVT * cvt)
{
    /* !!! FIXME: (cvt) should be const; stack-copy it here. */
    /* !!! FIXME: (actually, we can't...len_cvt needs to be updated. Grr.) */

    /* Make sure there's data to convert */
    if (cvt->buf == NULL) {
        return SDL_SetError("No buffer allocated for conversion");
    }

    /* Return okay if no conversion is necessary */
    cvt->len_cvt = cvt->len;
    if (cvt->filters[0] == NULL) {
        return 0;
    }

    /* Set up the conversion and go! */
    cvt->filter_index = 0;
    cvt->filters[0] (cvt, cvt->src_format);
    return 0;
}

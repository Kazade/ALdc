/*
 * This file is essentially a copy-paste of the minimum code from SDL required to power MojoAL, with the exception
 * of the audio device code which is Dreamcast specific.
 *
 * It's quick and dirty, but it's also the fastest way to get some kind of OpenAL implementation on the DC without
 * doing it from scratch
 */

#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#include <kos/mutex.h>
#include "aldc.h"

// FIXME: How to implement this on SH4?
#define PAUSE_INSTRUCTION()

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

SDL_AudioDeviceID SDL_OpenAudioDevice(
    const char*          device,
    int                  iscapture,
    const SDL_AudioSpec* desired,
    SDL_AudioSpec*       obtained,
    int                  allowed_changes
        ) {

}

void SDL_CloseAudioDevice(SDL_AudioDeviceID dev) {

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

}

const char *SDL_GetAudioDeviceName(int index, int iscapture) {

}

int SDL_BuildAudioCVT(
    SDL_AudioCVT * cvt,
    SDL_AudioFormat src_format,
    Uint8 src_channels,
    int src_rate,
    SDL_AudioFormat dst_format,
    Uint8 dst_channels,
    int dst_rate
        ) {

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

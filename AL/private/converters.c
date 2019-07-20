#define DIVBY128 0.0078125f
#define DIVBY32768 0.000030517578125f
#define DIVBY8388607 0.00000011920930376163766f

void
SDL_Convert_S8_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Sint8 *src = ((const Sint8 *) (cvt->buf + cvt->len_cvt)) - 1;
    float *dst = ((float *) (cvt->buf + cvt->len_cvt * 4)) - 1;
    int i;

    for (i = cvt->len_cvt; i; --i, --src, --dst) {
        *dst = ((float) *src) * DIVBY128;
    }

    cvt->len_cvt *= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Convert_U8_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Uint8 *src = ((const Uint8 *) (cvt->buf + cvt->len_cvt)) - 1;
    float *dst = ((float *) (cvt->buf + cvt->len_cvt * 4)) - 1;
    int i;

    for (i = cvt->len_cvt; i; --i, --src, --dst) {
        *dst = (((float) *src) * DIVBY128) - 1.0f;
    }

    cvt->len_cvt *= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Convert_S16_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Sint16 *src = ((const Sint16 *) (cvt->buf + cvt->len_cvt)) - 1;
    float *dst = ((float *) (cvt->buf + cvt->len_cvt * 2)) - 1;
    int i;

    for (i = cvt->len_cvt / sizeof (Sint16); i; --i, --src, --dst) {
        *dst = ((float) *src) * DIVBY32768;
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Convert_U16_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Uint16 *src = ((const Uint16 *) (cvt->buf + cvt->len_cvt)) - 1;
    float *dst = ((float *) (cvt->buf + cvt->len_cvt * 2)) - 1;
    int i;

    for (i = cvt->len_cvt / sizeof (Uint16); i; --i, --src, --dst) {
        *dst = (((float) *src) * DIVBY32768) - 1.0f;
    }

    cvt->len_cvt *= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Convert_S32_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const Sint32 *src = (const Sint32 *) cvt->buf;
    float *dst = (float *) cvt->buf;
    int i;

    for (i = cvt->len_cvt / sizeof (Sint32); i; --i, ++src, ++dst) {
        *dst = ((float) (*src>>8)) * DIVBY8388607;
    }

    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_F32SYS);
    }
}

void
SDL_Convert_F32_to_S8(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Sint8 *dst = (Sint8 *) cvt->buf;
    int i;

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 127;
        } else if (sample <= -1.0f) {
            *dst = -128;
        } else {
            *dst = (Sint8)(sample * 127.0f);
        }
    }

    cvt->len_cvt /= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_S8);
    }
}

void
SDL_Convert_F32_to_U8(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Uint8 *dst = (Uint8 *) cvt->buf;
    int i;

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 255;
        } else if (sample <= -1.0f) {
            *dst = 0;
        } else {
            *dst = (Uint8)((sample + 1.0f) * 127.0f);
        }
    }

    cvt->len_cvt /= 4;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_U8);
    }
}

void
SDL_Convert_F32_to_S16(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Sint16 *dst = (Sint16 *) cvt->buf;
    int i;

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 32767;
        } else if (sample <= -1.0f) {
            *dst = -32768;
        } else {
            *dst = (Sint16)(sample * 32767.0f);
        }
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_S16SYS);
    }
}

void
SDL_Convert_F32_to_U16(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Uint16 *dst = (Uint16 *) cvt->buf;
    int i;

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 65535;
        } else if (sample <= -1.0f) {
            *dst = 0;
        } else {
            *dst = (Uint16)((sample + 1.0f) * 32767.0f);
        }
    }

    cvt->len_cvt /= 2;
    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_U16SYS);
    }
}

void
SDL_Convert_F32_to_S32(SDL_AudioCVT *cvt, SDL_AudioFormat format)
{
    const float *src = (const float *) cvt->buf;
    Sint32 *dst = (Sint32 *) cvt->buf;
    int i;

    for (i = cvt->len_cvt / sizeof (float); i; --i, ++src, ++dst) {
        const float sample = *src;
        if (sample >= 1.0f) {
            *dst = 2147483647;
        } else if (sample <= -1.0f) {
            *dst = (Sint32) -2147483648LL;
        } else {
            *dst = ((Sint32)(sample * 8388607.0f)) << 8;
        }
    }

    if (cvt->filters[++cvt->filter_index]) {
        cvt->filters[cvt->filter_index](cvt, AUDIO_S32SYS);
    }
}



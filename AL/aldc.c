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
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <kos/mutex.h>
#include <dc/sound/stream.h>

#include "private/core.c"
#include "private/endian.c"
#include "private/errors.c"
#include "private/atomics.c"
#include "private/data_queue.c"
#include "private/streams.c"
#include "private/converters.c"
#include "private/device.c"

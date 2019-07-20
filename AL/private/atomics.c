
typedef struct { int value; } SDL_atomic_t;
typedef int SDL_SpinLock;

void SDL_AtomicLock(SDL_SpinLock *lock);
void SDL_AtomicUnlock(SDL_SpinLock *lock);

#define SDL_CompilerBarrier()   \
    { SDL_SpinLock _tmp = 0; SDL_AtomicLock(&_tmp); SDL_AtomicUnlock(&_tmp); }

#define SDL_MemoryBarrierRelease() __asm__ __volatile__ ("" : : : "memory")
#define SDL_MemoryBarrierAcquire() __asm__ __volatile__ ("" : : : "memory")

#ifndef SDL_AtomicIncRef
#define SDL_AtomicIncRef(a)    SDL_AtomicAdd(a, 1)
#endif

#ifndef SDL_AtomicDecRef
#define SDL_AtomicDecRef(a)    (SDL_AtomicAdd(a, -1) == 1)
#endif

// FIXME: How to implement this on SH4?
#define PAUSE_INSTRUCTION()

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
#ifdef _arch_dreamcast
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
#else
    return SDL_TRUE;
#endif
}

SDL_bool SDL_HasSSE() {
#ifdef _arch_dreamcast
    return SDL_FALSE;
#else
    return SDL_TRUE;
#endif
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

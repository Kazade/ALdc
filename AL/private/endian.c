#pragma once


SDL_FORCE_INLINE Uint16 SDL_Swap16(Uint16 x) {
    return SDL_static_cast(Uint16, ((x << 8) | (x >> 8)));
}

SDL_FORCE_INLINE Uint32 SDL_Swap32(Uint32 x) {
    return SDL_static_cast(Uint32, ((x << 24) | ((x << 8) & 0x00FF0000) |
                                    ((x >> 8) & 0x0000FF00) | (x >> 24)));
}

SDL_FORCE_INLINE Uint64 SDL_Swap64(Uint64 x)
{
    Uint32 hi, lo;

    /* Separate into high and low 32-bit values and swap them */
    lo = SDL_static_cast(Uint32, x & 0xFFFFFFFF);
    x >>= 32;
    hi = SDL_static_cast(Uint32, x & 0xFFFFFFFF);
    x = SDL_Swap32(lo);
    x <<= 32;
    x |= SDL_Swap32(hi);
    return (x);
}

#pragma once

#include "../aldc.h"

void SDLCALL SDL_Convert_S8_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_U8_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_S16_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_U16_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_S32_to_F32(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_S8(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_U8(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_S16(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_U16(SDL_AudioCVT *cvt, SDL_AudioFormat format);
void SDLCALL SDL_Convert_F32_to_S32(SDL_AudioCVT *cvt, SDL_AudioFormat format);

#pragma once
#include <stdint.h>
typedef uint8_t Uint8;
typedef unsigned SDL_AudioDeviceID;
typedef struct {int unused;} SDL_Thread;
typedef struct {int freq;unsigned format,channels,samples;void(*callback)(void*,Uint8*,int);} SDL_AudioSpec;
#define SDL_INIT_AUDIO 16
#define AUDIO_S16LSB 0x8010
int SDL_InitSubSystem(unsigned);
void SDL_CloseAudioDevice(unsigned);
unsigned SDL_OpenAudioDevice(const char*,int,const SDL_AudioSpec*,SDL_AudioSpec*,int);
SDL_Thread* SDL_CreateThread(int(*)(void*),const char*,void*);
void SDL_Delay(unsigned);
void SDL_PauseAudioDevice(unsigned,int);
void SDL_LockAudioDevice(unsigned);
void SDL_UnlockAudioDevice(unsigned);

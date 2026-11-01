// #125 스파이크-B — SDL_mixer.h 심 헤더 (폐기 코드).
// 엔진 TU가 참조하는 최소 표면만 선언(SDK 실물 헤더의 부분 복사 — 시그니처
// 호환만 보장). 실물 조달은 본판 이식의 몫.
#ifndef SPIKE_B_SDL_MIXER_SHIM_H
#define SPIKE_B_SDL_MIXER_SHIM_H

#include "SDL_stdinc.h"
#include "SDL_error.h"

typedef struct Mix_Chunk Mix_Chunk;
typedef struct Mix_Music Mix_Music;

#ifdef __cplusplus
extern "C" {
#endif
extern int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
extern int Mix_AllocateChannels(int numchans);
extern int Mix_Volume(int channel, int volume);
extern int Mix_VolumeMusic(int volume);
extern Mix_Chunk* Mix_LoadWAV(const char* file);
extern Mix_Music* Mix_LoadMUS(const char* file);
extern void Mix_FreeChunk(Mix_Chunk* chunk);
extern void Mix_FreeMusic(Mix_Music* music);
extern int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops);
extern int Mix_PlayMusic(Mix_Music* music, int loops);
extern int Mix_HaltChannel(int channel);
extern int Mix_HaltMusic(void);
extern void Mix_PauseMusic(void);
extern void Mix_ResumeMusic(void);
extern int Mix_Playing(int channel);
extern void Mix_CloseAudio(void);
extern const char* Mix_GetError(void);
#ifdef __cplusplus
}
#endif

/* MIX_MAX_VOLUME / MIX_CHANNELS (실물 헤더 값) */
#define MIX_CHANNELS 8
#define MIX_MAX_VOLUME 128

/* MIX_DEFAULT_FORMAT (실물 헤더 값 — AUDIO_S16SYS = SDL_byteorder 따름) */
#ifndef AUDIO_S16LSB
#define AUDIO_S16LSB 0x8010
#endif
#ifndef AUDIO_S16MSB
#define AUDIO_S16MSB 0x9010
#endif
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define MIX_DEFAULT_FORMAT AUDIO_S16MSB
#else
#define MIX_DEFAULT_FORMAT AUDIO_S16LSB
#endif

#endif /* SPIKE_B_SDL_MIXER_SHIM_H */
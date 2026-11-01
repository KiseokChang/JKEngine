// #125 스파이크-B — SDL2_mixer 음소거 심 (폐기 코드).
// 엔진이 참조하는 Mix_* 표면(grep 실측 — JKSDLAudioBackend.cpp 20종)을
// no-op로 봉인해 링크를 살린다. 실측: 스파이크 화면은 무음 — 오디오 금형은
// 본판 이식의 몫.
#include "SDL_mixer.h"

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize)
{
    (void)frequency; (void)format; (void)channels; (void)chunksize;
    SDL_SetError("spike-B: audio muted (mixer shim)");
    return -1;  /* 정직 설계: Init 실패를 엔진이 fail-closed로 봉인 */
}

int Mix_AllocateChannels(int numchans) { (void)numchans; return 0; }
int Mix_Volume(int channel, int volume) { (void)channel; (void)volume; return 0; }
int Mix_VolumeMusic(int volume) { (void)volume; return 0; }
Mix_Chunk* Mix_LoadWAV(const char* file) { (void)file; return NULL; }
Mix_Music* Mix_LoadMUS(const char* file) { (void)file; return NULL; }
void Mix_FreeChunk(Mix_Chunk* chunk) { (void)chunk; }
void Mix_FreeMusic(Mix_Music* music) { (void)music; }
int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops)
{ (void)channel; (void)chunk; (void)loops; return -1; }
int Mix_PlayMusic(Mix_Music* music, int loops) { (void)music; (void)loops; return -1; }
int Mix_HaltChannel(int channel) { (void)channel; return 0; }
int Mix_HaltMusic(void) { return 0; }
void Mix_PauseMusic(void) {}
void Mix_ResumeMusic(void) {}
int Mix_Playing(int channel) { (void)channel; return 0; }
void Mix_CloseAudio(void) {}
const char* Mix_GetError(void) { return SDL_GetError(); }
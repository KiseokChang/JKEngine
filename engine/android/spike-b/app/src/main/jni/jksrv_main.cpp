// #125 스파이크-B — SDL_main 심 (폐기 코드).
// SDLActivity가 libjkdesk.so에서 dlsym("SDL_main")한 뒤 SDL_Android_Init을
// 마친 스레드에서 부른다. 우리는 엔진의 plain main()을 그대로 호출한다 —
// SDL_AndroidInit 이후이므로 SDL_AndroidGetInternalStoragePath()가 유효하고,
// 이 시점이 env(TMPDIR)를 심을 수 있는 최후의 해자(server spawn이 자식에게
// environ을 그대로 물려준다 — fork+exec 계약).
#include <SDL.h>
#ifdef main
#undef main
#endif

#include <android/log.h>
#include <sys/stat.h>
#include <cstdlib>
#include <string>

extern "C" int main(int argc, char* argv[]);

__attribute__((visibility("default")))
extern "C" int SDL_main(int argc, char* argv[]) {
    __android_log_print(ANDROID_LOG_INFO, "jksrv", "[spikeB] SDL_main argc=%d argv0=%s",
                        argc, argc > 0 ? argv[0] : "(null)");

    // TMPDIR — 엔진 TempDir()/shm 폴백·인스턴스 락·IPC 소켓이 전부 이곳을
    // 기준으로 만들어진다. files/tmp (스파이크 규약 — app files dir 하위).
    if (!getenv("TMPDIR")) {
        const char* internal = SDL_AndroidGetInternalStoragePath();
        if (internal) {
            std::string tmp = std::string(internal) + "/tmp";
            ::mkdir(tmp.c_str(), 0700);
            std::string fonts = std::string(internal) + "/fonts";
            ::mkdir(fonts.c_str(), 0700);
            ::setenv("TMPDIR", tmp.c_str(), 1);
            __android_log_print(ANDROID_LOG_INFO, "jksrv", "[spikeB] TMPDIR=%s JK tmp seeded",
                                tmp.c_str());
        }
    }

    static bool started = false;
    if (started) {
        // SDLActivity 재시작 시 이전 스레드가 살아있으면 중복 SDL_main 방지.
        __android_log_print(ANDROID_LOG_WARN, "jksrv", "[spikeB] SDL_main re-entry refused");
        return 0;
    }
    started = true;
    return main(argc, argv);
}
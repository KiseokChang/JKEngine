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
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" int main(int argc, char* argv[]);

__attribute__((visibility("default")))
extern "C" int SDL_main(int argc, char* argv[]) {
    __android_log_print(ANDROID_LOG_INFO, "jksrv", "[spikeB] SDL_main argc=%d argv0=%s",
                        argc, argc > 0 ? argv[0] : "(null)");

    // Android 15가 타앱 logcat을 필터링한다(14:36 실측 — spikeB 흔적 0) →
    // stderr/stdout을 files/srv.log로 freopen해 원문을 내부 파일로 수취하고
    // MainActivity가 MediaStore.Downloads로 내보낸다(Termux scp 수취 채널).
    const char* internal = SDL_AndroidGetInternalStoragePath();
    if (internal) {
        std::string logPath = std::string(internal) + "/srv.log";
        if (!std::freopen(logPath.c_str(), "w", stdout))
            __android_log_print(ANDROID_LOG_WARN, "jksrv", "[spikeB] stdout freopen miss");
        if (!std::freopen(logPath.c_str(), "a", stderr))
            __android_log_print(ANDROID_LOG_WARN, "jksrv", "[spikeB] stderr freopen miss");

        // 정직 원문 수취 — argv·/proc/self/exe·exe-dir 관측(각 SDL_main마다 1행):
        char selfExe[512] = {};
        const ssize_t selfLen = ::readlink("/proc/self/exe", selfExe, sizeof(selfExe) - 1);
        if (selfLen > 0) selfExe[selfLen] = '\0';
        std::fprintf(stderr, "[spikeB] SDL_main argv0=%s selfExe=%s args=%s%s%s\n",
                     argc > 0 ? argv[0] : "(null)",
                     selfLen > 0 ? selfExe : "(unread)",
                     argc > 1 ? argv[1] : "",
                     argc > 2 ? " " : "",
                     argc > 2 ? argv[2] : "");
        // 엔진 exe-dir 규약의 Android 수형: argv[0]=nativeLibraryDir/libjkdesk.so
        // → JK_EXE_PATH 심(src/fs/JKFs_posix.cpp GetExecutablePath 스파이크 leg).
        if (argc > 0 && argv[0][0] == '/')
            ::setenv("JK_EXE_PATH", argv[0], 1);
        else
            std::fprintf(stderr, "[spikeB] WARN argv0 not absolute — JK_EXE_PATH unsown\n");
        // 클라 exec 흔적 — 서버 스폰 자식이 이 fd를 물려받아 같은 파일에 남는다.
        if (argc > 1 && std::strcmp(argv[1], "--client") == 0)
            std::fprintf(stderr, "[spikeB] CLIENT MAIN REACHED app=%s — exec leg OK\n",
                         argc > 2 ? argv[2] : "?");
    }

    // TMPDIR — 엔진 TempDir()/shm 폴백·인스턴스 락·IPC 소켓이 전부 이곳을
    // 기준으로 만들어진다. files/tmp (스파이크 규약 — app files dir 하위).
    if (!getenv("TMPDIR") && internal) {
        std::string tmp = std::string(internal) + "/tmp";
        ::mkdir(tmp.c_str(), 0700);
        std::string fonts = std::string(internal) + "/fonts";
        ::mkdir(fonts.c_str(), 0700);
        ::setenv("TMPDIR", tmp.c_str(), 1);
        __android_log_print(ANDROID_LOG_INFO, "jksrv", "[spikeB] TMPDIR=%s JK tmp seeded",
                            tmp.c_str());
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
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
#include <dlfcn.h>
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
        // stderr를 독립 파일로 — r10 원문 함정(stdout "w" truncate가 stderr
        // 내용과 같은 파일을 갈라서 스냅 순서 교차) 소각.
        std::string errPath = std::string(internal) + "/srv.err";
        if (!std::freopen(errPath.c_str(), "w", stderr))
            __android_log_print(ANDROID_LOG_WARN, "jksrv", "[spikeB] stderr freopen miss");
        std::string outPath = std::string(internal) + "/srv.log";
        if (!std::freopen(outPath.c_str(), "w", stdout))
            __android_log_print(ANDROID_LOG_WARN, "jksrv", "[spikeB] stdout freopen miss");
        std::setvbuf(stderr, nullptr, _IONBF, 0);  // 즉시 수체 — 스냅 0바이트 함정 여지 소각

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
        // 엔진 exe-dir 규약의 Android 수형 — r11 실측: /proc/self/maps 단발
        // fread(256KB)는 libjkdesk.so 매핑 앞에서 끊겨 미검출(r11 원문).
        // r12 = dladdr 전환: SDL_main 자체의 주소가 속한 물리 경로를
        // dynamic linker에게 물어본다 — libjkdesk.so의 nativeLibraryDir 경로.
        // 동시에 W^X 실측: nativeLibraryDir 파일이 X_OK인지 원문으로 (docs/101
        // §3 열린 질문 — "jniLibs .so exec 불가" 구 원장과 충돌).
        if (selfLen <= 0 || std::strncmp(selfExe, "/system/bin/app_process", 23) == 0) {
            Dl_info info{};
            if (::dladdr(reinterpret_cast<void*>(&SDL_main), &info) != 0 &&
                info.dli_fname) {
                const std::string libPath(info.dli_fname);
                const size_t slash = libPath.find_last_of('/');
                const std::string resolved =
                    (slash == std::string::npos)
                        ? libPath
                        : libPath.substr(0, slash + 1) + "libjkdesk.so";
                const bool xok = ::access(resolved.c_str(), X_OK) == 0;
                ::setenv("JK_EXE_PATH", resolved.c_str(), 1);
                std::fprintf(stderr,
                    "[spikeB] exe=%s X_OK=%d (W^X 실측: %s)\n",
                    resolved.c_str(), (int)xok,
                    xok ? "nativeLibraryDir exec 허용"
                        : "exec 거부 — 방향 B 클라 exec leg 불성립 원문");
            } else
                std::fprintf(stderr, "[spikeB] FATAL dladdr 실패 — exe-dir 규약 무해 저하\n");
        }
        // 상대 에셋 경로("assets/…")의 CWD 수형 — r11 원문: 서버가
        // assets/backgrounds·icons를 CWD에서 못 찾음(zygote 계승 CWD).
        // MainActivity가 files/assets 트리로 추출해 둔 곳으로 chdir한다.
        if (internal) {
            std::string assetsDir = std::string(internal) + "/assets";
            ::mkdir(assetsDir.c_str(), 0700);
            if (::chdir(internal) == 0)
                std::fprintf(stderr, "[spikeB] CWD=%s (relative assets/ 해석점)\n", internal);
        }
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
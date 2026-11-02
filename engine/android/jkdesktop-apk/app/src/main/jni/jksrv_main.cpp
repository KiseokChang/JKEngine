// #126 jkdesktop-apk — SDL_main 심.
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
extern "C" int JKVoiceAutoturnStart();
#include <string>

// fdsan 합법 수신부(SDL2의 stdio unique_fd 점유 정리 — 01:36 사형 원존).
// fdsan API는 29 도입(빌드 타깃 28 = 헤더 게이트 미노출 원문) → dlsym으로
// 런타임 해석(Android 15 실기 = 29+ 존재).
static void TakeoverStdio() {
    auto getTag = reinterpret_cast<uint64_t (*)(int)>(
        ::dlsym(RTLD_DEFAULT, "android_fdsan_get_owner_tag"));
    auto exchange = reinterpret_cast<void (*)(int, uint64_t, uint64_t)>(
        ::dlsym(RTLD_DEFAULT, "android_fdsan_exchange_owner_tag"));
    if (!getTag || !exchange) return;
    for (int fd : {1, 2}) {
        const uint64_t oldTag = getTag(fd);
        if (oldTag != 0) exchange(fd, oldTag, 0);
    }
}

extern "C" int main(int argc, char* argv[]);

__attribute__((visibility("default")))
extern "C" int SDL_main(int argc, char* argv[]) {
    __android_log_print(ANDROID_LOG_INFO, "jksrv", "[jkdesk] SDL_main argc=%d argv0=%s",
                        argc, argc > 0 ? argv[0] : "(null)");
    TakeoverStdio();  // 로그 채널 — freopen의 합법 전재(01:36 fdsan 원존)
    if (const char* logBase = SDL_AndroidGetInternalStoragePath()) {
        const std::string logPath = std::string(logBase) + "/srv.err";
        if (!std::freopen(logPath.c_str(), "w", stderr)) {
            __android_log_print(ANDROID_LOG_WARN, "jksrv",
                                "[jkdesk] stderr freopen miss");
        }
        std::setvbuf(stderr, nullptr, _IONBF, 0);
    }

    const char* internal = SDL_AndroidGetInternalStoragePath();

// 정직 원문 수취 — argv·/proc/self/exe·exe-dir 관측(각 SDL_main마다 1행):
        char selfExe[512] = {};
        const ssize_t selfLen = ::readlink("/proc/self/exe", selfExe, sizeof(selfExe) - 1);
        if (selfLen > 0) selfExe[selfLen] = '\0';
        std::fprintf(stderr, "[jkdesk] SDL_main argv0=%s selfExe=%s args=%s%s%s\n",
                     argc > 0 ? argv[0] : "(null)",
                     selfLen > 0 ? selfExe : "(unread)",
                     argc > 1 ? argv[1] : "",
                     argc > 2 ? " " : "",
                     argc > 2 ? argv[2] : "");

        // 엔진 exe-dir 규약의 Android 수형 — r11/r13 실측: SDLActivity는 argv0
        // 에 "app_process"를 준다(lib 경로 아님) → dladdr로 이 진입 심보가
        // 속한 물리(libjkdesk.so) 경로를 얻는다. 동시에 W^X 실측 원문.
        // r13 원문: exec 자식("libjkc.so" PIE)은 bionic 링커 기동까지 성립했으나
        // "libSDL2.so not found" — 앱 프로세스 env에 nativeLibraryDir가 없어
        // 자식 DT_NEEDED 해석 실패 → LD_LIBRARY_PATH에 nativeLibraryDir를
        // 추가해 자식에게 물려준다(exec가 environ을 상속하는 계약).
        if (selfLen <= 0 || std::strncmp(selfExe, "/system/bin/app_process", 23) == 0) {
            Dl_info info{};
            if (::dladdr(reinterpret_cast<void*>(&SDL_main), &info) != 0 &&
                info.dli_fname) {
                const std::string libPath(info.dli_fname);
                const size_t slash = libPath.find_last_of('/');
                const std::string natDir = libPath.substr(0, slash + 1);
                const std::string resolved = natDir + "libjkdesk.so";
                const bool xok = ::access(resolved.c_str(), X_OK) == 0;
                ::setenv("JK_EXE_PATH", resolved.c_str(), 1);
                std::fprintf(stderr,
                    "[jkdesk] exe=%s X_OK=%d (W^X 실측: %s)\n",
                    resolved.c_str(), (int)xok,
                    xok ? "nativeLibraryDir exec 허용"
                        : "exec 거부 — 방향 B 클라 exec leg 불성립 원문");
                std::string ldl;
                const char* cur = ::getenv("LD_LIBRARY_PATH");
                if (cur) ldl = cur;
                if (ldl.find(natDir) == std::string::npos)
                    ldl = natDir + (ldl.empty() ? "" : ":") + ldl;
                ::setenv("LD_LIBRARY_PATH", ldl.c_str(), 1);
                std::fprintf(stderr, "[jkdesk] LD_LIBRARY_PATH=%s\n", ldl.c_str());
            } else
                std::fprintf(stderr, "[jkdesk] FATAL dladdr 실패 — exe-dir 규약 무해 저하\n");
        }
        // 상대 에셋 경로("assets/…")의 CWD 수형 — r11 원문: 서버가
        // assets/backgrounds·icons를 CWD에서 못 찾음(zygote 계승 CWD).
        // MainActivity가 files/assets 트리로 추출해 둔 곳으로 chdir한다.
        if (internal) {
            std::string assetsDir = std::string(internal) + "/assets";
            ::mkdir(assetsDir.c_str(), 0700);
            if (::chdir(internal) == 0)
                std::fprintf(stderr, "[jkdesk] CWD=%s (relative assets/ 해석점)\n", internal);
        }
        // 클라 exec 흔적 — 서버 스폰 자식이 이 fd를 물려받아 같은 파일에 남는다.
        if (argc > 1 && std::strcmp(argv[1], "--client") == 0)
            std::fprintf(stderr, "[jkdesk] CLIENT MAIN REACHED app=%s — exec leg OK\n",
                         argc > 2 ? argv[2] : "?");

    // TMPDIR — 엔진 TempDir()/shm 폴백·인스턴스 락·IPC 소켓이 전부 이곳을
    // 기준으로 만들어진다. files/tmp (#126 규약 — app files dir 하위).
    if (!getenv("TMPDIR") && internal) {
        std::string tmp = std::string(internal) + "/tmp";
        ::mkdir(tmp.c_str(), 0700);
        std::string fonts = std::string(internal) + "/fonts";
        ::mkdir(fonts.c_str(), 0700);
        ::setenv("TMPDIR", tmp.c_str(), 1);
        __android_log_print(ANDROID_LOG_INFO, "jksrv", "[jkdesk] TMPDIR=%s JK tmp seeded",
                            tmp.c_str());
    }

    static bool started = false;
    if (started) {
        // SDL_main 재진입(닫았다 다시 열기 — 같은 프로세스): 선행 부트의
        // leaked fd/acceptor가 있으므로(r16 원문 — 무성 return 1 사망)
        // 가드 우회 플래그를 심고 재실행한다.
        ::setenv("JK_SPIKE_NO_GUARD", "1", 1);
        std::fprintf(stderr, "[jkdesk] RE-ENTRY — guard bypass armed\n");
    }
    JKVoiceAutoturnStart();
    started = true;
    return main(argc, argv);
}
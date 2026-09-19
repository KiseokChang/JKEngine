// jkwinserver.exe — the thin window-server host (P1 ③, spec §2). The desktop
// shell stays in-process (privileged shell, spec D7) and the client host is
// jkdesktop.exe, declared here explicitly via SetClientHostExe. Window title
// must stay "JKENGINE Window Server" — probes find the server with it
// (maximize-probe lesson: FindWindow("SDL_app", title)).
#include <server/JKWindowServer.h>
#include <ipc/JKWireEndpoints.h>
#include <theme/JKTheme.h>

// SDL.h #defines main to SDL_main on Windows; we use a plain main() entry
// point like jkdesktop's main.cpp (SDL is initialized inside JKWindowServer).
#ifdef main
#undef main
#endif

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    // theme.json 프리셋 로딩 (P2 단계 2) — 파일 없으면 다크 기본값 유지
    jk::theme::loadPresetFromFile(jk::theme::DefaultThemePath());
    jk::server::JKWindowServer server;
    if (!server.Init("JKENGINE Window Server", 1280, 720)) {
        return 1;
    }
    server.SetClientHostExe("jkdesktop.exe");
    if (!server.StartAcceptor(jk::ipc::kWindowServerPipeName)) {
        return 1;   // 단일 인스턴스 가드 — 다른 서버가 파이프 점유 중
    }
    server.Run();
    return 0;
}

// [P1 — docs/108 1단계] 서버 상태 경로·지문 헬퍼 — JKWindowServer.cpp에서
// 원문 이동(동작 무변). 호출측은 별칭(같은 이름 static 래퍼)으로 승계.
#include <server/JKServerState.h>

#include <fs/JKFs.h>
#include <filesystem>

namespace jk {
namespace server {
namespace state {

std::string ExeDirNoSlash() {
#if defined(__ANDROID__)
    // T8 Step 1 — APK의 state seam 수형: exe-dir = nativeLibraryDir(읽기전용
    // 원존) → CWD(app-private files — jksrv_main 부트 원문) 상대. 소비자 =
    // settings.json·permissions·trust 등 전 state 파일 — 1자 수형.
    return std::string(".");
#endif
    const std::string exe = jk::fs::GetExecutablePath();
    const size_t cut = exe.find_last_of("\/");
    return cut == std::string::npos ? std::string() : exe.substr(0, cut);
}

std::string StateDir() {
    std::string dir = ExeDirNoSlash();
    dir += "/state";
    std::error_code dirEc;
    std::filesystem::create_directory(std::filesystem::path(dir), dirEc);
    return dir;
}

std::string SettingsKvPath() {
    const std::string dir = ExeDirNoSlash();
    return dir + "/state/settings.json";
}

std::string NotesPath() {
    const std::string dir = ExeDirNoSlash();
    return dir + "/state/notes.json";
}

bool ValidFingerprint(const std::string& fp) {
    if (fp.size() != 7 + 64 || fp.compare(0, 7, "sha256:") != 0) return false;
    for (size_t i = 7; i < fp.size(); ++i) {
        const char c = fp[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

}  // namespace state
}  // namespace server
}  // namespace jk

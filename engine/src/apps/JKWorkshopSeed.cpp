// 출하 시딩 원천 판정 (스펙 2026-10-05-slot-ship-tool §1-2/§4 — docs/74 §5
// 단 2 출하 라인). 순수 파일 판정 함수 — 창·서버 접촉 없음(headless 셀프테스트
// 1j가 직접 단언한다). 규약 3단: 외부 진실원 존재 → 무변(0), 파묻힌 SCRI
// 존재 → 그 원문으로 시딩(1), 둘 다 없음 → 템플릿(2, 현행 회귀), 쓰기 실패
// → -1. 파일 접근 3 함수: 원본: JKAppModule_script.cpp ReadTextFile /
// WriteTextFile / EnsureParentDirs — 익명 네임스페이스 소속이라 TU 밖 재용이
// 불가하므로 동일 로직을 로컬 복사한 것(소각 아님: 원본은 그대로 살아 있다).
#include "apps/JKWorkshopSeed.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace jk {
namespace {
// 원본: JKAppModule_script.cpp ReadTextFile — 읽기 실패(열기 실패)=빈, 성공=원문.
bool ReadWhole(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}
// 원본: JKAppModule_script.cpp EnsureParentDirs + WriteTextFile — 쓰기 전
// 부모 디렉터리 생성(없을 시에만), 실패=false.
bool WriteWhole(const std::string& path, const std::string& text) {
    std::filesystem::path p(path);
    std::error_code ec;
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) return false;
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(text.data(), (std::streamsize)text.size());
    return (bool)f;
}
} // namespace

// 헤더 주석의 계약 그대로 (스펙 §1-2 — 외부가 이기고, SCRI가 다음, 템플릿 마지막).
int WorkshopSeedScript(const std::string& externalPath,
                           const std::string& shippedScriptPath,
                           const std::string& templateText,
                           std::string& error) {
    std::string existing;
    if (ReadWhole(externalPath, existing)) return 0;   // 외부=B진실원 — 불요
    std::string shipped;
    if (!shippedScriptPath.empty() && ReadWhole(shippedScriptPath, shipped)) {
        if (!WriteWhole(externalPath, shipped)) {
            error = "cannot seed shipped script to '" + externalPath + "'";
            return -1;
        }
        return 1;
    }
    if (!WriteWhole(externalPath, templateText)) {
        error = "cannot seed template to '" + externalPath + "'";
        return -1;
    }
    return 2;
}
} // namespace jk
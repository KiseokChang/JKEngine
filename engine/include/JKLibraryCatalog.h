#ifndef JKLIBRARYCATALOG_H
#define JKLIBRARYCATALOG_H

// 라이브러리 카탈로그 (스펙 2026-10-06-app-library §2 — 설치 앱 3원 스캔).
//   1. .jkx 컨테이너 — apps/*.jkx, MANI 소비: name/title/capabilities/icon.
//   2. 콘솔 앱 — apps/<dir>/manifest.json(name/cmd/desc). .jkx 동명 스킵
//      (런처 ScanConsoleApps 규약 동일 — .jkx 우선).
//   3. 내장 — minesweeper·tetris(항상) + terminal: lf/hx(파일 존재 시만;
//      접미 차이 win32 .exe/posix 무접미).
// 창·imgui 무접촉 pure 스캔 — CLI(library-list)·클라 앱(jkapp_library)·
// 셀프테스트(케이스 1m)가 같은 진실원을 먹는다. 스펙 Q1 룰링상 런처
// (JKDesktopShell)는 이 카탈로그를 쓰지 않는다(접촉 0 — 접미 유지).
//
// 읽기 전용 설비(스펙 §0) — 어떤 파일도 쓰지 않는다(uninstall 스킵 결제).

#include <string>
#include <vector>

namespace jk {

enum class LibrarySource { Jkx, Console, Builtin };

struct LibraryEntry {
    std::string appName;      // 스폰 키 — launch_app {"app":...}. 콘솔 내장
                              // lf/hx는 런처 관례 "terminal:<cmdline>" 전체.
    std::string title;        // 표시명 — MANI title → 콘솔 desc → 스폰 키 순.
    std::string capabilities; // MANI capabilities 원문(""=선언 없음 — 숨기지
                              // 않는다, docs/76 배지 계약). 콘솔·내장은 "".
    std::string path;         // .jkx 절대경로 / 콘솔 dir 절대경로 / 내장 "".
    LibrarySource source = LibrarySource::Builtin; // 발견 원 — 3원(.jkx/콘솔/
                              // 내장) 표기(스펙 §2 — 라이브러리 목록의 출처 열)
    long long sizeBytes = 0;  // .jkx 파일 크기. 콘솔·내장 0.
    bool hasIcon = false;     // .jkx ICON 엔트리 존재(디코딩은 클라 몫).
    std::string manifestRaw;  // 매니페스트 원문 — .jkx=패키지 내 MANI 엔트리
                              // bytes, 콘솔=manifest.json bytes, 내장="". 스펙
                              // §3 "상세: MANI 원문·크기·경로"의 원천(클라 상세
                              // 창이 그대로 인쇄 — 가공·정규화 없음).
};

// basePath(규약: exe dir — 뒤 구분자 없음)의 apps/를 스캔. 반환 = out에 채운
// 엔트리 수. apps/ 부재 등 스캔 불가는 0을 돌려준다(오류 전파 없음 — 라이브러리
// 빈 목록이 정당한 상태). out은 지우지 않고 push_back한다.
int LibraryScan(const std::string& basePath, std::vector<LibraryEntry>& out);

} // namespace jk
#endif // JKLIBRARYCATALOG_H

#ifndef JKSERVERSTATE_H
#define JKSERVERSTATE_H

// [P1 — docs/108 1단계] 서버 상태 파일 계층 — 경로·지문 규약의 단일 TU.
// docs/106 §1-3(서버 TU 분리)의 선행 추출: HandleAgentQuery 등 허브 코드가
// 같은 TU의 파일 스코프 조력자에 직접 결합돼 있던 것을 이 모듈로 수렴한다.
// 소비 = JKWindowServer.cpp(별칭 경유 — 콜사이트 무변경)·향후 JKAgentHub.
#include <string>

namespace jk {
namespace server {
namespace state {

// exe-dir(구분자 없음). Android = CWD(".") — app-private files state seam
// (nativeLibraryDir 읽기전용 원존, jksrv_main chdir 계약 승계).
std::string ExeDirNoSlash();

// state 디렉토리(생성 보증) = ExeDirNoSlash + "/state".
std::string StateDir();

// settings.json / notes.json 경로 규약.
std::string SettingsKvPath();
std::string NotesPath();

// 지문 형식: 정확히 "sha256:" + 64 소문자 hex(로더 형식 — docs/37).
bool ValidFingerprint(const std::string& fp);

}  // namespace state
}  // namespace server
}  // namespace jk

#endif  // JKSERVERSTATE_H

#ifndef SCRIPT_JKWORKSHOPSTORE_H
#define SCRIPT_JKWORKSHOPSTORE_H

// 워크숍 단 1 파일 스토어 (docs/67 단 1, docs/60 §3 승계):
//   슬롯 = <scriptsDir>\<slot>.js (파일=진실원, 원칙 1)
//   버전 리본 = <scriptsDir>\.history\<slot>\NNNN.js (4자리 영패딩 — 사전순=
//     시간순, 엔진 최초 N세대 규약. 기존 전역 규약은 .bak 1세대
//     JKWindowServer.cpp:2620-2629; 쓰기 사다리 .new→rename를 그대로 확장)
//   마지막 슬롯 = <scriptsDir>\.current_<appName> (도트 접두 — ListSlots 비가시)
//
// windows.h-clean 헤더(docs/60 §7 세그폴트 전례): 순 STL API만. 실제
// FindFirstFileA/CreateDirectoryA는 구현 TU(JKWorkshopStore.cpp)가 스스로
// windows.h를 포함해 SDK로 호출한다 — 수기 선언 없음(FilesListOpJson
// JKWindowServer.cpp:2880 패턴).

#include <cstddef>
#include <string>
#include <vector>

namespace jk::workshop {

struct HistoryEntry {
    int gen = 0;             // NNNN.js의 NNNN — 1 = 최오소
    size_t bytes = 0;
    long long mtimeSecs = 0; // epoch 초 (FilesListOpJson 절단 규약과 동일)
};

// 슬롯명 = 파일 스템. [A-Za-z0-9_-]{1,32} — '.' 자체가 금지라 트래버설과
// .history/.current_ 예약명이 자동 차단된다.
bool IsValidSlotName(const std::string& slot);

// 파일 경로의 부모 디렉터리(구분자 제거, 구분자 없으면 ".").
std::string DirOf(const std::string& filePath);

std::string HistoryDir(const std::string& scriptsDir, const std::string& slot);

// scriptsDir의 *.js 스템 목록(이름 오름차순). .history/.current_*는 도트 접두라
// 패턴에 걸리지 않는다. 디렉터리 부재 = false(스템 없음 아님).
bool ListSlots(const std::string& scriptsDir, std::vector<std::string>& out);

bool ListHistory(const std::string& scriptsDir, const std::string& slot,
                 std::vector<HistoryEntry>& out);

// prev(덮어쓰기 직전 원문)를 다음 세대로 보관하고 세대 번호를 반환. prev 비어
// 있으면(신규 슬롯 첫 쓰기) 0 — 스냅샷 없음. 캡 20세대, 최오소 프룬.
int AppendSnapshot(const std::string& scriptsDir, const std::string& slot,
                   const std::string& prevSource);

bool ReadGen(const std::string& scriptsDir, const std::string& slot, int gen,
             std::string& out);

// 마지막 활성 슬롯 영속(.current_<appName>). 내용 = 슬롯명(공백 트림).
// appName이 빈 값이면 읽기=실패/쓰기=no-op — MANI name 없는 앱은 영속 안 함.
bool ReadCurrentSlotFile(const std::string& scriptsDir,
                         const std::string& appName, std::string& slot);
bool WriteCurrentSlotFile(const std::string& scriptsDir,
                          const std::string& appName, const std::string& slot);

} // namespace jk::workshop

#endif // SCRIPT_JKWORKSHOPSTORE_H
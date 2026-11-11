# 108 — 서버 TU 분리 설계: 에이전트 허브 추출 (2026-11-11)

docs/106 §1-3(✕ 기계적 절단 불가 판정)의 후속 — 인터페이스 설계 후 2-TU 추출
계획. 대상 = `engine/src/server/JKWindowServer.cpp` 9,649줄.

## 1. 왜 기계적 절단이 불가였나 (원문 계측)

- `HandleAgentQuery` 4062-7430행(3,369줄) — distinct 도구 분기 **44종**.
- 함수가 같은 TU의 파일 스코프 조력자를 직접 소비:
  - `JsonEsc`(:1331) — 서버 전역 JSON 이스케이프
  - `SettingsKvPath`(:3194)·`NotesPath`(:3458)·파일 허브(:3617)·`ValidFingerprint`
    (:3856) — 경로·트러스트 헬퍼
  - 사용 저널 writer·recent-event 훅(함수 말미) — 서버 전용 문맥
- 즉 "함수만 잘라 옮기기"는 링크 파산 — 의존 클러스터가 함께 가야 한다.

## 2. 목표 구조

```
server/
  JKWindowServer.{h,cpp}     — IPC 디스패치·클라이언트 표·포커스·스폰 위임만
  JKAgentHub.{h,cpp}         — HandleAgentQuery + 도구 44종 분기 + 트러스트/설정/노트/파일 허브
  JKServerServices.h         — 허브가 서버에 요구하는 최소 인터페이스
  JKServerState.h/.cpp       — 설정KV/노트/파일/저널 경로·쓰기 헬퍼(양쪽 공유)
```

### 2.1 `IWindowServerServices` (허브 → 서버 최소 표면)

```cpp
struct IWindowServerServices {
    virtual ~IWindowServerServices() = default;
    // 창 열거·포커스·스폰 — 허브가 도구 실행에 필요한 전부.
    virtual std::vector<AgentWindowRef> ListWindows() = 0;
    virtual bool  FocusWindow(uint32_t id) = 0;
    virtual bool  CloseWindow(uint32_t id) = 0;
    virtual uint32_t SpawnClient(const char* appName, bool fromJkx) = 0;
    virtual std::string StateRoot() const = 0;      // path 헬퍼 진실원
    virtual void PushAgentEvent(const char* topic, uint32_t id,
                                const void* extra) = 0;   // 기존 계약 승계
};
```

`JKWindowServer`가 이를 구현(기존 멤버 위임), 허브는 인터페이스만 소비.
**부수 이득**: 허브 단위 셀프테스트(가짜 서비스 구현으로 도구 44종 분기
전수 호출) 가능 — 현재 커버리지 0의 안드로이드/도구 분기 회귀망.

### 2.2 이전 순서 (각 단계 = 빌드+셀프테스트 게이트)

1. `JKServerState.{h,cpp}` 추출 — 경로/쓰기 헬퍼 6종 이동 + 양쪽 re-export.
   (기계적·저위험 — 여기서 `StateDir` P0 수형도 이 파일로 수렴.)
2. `IWindowServerServices` 선언 + `JKWindowServer` 구현(동작 무변).
3. `HandleAgentQuery` 몸통을 `agent_hub::DispatchTool(this, tool, args, reply)`
   자유 함수로 1차 이동(라인수 이동만 — 내부 static 의존 남은 것은 컴파일
   오류로 드러나는 대로 상태 헬퍼로 치환).
4. 허브 TU 분리 완료 후 허브 셀프테스트 1편(가짜 서비스 + 도구 분기 스모크).

## 3. 게이트 (진행 판정)

- 각 단계 후: PC 빌드 + `jkdesktop test` 993 PASS 유지 + `--version` 지문.
- 안드로이드: 단계 3 완료 시 1회 APK 배포 검증(BUILD_SHA 원문).
- 금지: 한 커밋에 2단계 이상(원인 격리 — 오늘의 문자 분절 사고 교훈).

## 4. 리스크·대안

- 리스크: 도구 분기 내부의 상태(승인 파킹·배지·타이머)가 서버 표면에 깊게
  물려 있어 인터페이스가 커질 수 있다 → 커지면 **분리 이득 < 결합 손실**
  역치에서 중단(임계 = 인터페이스 메서드 20개 초과 시 재판정).
- 대안: 허브 추출 포기 시 최소 수형 = 파일 내 섹션 주석 + `#region` 대신
  "도구 표" 자동 생성(마커 파싱)으로 탐색성만 보전 — 차선 원장.

## 5. 단계별 as-built (원문 기록)

### 5.1 1단계 — `JKServerState.{h,cpp}` (커밋 156a748)
`ExeDirNoSlash`/`StateDir`/`SettingsKvPath`/`NotesPath`/`ValidFingerprint` →
`jk::server::state` 네임스페이스. 서버 TU는 동명 static 별칭 1줄씩으로 무변경
호출 유지(`using` 선언은 네임스페이스를 지목할 수 없어 `namespace state =` 별칭).

### 5.2 2단계 — `IWindowServerServices` (커밋 4d9ccc5)
`AgentWindowRef` + `IWindowServerServices`(AgentWindowSnapshot/StateRoot/
SpawnAgentClient) 선포, `JKWindowServer`가 구현. 계획 2.1의 6메서드 중 3개만
실수요(창 포커스·닫기·이벤트 푸시는 허브 TU가 서버 멤버 직접 호출로 잔류 —
인터페이스 비대화 회피, §4 역치 20개 대비 3개).

### 5.3 3단계 — `JKAgentHub.cpp` (이 커밋)
- `HandleAgentQuery` 몸통 3,369줄(구 4044-7412) → `src/server/JKAgentHub.cpp`.
  서버 TU 9,649 → 6,192줄(−36%).
- `AgentPermRow`·`kPermMatrix` → `include/server/JKAgentPermissions.h` 공유
  헤더(단일 진실원, static 사본 = TU별 동일). **함정 원존**: 헤더를 함수 옆
  (namespace 내부)에 include하면 헤더 자신의 `namespace jk{server{`가
  `jk::server::jk::server`로 중첩 — 파일 스코프 include 필수(컴파일 원문 확인).
- 조력자 static 사본(interim): LoadSettingsKv/WriteSettingsKv/ReadNotes/
  WriteNotesFile/PruneReceipts/NotesArrayRows/WritePermissionsEntry/
  TrustRecordText/Utf8TrimTo/ValidFontScale + `state::` 경로 별칭 3종.
- 공유 수형(정의는 서버 TU 1곳, hub는 extern 선언): `JsonEsc`(익명
  네임스페이스 → extern 승격) · `WritePng`/`EncodeLayerJpegB64`/`B64Encode`
  (동일 승격) · `FilesPermRaw`/`ValidFilePath`/`Files*OpJson`/`RevokeTrustRecord`
  (static 해제) · `kReservedTopicPrefixes[7]`(완전 형 필수 — 불완전 배열형은
  range-for 불가).
- 게이트: PC 빌드 RC=0 · `jkdesktop test` 998 PASS 0 FAIL · `--version`
  4d9ccc5(빌드 지문은 커밋 전 sha — 커밋 후 재빌드 시 갱신).
- 후속(docs/106 §2-5): 조력자 사본은 통합 대상 — `jk::json::Escape`/
  `jk::fs::ReadAll` 유틸로 수렴 시 사본 소각.

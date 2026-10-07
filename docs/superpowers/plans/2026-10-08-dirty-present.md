# 더티프레젠트(부분 X11 업로드) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to execute this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 폰(서버 SW 렌더러) 프레임 업로드가 더티 rect에 비례 — idle 합성 present ~70ms → rect 사이즈 수준, 폰 idle CPU 한 자릿수.

**Architecture:** 컴포지터의 `SDL_RenderPresent` 단일 지점을 "더티 rect 수집(와이어 DirtyRect 원용+레이어 사건 보수) → SW 렌더러면 SDL_RenderReadPixels+UpdateWindowSurfaceRects 부분 업로드 → 그 밖 전체 프레젠트"로 교체. 와이어 신규 0, 클라 단독 모드 접촉 0.

**Tech Stack:** C++(jkserver 계열), SDL2(SW 렌더러·RenderReadPixels·UpdateWindowSurfaceRects), posix selftest 케이스, bash probes(wsl/폰 cpustat+[compst] 원문).

**Spec:** docs/superpowers/specs/2026-10-08-dirty-present-design.md

## Global Constraints

- **계약 무변**: 와이어 신규 완전 불요(DirtyRect[] 원용), 클라 단독 모드·Windows 렌더 경로 접촉 0, docs/78 원칙 "Windows·WSL은 ACCELERATED 유지" 승계 — SW 전용 부분 업로드.
- **fail-safe 사다리**: SW 밖 렌더러·서피스 획득 실패·rect 수집 실패 = 전부 기존 SDL_RenderPresent. 기본 회귀 없음.
- **커밋 전 rev-parse 실측, amend 금지, origin main 직행**(확립 관행).
- **가짜 결제 기록 금지** — 폰 결제는 사용자 육안 선언만.
- 캐논 계보 기록 의무(현 Win 495/WSL 472/posix 203/폰 472).
- 폰 ssh 수속 계약 (docs/81 §6 원문): PHONE_HOST 환경변수(기본값·기록 금지), `$TMPDIR`, pkill 브래킷, 원격 스크립트 자기 소각+REMNANT 검사, tar 오염 게이트(HEAD blob 스테이징 — 배포 대상 소스 더러우면 게이트).

---

### Task 1: 프레임 더티 계산기 — 순수 로직+셀프테스트

**Files:**
- Create: `engine/src/server/JKFrameDirty.cpp`
- Create: `engine/include/server/JKFrameDirty.h`
- Modify: `engine/tools/posix_selftest/main.cpp` (신설 케이스 계열 `1p`)

**Interfaces:**
- Consumes: `ipc::DirtyRect`(JKWireProtocol.h:102), `JKCompositorLayer`(dst/IsDirty).
- Produces: `jk::server::FrameDirtyAccumulator` (헤더 계약 — 구현자가 신설; 최소 계약 아래):
  - `void AddSurfaceRect(uint32_t layerId, int sw, int sh, float scaleX, float scaleY, int lx, int ly, const ipc::DirtyRect& r)` — 표면 좌표 rect → 물리 화면 rect 매핑 적립(오버플레이/화면 경계 클램프).
  - `void AddDirtyLayerRect(uint32_t layerId, const SDL_Rect& screenDst)` — 커밋 rect 부재 레이어의 dst 전체.
  - `void AddLayerMove(uint32_t layerId, const SDL_Rect& oldDst, const SDL_Rect& newDst)` — 이전∪새.
  - `void ForceFull()` — 포커스 재정렬/오버레이 훅/강제.
  - `bool TakeDirty(std::vector<SDL_Rect>& out)` — 비어 있으면 false(제시 스킵 원료), rect 합집합 병합(인접/중첩 정리), 채우고 초기화.
  - `bool IsFull()` const — ForceFull/역치 초과 판정.
  - 역치: 전체 면적 대비 누적 rect 면적 ≥ 40% → full. 스케일 매핑=반올림 좌표, 화면 경계 클램프.

**주의(레이어 사건 수집 주의):** Composite 내부 `UpdateLayerTexture`가 `ClearDirty` 후 제시되므로, 계산기 수집은 Composite 시작 시 이전 프레임 이후 쌓인 사건 원용 — 수집 모델은 구현자가 실측 후 정확 원장(레이어 dirty 플래그는 Composite 도중 소각됨 — AddDirtyLayerRect는 **제시 시점에** IsDirty가 참이었던 레이어 대상으로 Composite 내부 수집이 맞는지 T2 배선과 정합 — 아래 T2의 수집 원문과 반드시 일치).

- [ ] **Step 1:** 실패 케이스 먼저 — posix_selftest `1p` 계열 신설: ①매핑(+스케일 2.0) ②합집합 병합 ③역치(full 전환) ④빈=TakeDirty false ⑤AddLayerMove 이전∪새 ⑥ForceFull.
- [ ] **Step 2:** 3축 selftest 실측 — Windows 캐논 495+N, WSL 472+N, posix 203+N(계보 기록).
- [ ] **Step 3:** 커밋 `feat(server): 프레임 더티 계산기 (T1)`

---

### Task 2: JKCompositor 배선 — 와이어 DirtyRect 수집+부분 프레젠트+사다리

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp:2413-2425` (CommitSurface 핸들러 — DirtyRect[]를 버리지 말고 **레이어별 보류 큐로 전달**)
- Modify: `engine/src/server/JKCompositor.h`·`engine/src/server/JKCompositor.cpp:255-350` (수집+제시 교체)
- Test: `engine/tools/posix_selftest/main.cpp` (배선 케이스 — SDL 렌더 없이 가능한 수준은 T1 계열 확장으로, 렌더러 의존 본체는 수기 실측으로)

**Interfaces:**
- Consumes: Task 1 `FrameDirtyAccumulator`; 기존 `MarkDirty(client.Id())`; `UpdateLayerTexture`·`dst rect` 산식(JKCompositor.cpp:294-299 — outputScale·layer scale 포함).
- Produces: Composite의 제시 사다리 — 더티 rect 0개 = 제시 스킵(**레이어 dirty≠screen dirty 아님 — 레이어가 dirty면 rect 수집은 커밋 rect나 dst 전체 둘 중 하나로 반드시 채워진다는 배선 보증**), 있으면 부분 제시, `IsFull()`(역치 40%·ForceFull)이면 전체, SW 밖 렌더러=전체, `JK_PRESENT_FORCE_FULL=1`=전체.

**주의(함정 원장):** ① 수집은 `layersMutex_` 스코프 밖 눈금 — 합성 스레드/클라 리드 스레드 경합에 rect 큐 뮤텍스 ② `UpdateLayerTexture`가 ClearDirty와 레이어가 dirty인데 rect 미수집인 상황(이동·alpha 등 커밋 없는 변화)은 AddDirtyLayerRect dst 전체로 봉합 ③ close overlay 최대화/닫기 글리프 변화는 레이어 dst 상단 밴드 rect로 최소화(간단화: 그리기가 레이어 dst 전체면 — 전체로 보수 수용) ④ GL 렌더러에서 RenderReadPixels는 전체 전송 stall — SW 전용 게이트(SDL_GetRendererInfo)로 지킨다 ⑤ Windows 단독 모드·클라 렌더 백엔드 접촉 금지.

- [ ] **Step 1:** 실패 케이스 —posix_selftest 확장: CommitSurface DirtyRect 손질 수집 단정(레이어 표면→화면 매핑 T1 계산기 재용) — 배선 케이스가 SDL 렌더러를 요구하지 않는 형태로.
- [ ] **Step 2:** 구현 — 배선+제시 사다리+`JK_PRESENT_FORCE_FULL=1`+[compst] 확장: `[compst] layers=.. overlay=.. present=full|dirty(N)=..ms`.
- [ ] **Step 3:** 3축 selftest 실측+캐논 갱신, 커밋 `feat(server): 컴포지터 더티프레젠트 배선+사다리 (T2)` (Windows selftest로 무회귀 실측 — 캐논 유지).

---

### Task 3: WSL 서버 실측 — A/B 영수증

**Files:**
- Create: `engine/tools/probes/wsl_dirty_present.sh` (선례: docs/78 §5.7 cpustat+[compst] 원문 뽑는 라인)

**Interfaces:**
- Consumes: Task 2 배선+env 사다리; docs/72 플랜 G WSLg 부팅 원문.
- Produces: `DIRTY-VERDICT: DIRTY-OK(전 커밋치 ms 획득+idle %)`/`DIRTY-FAIL(원인)`, A/B 2회 실측 — 구판(FORCE_FULL) 현행과 신판(부분) 비교, [compst] present 수치 원문.

- [ ] **Step 1:** probe — WSLg 서버 부팅→태스크바 자동 스폰→터미널 1창(블링크 발생)→cpustat idle 10s 실측·[compst] present 라인 수집→FORCE_FULL=1 재실측(A/B)→정리. honest-fail rc=0 원칙, hard FAIL만 rc=1.
- [ ] **Step 2:** 실측 — 목표: 부분 모드 present가 전체 모드보다 실측 절감+idle % 절감(6.7% 기준선에서 감소 — 비빔면 0이 아니라 수행도 진보). 수치가 목표 미달해도 정직 원장(이유 행별).
- [ ] **Step 3:** 커밋 `test(server): WSL 더티프레젠트 A/B 실측 (T3)`

---

### Task 4: 폰 재배포+실측 결제 관문

**Files:**
- Create: `engine/tools/probes/phone_dirty_present.sh` (선례: phone_promote.sh — tar 오염 게이트 승계)

**Interfaces:**
- Consumes: Task 2/3 전부; docs/81 §6 폰 재배포 원문; docs/78 §5.7 폰 기준선(34.4%).
- Produces: 폰 cpustat idle 전후+[compst] 원문+육안 게이트 산출. 폰에 fit-scale 앱 등장 시 눈확인 유지(docs/78 함정 원장 승계 — SW 전환의 fit-scale 우려).

- [ ] **Step 1:** probe — 폰 재배포(tar 오염 게이트+REMNANT)→리빌드(9-10분)→3축 selftest 폰 축 실측(폰 472+N 계보)→cpustat+[compst] A/B(강제 환경변수 2회).
- [ ] **Step 2:** 결제 산출 — 사용자 폰 화면 육안(기존: 런처/터미널/앱 정상 표시·부분 업로드로 인한 깜빡임·끊김·찌꺼기 없음 — "정상" 선언만 결제).
- [ ] **Step 3:** 커밋 `test(server): 폰 더티프레젠트 실측 probe (T4)`

---

### Task 5: as-built docs/85 + 원장 착지

**Files:**
- Create: `docs/85_dirty_present_asbuilt.md` (docs/81 문체 계승)
- Modify: `docs/78_termux_stage_workplan.md` §5.7 후속 포인터 1-2행

**Interfaces:**
- Consumes: T1-T4 전부.
- Produces: §0 문서 체계·§1 배선 원장(커밋 계보 rev-parse 전체 SHA)·§2 캐논 표+WSL/폰 영수증(A/B 수치 2축)·§3 함정 원장(와이어 rect 폐기 실측·ClearDirty 순서·GL stall·클램프/반올림 좌표)·§4 deferred(그램파 단위 rect·X11 직접 경로·커서 그리기)·§5 사용자 결제 항목(육안만)·§6 커밋 원장.

- [ ] **Step 1:** docs/85 작성 — 커밋마다 rev-parse 실측 후 기록(전체 SHA).
- [ ] **Step 2:** 커밋 `docs(server): 더티프레젠트 as-built (T5)`

---

## 사용자 게이트 요약 (플랜 밖 정리 — 컨트롤러 운용)

1. 폰 재배포 후 사용자 육안 결제 — 화면 정상(깜빡임/찌꺼기 없음) 선언만 결제.
2. fit-scale 앱 등장 시 별도 눈확인(docs/78 승계 — 폰 SW 렌더러 선형 필터 우려).
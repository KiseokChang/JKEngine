# vplayer 재생 안정성 + 조그 휠 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** vplayer 3증상(이상한 파일 크래시/멈춤, 오디오 끊김, 시크 멈춤·틀어짐)의 근원을 제거하고 조그 노브에 마우스 휠 스크럽을 얹는다.

**Architecture:** 오픈을 워커로 이동 + interrupt_callback 타임아웃(D1); 시크 I/O를 락 밖으로 3단 분해(D3); 단일 워커 유지하되 오디오 우선 리필 정책(D2); 예외 방어막 + 에러 분류/노출. 휠 스크럽은 기존 드래그 jog 부품(SetJog/SeekScrub/디바운스) 재사용 + 400ms 아이들 릴리스(D5).

**Tech Stack:** C++20 / FFmpeg (avformat/avcodec/swresample) / SDL2 audio / Dear ImGui v1.92.9b

**Spec:** docs/superpowers/specs/2026-09-14-vplayer-stability-design.md

## Global Constraints

- **클록 도메인 불변** — 오디오 framesPlayed 마스터 + avDelay 표시 게이트 전용 유지 (스펙 D4)
- **단일 워커 유지** — 듀얼 스레드 재설계 금지, 오디오 우선은 정책으로 (스펙 D2)
- **단일 PlayerCore 구조 유지** — 파일은 ClientVPlayerApp.cpp/.h 2개만. 신규 스레드/락 도입 금지(오픈은 기존 워커 스레드에서 단계 수행)
- 시크 stage 분해 시 seekGen 검증 + 구식 폐기 (스펙 리스크 1)
- 테마 리터럴 0 (에러 상태는 기존 openError_ 의미색 경로 재사용)
- 빌드 전제 `export PATH="/c/msys64/ucrt64/bin:$PATH"`, exe 락 시 `taskkill //F //IM jkdesktop.exe`
- 실행 브랜치: main 직행. as-built는 docs/50

---

### Task 1: ① 이상한 파일 크래시/멈춤 — 비동기 오픈 + 방어막

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` + `engine/include/apps/ClientVPlayerApp.h`

**Interfaces:**
- Consumes: 기존 PlayerCore::Open(:304) / OpenPath(:662) / worker 루프(:455)
- Produces (T2/T3 소비): PlayerCore 상태 머신 확장 `opening` 상태 + `CancelOpen()`, `lastError` 분류(enum/문자열)

- [ ] **Step 1: 비동기 오픈** — PlayerCore에 `opening` 단계 도입: 워커 루프 첫 단계에서 avformat_open_input/find_stream_info/코덱 오픈 수행(기존 Open() :306-411의 디먹스/코덱 부분을 워커로 이전). UI의 OpenPath는 즉시 복귀 + "여는 중..." 상태 + 취소 버튼. AVFormatContext.interrupt_callback + 10s 타임아웃(콜백 1 반환 → AVERROR_EXIT → openError_ "열기 시간 초과/취소").
- [ ] **Step 2: 차원 상한** — 디코더 열기 전 videoW/H 각 8192, W*H*4 ≤ 256MiB 초과 시 오픈 실패 + openError_.
- [ ] **Step 3: 예외 방어막** — 워커 루프 전체 try/catch(...) → lastError + ended(terminate 방지). UI OpenPath의 new PlayerCore try/catch → openError_ "메모리 부족".
- [ ] **Step 4: 읽기 오류 분류** — av_read_frame: AVERROR_EOF만 ended, EAGAIN 재시도(소진 방지 슬립), 기타 = lastError + ended(이유 표시 — UI가 "다시 재생" 옆에 이유 표시). 디코드 오류는 연속 N(=30)건 실패 시 lastError 노출 + 카운터 리셋.
- [ ] **Step 5: 빌드 + 셀프테스트 + 실측** — exit 0 + 0 failures. 실측: 잘린 mp4/0바이트/헤더 조작본 3종 — 크래시 0 + openError_ + 취소 동작. (테스트 파일은 임시 생성 — 정상 mp4를 앞부분 잘라 만들기.)
- [ ] **Step 6: 커밋**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/include/apps/ClientVPlayerApp.h
git commit -m "fix(vplayer): async open with interrupt timeout + dimension caps + exception guards + read-error classification"
```

---

### Task 2: ③ 시크 멈춤/틀어짐 — 락 밖 시크 I/O + 실패 정정

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` + `engine/include/apps/ClientVPlayerApp.h`

**Interfaces:**
- Consumes: T1의 opening 상태 머신/lastError
- Produces: 시크 실패 시 클록 복원 + lastError

- [ ] **Step 1: stage 3단 분해** — DoSeekLockedStage1(:264)을 (a) `m` 하: 큐 플러시/클록 재기준/코덱 플러시 (b) `m` 해제: avformat_seek_file (c) 재잠금: seekGen 대조 — 구식이면 전부 폐기, 최신이면 계속(continue demux). 검증 실패 시 폐기가 클록/큐 불변식을 지키는지 추적.
- [ ] **Step 2: 시크 실패 정정** — avformat_seek_file 실패(:299): 클록 재기준 되돌림(사전 위치 복원), lastError "시크 불가 위치" 일회 표시, 폴백은 현재 위치 유지(무동작 — decode-forward 장거리 폐기 스캔은 도입하지 않음: 실패 정정이 목적).
- [ ] **Step 3: 빌드 + 셀프테스트 + 실측** — exit 0 + 0 failures. 실측: 정상 mp4에서 시크 다수(스크럽 포함) 정상, 인덱스 없는 파일(임시: mp4를 mp4 플래그로 재밍하지 말고 — 시크 실패 유발은 0바이트/잘린 파일로 대체)에서 클록/화면 정합 + 에러 표시.
- [ ] **Step 4: 커밋**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/include/apps/ClientVPlayerApp.h
git commit -m "fix(vplayer): seek I/O outside the state mutex + failed-seek clock restore (unseekable files no longer freeze/desync)"
```

---

### Task 3: ② 오디오 끊김 — 오디오 우선 리필 + 가시화

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` + `engine/include/apps/ClientVPlayerApp.h`

**Interfaces:**
- Consumes: T1/T2의 상태 머신
- Produces: 언더런 카운터(상태 텍스트 노출)

- [ ] **Step 1: 오디오 우선 리필** — 워커 루프: 링 버퍼 수위 < 저수위(200ms 상당 바이트)면 오디오 패킷을 연속 디코드·푸시(디먹스 진행)해 고수위(600ms)까지 충전, 그 사이 비디오 디코드 건너뜀. videoQ 백프레셔 대기(:511) 중에도 오디오 리필 진행. jogging 중엔 기존대로 오디오 디코드 생략(:479).
- [ ] **Step 2: 언더런 카운터 + 가시화** — AudioCallbackC 무음 분기(:446) 카운트(atomic). 상태 라인(디버그 표시 — 기존 UI 우하단 영역 재사용) "underrun: N". SDL 오디오 오픈 실패 시 상태 텍스트 "오디오 장치를 열 수 없음(무음 재생)" 노출(stderr만이던 :385 교체).
- [ ] **Step 3: 빌드 + 셀프테스트 + 실측** — exit 0 + 0 failures. 실측: 고해상도 mp4(200GANA-3420의 것) 재생 중 underrun 카운터 픽스 전/후 비교 보고, 시크 직후 오디오 회복 확인.
- [ ] **Step 4: 커밋**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/include/apps/ClientVPlayerApp.h
git commit -m "fix(vplayer): audio-first ring refill under video load + underrun counter + device-failure surfacing"
```

---

### Task 4: 조그 휠 → 마우스 휠 스크럽

**Files:**
- Modify: `engine/src/apps/ClientVPlayerApp.cpp` + `engine/include/apps/ClientVPlayerApp.h`

**Interfaces:**
- Consumes: 노브 hover(:899-900), SetJog(:255)/SeekScrub(:233)/Seek(:232), 40ms 디바운스(:948), 드래그 릴리스 경로(:916-925)
- Produces: 휠 스크럽 상태(휠-아이들 타이머)

- [ ] **Step 1: 휠 스크럽 진입** — 노브 hover에서 io.MouseWheel != 0: (재생 중이면) 드래그와 동일하게 자동 일시정지 + SetJog(true), wheelScrubbing_ 플래그, jogTarget_ += dy·sPerRev/16, 기존 40ms 디바운스 SeekScrub 재사용. 드래그와 휠이 jogTarget_/디바운스 공유(최신 승리).
- [ ] **Step 2: 아이들 릴리스** — wheelScrubbing_ 중 매 프레임 마지막 휠 입력에서 400ms 경과 시 드래그 릴리스 경로와 동일 마무리: 프레임 스냅 precision Seek + SetJog(false) + 일시정지 상태 복원. 플래그 해제.
- [ ] **Step 3: 빌드 + 셀프테스트 + 실측** — exit 0 + 0 failures. 실측: 노브 hover에서 휠 굴림 → 시간 이동, 400ms 후 스냅 + 재생 상태 복원, 드래그와 혼용 시 최신 승리.
- [ ] **Step 4: 커밋**

```bash
git add engine/src/apps/ClientVPlayerApp.cpp engine/include/apps/ClientVPlayerApp.h
git commit -m "feat(vplayer): mouse-wheel scrubbing on the jog knob (idle-timeout release, drag-path parity)"
```

---

### Task 5: 최종 게이트 (검증 전용 — 구현 금지)

**Files:** 없음 (보고서만: `.superpowers/sdd/<plan-dir>/task-5-report.md`)

- [ ] **Step 1: 풀 빌드 + mtime 게이트** — jkapp_vplayer.dll 포함 전 아티팩트 최신성.
- [ ] **Step 2:** `jkdesktop test` → 0 failures.
- [ ] **Step 3: 프로브 전량** — exit 0.
- [ ] **Step 4: e2e 실측** — 스펙 §2 전 시나리오: 이상한 파일 3종(크래시 0/취소), 오디오 underrun 비교, 시크 정합, 휠 스크럽. "여는 중" 상태 + 오류 상태 스크린샷 각 1장.
- [ ] **Step 5: 회귀** — 정상 mp4 재생/일시정지/볼륨/avDelay/jog 드래그 무훼손 확인.

---

### Task 6: 문서 산출물

**Files:**
- Create: `docs/50_vplayer_stability.md` (EOF 개행 필수)
- Modify: `docs/superpowers/specs/2026-09-14-vplayer-stability-design.md` (장부에 실행 직감 추가)

- [ ] **Step 1: docs/50** — 개요/커밋 표(실측 git log)/대응표(3증상→근원→픽스)/검증 실측/실행 직감/후속.
- [ ] **Step 2: 스펙 장부 행 추가.**
- [ ] **Step 3: 커밋**

```bash
git add docs/50_vplayer_stability.md docs/superpowers/specs/2026-09-14-vplayer-stability-design.md
git commit -m "docs: record vplayer stability execution — docs/50 + spec ledger"
```
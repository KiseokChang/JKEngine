# 클라 idle 스핀 수리 (#89) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** 타이머 틱이 렌더를 유발하는 3요소 관용구를 FrameDirty 진원으로 수리 — 폰 클라 idle CPU 100%→<5%.

**Architecture:** 게이트 수리(JKClientApplication DrainTimerChannel 비활동화)+16 ImGui 앱 관용구 조건화(Timer→더티는 내용 변화가 있을 때만)+RenderOverlay 끝 자기유지 더티를 "계속 필요" 상태로 조건화. vplayer(재생 중 유지)·terminal(이벤트 구동 — 미접촉) 예외.

**Spec:** docs/superpowers/specs/2026-10-09-client-idle-design.md

## Global Constraints

- **결정(재량·기각 가능)**: ①FrameDirty 진원 수리 ②폴백 1s+HasDirtyWindows 게이트 유지 ③무입력 커서 블링크 정지 수용.
- **대조군 보존**: terminal(7.8-8.9%)·taskbar(<4%) — 이벤트/더티 구동 원문 접촉 금지(회귀 판정의 앵커).
- vplayer: 재생 중 프레임마다 더티 유지(동영상), 비재생은 조건화.
- 캐논 계보(현 Win 569/WSL 546/posix 277/폰 546) — 신설 케이스 `2i` 계열, 계보 표기 의무.
- 커밋: git add 명시 경로만+`git diff --cached --stat` 검증+rev-parse 실측+origin main 직행+amend 금지+Co-Authored-By 트레일러.
- IP 게이트(192.168./dotted-quad 커밋 0건)·폰 ssh 수속 계약(known_hosts self-resolve·파일 기록 금지·$TMPDIR·브래킷 pkill·`< /dev/null` 금지·jkweb 절사 금지·서버 존중)+폰 캡처 ffmpeg x11grab.
- Windows 빌드 표준 2줄+`-j3`; posix 빌드 풀 로그; WSL selftest WSL 내부 리다이렉트.
- 서브에이전트 스폰 금지(구현자)·가짜 결제 금지(폰 육안은 사용자 선언만).

---

### Task 1: 게이트 수리 — 타이머 틱 비활동화

**Files:**
- Modify: `engine/src/client/JKClientApplication.cpp:309` 부근(DrainTimerChannel activity 마킹 해제 — 주석 갱신)
- Test: `engine/src/main.cpp`+`engine/tools/posix_selftest/main.cpp` — selftest `2i-a`: 타이머 틱 단독=렌더 유발 안 함(활동 게이트 원문 수형)·`2i-b`: 입력/에이전트/테마는 활동 유지 회귀
- **Interfaces:** Consumes: 기존 activity 게이트(docs/78 원문). Produces: T2 스윕과 같이 소비하는 "타이머 틱≠활동" 새 계약. **T1 단독 상태: 16앱의 Timer→frameDirty 관용구(T2 미수행)가 여전히 IsFrameDirty() 게이트를 통과 — 렌더 케이던스 불변·스핀 불변(T1 리뷰 룰링 r1). 스핀 소각 실질=T2, 폰 idle 실측은 T3(T2 이후).**
- [ ] **Step 1:** selftest 2i-a/b 먼저
- [ ] **Step 2:** 게이트 수리+주석 갱신
- [ ] **Step 3:** 3축 selftest(캐논 계보 표기)+Win 부팅 육안 스킵(리뷰어 몫)
- [ ] **Step 4:** 커밋 `fix(client): 타이머 틱 비활동화 — activity 게이트 수리 (T1)`

### Task 2: 16 ImGui 앱 관용구 스윕 — Timer 더티 조건화+자기유 소멸

**Files:**
- Modify: `engine/src/apps/Client*App.cpp` 16곳 — `Timer→frameDirty_=true` 조건화(내용 변화 있는 틱만)+`RenderOverlay` 끝 무조건 더티를 "진행 중 필요" 조건화
- gallery: 비동기 thumb 도착만 더티(스프라이크 :127 원문)·vplayer: 재생 중=프레임마다 유지·나머지 14곳: 정적 UI — Timer 더티 완전 제거(입력/에이전트 이벤트로 렌더, 슬로 변화는 폴백) — 개별 앱 실측 후 판정
- Test: selftest 쌍둥이 — `2i-c`: 스윕 후 각 앱 타이머 콜백 무더티 단정(16앱 표 기록)
- **Interfaces:** Consumes: T1 게이트(타이머 틱≠활동). Produces: idle 이벤트 부재 앱 = 무렌더(정적).
- **폰 실측 T3에서 검증할 것** — 앱별 열림 상태 렌더가 부서지면(호버 무반응 등) 그 앱은 더티 진원 회복 — 리포트에 사유.
- [ ] **Step 1:** 16앱 원문 표 작성(콜백 내용 변화 유무 판정 — 리포트)
- [ ] **Step 2:** 스윕 구현 — 1커밋(16앱+표)
- [ ] **Step 3:** 3축 selftest
- [ ] **Step 4:** 커밋 `fix(apps): 16 ImGui 앱 Timer→frameDirty 조건화 (T2)`

### Task 3: WSL+폰 실측 probe — idle 영수증

**Files:**
- Create: `engine/tools/probes/wsl_client_idle.sh`(신설 — [cpustat] 계열 실측, wsl_dirty_present.sh 선례)·`phone_client_idle.sh`(폰 축 — spike 선례 승계)
- **Interfaces:** Consumes: T1-T2 배포. Produces: idle 영수증(폰 갤러리 클라 <5%·frames/s<3·서버 존중·terminal/taskbar 비교 축).
- [ ] **Step 1:** probe 작성+WSL 실측
- [ ] **Step 2:** 폰 실측(재배포 포함)
- [ ] **Step 3:** 커밋 `test(apps): 클라 idle 실측 probe (T3)`

### Task 4: as-built docs/88

- Create: `docs/88_client_idle_asbuilt.md` (docs/87 문체) — §1 배선 원장·§2 캐논+영수증 표·§3 함정(16앱 표 포함)·§4 deferred·§5 EYES 게이트. Modify: docs/87 §4 이 라인 포인터.
- [ ] **Step 1:** docs/88 작성
- [ ] **Step 2:** 커밋 `docs(apps): 클라 idle as-built (T4)`

## 사용자 게이트 요약 (플랜 밖)
1. 폰 idle 모양+성능감 = 사용자 선언만 결제(스핀 소거 후).
2. 결정 3건 = 재량 — 기각 시 즉시 수리.
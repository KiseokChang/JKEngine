# docs/85 — 더티프레젠트(부분 X11 업로드) as-built (T1-T5, 2026-10-08)

더티프레젠트 라인(2026-10-08-dirty-present) T1-T5 원장. 계획 문서=
`docs/superpowers/plans/2026-10-08-dirty-present.md`, 스펙=
`docs/superpowers/specs/2026-10-08-dirty-present-design.md`, SDD 원장=
`.superpowers/sdd/2026-10-08-dirty-present/`(progress·task-1..5 brief/report —
워크숍 원장). 코드 계보는 git log가 진실원. 본 문서는 실측 영수증·함정·
deferred를 봉합한다.

발단(docs/78 §5.7): 폰 서버 합성 1회 = 레이어 blit 0.5ms vs **present ~70ms** —
93%가 SDL SW 렌더러 SDL_RenderPresent의 X11 **전체** 프레임 업로드. 커서
블링크 1건에 전체를 올린다. 스펙 착수 실측으로 **더티 rect가 이미 와이어에
흐른다**(`ipc::CommitSurfaceHeader.dirtyCount + DirtyRect[]`)를 확인 —
**신규 와이어 완전 불요**, 서버 폐기 지점을 수집으로 바꾸는 배선만.

## §1. 배선 원장 (T1-T4, 커밋 전체 SHA — rev-parse 실측)

| 태스크 | 커밋(SHA) | 내용 |
| --- | --- | --- |
| 스펙+플랜 | `f6cde5d5deb8212baaab65b62a285551b9862b4b` | docs(superpowers): 더티프레젠트 스펙+플랜 (2026-10-08 사용자 설계 승인) |
| T1 | `2afe5dace2fd5b9c0964a5f754fcc23011cfc8fd` | feat(server): 프레임 더티 계산기 (T1) — JKFrameDirty 순수 계산기+selftest 1p 17검정 |
| T2 | `d5390e40e50e62933b7f25c8cb515a6e68558e83` | feat(server): 컴포지터 더티프레젠트 배선+사다리 (T2) |
| T2 정정 | `df17d090595c5c026e8e27c94e084d6d3a7bb0a6` | fix(server): 컴포지터 스레드 규약 행번호 실측 정정 (T2 fix r1) |
| T2 fix r1 | `8dbb2284b1ecab283c359908286acb8b10432c7e` | fix(server): 셸 동적 드로잉 툴팁 회귀+리사이즈 ForceFull (T2 fix r1) — NT2 5건 |
| T3 | `bf4e5abe51b7c0a6b1d35fdb78465b626edd0ac6` | test(server): WSL 더티프레젠트 A/B 실측 (T3) — probe 신설, 소스 무변경 |
| T4 | `844215cb89c28fd4538ec37a3b286c31c0ef5a3c` | test(server): 폰 더티프레젠트 실측 probe (T4) |
| T5 | 본 문서를 포함하는 커밋 | docs(server): 더티프레젠트 as-built (T5) — SHA는 git log가 진실원 |

파일 축: `engine/include/server/JKFrameDirty.h`(+cpp, T1 신설)·
`engine/src/server/JKCompositor.cpp`(+h, T2 수집+제시 사다리+NotifyDynamicDraw)·
`engine/src/server/JKWindowServer.cpp`(+h, T2 커밋 수신점·ShellHost 토크백·
UpdateOutputBounds ForceFull)·`engine/src/desktop/JKDesktopShell.cpp`(+h,
T2 fix r1 툴팁 사건화)·`engine/src/main.cpp`+`engine/tools/posix_selftest/main.cpp`
(1p 계열 23검정 쌍둥이)·`engine/tools/posix_selftest/build.sh`·
`engine/CMakeLists.txt`(T1)·신규 probe
`engine/tools/probes/wsl_dirty_present.sh`·`phone_dirty_present.sh`.
**와이어 프로토콜·클라 단독 모드·Windows 렌더 경로 접촉 0** (계약 — 실측 git diff
범위 단정).

배선 계약(원장 합의 — T1/T2 정합): 수집은 **커밋 수신점 사건 원용**(Composite
시작 시 이전 제시 이후 쌓인 사건) — 레이어 dirty면 rect는 **커밋 rect 또는
dst 전체 둘 중 하나로 반드시 채워진다**. `queueCommitRects` 보류 큐(풍량 가드
2048 — 초과 시 큐 소각+ForceFull fail-safe) → Composite drain(commitRectsMutex_
스코프 닫힘 후 layersMutex_ — 락 중첩·순서 사이클 없음, 전부 Run 스레드) →
`AddSurfaceRect` 매핑. 리드 스레드는 PopMessage 큐까지만 채운다(스레드 규약
주석 — Run:911 ProcessPendingMessages·Run:824/932 Composite 실측).

## §2. 캐논 표 + 실측 영수증

### §2.1 selftest 캐논 계보

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(brief/플랜) | 495 | 472 | 203 | 472 | — |
| T1 `2afe5da` | **512** | **489** | **220** | (472) | +17 (1p-1..1p-6 계열) |
| T2 `d5390e4`→`8dbb228` | **518** | **495** | **226** | (472) | +6 (1p-7·7b·7c·8a·8b·8c — fix r1 동결) |
| T3 `bf4e5ab` | 518 | 495 | 226 | 472 | 0 (probe만 — 소스 무변경) |
| T4 `844215c` | 518 | 495 | 226 | **495** | 0 (리빌드만 — 207스텝) |

- **폰 495 = T1+T2 단정(472+17+6, 1p-PASS=23)** — `CANON-INCLUSION=
  FULL-T1-T2-FIXR1`. **주의: fix r1은 캐논 상승에 기여하지 않는다**(신설 케이스
  0 — NT2-2는 문자열만) — "fix r1은 배포 신선도 단정" 병기. 폰 캐논을
  docs에 심을 때는 이 문구로.
- 3축+폰 4축 정합, 1p-PASS=23 (posix 하네스 case 17 총계).

### §2.2 WSL 4레그 영수증 (`wsl_dirty_present.sh` — 2026-10-08 실측, T3)

| 레그 | 조건 | idle(서버/22s Δ) | [compst] present |
| --- | --- | --- | --- |
| leg0 | 기본 GL(ACCELERATED) | **6.9%** (기준선 6.7% 재현 ✓) | full 전부, mean **3.3ms** (2.8-3.8) |
| leg1 | SW 강제 + 터미널 기본 800x500 | 4.0% | **DIRTY=0 — 43.4%>40% 역치 접힘 실증** |
| legA | SW 강제 + 420x300 (13.7%) | 4.0% | **dirty(1) 전부, mean 0.30ms / max 0.40** (재실측 0.26) |
| legB | SW + 420x300 + `JK_PRESENT_FORCE_FULL=1` | 3.8% | full 전부, mean **0.70ms** / max 1.00 (재실측 0.63) |

- **A/B(동일 SW·동일 기하 — legA vs legB)**: present mean 0.70→0.30ms =
  **절감 0.40ms(57%)** — 리뷰어 원문 전체 재실행 재실측 0.63→0.26 = **59%** (동차).
  skip 혼입 0(양레그 SKIP=0).
- **WSL idle 소등은 측정 불가로 해산**: 이론 기대 = 0.40ms×2합성/s ≈ **0.008% of
  1 core** — /proc Δ 해상도(0.1pp) 아래. idle % 절감 영수증은 폰(T4) 담당
  (측정 설비로 `SDL_RENDER_DRIVER=software` 강제 — docs/78 원칙
  "Windows·WSL은 ACCELERATED 유지" 유지, env 강제는 레그 조건일 뿐).

### §2.3 폰 5레그 영수증 (`phone_dirty_present.sh` — 2026-10-08 실측, T4)

폰 Termux X11 DISPLAY=:1, 데스크톱 **1280x720 확정**(window_fullscreen 왕복
관측), 측정창 22s /proc Δ, [compst] 8합성당 1인쇄:

| 레그 | 조건 | idle 서버/클라 | [compst] present |
| --- | --- | --- | --- |
| legA | 부분 기본(출하), 800x500 | 23.7% / 11.0% | **DIRTY=0 — 역치 접힘(WSL leg1 동차)**, full mean **71.74ms** |
| legA2 | + 터미널 420x300 | **23.2%** / 7.0% | **dirty(1) 전부 — 부분 경로 실발화**, mean **68.58ms** |
| legB | FORCE_FULL=1 + 420x300 | 23.5% / 7.1% | full 전부, mean **71.20ms** |
| legFS | vector.jkx 관찰 | — | full 67.9-72.3ms (fit-scale 미재현 — 1:1 960x640) |
| final | 출하 상태(서버 UP 상시) | 23.3% / 10.7% | full mean **69.12ms** |

- **공정 A/B(legA2 vs legB)**: 71.20→68.58ms = **절감 2.62ms(4%)** — WSL 57%
  (59%)와 대조. 상세는 §3 함정 원장 #5 정직 원장.
- 게이트: `CONTAMINATION-GATE=CLEAN`(tracked-dirty-count=0)+`DEPLOY-FRESHNESS-OK`
  (13파일 크기 일치)+`COMPOSITOR-MARKER=PresentPartialSurface phone-grep-count=2`
  +`NINJA-RC=0`+`REMNANT-LS-RC=2`. PHONE_HOST 환경변수만(기본값·기록 0 —
  커밋전 IPv4 리터럴 grep 이중 게이트 0건).
- 원본 로그: `engine/tmp/phone_dirty_present.log` (438행).

## §3. 함정 원장 (T2-T4 리포트+리뷰 건단 종합 — docs/85 착지)

1. **와이어 DirtyRect 폐기→수집(승격 원본)** — 클라는 이미
   `ipc::CommitSurfaceHeader.dirtyCount + DirtyRect[]` 로 실제 dirtyRects_를
   보내고 있었다(`engine/include/ipc/JKWireProtocol.h:102-112`). 서버
   JKWindowServer.cpp:2413-2425(스펙 착수 시점 실측)는 개수만 보고 폐기
   중(MarkDirty 플래그만) — T2 배선 = 이 폐기 지점을 `QueueCommitRects` 보류
   큐로 교체. **신규 와이어 0**.
2. **클라는 항상 CommitFull(표면 전체 rect 1건)** —
   `JKClientApplication::RenderAndCommit`(JKClientApplication.cpp:864)이
   블링크 커밋도 `surface_->CommitFull()`. 즉 **부분 rect의 최소 단위 = 클라
   표면**(WSL/폰 실측 dirty(1) 상수, RECTS=표본수와 합). 셀 단위 rect는 v1
   백로그(§4). 폰 커밋 rect가 셀 단위라는 오독 방지.
3. **WSL 생산 렌더러 = GL(llvmpipe) → 부분 경로 생산 미발화** —
   `SDL_CreateRenderer(ACCELERATED|PRESENTVSYNC)`가 WSLg에서 GL로 성립 → SW
   게이트(IsSoftwareRenderer "software" 문자열 판정)가 부분 경로 봉쇄 → WSL
   leg1/leg0 원문 전부 `present=full`. **WSL 생산 이득은 이번 착지에서 0**
   (부재가 결함이 아니라 SW 전용 설계의 함의). 측정 설비로
   `SDL_RENDER_DRIVER=software` 강제(부팅+합성+부분 경로 전부 실측 OK).
4. **역치 40% = 무중첩 병합 목록 총합(합집정 아닌 병합 총합)** [NT2-2] —
   1p-8 케이스 원장: 화면 100x100·가산 5000(50x50 x2)≥4000이어도 **병합 목록
   총합 3600**(merged-bbox {0,0,60,60} — **정확 합집정 3400 대비 bbox 과대 =
   full 조기 보수 방향**)<4000 = IsFull false. 표기 계보: 원장 초판
   "합집합 3400" → T2 산술 실수 정정({30,30,50,50}은 중첩 400·합집정 4600) →
   fix r1 NT2-2 문자열 정정(**{10,10} 오프셋 정판 — 가산 5000/bbox 3600/
   합집정 3400 3산치 구분 단정**). 컨트롤러 룰링: 역치 면적 = 합집합
   (중복 가산은 rect 산개 시 full 오전환 — 스펙 의도 "40% 사이드라 전체가
   값싼 구조"와 정합).
5. **★폰 정직 원장: 부분 업로드 면적 86% 절감 → ms 절감 4%뿐**(71.20→68.58) —
   420x300(표면 13.7%) 부분 업로드가 1280x720 전체 업로드와 사실상 동가
   (표본 편차 내 소차). WSL XWayland는 면적 미비례 오버헤드 상수를 포함해도
   57%가 나왔으나, 폰(Termux X11+SDL SW)의 ~70ms는 **rect 크기 무관
   per-present X11 업로드 고정비 지배**(SDL_UpdateWindowSurfaceRects 통과 후
   X 서버 측 전체 버퍼 플러시로 추정 — 원인 판명은 다음 과제). →
   **부분 경로가 폰 idle 목표(한 자릿수)를 미실현**: idle 23.2%(부분) vs
   23.5%(전체) — dirty 경로 기여 ~0.3pp 이내. 기준선 34.4→23.3의 11.2pp는
   **조성 차**(기준선 조성은 probe 앱 시계 포함 — full 레그인 legA/legB/final도
   23.3-23.7%가 근거). **다음 소등 후보 = X11 업로드 고정비(별도 과제)** —
   XShm/전송 결로·Termux:X11 측 플러시 구조 확인이 스펙 폰 idle 목표의
   유일 실현로로 보임. DIRTY-OK 라벨은 **기계 정의 부합**(발화+절감)으로
   성립하되 "폰 idle 소등 달성"이 아니라 **효과 크기는 별기(4%)** —
   플랜 "수치 미달도 정직 원장"의 이행(가짜 결제 아님, 리뷰어 특별 검증).
6. **ClearDirty 순서(수집 시점 계약)** — Composite 내부
   `UpdateLayerTexture`가 ClearDirty 후 제시하므로 레이어 dirty 플래그를
   제시 시점에 보면 이미 소각된다. 해소: 수집은 제시 시점이 아니라 **커밋
   수신점 사건 원용** — 뒤집힌 의존이 없다. 커밋 rect 없는 dirty 레이어(이동/
   alpha 등)는 `AddDirtyLayerRect(dst 전체)`로 봉합(배선 불변).
7. **GL stall (봉합 ④)** — GL 렌더러의 SDL_RenderReadPixels는 전체 전송
   stall → 부분 경로는 **SW 렌더러 전용**(SDL_GetRendererInfo 문자열 판정,
   판정 실패=SW 밖=full). 사다리 전 단계 실패 = 전체 SDL_RenderPresent 폴백
   (기본 회귀 없음).
8. **클램프/반올림 좌표** — 스케일 매핑 = `std::lround` half-away
   (1p-1c `{3,5,4,6}`→`{5,8,6,9}` 스케일 1.5 단정), rect는 레이어 dst 절단
   (오버플레이 클램프)→화면 경계 클램프 2단. 극단: 리사이즈×툴팁 전이가
   정확히 같은 프레임에 겹치면 T1 클램프가 옛 화면 크기로 계산할 수 있으나
   NT2-4 ForceFull이 같은 프레임을 덮는다(화면 영향 없음 — fix r1 concern 1).
9. **DISPLAY_CHANGED DPI 전환(논리 크기 무변동)에서 ForceFull 미발화** [T2
   re-review minor] — UpdateOutputBounds의 강제는 논리 크기 게이트
   (`lastDesktopW_ != logW || lastDesktopH_ != logH`) 안이라 DPI 전환만의
   전환(스케일만 변동)은 rect 사건·강제 둘 다 없다. 셸 셀/배경은
   "caller clears" 전제에 rect 추적이 없다. 빈도 극히 낮음(모니터 이동·DeX
   진입) — fail-safe 미보장 케이스, 수리는 §4 deferred.
10. **셸 툴팁 사건화(NT2-1 해소 원리)** [T2 fix r1] — T2 본체의 스킵
    (TakeDirty false) 분기가 SW 게이트 앞이라 **렌더러 무관** 적용 → 서버 측
    셸 직접 드로잉(런처 툴팁 — hoverActive_ 시간 기반 전이, rect 사건 0)이
    전 축 회귀(리뷰 Important). 수리: `ShellHost.onDynamicDraw(SDL_Rect)`
    토크백 — 셸 Draw가 Composite보다 앞서 실행되므로 같은 프레임
    `NotifyDynamicDraw(rect) → AddDirtyLayerRect(0, rect)` 봉합 → TakeDirty
    사전 관문 통과 불가(전 축 도달). 전이(off→on·자리 이동·on→off —
    DrawTooltip이 실제 그린 dst 반환)만 사건, 전이 없는 프레임은 사건 0 —
    스킵 이득 유지. 가문: 테마 핫스왑 = RequestFullPresent(색 전체 재칠).
11. **leg 기하 list_windows 미검증 인쇄** [T3 minor] — probe의 레그 기하
    표기(800x500·420x300)는 window_resize 의존이며 list_windows로의 재단정
    부재 — 기하 인쇄는 조작 의도의 기록이지 독립 검증이 아님. 도구 부재/
    거부 환경에서는 legA/B hard FAIL(의도된 인프라 실패).
12. **probe 트랩 원장**(T3/T4 파스 실패-수리 계보 — 설비 신뢰성):
    ① RES 파싱 열 파열 — 한 행 다수치+접두 sed 절단 실패 → **메트릭별 분리
    행**으로 수리(판정 오류는 원문 [compst]으로 교차 검증 가능) ②
    `list_windows` 직후 빈 배열(launch ok 응답이 클라 등록보다 빠름) → 2s
    간격 5회 재시도 루프 ③ [compst] 표본 창은 22s(8합성당 1인쇄 → 10s는
    표본 3개뿐, 22s로 5-6개) ④ `tr -d ' \r'` 공백 파열 — 다치 행은
    `tr -d '\r'`만 ⑤ `.*PASS=` greedy awk가 "1p-PASS=23"행을 쳐 PASS 파괴 →
    줄 앵커+단수 행 ⑥ EOF 개행 결손(T1 F4 리뷰 렛슨 — 3회 재발) ⑦ FAIL
    경로 RES 잔존(판정 전 원문 전량 인쇄가 방어) ⑧ DIRTY-OK 라벨 = 발화+절감
    의 기계적 통과 — **효과 크기(WSL 57%/폰 4%)는 별기** ⑨ verdict printf
    슬롯/실인수 불일치(awk 치명 — 합성 재생으로 선 포착) ⑩ legB env와 기하를
    한 매개에 합침 사건(if/else 분리) ⑪ METRIC 행 인쇄는 FSTAT/DSTAT 계산
    후 ⑫ drvfs 재빌드 충돌 — pre-clean(브래킷 pkill+-9 에스컬레이션)이 빌드
    앞 ⑬ FORCE_FULL env는 프로세스 재기동 필요(static 1회 판정 — 스킵 강제는
    못한다[NT2-3], A/B 비교는 실제 제시 프레임 축으로) ⑭ REMNANT-LS-RC=2 =
    원격 스크립트 자기 소각 확인.
13. **SKIP 관측 0(정상 미재현)** — 전 레그 창에서 present=skip 0 — 블링크
    커밋 2/s가 항상 rect 보유(CommitFull)라 dirty-0 프레임이 창에 미재현.
    skip 계약(더티 0=업로드 0)은 posix 단정으로 봉합. Composite(false)
    (capture_region 리드백)는 제시 않지만 커밋 큐 drain — rect 이월 손실 0.
14. **posix 빌드 $? 파이프 함정**(레슨) — `sh build.sh | tail`의 $?는 tail의
    것(빌드 실패 위장). 풀 로그 리다이렉트+grep error가 정판. WSL selftest
    출력을 Git Bash 파이프로 빼면 조각 유실 편차 — **WSL 내부 리다이렉트**
    (T1 리뷰 함정 렛슨).
15. **[compst] layers 수치에 폰 창 크기 법칙 혼입** — legA 4.9 vs legA2 1.8
    (클라 CPU도 11.0→7.0%) — 부분 경로와 무관한 창 크기 법칙. 폰 기준선
    34.4% 대조는 **레그 조성을 명시해야** 동작.

## §4. Deferred (소등 후보 — 유예 라인)

| ID | 사항 | 근거 |
| --- | --- | --- |
| D1 | **X11 업로드 고정비 소거**(폰 present ~70ms = per-present 상수 — XShm/전송 결로·Termux:X11 플러시 구조 판명) | §3 #5 — 폰 idle 한 자릿수 유일 실현로, **다음 소등 후보(별도 과제 권고)** |
| D2 | 그램파(셀) 단위 rect — CommitFull(클라 표면)보다 촘촘한 단위 | §3 #2 — 터미널 셀 단위는 v1 백로그(스펙 결정 5, YAGNI) |
| D3 | 커서 그리기·블링크 rect 수준 최적화 | 스펙 범위 밖 — D2와 동일 계열 |
| D4 | X11 직접 경로(XShm 등) — SDL 계약 밖 | 스펙 범위 밖(D1와 합침 가능) |
| D5 | GL partial present(GL texture 업로드는 전체가 값싼 구조) | 스펙 범위 밖 — SW 전용 설계에 흡수 |
| D6 | DISPLAY_CHANGED DPI 전환 ForceFull 1발(논리 크기 무변동 케이스) | §3 #9 — 빈도 극히 낮음, 관용 수용도 성립 |
| D7 | WSL SW 전환 검토(생산 idle 6.9% 소등 — 렌더러 교체 이득, 더티프레젠트와 별개) | §3 #3+T3 발견 원장 5 |
| D8 | fit-scale×더티프레젠트 상호작용 관측 | T4 발견 원장 7 — fit-scale 앱 개기 기회에 별도 관측(docs/78 눈확인 승계) |

## §5. 사용자 결제 게이트 (대기 — 기록은 사용자 선언만)

폰은 T4 probe가 다음 상태로 두어졌다(서버 pid 20037 UP + 터미널 800x500 +
태스크바 — 부분 출하 기본 구성). **미결 — 결제 대기**:

- 육안 항목: ①런처/태스크바/터미널 정상 표시 ②커서 블링크 정상
  (800x500은 역치 접힘 full — 420x300 축소 시 부분) ③부분 업로드로 인한
  깜빡임·끊김·찌꺼기 없음 — **"정상" 선언만 결제**.
- fit-scale 앱 등장 시 별도 눈확인(docs/78 승계 — 폰 SW 렌더러 선형 필터
  우려, T4는 미실측 §4 D8).
- probe/본 원장은 결제를 기록하지 않는다 — 사용자 육안 선언으로만 채운다.
- 참고: T4 pre-clean이 pre-T1 구 서버·폰 창 전량을 소각(ETXTBSY 리링크
  필수) — jkweb만 승격 라인 사용자 결제 창 계약 존중 생존.

## §6. 커밋 원장 (rev-parse 전체 SHA)

T1-T4 전체 SHA는 §1 표(rev-parse 실측). T5 커밋은 본 문서+docs/78 §5.7
후속 포인터를 포함하는 커밋 `docs(server): 더티프레젠트 as-built (T5)` —
SHA는 git log가 진실원. 커밋전 IP grep 게이트(본 커밋 신설/수정 행 4-옥텟
패턴) 0건 예정 — PHONE_HOST 환경변수 화법만(기본값·기록 금지, docs/84 §6
스윕 계약 "커밋 스코프 행 전수" 한정 승계).
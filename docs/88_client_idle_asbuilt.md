# docs/88 — 클라 idle 스핀 수리 as-built (T1-T3+fix, 2026-10-09)

클라 idle 스핀 라인(#89) T1-T3 원장(자기 fix r2 포함). 계획 문서=
`docs/superpowers/plans/2026-10-09-client-idle.md`, 스펙=
`docs/superpowers/specs/2026-10-09-client-idle-design.md`, SDD 원장=
`.superpowers/sdd/2026-10-09-client-idle/`(progress·task-1..4 brief/report/review —
워크숍 원장). 결함 발견 원장=진단 스파이크
`.superpowers/sdd/2026-10-09-clt-spin/spike-report.md`(D1 수리 리뷰 I-2 승계).
코드 계보는 git log가 진실원. 본 문서는 실측 영수증·함정·deferred·사용자 결제
게이트를 봉합한다.

**부기(본 문서 작성 시점)** — T4는 ruling spd r1에 따라 T3 리뷰와 병행 작성됐다
(리뷰어 직접 실측과 docs 서술은 원천 독립 — 라인 폐곡은 둘 다 있어야). 초고
작성 시작 시 `task-3-review.md`가 미출하(18:04 부재 확인 — 3분 간격 폴링)였고
**커밋 전 최종 확인(18:16)에 출하**를 확인해 **verdict를 본 문서에 반영했다**:
T3 리뷰 **계약 PASS·품질 PASS — Critical 0·Important 1·Minor 6**. 리뷰어 독립
재실측(WSL 재실행 `WSL-IDLE-OK` 0.47%·0.00fps 재현 · WSL 565·posix 재빌드
18:07 296 PASS · 폰 런 원문 판독)+룰링(계획 밖 fix(client) 커밋 정당 — §3 #3)
을 docs 표기에 흡수했고, Important 1(REMNANT 검사 미게이트 — 런 무해)·
Minor 6건은 §4.1 원장으로 승계했다.

## §0. 문서 체계

docs/85(더티프레젠트 as-built)·docs/86(텍스트 스케일 as-built)·docs/87(갤러리
as-built)의 §0-§6 구조를 계승한다 — §1 배선 원장(rev-parse 전체 SHA)·§2 캐논 표+
idle 영수증 표·§3 함정 원장(정직 기록 축 포함)·§4 deferred+park(§4.1)·§5 사용자
결제 게이트(EYES-PENDING)·§6 커밋 원장(IP grep 게이트 병기). 창작·추정 수치
소각 — 모든 SHA·캐논·측정치는 SDD 원장 리포트/review/spike 원문에서 인용했고,
본 문서 작성 시점(T4)에 새로 실측한 것만 "T4 실측"으로 부기한다.

발단: docs/85 D1 수리 리뷰 I-2 — 폰 gallery 클라 95.9% CPU 스핀(서버 present
소거 후에도 유지 — spike §1 재측정 100.1%로 확정). 원인 확정을 진단 스파이크
별도 문(clt-spin)으로 수행 → **3요소 관용구**가 docs/78 activity 게이트를
무력화(§3 #1). 스펙 원리=「레트야 하는 이유가 있을 때만 그린다」 — 타이머 틱은
배송이지 활동이 아니다. 스펙 결정 3건(①FrameDirty 진원 수리 확정 — 타이머 간격
승격·페이싱 수리는 폰에서 무효 ②폴백 1s+HasDirtyWindows 게이트 유지 ③커서
블링크 무입력 정지 수용)=컨트롤러 재량 확정(사용자 "재량껏 쭉쭉 진행하라" 지시,
2026-10-09 — 기각 시 수리 커밋 계약). worktree 미사용·main 직행·서브에이전트
스폰 0건.

## §1. 배선 원장 (라인 전체 — rev-parse 전체 SHA 실측, T4 재측정)

| 태스크 | 커밋(SHA — T4 실측 `git rev-parse`) | 내용 |
| --- | --- | --- |
| 직전 라인 종착(#85 D1 소거 상태 갱신) | `bae8d510fa6f0f53218843997649f229eeec9e06` | docs(server): D1 소거 완료 상태 갱신 — 힌트 배선+폰 present -90.9% (D1 review I-1/I-2 부기) |
| 스펙+플랜(라인 BASE) | `8e9a5f87a27641001b2978b5646b320e194920a9` | docs(apps): 클라 idle 스핀 수리 스펙+플랜 (#89 — FrameDirty 진원) |
| T1 | `059b54fdc8bd3ec026568ad3be45fc60f677788b` | fix(client): 타이머 틱 비활동화 — activity 게이트 수리 (T1) |
| 플랜 정정(ruling r2) | `7b66d9e1a7a4b604affd0562944595e7c0621e97` | docs(apps): #89 플랜 T1/T2 인과 표기 정정 — T1 단독 케이던스 불변 (ruling r1) |
| T2 | `0a276e1568610e695f18100988e29767d520d65b` | fix(apps): 16 ImGui 앱 Timer→frameDirty 조건화 (T2) |
| T2 fix r1 | `f8eeb435efcf2833da827b98e943ba0b0a6554b4` | fix(apps): palette·filedlg 입력 경로 복원 — Timer 훅 스윕 동봉 삭제 수리 (T2 fix r1) |
| T1 fix r2(계획 밖 — T3 발견) | `f341f0021d5529058437ffe5e635b2902cd27c79` | fix(client): idle 게이트 호출부 이중부정 수리 — 매틱 렌더 원장 (T1 fix r2) |
| T3 | `52699b87b1fa0a0a434ccacef96c7d2726ce5df1` | test(apps): 클라 idle 실측 probe (T3) |
| T4 | 본 문서를 포함하는 커밋 | docs(apps): 클라 idle as-built (T4) — SHA는 git log가 진실원 |

폐곡 계보: T1 리뷰 **APPROVE**(C0/I0/M5) → 플랜 정정 커밋 7b66d9e(룰링 r2 —
"T2 전까지 16앱 폴백 후퇴" 문형 교정: 실제는 T1 단독 케이던스 불변·스핀 소각
실질=T2) → T2 리뷰 **REJECT**(Critical 1 — 동봉 삭제 §3 #2) → T2 fix r1
re-review **합격**(3건 전부 실측 해소·신규 파손 0) → T3(폰 본판정 IDLE-OK) →
T3 리뷰 **계약 PASS·품질 PASS**(C0/I1/M6 — I1 = REMNANT 검사 미게이트·런
무해·§4.1 park; 룰링 = 계획 밖 f341f00 정당 §3 #3). T1 fix r2(f341f00)는 T3
probe 실측 도중 발견된 계획 밖 결함 수형 — T1의 2차 fix이며 커밋 순서상 T3
블록 내에 위치하고, **플랜 Task 3 스코프 밖 터미널 엔트리**인 것을 T3 리뷰
룰링 §4가 명문화했다(as-built 반영 = 본 §1 표의 "T1 fix r2(계획 밖)" 행).

## §2. 캐논 표 + idle 영수증

### §2.1 selftest 캐논 계보 (2i 계열 — Win/WSL/posix/폰)

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#87 갤러리 종착 = docs/87 최신) | 569 | 546 | 277 | (546) | — |
| T1 `059b54f` | **579** | **556** | **287** | (546) | +10 (2i-a×5+2i-b×5) |
| T2 `0a276e1` | **588** | **565** | **296** | (546) | +9 (2i-c) |
| T2 fix r1 `f8eeb43` | 588 | 565 | 296 | (546) | 0 (유지) |
| T1 fix r2 `f341f00` | 588 | 565 | 296 | (546) | 0 (케이스 신설 없음) |
| T3 `52699b8` (폰 축) | 588 | 565 | 296 | **565** | 폰 = 546+2i 19 |

- (표기=갤러리 종착 기준치 — 최종 폰 캐논 565=506+2g 40+2i 19, §2 계보 표 참조)
- **2i 19건 = 라인 전체 신설**(2i-a 5+2i-b 5 = T1·2i-c 9 = T2). 폰 565 = 텍스트
  스케일 506+갤러리 2g 40+2i 19 = 산치 등호(T3 원장 `CANON-INCLUSION=
  FULL-T1-T2-FIXR1`·래더 대조: pre-T1 546·T1만 556 — T3 리포트).
- T3 리포트 표기의 "폰 selftest 565 PASS"가 폰 축 최신 캐논이다(브리프 표기
  "폰 546"은 갤러리 종착 기준치 — 본 표가 계보를 봉합).
- T4에서 새 캐논 재실행하지 않음(docs 전용 커밋 — 52699b8 이후
  `engine/src|engine/include` 변경 0건 예정).
- **실측 분기 원장(T2/T3 리뷰+T3 리뷰 Minor 1)** — fix r2(f341f00) 이후 재실측은
  WSL 565(run2)·폰 565·**T3 리뷰어 독립 재실측(WSL 재실행 565+posix 재빌드
  18:07 296 — 수치 자체 확정)**. Win 588은 fix r2 이후 재실행 로그 부재 — T2
  시점 수치의 상각(리뷰 관행 — "Win 588 재실행 미수행"과 동급 서술).

### §2.2 idle 영수증 (본판정 = 폰 — 스펙 하단 계약)

**스핀 진행 상태(수리 전)**: 폰 갤러리 클라 **100.1~102.6%**(메인 스레드
단독 — 스레드 귀속 실측)·**19fps 무변화 풀코어**·프레임 작업 ≈52ms>16ms →
페이싱 `SDL_Delay(1)` 폴백 붕괴(spike §1). 서버 동반 idle 12.6% →
34.6~35.4%(CommitFull 19회/s 합성 유발 — D1 힌트 후에도). 스펙 발단 수치
95.9%는 D1 리뷰 계측 값 — spike 재측정이 더 크게 나와 원장에 병기. 대조군
정상: terminal 7.8~8.9%·taskbar <4%.

**T3 수리 후 본판정**: `PHONE-IDLE-VERDICT: IDLE-OK (갤러리 클라 0.93% < 5% ·
0.00fps < 3 — 스펙 하단 충족)` — **폰 갤러리 클라 100.1~102.6%·19fps →
0.93%·0.00fps**. WSL 동형 1독: `WSL-IDLE-OK (클라 0.53% < 5% · 0.00fps < 3)`,
서버 idle 합성 0(`[compst]` 0행) — **T3 리뷰어 재실행 재현 0.47%·0.00fps
IDLE-OK**. **회귀 없음**: 폰 terminal 9.00%·2.08fps(blink 유계 — 앵커
7.8~8.9% 이내±)·폰 taskbar 3.50%·0.00fps — 대조군 스펙 대조 통과. 폰
selftest 565 PASS/2i 19건·폰 트리 전수 sweep 복구 후 `SWEEP-DIFF=0 / 313`.

| leg | WSL 전(수리 전 f8eeb43) | WSL 후(f341f00) | 폰 후(f341f00) |
|---|---|---|---|
| base taskbar | 13.67%·62.83fps | 1.75%·0.00fps | 3.50%·0.00fps |
| **gallery 본판** | **80.07%·124.73fps** | **0.53%·0.00fps** | **0.93%·0.00fps** |
| terminal(대조) | 36.00%·125.33fps | 1.50%·1.92fps(blink 유계) | 9.00%·2.08fps(blink 유계) |
| notify 토스트 | 56.65%·125.10fps | 7.10%·6.35fps 유계 | 10.75%·1.80fps 유계 |
| filedlg | 59.67%·125.17fps | 0.67%·0.00fps | 1.00%·0.00fps |
| palette idle | paletteIdle leg 49.80%·124.90fps(run1 — T2 이전 관용구 유가정보) | 0.60%·0.00fps | 1.10%·0.00fps |
| imguidemo | (leg 없음) | (별도 leg 없음) | 74.50%·5.42fps(§4 유계 관찰) |

- "WSL 전"은 T3 probe의 변경 전 원문(
  `engine/tmp/cltidle_wsl_run.log` — `IDLE-VERDICT: WSL-IDLE-FAIL(클라
  80.07%·124.73fps)`): 이 시점은 T1+T2가 적용된 f8eeb43 위 게이트 호출부
  이중부정이 살아 있는 상태라 idle 클라가 매 틱 렌더(§3 #3 — T1 fix r2 원인)
  이다. 폰 "전" 100.1~102.6%·19fps는 spike 계측(docs/78 게이트 시대)이라
  열 근거가 다르다(전자=이중부정, 후자=3요소 관용구) — 표를 수치 나열로만
  쓰지 말고 각 열의 계측 커밋 기점을 따져 볼 것(본 문서 §2.2/§2.3이 봉합).
  정밀 부기(T4 로그 재판독): 표의 base taskbar **13.67%**는 base leg 독립
  측정이고, 갤러리 leg **동반** taskbar는 16.80%(전행 62fps — T3 리뷰가
  인용한 16.80%·62.83fps는 leg 혼성 — 스핀 동반 여파라 서로 정합).
- FRAMES-SEQ 귀속: base 태스크바 idle이 전행 0(WSL·폰 쌍) → 이후 창 frames>0
  행을 열어둔 앱 귀속으로 단정 — terminal blink "2 0 2 0 1 0 2 0…", notify
  토스트 21/62/41(WSL)·8/18/10(폰·총36프레임 버스트) 후 정지(토스터 FPS 정지
  = 수형 — 페이드 구간만 유계).
- probe 파일: `engine/tools/probes/wsl_client_idle.sh`(303행)·
  `phone_client_idle.sh`(453행) — wc T4 실측(T3 리포트 표기 304/454와 1행 차,
  미정검 부기). 구동 로그 `cltidle_wsl_run.log`·`cltidle_wsl_run2.log`·폰 런
  원문은 engine/tmp untracked(커밋 금지 — §6).

### §2.3 스핀 발견 원장 (진단 스파이크 — 3요소 관용구)

`.superpowers/sdd/2026-10-09-clt-spin/spike-report.md` — D1 리뷰 I-2 승계 진단.
3요소 관용구:

1. `JKClientApplication.cpp:309 DrainTimerChannel` — 소비 수>0을 활동으로
   계수 → 타이머 틱 그 자체가 렌더 유발(spike §1a).
2. 16개 ImGui 앱 `PreProcessMessage`: `Timer 이벤트→frameDirty_=true`(16곳
   `SetTimerInterval` grep 실측 — gallery :100-102 등, spike §2). 앱별
   조건화/삭제/폴백 분리 처분 상세 = §3 표 A.
3. `RenderOverlay` 끝 무조건 `frameDirty_=true`(gallery :127) — 렌더 후에도
   더티 유지(자기유지).

기계적 붕괴 지점: `JKClientApplication.cpp:435-436` 페이싱 — 폰에서
작업≈52ms → `SDL_Delay(1)` 폴백 페이싱이 52ms>16ms에서 붕괴 → 루프
주기≈53ms·CPU≈98-100%(스파이크 원장 정합). D1 이후 빌드에서도 스핀 유지(100.1%) — **D1 폴백 힌트는 이 결함에
무영향**. 원진 분해의 [idletrace] 원문은 WSL /tmp 측 소각으로 미보존(T3 리뷰
Minor 5 — 스크립트 idle_diag4b/4.sh만 보존 — §4.1·후속 원장 관행: /tmp 대항
로그는 스크래치 이동 기록 권장).

**D1 전후 접측(docs/85 §4)** — D1(3f76f21·bae8d51)은 **서버 present 경로의
소거**(폰 present 71.74→6.52ms, −90.9% — SDL 프레임버퍼 폴백 경유 X11
업로드 고정비)이고, 본 라인은 **클라 FrameDirty 진원의 소거**다. D1에도 폰
갤러리 클라 CPU는 해소되지 않은 것(I-2)이 본 라인의 출발점 — 스파이크 단정
"D1 폴백 힌트는 무영향"과 정합. **두 소거의 합**이 폰 CPU 청구의 완결:
D1=1프레임 present 비용, #89=프레임 빈도 자체(19fps 무변화 풀코어 → 0fps).
폰 서버 프로세스 CPU 청구(D1 리뷰 I-2 잔여 — 클라 스핀 소거 전 미실측 유지)
는 본 라인 후에도 미실측 승계(§4).

## §3. 함정 원장 (T1-T3 리포트+리뷰 종합 — docs/87 §3 문체 계승)

**표 A — 16 ImGui 앱별 Timer 더티 처분(T2)** — 원천:
`.superpowers/sdd/2026-10-09-client-idle/task-2-report.md` §1 표 원문 그대로
편성. 처분 구분: **조건화**(내용 변화 틱만 더티)/**삭제**(정적 의도 — Timer
더티 원 소각)/**폴백 분리**(응답 폴백을 OnIdle로 이동+수령 시 더티).

| # | 앱 | 타이머 콜백이 낳는 내용 변화 | 처분 | 코드 근거 |
|---|---|---|---|---|
| 1 | gallery | 썸네일 디코드는 BuildUi **동기**라 도착 프레임이 곧 마지막 프레임 — 유일 재요청 원은 풀 상한+동프레임 유보(AcquireThumbSlot==nullptr) | Timer 분기 삭제 + 말단 자기유지(:127)→`frameDirty_ = pendingThumbs_` | ClientGalleryApp.cpp:100(분기 소각), :136(조건화), ClientGalleryApp.h `pendingThumbs_` |
| 2 | vplayer | 재생 중 프레임 클록·비동기 열기 스피너·역재생·조그/휠 스크럽·시크 UI·극장 OSD 페이드 | Timer 분기 조건화 `FrameClockNeeded()`(순수 원문=jk::idle::Progressing) + 열기 종료 경계 래치 | ClientVPlayerApp.cpp:2115-2151, ClientVPlayerApp.h:38(:FrameClockNeeded), :95(openingLatched_), ClientIdlePolicy.h VplayerProgress |
| 3 | browser | 콜백 자체는 내용 변화 없음 — 실 내용 원=on_paint(페이지 픽셀 도착); CEF 펌프는 렌더와 무관히 돌아야 함 | Timer 더티 분기 삭제 + 펌프를 OnIdle로 분리, `g_painted` 표식 도착 프레임만 더티 | ClientBrowserApp.cpp:529(분기 소각), :46(g_painted), :256(표식), :657-666(OnIdle 펌프) |
| 4 | taskmgr | 500ms 샘플 경계의 CPU%/플롯 스크롤 — 진짜 내용 변화지만 19fps 불요 | 조건화 `listDirty_ \|\| jk::idle::SampleDue(500ms)` (~2fps) | ClientTaskmgrApp.cpp:228-235, ClientIdlePolicy.h:52 |
| 5 | notify | 토스트 페이드(마지막 2s alpha 램프) + 만료 순간 화면 복귀 1프레임; 풀알파 3s 구간은 정적 | 조건화 `jk::idle::ToastFading` + `toastLatched_` 만료 경계 1프레임 | ClientNotifyApp.cpp:86-99, :217(래치), :248(해제), ClientNotifyApp.h:75 |
| 6 | imguidemo | 패널 롤링 웨이브(sine+noise) — 60fps 프레임 클록 불요, 5fps 샘플로 충분(WX 경계) | 조건화 200ms 샘플 경계(`waveLast_`는 실제 웨이브 렌더 프레임에 갱신 — 기점=출력) | ClientImGuiDemoApp.cpp:85-89, :140(기점 갱신), ClientImGuiDemoApp.h `waveLast_` |
| 7 | shot | 없음 — 완전 정적 UI | Timer 분기 **삭제** + RenderOverlay 말단 무조건 더티 **삭제** | ClientShotApp.cpp:85 |
| 8 | snap | 없음(드래그 밴드=마우스 이동=activity) — 단 응답 폴백(캡처 결과/3s 사망선)이 마우스업 뒤에 늦게 올 수 있어 폴백 루프 유지 필요 | Timer 분기 삭제 + 말단 자기유지→`pendingQueryId_ != 0`만 | ClientSnapApp.cpp:77(분기 소각), :99(조건화) |
| 9 | files | 없음 — 에이전트 응답 수령 시에만 내용 변화 | Timer 분기 삭제 + **폴백 분리**: `PollReplies`를 RenderOverlay→`OnIdle()` 이동, 수령 시 더티 | ClientFilesApp.cpp:248 OnIdle |
| 10 | notes | 없음 — 동 files | 동 files | ClientNotesApp.cpp:217 OnIdle |
| 11 | settings | 없음 — 동 files | 동 files | ClientSettingsApp.cpp:96 OnIdle |
| 12 | chat | 없음 — PollReplies가 :214에서 이미 수령 더티 보유(보존) | Timer 분기 삭제 + OnIdle 폴백 분리 | ClientChatApp.cpp:110 OnIdle |
| 13 | library | 없음 — launchId 응답 도착 시 상태문 변화 | Timer 분기 삭제 + OnIdle 분리 + `reply.queryId==launchId_` 수령 더티 추가 | ClientLibraryApp.cpp:136 OnIdle |
| 14 | agentmgr | 없음 — 동 files | 동 files | ClientAgentMgrApp.cpp:296 OnIdle |
| 15 | palette | 없음 — 응답+피드/로그 도착 시에만 변화 | Timer 분기 삭제 + OnIdle 분리(`PumpReplies`) + `OnAgentEvent`/`AppendLog` 도착 더티 | ClientPaletteApp.cpp:71(소각), :78(OnIdle), OnAgentEvent/AppendLog 더티 |
| 16 | filedialog | 없음 — params 승인·응답 수령 시에만 변화 | Timer 분기 삭제 + OnIdle 분리(`RequestParams`+`PumpReplies`, 승인 시 `requesterConnId_` 더티) | ClientFileDialogApp.cpp:165(소각), :172(OnIdle) |

아래 번호 항목은 함정 서사(사건·렛슨) — 앱별 처분의 근거는 위 표 A로 봉합.

1. **★3요소 관용구 — 관용구의 게이트 무력화(spike)** — docs/78 activity 게이트
   는 올바른 원리(이벤트 부재 시 렌더 스킵)였으나 관용구 3요소(§2.3)가
   wantRender를 항시 참으로 만들었다. 렛슨: **게이트는 소스 측 원을 통제하지
   못한다** — 앱 측 더티 원의 조건화(T2)가 본체. 타이머 간격 승격·페이싱
   수리(폰에서 52ms>16ms라 본질 아님)는 기각 확정(스펙 결정 1).
2. **★T2 Critical — palette·filedlg 입력 피더 동봉 삭제(REJECT — f8eeb43
   폐쇄)** — 0a276e1의 Timer 분기 소각이 `PreProcessMessage`의
   `ImGui_ImplJKWindow_ProcessJKEvent(ev);` 복원 라인을 **동봉돼 통째로
   삭제**(16앱 중 이 2곳만 0건 — 형제 14곳 보유, 리뷰 git grep 실측). 이 호출이
   앱별 ImGui 백엔드의 유일 입력 경로 → 명령 팔레트 타이핑·filedlg 모달
   조작 전멸. **렌더는 입력 activity로 1프레임씩 일어나 "화면은 그려지는데
   조작 불가"라 캐논/스크린샷 계열로 못 잡는 유형**. 수형 = 두 파일
   PreProcessMessage 선두 복원(chat 선례 동형·f8eeb43) — Timer 더티 처치는
   그대로(재조건화로 되돌리지 않음). 렛슨: **브로드 스윕 diff에서 라인 삭제는
   훅 전체를 삼킨다 — 삭제 근거와 무관한 부속 라인의 `git grep` 인벤토리
   균일성을 스윕 전후로 채점할 것**(re-review 실측 = 16앱 각 1건 과다/과소 0).
   T2 리뷰 원문 요구(palette 텍스트 입력·filedlg 열기/취소 육안 체크리스트
   추가)는 §5 ②에 승계.
3. **★T1 호출부 이중부정(계획 밖 — T3 발견, f341f00 폐쇄)** — `GateWantRender`
   7번째 매개변수는 "이미 한 프레임 그렸나"(`renderedOnce`)인데 T1 호출부가
   `!renderedOnce`를 넘겨 게이트 본문의 단수 부정과 이중 부정 → 첫 렌더
   1프레임만 안 나고 그 뒤 wantRender 항시 참 → **모든 idle 클라가 활동·더티·
   폴백 0에서 매 16ms 틱 렌더**(WSL 원문 `cltidle_wsl_run.log` — idletrace
   `[idletrace] first=0 other=62 skip=0` — fd/act/fallback 전무, 레거시 항만
   62/s → 게이트 매개변수 7번째 자리 판명). 수형 = 1토큰(호출부
   `!renderedOnce`→`renderedOnce`)+`JKActivityGate.h` 인자 연결 계약 주석.
   **렛슨: 셀프테스트 2i는 게이트를 정방향 인자로 직접 단정해 호출부만의
   결함을 못 잡는다** — 부품 단정과 호출부 배선 단정은 다른 축이고 T1 리뷰의
   호출부 동치 코드 분석도 못 잡았다(수형은 probe 실측에서만 판명).
   **T3 리뷰 1-2의 구조 원장(증명 커버리지 밖 결함)** — T1 동치 증명 자체는
   옳았다(게이트 본문 식=원문 식, 오염 아님). 결함은 **추출 경계의 와이어링**:
   원문 "첫 렌더 전"=양의 조건 `!renderedOnce`인데 매개변수 명을
   `renderedOnce`로 바꾸고 본문에서 단수 부정하며, 호출부는 원문 토큰을 낱개로
   옮겨 적었다. 못 잡은 이유 2건: ①2i 정방향 명명 인자 직단정 — 호출부 밖
   ②T1 단계 행동 마스킹 — 16앱 관용구 잔존(T2 미수행)으로 `IsFrameDirty()`
   매 틱 참 → 게이트 1항 선행 발화 → 이중부정의 행동 차 불가시(노출은 T2가
   관용구를 뜯어낸 뒤 = T3 영수증 순서). 유일 방어=f341f00의 게이트 본문
   반대 부호 계약 주석(6행) 또는 named-args 호출 — 후속 설계 후보(원장).
   **룰링(T3 리뷰 §4)**: 계획 밖 코드 커밋 정당 — ①발견 표면이 정확히 T3
   산출물 ②수리 없이는 T3 판정 자체가 "수리 전 고장 계측" ③fix(client) 1건을
   test 커밋과 분리(T2 fix r1 선례) ④원장 삼중 봉인(커밋 메시지+계약
   주석+리포트 §0). 부기(T3 리뷰 Minor 2): .cpp 호출부 원장 주석 1행이
   "1번째 인자 자리"로 오기(실제 게이트 서명 7번째 — 커밋 메시지·헤더 주석은
   정확) — 소각 배치 정정 권장(§4.1).
4. **2i-c 재유입 가드 미봉인 — park(T2 review Minor 2)** — 2i-c는 `jk::idle`
   산치(vplayer/notify/taskmgr)와 게이트 합성 2건만 단정 — "정적 앱 11곳의
   Timer 분기 부재"는 selftest가 전혀 검증하지 않는다(구현자도 git grep
   손검으로만 입증). 새 앱/템플릿이 `Timer→frameDirty_=true`를 다시 베끼면
   3축 캐논 전부 통과한다. 소스 스캔 가드(`JKEventType::Timer` 화이트리스트
   산치 — 조건화 4곳+범위 밖 목록)는 별도 문·라인 원장 park(§4.1).
5. **플랜 표기 착오 — T1 단독 케이던스 불변(룰링 r2·7b66d9e)** — 플랜/브리프의
   "T2 전까지 16앱 폴백 1s 후퇴" 문형은 코드상 오류 판명: 16 곳의
   `Timer→frameDirty_` 관용구가 남아 `IsFrameDirty()` 오버라이드가 게이트를
   통과시키므로 **T1 단독으로 16앱 렌더 케이던스 불변(스핀 불변)**. 중간 폰
   실측을 T2 전에 내면 스핀 유지가 **예정된 상태** — T3 순서 유지가 성립
   조건(T1 리뷰 1-3 실측: diff stat terminal/taskbar/apps 0변경).
6. **폰 probe sweep 결함 2건(T3 자기 결함 실측·런 중 발각·수리)** — ①sweep
   이중 축약: TREE를 `size path` 2필드로 축약해 놓고 다시 `$4/$5`를 세어 전부
   공란 → 폰 공란 수신=전행 MISSING → DIFFFILES 공란 → tar 0파일 "Cowardly
   refusing" 사망(수리: 원본 ls-tree 파싱 유지+`NF==2·형식 canary` 앞잡기)
   ②comm 정렬 규약: Git Bash sort vs 폰(Termux busybox) sort collation
   불일치 → post-sweep 306행 가짜 오차+`comm: not in sorted order`(수리: 양측
   수취 파일 **LC_ALL=C 재정렬 후** diff/comm). 복구 실측: 폰 트리가 HEAD
   대비 **282파일 낙후**(기대 34집합 밖) → D1 원장 전량 `git archive` 경로
   복구·2회차 `SWEEP-DIFF=0 / 313`+`DEPLOY-FRESHNESS-OK`.
7. **폰 축 재배포 계약(D1 원장 승계)** — 전수 sweep(313파일 HEAD blob 크기 vs
   폰 CR-stripped) → DIFF⊆기대 집합(3f76f21 이후 계보 34파일)이면 tar(`git
   show HEAD` blob 스테이징=오염 게이트)·밖이면 전량 git archive — docs/87
   §3 #11 tar 스테이징+§3 #7 병합 원복 선례 승계. PHONE_HOST 필수(주소 기록
   금지 — 본 문서에도 주소 미기록)·브래킷 pkill+-9·`< /dev/null` 금지·
   jkweb 절사 금지·원격 스크립트 자기 소각+REMNANT·RUN-DROPPED(ssh rc=255)
   재실행 안내.
8. **커밋 메시지 오타 봉합** — 52699b8 메시지 "WSP"(WSL 의미)·"本판정" 히자
   1건 — amend 금지 계약으로 그대로 봉합(본문 의미는 오차 없음).
9. **T1 리뷰 Minor 5건 — T4 재확인 실측(계약 승계)** — ①프로브 미호출
   미단정 케이스(2i probeCount==0 부재 — **재확인 T4: 여전히 없음**(
   main.cpp·posix main.cpp probeCount==1 어설션만 — grep 실측) — §4.1 park)
   ②신설 헤더 말미 개행 부재(**재확인 T4: 여전히 부재** — 꼬리 1바이트
   실측 `H`) ③`timerDelivered` 죽은 인자(설계 수용 재확인 — `(void)` 주석
   "타이머 틱은 배송 기록일 뿐" 잔존 — 5채널 고정 계약·앱 측 조건화로만
   봉함) ④선존 주석 노후 `JKClientApplication.cpp:285 "-1000
   기점(activity 게이트)"`(**재확인 T4: 원문 잔존** — 용어 소각 §4.1 park)
   ⑤리셋 상호작용 selftest 미단정 — **재확인 T4**: T2 콜백 조건화가 리셋
   분기에 닿지 않음 = T2 fix r1 re-review "신규 파손 0"+T3 폰/WSL 영수증
   (idle 0fps 안정)으로 간접 봉합, 셀프테스트 직단정 계약은 park(§4.1).
10. **폰 잔존 클라(legS 독립성)** — legD 계측에 chat 0.83%·palette 0.92%
    동행 — legS에서 연 chat이 boot pkill에서 생존(폰 pkill 경합) — 유휴
    수치로는 오효(레그 독립성 원장).

## §4. Deferred (소등 후보 — 유예 라인, docs/87 §4 문체 계승)

| ID | 사항 | 근거 |
| --- | --- | --- |
| C1 | imguidemo 74.50%·5.42fps 자기 더티 유계 | 폰 T3 관찰 — 초당 `0 5 0 0 0 5 0 0…`(~4s마다 5프레임 버스트)·폰 aarch64 프레임당 중량 데모 렌더. 데모 장면이 폴백 1s 더티 잔존 항 소비로 유지하는 **합리적 유계이나 0으로 내려가지 않는 유일 앱** — 별도 문 후속 관찰 |
| C2 | vplayer 극장 OSD 페이드 말미 잔광 ≤2% | FrameClockNeeded 페이드 창 `osdAlpha_ > 0.02f` — 마지막 소각 프레임 미렌더 가능. 육안 실질 0 — 관찰 사항 |
| C3 | 입력→렌더 유지 미증명 | 폰 permissions.json에 send_input allow 없음(`SEND-INPUT-SKIPPED: request id 확보 불가`)·palette resize는 툴 축 한계(`window_resize → {"error":"no_window"}` — 클라 pid≠창 id) → resize bounce leg는 idle 정지 재측정만 실측. 대변은 2i-b 입력 채널 활동 합산+게이트 케이스 뿐 — 승인 계약은 별도 사용자 결정 |
| C4 | 스크립트 앱 애니메이션 폴백 1s 승계 | docs/78 "setInterval 활동 만들기" 문장이 T1으로부터 무효 — Invalidate 더티 → 폴백 1s(HasDirtyWindows) 승계(간격 유지 애니메이션 최악 1s 지연) — Run() 주석 재계약 수형 기록+EYES 게이트(§5 ③) |
| C5 | 브라우저 주소줄 래그 상한 | 폴백은 `fallback && dirtyWindow()`(JKActivityGate.h:50)라 장면 더티 없으면 1s 폴백도 스킵 — **래그 상한="다음 on_paint/입력/테마 활동까지"(사실상 영구 래그)**. 실 노출 낮음(내비게이션·에러 페이지는 통상 페인트) — 1s 상한으로 기대 금지(T2 fix r1 정정 원문) |
| C6 | 커서 블링크 무입력 정지·imguidemo FPS 표시 얼음·notify 풀알파 3s 무렌더 | 관용구 소각의 정상 귀결(스펙 결정 ③ 수용) — 육안 관찰 사항(T3 폰 육안 체크리스트) |
| C7 | 폰 서버 프로세스 CPU 청구 | D1 리뷰 I-2 잔여 — 클라 스핀 소거 후에도 본 라인에서 재실측하지 않음(스펙 비-목표) — 별도 측정 시점 |
| C8 | X11 업로드 고정비 D1 백로그(서버 축) | docs/85 §4 D1 승계 — 본 라인과 무관 |

### §4.1 원장 park 목록 (리뷰 Minor 종합 — 전부 비임계)

| 출처 | 사항 | 처분 근거 |
| --- | --- | --- |
| T1 리뷰 Minor ① | 2i probeCount==0 케이스 부재 | **T4 재확인: 잔존** — 2i-c에도 신설 안 됨 — park(2i 1케이스 추가로 소각 가능) |
| T1 리뷰 Minor ② | JKActivityGate.h 말미 개행 부재 | **T4 재확인: 잔존** — park(cosmetic·fix r2가 이 파일 접촉 시 1바이트 소각) |
| T1 리뷰 Minor ③ | timerDelivered 죽은 인자 | 설계 수용 재확인 — 5채널 고정 계약·`(void)` 주석 유지 |
| T1 리뷰 Minor ④ | `:285 "-1000 기점(activity 게이트)"` 노후 용어 | **T4 재확인: 잔존** — park(§4.1 — 다음 client 라인 접촉 시 소각) |
| T1 리뷰 Minor ⑤ | 리셋 상호작용 selftest 미단정 | 간접 봉합(T2 re-review 파손 0+T3 영수증) — 셀프테스트 직단정은 park |
| T2 리뷰 Minor 1 | 죽은 이중 return true 3곳 | **f8eeb43 소각 완료** — 원장 소각 |
| T2 리뷰 Minor 2 | 2i-c 재유입 가드 미봉인 | §4.1 park — 소스 스캔 가드 별도 문(§3 #4) |
| T2 리뷰 Minor 4 | 브라우저 리사이즈 1프레임 래그 | 관찰 — 실질 무해·기록만 |
| T3 리포트 ① | 커밋 메시지 "WSP"·"本판정" 오타 | §3 #8 — amend 금지 봉합 |
| T3 probe ①② | sweep 이중 축약·comm 정렬 결함 2건 | **런 중 수리 완료**(§3 #6) — 원장 소각 |
| T3 probe ③ | 폰 트리 282파일 낙후 | **git archive 복구 완료**(SWEEP-DIFF=0/313) — 원장 소각 |
| T3 리포트 | paletteResize leg가 idle 재측정에 그침(툴 축 한계) | §4 C3 — 승계 |
| T3 리포트 | legS 잔존 클라(chat 0.83% 동행) | §3 #10 — 레그 독립성 원장 |
| T3 리뷰 I1 | phone driver REMNANT 검사 미게이트(`REMNANT-LS-RC` 기록만 — hard FAIL 서술과 불일치) | **런 무해**(소각 성공 RC=2 실측) — **T4 승계 park·소각 배치 1행 수형**(`[ RC -ne 2 ] && FAIL "REMNANT survived"`) — T1 NR1 원격 스크립트 자기 소각 마지막 관 |
| T3 리뷰 M1 | 리포트 "수리 후 캐논 3축" — Win 588·posix 296은 T2 상각 | §2.1 실측 분기 원장 — 리뷰어 재실측(WSL 565·posix 296)으로 수치 확정 |
| T3 리뷰 M2 | .cpp 호출부 주석 "1번째 인자" 오기(실제 7번째) | §3 #3 부기 — 소각 배치 정정 권장 |
| T3 리뷰 M3 | probe 신설 2파일 말미 뉴라인 부재 | JKActivityGate.h 동류 위생 — park |
| T3 리뷰 M4 | phone driver 섹션 번호 중복("2. 원격 스크립트 생성"/"2. 재배포") | 로그 판독 가해만 — park |
| T3 리뷰 M5 | [idletrace] 원문 미보존(WSL /tmp 소각) | §2.3 부기 — 후속 원장 관행: /tmp 대항 로그 스크래치 이동 기록 |
| T3 리뷰 M6 | run2 로그 훼손 1행(죽은 setsid job "Killed" 고지×CANON 행 레이스) | 로그-only — 판정 수치 오차 없음(tee+process substitution 인터리브 렛슨) |
| T3 리뷰 관찰 | fixwave_* 로그(jb/pat)는 동시 대역 타 라인 파형 | 결함 아님 — T3 산출물 무접촉 |

## §5. 사용자 결제 게이트 (EYES-PENDING 봉인)

**EYES-PENDING — 라인 최종 결제는 사용자 육안 선언만으로 성립한다. probe와
본 문서는 결제를 기록하지 않는다**(T3 원문 — 폰 본판정 IDLE-OK는 수치 영수증
라인이지 육안 선언 아님). 본 라인의 결제 상태는 **미결제(대기)**.

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **폰 idle 육안** | 갤러리 열어두고 무입력 5s 이상 — 팬/발열/화면 프레임 정지(스파이크 진원이 폰 갤러리였다 — 스펙 성공 판정의 최종 결제) | **대기(육안)** |
| ② | Windows 모양 idle | Win 축은 자동 probe 미실측 — 사용자 기동 육안: 스핀 소거 후(무입력 상태에서 케이던스 정지)·terminal blink·notify 토스트 페이드·커서 블링크(무입력 정지 수용 관찰)+**palette 텍스트 입력·filedlg 열기/취소 조작**(T2 Critical 수리 — 리뷰 원문 체크리스트 승계) | **대기(육안)** |
| ③ | 스크립트 앱 애니메이션 | setInterval 앱 1건 — 폴백 1s 승계(간격 유지 최악 1s 지연) 육안 실측(T1 리뷰 게이트 승계) | **대기(육안)** |
| ④ | Win 서버 모드 라이브 관측 | 사용자 실측 환경에서 서버 모드 라이브 관측(idle 클라 CPU·수치는 §2.2 원장 대조) | **대기(육안)** |
| ⑤ | 스펙 결정 3건 재량 | ①진원 수리 확정 ②폴백 1s+게이트 유지 ③커서 블링크 정지 수용 — 컨트롤러 재량 확정(§0). **기각 시 수리 커밋** | **대기(유효)** |

- 폰 기기 상태: probe 복원 계약 폐곡 — 종결 구성(서버 UP+터미널 상주
  `list_windows` 실측)+전수 sweep 등호 마감(SWEEP-DIFF=0/313) — 사용자 측
  잔상 없음. 결제 시점에 폰이 이 원문 상태라는 보증으로 쓴다.

## §6. 커밋 원장 + IP grep 게이트

- 커밋 체인은 §1 표(전부 T4 실측 rev-parse 전체 SHA) — bae8d51 → 8e9a5f8 →
  059b54f → 7b66d9e → 0a276e1 → f8eeb43 → f341f00 → 52699b8 → 본 T4 커밋(git
  log가 진실원). push 전부 origin main 직행·amend 없음(T3 커밋 오타 봉합
  포함 — §3 #8).
- **IP 리터럴 게이트(T4 실측)** — 라인 커밋 원문 전체(`git show`
  8e9a5f8/059b54f/7b66d9e/0a276e1/f8eeb43/f341f00/52699b8)에서 dotted-quad
  grep = **7/7 커밋 0건**(T3 리뷰 f341f00·52699b8 0건 실측 재실측 등호).
  `192.168.` 문자열은 8e9a5f8(스펙+플랜)에 1건 존재하나 **게이트 명세의 패턴
  표기 자체**(주소 아님 — 접속 정보는 PHONE_HOST 환경변수만·기록 금지 계약 —
  스크립트 표기는 자리표시·빈 기본값 fail-closed뿐). PHONE_HOST 자리표시만
  3곳(T3 리뷰 실측 동일).
- 본 T4 커밋은 docs 2파일(`docs/88_client_idle_asbuilt.md` 신설+
  `docs/87_gallery_asbuilt.md` §4 포인터 — git add 명시 경로만, #83 사건
  레슨 유지)로 제한된다. probe·로그·진단 폐기물(`engine/tmp/idle_diag*
  wsl.sh`·`cltidle_wsl_run*.log`·`JKClientApplication.cpp.bak` 등)은 untracked
  — **커밋 금지 충족**(add 경로 명시로 구조 봉합).
# 진단 보고 — docs/50 §9.6 부수 관찰 ② (프로브 자동 휠 런 한정 UI 스톨 창)

- 날짜: 2026-09-15
- 대상: vplayer (jkapp_vplayer.dll) + 클라이언트 프레임워크 (JKClientApplication::Run)
- 결론 요약: **PARTIAL** — 증상의 실체(UI 스레드 단독 블록)는 로그 산술로 확정했으나, 재현에는 실패했다(양쪽 바이너리에서 7회 이상 0건). 루트 원인 후보를 블로킹 지점별로 배제했고, 최유력 가설과 재발 시 즉시 귀속되도록 하는 감시 장치 제안을 남긴다.
- 진행 규칙 준수: PNG/이미지 파일 Read 금지(판정은 전부 로그/텍스트), Heisenberg 3단(계측 → 완전 원복 → 클린 바이너리 재실행), 커밋 없음, 작업 트리 추적 파일 기준 클린.

---

## 1. 증상 (관측 기록 그대로)

task-3 계측 빌드의 1초 윈도 카운터(`[vpt3] ro=... tim=... sync=... att=... ok=... empty=... gate=... drop=... vpush=... clkDiff=... stepAvg=... parkN=... parkMs=... parkMax=...`)에서:

- `ro=1` — RenderOverlay가 1초에 1회만 호출 (정상 48~60/s)
- `tim=71~539` — 타이머 이벤트가 한 프레임에 수십~수백 개씩 몰려 배출
- `clkDiff=1` / `stepAvg=1207~8916ms` — UI 시계 샘플이 1.2~8.9초 간격으로 스텝
- `parkMs ≈ stepAvg` — 디먹서가 백프레셔로 파킹한 시간이 스텝 폭과 일치

무입력 수동 런에서는 0건, 프로브(호버/휠/전경 전환) 자동 런에서만 발생. 스톨 행들은 조그 릴리스(정밀 시크) 직후 창에 군집했다.

## 2. 증거 (보존된 원본 로그 발췌)

원본 로그는 `engine/build/vpt3_jog.log`였으나, 이번 진단 중 프로브 재실행이 클라이언트 stderr 리다이렉트(`-RedirectStandardError`)로 파일을 절단했다(§8 위생 교훈). 아래 행은 절단 전 발췌분으로, 이 문서가 유일한 보존본이다:

```
(line 227)  ro=1 tim=265 sync=1 att=1 ok=1 empty=0 gate=0 drop=2 vpush=5 ... stepAvg=4365.4ms parkN=188 parkMs=4361.6 parkMax=50.2ms
(line 364)  ro=1 tim=164 ... stepAvg=2740.0ms
(line 450)  ro=1 tim=104 ... stepAvg=1671.8ms
(line 506)  ro=1 tim=71  ... stepAvg=1207.4ms
(line 546)  ro=11 tim=62 ... stepAvg=835.9ms      # 복구 직전 감쇠 열
(line 1040) ro=1 tim=539 ... stepAvg=8916.5ms parkN=316 parkMs=7315.8 parkMax=51.2ms
```

같은 시간대 오디오 콜백 스텝 `cbStep=46.4ms`는 연속 정상 — 오디오 시계는 멈추지 않았다.

## 3. 분석 — 증상의 실체 (확정)

`tim` 배경 산술이 블록의 소재와 길이를 정확히 말해준다:

- 레거시 타이머 스레드는 16ms 간격으로 정확히 1 이벤트/틱을 던진다(62.5/s, 밀림 보상 없음 — `engine/src/JKTimerThread.cpp`).
- 따라서 `tim=N`의 백로그 폭발 = UI 스레드가 타이머 채널을 N×16ms 동안 배출하지 않았다는 뜻. `tim=539` ≈ **8.6초**, `tim=265` ≈ 4.2초 — `stepAvg`와 일치.
- 오디오 콜백(별도 스레드)과 디코드 스레드(40~50ms 케이던스)는 정상 — 블록은 **UI 스레드 단독**.
- `parkMs ≈ stepAvg`는 원인이 아니라 결과다: UI가 `PopVideoFrame`을 안 하니 videoQ가 차고, 디먹서가 백프레셔 파킹(≤50ms 타임드 웨이트 반복)으로 그 시간을 채운다.

즉 관찰 ②의 "UI 스톨 창" = UI 스레드가 1.2~8.9초간 한 번도 루프를 돌지 못한 구간.

## 4. 배제된 후보 (근거와 함께)

| 후보 | 배제 근거 |
|---|---|
| 파이프 커밋 블로킹 | 서버가 클라이언트별 전용 리드 스레드 사용(JKClientConnection::StartReadThread) — 커밋이 서버에서 밀릴 수 없음. 8.9s×~60커밋 ≈ 24KB < 64KB 파이프 버퍼 |
| PlayerCore/링 뮤텍스 앱-사이드 데드락 | 전 cv 대기는 뮤텍스를 해제한 채 대기. SeekCommon/finishScrub/JogTo는 전부 비동기 post(블로킹 없음). 뮤텍스 홀드는 ms 스케일 |
| Read 스레드가 inputMutex_를 ReadFile에 걸쳐 홀드 | 아님 — QueueInputEvent/PollInputEvent가 push/pop에만 짧게 홀드(JKClientSurface.cpp) |
| ImGui 백엔드 블로킹 | 얇음, 클립보드는 paste 시에만 |
| 데드라인 타이머 홍수 | 클라이언트에 AddTimer 호출자 없음 |
| CPU 기아(20스레드 스피닝 부하) | 부하 하 계측 런에서 [uistall] 0건, UI ~60fps 유지 |

## 5. 재현 시도 (전부 0건)

계측 프레임워크 워치독(`engine/src/client/JKClientApplication.cpp` Run 루프에 TEMP DIAGNOSTIC 블록 — 각 반복 페이즈(timer/input/idle/render/gap) 시간 측정, 200ms 초과 시 `[uistall] iter=... ph gap=... timer=... ...` + 이벤트 톨리 행 배출, RenderAndCommit 커밋 스테이지 100ms 초과 시 `[uistall] commit comp=... overlay=... readpix=... commit=...`)을 심고:

1. 현행 바이너리 + `vptdiag_stall.ps1`(신규: 픽셀 해시 텍스트 라이브니스 샘플러) — 전향/후향 휠 + 호버 + 전경 스톰, 4라운드 × 2회
2. **시대 재현 바이너리** — `git checkout e0548a4` (task-3 시점 앱 코드: ① 결함(stale-audio 게이트) 존재 + 398fe30 역전 게이트 존재 확인) + 원본 `vpt3_pacing_jog.ps1` + 릴리스 되감기가 링 용량(~5.8s)을 넘기게 설계한 딥 백워드 휠(-90/-120)
3. 현행 바이너리 + 20스레드 CPU 스피닝 부하(`spin_load.ps1`)
4. 구(舊) 바이너리 + 프로브 직후 정지 화면 호버 방치 관찰(~2.5분)

합계 7회 이상, `[uistall]` 발화 0건. 참고: vptdiag 해시 샘플러의 라운드 3-4 "프리즈"는 가짜 양성이었다 — `[uistall]` 부재 + 하트비트로 ended=1 pos=30.000(EOF 정지 화면) 확인. 정적 화면과 스톨을 구분 못 하는 샘플러 한계로 기각.

## 6. 루트 원인 (현재 판정)

**확정**: UI 스레드 단독 1.2~8.9초 블록 (§3). 어느 페이즈가 막혔는지는 계측이 발화하지 못해 미확정.

**최유력 가설 (순서대로)**:

1. **발화 조건이 이후 픽스로 소멸** — 스톨 행은 "조그 릴리스 정밀 시크 직후" 창에 군집했고, 그 정확한 창에서 ① 결함(부실 오디오 게이트 해제 구멍, rewind 시 오디오 링 홍수 → RingPush 파킹 폭증, `parkN=316`)이 7c04ee5로 픽스됐다. park 폭증이 UI를 막은 2차 경로(예: 시크 스톰 처리 중 UI 스레드의 어느 대기)가 있었다면 픽스와 함께 사라졌을 수 있다.
2. **렌더/프레젠트 경로의 GPU/DWM 동기 스톨** — 배제 목록에 없는 유일한 블로킹 지점군. `SDL_RenderReadPixels`(GPU→CPU 다운로드, 서버 창 위 컴포지션 상태와 상관 가능)와 커밋 직전 스테이지. 계측이 발화했다면 `[uistall] commit readpix=`로 즉시 판별 가능했을 것.

재현 0건이므로 어느 쪽도 확증 아님. **상태: PARTIAL — 실체 확정, 위치 미확정.**

## 7. 수정 제안

1. **영구 저비용 워치독 (1순위)** — `JKClientApplication::Run` 루프에 반복 갭 감시(임계 500ms, 로그 전용, 페이즈 귀속: gap/timer/input/idle/render + 직전 커밋 스테이지 분해). 이번 진단의 계측 코드가 그대로 품질 게이트로 승격 가능한 형태다. 재발 시 그 자리에서 블로킹 페이즈가 자기 귀속된다 — 재현 노력의 대부분은 "어디서 막혔는지"를 몰라 소모됐다.
2. 재발 시 1번 로그의 `ph=` 값에 따라 분기: `readpix`/`commit`이면 GPU/DWM 경로(SDL_RenderReadPixels 대체: 더티 리전트 업로드 or 프레젠트 후 비동기 캡처) 조사, `idle`/`timer`면 앱 이벤트 처리 경로 조사.
3. ① 관련 잔여 레저(docs/50 §9.6 ① 이월분)와 무관하게, 조그 릴리스 창의 시크 스톰 중 UI 페이즈 갭을 한 번 더 스캔할 것.

## 8. 위생 교훈 (진단 중 발생)

- **증거 로그 선(先)복제**: 프로브는 클라이언트 stderr를 `-RedirectStandardError`로 연다 = **절단**. 이번에 원본 `vpt3_jog.log`(447행 카운터, 스톨 행 포함)가 절단되어 발췌본만 이 문서로 생존. 프로브가 같은 경로에 쓰는 증거 로그는 실행 전 복제.
- `agentctl` 1회 5분+ 행발 관찰(프로브 Find-Layer 단계). 사후 직접 테스트에선 정상 응답 — 재현 불확실, 레저 가치 있음(서버 에이전트 경로 행발).
- vptdiag 픽셀 해시 샘플러는 정지 화면(EOF/일시정지)과 스톨을 구분 못 한다 — 라이브니스 판정엔 로그 기반 하트비트가 필요.

## 9. Heisenberg 3단 기록

| 단계 | 내용 | 결과 |
|---|---|---|
| (a) 계측 빌드 | 프레임워크 워치독 빌드 + 7회 재현 시도 | 스톨 미발화 (0/7) — 위 §5 |
| (b) 완전 원복 | `git checkout HEAD -- ClientVPlayerApp.cpp ClientVPlayerApp.h JKClientApplication.cpp`, 재빌드 jkapp_vplayer + jkx_packages | `git status` 추적 파일 0건, dll 11:41:48 / jkx 11:41:58 > 소스 11:41:32 (mtime 게이트 통과) |
| (c) 클린 런 | `vpt3_pacing_jog.ps1` 1회 (클린 바이너리, 계측 없음) | "layer found" → 20 휠 틱 @150ms → 400ms 릴리스 → "DONE jog session", shot 3건 저장(11:51:06~14), 클라이언트 로그는 접속 2행뿐 이상 없음 |

클린 런 한계: 계측이 없으므로 심부 스톨 탐지는 불가 — 목적(현상이 계측물의 산물이 아님 확인 + 클린 빌드 정상 동작)은 충족. 휠 경로 유효성은 사전 계측 런들에서 동일 프루브로 조그 시크 발화를 확인한 바 있다.

## 10. 상태

- **ROOT_CAUSED 아님 / UNRESOLVED 아님 → PARTIAL**: 실체(UI 스레드 단독 블록, 길이 산술) 확정, 위치 미확정, 재현 0건.
- 커밋 없음. 추적 파일 기준 작업 트리 클린. 신규 비계측 프루브 2종(`engine/tools/probes/vptdiag_stall.ps1`, `spin_load.ps1`)과 빌드 디렉터리 스크래치 로그는 기존 프루브 무추적 관행에 준해 그대로 둠.
- 남은 레저: (1) Run 루프 영구 워치독 착수 시 이 문서의 계측 블록 재사용, (2) agentctl 행발 1건 관찰 기록, (3) 증거 로그 절단 사고의 재발 방지(프루브 stderr 리다이렉트를 append 또는 타임스탬프 파일로).
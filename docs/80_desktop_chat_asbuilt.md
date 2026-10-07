# docs/80 — 데스크톱 채팅 앱 as-built (2026-10-07)

스펙 docs/superpowers/specs/2026-10-07-desktop-chat-app-design.md(결제판 —
stub MVP·ollama 실험·중반 개편 §5 "입력=X11 밖"·룰링 3 §5.1 "폰 웹 채팅
jkweb")의 as-built 원장. 플랜:
docs/superpowers/plans/2026-10-07-desktop-chat-app.md. SDD 세션 원장:
.superpowers/sdd/2026-10-07-desktop-chat-app/pre-flight.md(T1-T9 전
dispatch·review·fix 기록). 문체·절 구성 선례: docs/79.
BASE=5074375(스펙 결제판)·24e0c54(플랜). 원장 체인: docs/77 출하 도구 →
docs/79 앱 라이브러리 → **채팅(본 문)** — jkapp_chat은 settings/library
4호 멤버 가족, jktalk·jkweb은 tools 신설 소식체.

## 0. 스펙 체인 — 결제 → 구현 대응

| 스펙 | 구현 | 비고 |
|---|---|---|
| §0 비전(DeX+폰 구두 조작 — 엔진은 텍스트→턴만) | T8 실험 결론: 구두 입력=폰 입력수단 몫(STT 미실장), 엔진은 jktalk CLI·jkweb 브라우저 2개 X11 밖 표면 | STT 파이프라인=별도 백로그 |
| §1.1 jkapp_chat(ImGui 클라 720x540) | T2 `JKAppModule_chat.cpp`+`ClientChatApp.h/.cpp`, meta `{"chat","Chat",720,540}` | 중반 개편 후 **데스크톱 UI 멤버로 유지**(룰링 1) — IME 게이트는 보조로 강등(§5) |
| §1.2 플러그형 턴 백엔드(cfg 승격) | stub 실장+슬롯 주석, ollama 실험 T8 → **stub 유지 판정**(§3) | §2-2 "cfg 승격 시 UI·배선 무수정" 불변식이 판정 기준 |
| §1.3 stub 어휘 | T1 `jk::ChatRouterRoute` — 어휘 표 14행·별명 표 4행 정적 배열 single-source | 셀프테스트 1n 17 check 잠금 |
| §1.4 IME 리스크 | T5 폰 실측 CHAT-PHONE-OK + 육안 게이트 명시(자동 어설션 불가) → 중반 개편으로 보조 게이트 | jktalk·jkweb이 본 경로 — IME 리스크 소멸(룰링 2·3) |
| §2 계약 5불변식 | 전부 성립 — 도구 직접 실행 0(서버 위임, §1)·cfg 승격 불변식(T8 판정 준수)·자기 진실원 없음(대화 기록 1개=앱 메모리)·Windows 서버 스폰 금지·권한 행렬 우회 0 | |
| §3 함정 원장 승계 | Begin/End 본문·AlwaysVerticalScrollbar·posix 브래킷 pkill·probe 수신=어설션·16ms 항시 렌더 — 전부 준수 실측 | 결함 재발 0(리뷰 Critical 0) |
| §5 개편(X11 밖 입력) | T6 `jktalk` CLI 4모드 — Termux 셸 자유 텍스트 → 라우터 → 서버 위임 | 52ca22a(사용자 결정 반영) |
| §5.1 jkweb(룰링 3, 7dfed70) | T7 `jkweb` HTTP 서버 8090 — 폰 브라우저 native 키보드, fetch POST /talk | 591b144·74b67ea·ba786b3 |
| §4 플랜 T 순서 | T1→T5 원 계획대로 랜딩, T6/T7/T8은 개편 후 재번호(ollama 실험=T8) | 컨트롤러 ledger — T7 미 dispatch 시점이라 안전 |

## 1. 배선 원장 — 라우터 단일 진실원 → 삼 소식체 + 서버 위임

- **라우터(jkcore pure, T1)**: `jk::ChatAction{Kind{Launch,Close,Focus,
  ListWindows,Info}, app}` + `ChatRouterRoute(text, out)` — 응답 반환 문자열은
  도구 지시가 아니라 **사용자 표시용 한국어 확인문**. 어휘 표(`닫아줘/꺼줘`→
  Close · `앞으로 가져와/앞으로/포커스`→Focus · `창 목록/창목록/뭐 떠 있어/
  뭐떠`→ListWindows · `열어줘/켜줘/실행해줘/실행`→Launch)·별명 표(지뢰찾기→
  minesweeper 등 4행)가 ChatRouter.cpp 상단 정적 배열 1개진. 긴 트리거 우선
  판정(1n-9b)·표 밖 앱어 fail-open(서버 존재 검증 몫, 1n-5)·`app=""` Close=
  포커스 창 위임(1n-7)·"켜줘" 단독=Info(1n-6) 전부 어설션 잠금.
  **소비자 3좌**가 같은 호출 1곳을 먹는다:
  1. `jkapp_chat` stub 백엔드(`ClientChatApp.cpp` — 데스크톱 창 Chat, T2)
  2. `jktalk` CLI(`engine/tools/jktalk/main.cpp`, T6) — 대화 REPL/원컷
     `--ask`/파이프 non-TTY/`--route` 서버무접촉 진단 4모드
  3. `jkweb` HTTP 서버(`engine/tools/jkweb/main.cpp`, T7) — POST /talk,
     브라우저 native 키보드(IME 리스크 소멸이 본 존재 이유)
- **서버 위임 계약(스펙 §2-1·§2-5)**: 세 소식체 모두 도구를 직접 실행하지
  않는다 — JKAgentClient 경로(JKWindowServerPipe)로 위임:
  - Launch → `launch_app {"app":"<escaped>"}`(JsonEsc — ClientChatApp::
    EscapeJson 규약 삼소식체 동형)
  - **Close/Focus → argless `{}`** — 서버 `close_window`(JKWindowServer.cpp
    :4074)·`focus_window`(:3505)가 **`args.id`(창 client id) 한정**이고 앱
    지명 인수가 존재하지 않기 때문(:4074 `args.id` 미지정·미상=무조건
    `window_not_found`). 라우터의 Close.app/Focus.app은 보낼 곳이 없어
    **window_not_found를 정직 표시**(T2/T6/T7 전부 동형 계약 승계 — 세 표면
    거짓 성공 없음: 앱=`[!]` 접두 기록 행, jktalk=`회신: {원문}` 인쇄,
    jkweb=fix r1 `ok:false`+`"server"` 원문 승계·err 스타일, §2.3 수락 테스트).
  - ListWindows → `list_windows {}` — 결과는 reply.json 원문 인쇄 계약.
  - Info → 서버 무접촉(안내문만 — 전송 필요한 Kind만 fail-loud).
- **백엔드 슬롯(cfg 승격 계약 — §1.2·§2-2)**: 세 소식체 전부 `JKLlmEngine`을
  인스턴스화하지 않았다(posix dead-code 룰링 — 폰에 ollama launch 통합 소유
  claude CLI 부재) — `kBackendName = "stub-router(T1 ChatRouterRoute)"`
  슬롯 주석+ProcessTurn/HandleTalk ① 교체 자리만(jktalk main.cpp:97-108,
  jkweb main.cpp:37). 백엔드 교체는 "ChatRouterRoute 호출 1자리 갈아끼우기"
  — UI·REPL·도구 전송·인쇄는 백엔드 무관. 현재 승격 판정=stub 유지(§3).
- **카탈로그/셀프테스트**: `JKLibraryCatalog` 내장 pushBuiltin("chat",
  "Chat") — Windows·WSL·폰 동일 진실원(count=30/7/4, §2). 셀프테스트 케이스
  1n(라우터 17 check)+1m-1/1m-15 내장 3원 전면화(n==6).

## 2. 실측 영수증 (전부 리포트 원문 — as-far-verified, 3축 전수)

### 2.1 Windows (T1-T3·T6·T7 — 서버 기동·기존 프로세스 킬 없음)

- 빌드 `ninja -C engine/build -j3` **rc=0**(T6 jktalk 링크 3스텝·T7 jkweb
  링크 포함). 셀프테스트 `./engine/build/jkdesktop.exe test` — **PASS 412 /
  FAIL 0** `AppSelfTest: 0 failure(s)` rc=0(1n-1..1n-13·1n-3b·1n-4a/b·
  1n-9b·1n-10b 포함 — 신규 1n은 플랜 목표 10을 17로 초과 달성).
- `library-list` **count=30**(29→30 실측 갱신 — docs 기대치 갱신 아님),
  `name=chat title=Chat source=builtin` 행. probe_library_list.ps1 4정판
  OK(BASE count=30·CAPS timer,canvas·ARG expect=5 got=5 builtins=3·CLEAN)
  — ARG 기대치 4/5 재산정은 1f3b89e가 본편으로 수행(T3 실측: stale 상수
  부재 확인, no-builtin 구설 기반 우려 해소).
- jkbridge relink 유예(T1 concern) **소각 완료** — 컨트롤러가 신규
  jkbridge.exe(08:04 기동, libjkcore.a 08:00 물음)로 8899 게이트웨이 복원.
- **jktalk 영수증**(T6): `--route "지뢰찾기 켜줘"` → `[시스템] '지뢰찾기' 앱을
  실행합니다.` + `[route] kind=Launch app=minesweeper (backend=stub-router...)
  ` **rc=0**("창 목록"→ListWindows·"메모장"→Info 폴백도 rc=0). 서버 부재
  `--ask "지뢰찾기 켜줘"` → 한국어 stderr+**rc=1 fail-loud**
  (`CreateFileA('\\.\pipe\JKWindowServerPipe') failed: 2` — 이 CLI는 서버를
  기동하지 않는다). Info 원컷은 서버 부재에서도 rc=0(도구 지시 없음 계약).
- **jkweb 영수증**(T7): `ninja` rc=0 → 기동 인쇄 URL localhost:8090(0.0.0.0
  수신) → `curl GET /` **code=200**(한국어 페이지 마커) → POST /talk →
  창 서버 부재 **정직 500**(3연발, hang 없음 — "지뢰찾기 켜줘"→Launch 판정의
  부수 증거). 빈 발화 400·미지 경로 404 부수 확인. fix r1판: `지뢰찾기 꺼줘`
  → 500이면서 `ok:false` body(§2.3 수락 테스트와 동형 — 서버 회신 정직
  승계, 리뷰 I1 소각).

### 2.2 WSL (T4 — CHAT-BOOT-OK, probe rc=0)

```
=== 1. ninja rebuild (buildwsl) ===  WSL-BUILD-RC=0
=== 2. library-list ===  rc=0 — jkx 트리거 4 + 내장 3 = count=7,
  name=chat title=Chat source=builtin  (Windows 진실원 동일행)
=== 3. boot (setsid nohup, WSLg :0) ===  server pids: 16305 16338
  ping reply: {"ok":true,"pong":true}
=== 4. launch_app {app:chat} ===  launch reply: {"ok":true}
=== 5. list_windows ===
  {"id":4,"title":"Chat",...,"w":720,"h":540,"dw":720,"dh":540,"focused":true,...}
=== 6. cleanup (pkill 브래킷) ===  CHAT-BOOT-OK / PROBE-RC=0
```

- 스캔행 계약 결정: chat은 `[chat]` stderr 스캔 행을 인쇄하지 않는다(ClientChatApp
  OnInit에 fprintf 후보 없음 — `[library] apps=%d` 선례와 다름) → brief 분기대로
  스캔행 어설션 생략+launch ok+창 기하 단정(probe 헤더에 결정 원문 기록).
  `[chat] apps=…` 실장은 후속 과제(report 기록 — WSL 진단 개선 원하면 채택).

### 2.3 폰 (T5·T7·T8 — aarch64 실측 3검장)

- **CHAT-PHONE-OK(T5, exit 0)**: tar 13입자 337,920B(chat 신규 5종 포함) →
  aarch64 리빌드 `ninja -C buildterm -j4` **rc=0 duration=551s**(175 targets
  — 실빌드 타임라인 검증) → selftest **0 failure(s)** → library-list **count=4**
  chat builtin → 부팅(tx4_boot) → ping ok → launch ok → **list_windows id=4
  Chat 720x540 focused 단정** → srvx.log `client surface 4 created (720x540)`
  → 종료 상태=서버 UP+Chat 창 유지(IME 육안 게이트 보존). fix r1(be8f008):
  tetris 회귀 어설션 3행+오타 수리, 폰 기준 TETRIS-ASSERT-OK 재검증 —
  persisted log 재검증 영수증.
- **WEBCHAT-PHONE-OK(T7, rc=0, 폰 547s 리빌드 NINJA-RC=0)**: jkweb 배포
  (폰 buildterm/jkweb 1,131,192B) → selftest 0 failure → 서버 UP+ping →
  jkweb nohup 기동(8090) → 폰 측 `GET /` 200 한국어 마커 → **POST /talk
  "지뢰찾기 켜줘" → HTTP 200 `kind":"Launch","app":"minesweeper"`** →
  list_windows **Minesweeper id=4 focused(320x380)** → tetris 회귀
  어설션("테트리스 켜줘"→app=tetris, **Tetris id=7 focused**) → srvx.log
  created 1→3 유효 증가 → 종료 상태=서버 UP+jkweb 유지.
  **수락 테스트(fix r1 뒤, 리뷰 I1)**: "지뢰찾기 꺼줘" → HTTP 200이면서
  `{"ok":false,"kind":"Close","app":"minesweeper","server":"{\"ok\":false,
  \"error\":\"window_not_found\"}"}` — HTTP 200 거짓 성공 소멸(브라우저는
  err 스타일로 표시).
- **OLLAMA-TRY-OK(T8, probe rc=0, phone-side 566s)**:
  - **Receipt A — jktalk stub 턴(hard receipt PASS)**: 채팅 파티클 5종 sha256
    비교 전부 동일 → tar·ninja 전량 스킵(해시 신선도 경로 실측). `list_windows`
    기준선 사전 수집 → `printf "지뢰찾기 켜줘"` | jktalk(폰 비TTY 파이프) →
    `[시스템] '지뢰찾기' 앱을 실행합니다.` + `실행 요청됨 (launch_app
    {"app":"minesweeper"})` + `회신: {"ok":true}` → 폰 측 agentctl
    list_windows 기준선 diff로 **신규 Minesweeper id=14**(focused, 320x380)
    생성 단정+(srvx.log created 4→5 보조 증거) — **stub 파이프라인
    Termux→jkdesktop end-to-end 폰 실측 성립**.
  - **Receipt B — ollama 설치 실험(PASS)**: 패키지 **이미 설치**(termux 공식
    ollama 0.23.2 aarch64, 40,439,992B, `pkg install` 불요)·serve 기동
    health ok·qwen2.5:0.5b **pull 성공 140s 397MB**(Q4_K_M, 494M params,
    capabilities tools) — 기술 전부 성립.
  - **Receipt C — 턴 실측(대체 영수증, 정직 라벨)**: jktalk 턴이 아니라
    폰 curl→ollama HTTP(/api/generate, stream:false) 단독 실측. 발화→응답
    **101.8s**(load 3.4s 포함), 25 토큰 생성 87.4s → **≈0.29 tok/s**(폰
    순수 CPU; C2 스트림에서 토큰간 간격 3-4초 재확인, TTFB 4.09s). 0.5B
    한국어 응답 품질도 도구 불능("지도를 열어주세요?") — LLM이 action을
    고르는 경로는 0.5B로 현실 불가.

## 3. 승격 판정 (스펙 §1.2 계약 — 컨트롤러 보고용 한 줄)

**stub 유지.** ollama 실험은 기술 성립(설치·serve·pull·턴 실측 전부 통과)
이나 승격 불가 — 근거 2류:

1. **UX 부적합**: 0.5B 폰 순수 CPU 추론 ≈0.29 tok/s(발화→응답 101.8초) —
   채팅 UX 성립 불가. 스트림화(TTFB 4.1s)로 첫 토큰은 개선되나 완결 턴이
   수십~수백 초인 건 동일.
2. **배선 갭 4건(jktalk↔JKLmEngine — "cfg만으로 배선 불가"의 정확한
   내역, 코드 변경 0 정직 유예 접수)**:
   - 동기 브리지: 슬롯 주석이 상정한 `llm.Route(text, action)`은 존재하지
     않는다 — 실 API `StartTurn(prompt, resumeSessionId, DeltaFn, DoneFn)`
     비동기 스레드+콜백(JKLmEngine.h:56-67), ProcessTurn은 동기.
   - 액션 매핑 부재: LLM 결과는 자유 텍스트 — 텍스트→ChatAction 번역 경로가
     ChatRouterRoute(한국어 어휘 규칙)뿐, LLM판정→ChatAction 매핑 코드 없음.
   - ollama HTTP 어댑터 부재: BuildEngineCmd의 ollama 경로는 `ollama launch
     claude ...`(claude CLI 하위 요구 통합 런처) — 폰에 claude CLI 부재
     (`Error: claude is not installed` rc=1 실측). 폰 대체 경로=/api/generate
     신규 어댑터 파선 필요.
   - cfg directory 재정의: LoadChatConfig 디폴트 chat.json이 `engine=ollama,
     directory I:\progwork\JKENGINE`(Windows 경로) — posix 로드 시 그대로
     workingDir로 적용되므로 폰 배선 시 chat.json 전면 재정의 필수.
   - (링크 자체는 무해 — JKLmEngine.cpp는 jkcore에 이미 포함, CMakeLists:217.)

**T8 M4 봉합(분기 이유 한 줄)**: probe가 `OLLAMA-VERDICT: PROMOTE-CANDIDATE`
를 인쇄한 것은 **TURN_OK 게이트만 보는 probe의 자동 판정이고**(stub 턴
Receipt A+ollama 설치 성립까지가 게이트 범위), 리포트의 최종 판정은 위
승격 계약(스펙 §2-2 cfg-무수정 불변식 — cfg만으로 UI·배선 무수정 승격이
성립해야 승격)으로 **stub 유지**. 두 표기는 모순이 아니라 게이트 범위
차이 — 본 원장이 리포트 판정을 진실원으로 봉합한다.

## 4. T9 귀속 봉합 (리뷰 T6 I2 — 댕글링 표기의 소화)

`ClientChatApp.cpp:151`의 주석 "앱→창 id 상관은 list_windows 합성 필요 —
T6 과제"는 **댕글링 표기였다**: T6 brief에 없었고(리뷰가 발각) 소유자
실종 → **백로그 항목으로 귀속** 봉합. 내용:

- 현재 Close/Focus는 서버가 `args.id` 한정이라 argless 폼만 가능(JKWindowServer
  .cpp:4074·3505 실측) — **앱 지명으로 닫기/포커스를 보낼 수 없다**.
- 해법은 **list_windows → (title/appName ↔ id) 상관 합성** 1건이 둘(Close·
  Focus)의 해법: `list_windows {}` 회신의 `id`·`title`로 라우터의 app 키를
  창 id로 해석해 `close_window/focus_window {"args":{"id":N}}`를 조리는
  소식체 공통 헬퍼(jkcore 위치 후보 — 세 소식체가 같은 필요를 갖는다).
- 소유자: 백로그 항목(라인 신설 과제 없음) — 다음에 채팅 표면을 손볼 때
  최우선 후보. jktalk/jkweb/앱 어느 쪽에서 실장해도 라우터 배선은 무수정
  (계약 위임은 그대로 — 수리는 "호출 전 id 조리" 계층만).

## 5. deferred minors 트라이아지 — 원장 명단 전수

처분 원칙: 채택 처분=이 라인에서 전부 deferred 유지(docs/77·79 선례) —
수요 없으면 소각 없음, 전체 리뷰가 승격 여부 재판정. 다음 probe 손때
후보는 우열 표기.

| 태스크 | 항목 | 처분 |
|---|---|---|
| T1(리뷰) | 접미-중첩 주석 문서성 · Close+app 서버 타깃 계약 미실측(**T2 게이트 위탁 — 이미 소각: ClientChatApp.cpp:147-158 원장 주석+argless 폼 실측 채택**) · deferred 3(부사 스캔="다시 켜줘" 앱어 절단 fail-open·주석 정확화) | Close 타깃=소각 / 나머지 유예(부사 스캔=앱어·발화 순서 확장 백로그 소속) |
| T2(리뷰) | **Minor 6건** — 컨트롤러 원장에 건수만 기록(개별 file:line 명단 미전달, 세션 패키지 diff에는 무존재) | 전부 유예 유지 — 명단 원문이 필요하면 전체 리뷰 때 리뷰어 원 판정 참조 |
| T2(원장명단) | **T6 과제 2건** ①앱→창 id 상관(Close/Focus argless) ②백엔드 슬롯 실장(JKLmEngine 플러그) · **GUI 육안 4점**(창 뜸·한글 전각·스크롤 앵커·launch 전송) | ①=제4절 귀속 봉합 / ②=제3절 승격 판정 소유(장기) / 육안 4점=사용자 게이트 대기(§8) |
| T4(리뷰) | `w:720` 서브스트링 어설션은 이론적 dw 매치(720x540 고정이라 실해 없음) · teardown survivor WARN 인쇄 후 rc 0(소프트패스) · inline `bash -lc` PIPESTATUS 안티패턴 헤더 노트 미기재 | 전부 유예 — 2건째(softpass WARN→hard FAIL)가 probe 손때 후보(T4 계열 다음 손때 1차) |
| T6(리뷰 I2·Minor 4) | I2=ClientChatApp.cpp:151 댕글링 표기 | **소각 — 제4절 귀속 봉합(본 원장이 소화)** |
| T6(Minor 4 나머지) | resize UB형식(창 resize 시 UB 위험 형태 — 재현 실측 없음) · EscapeJson 제5호 편입(tools 소식체별 복제 집계 — 공용 헤더 호이스트 후보, docs/79 EscapeJson 3호 계열 레저 승계) · RunMain 주석 문언(설명과 실제 분기 어긋남 — 1행 주석 수리) · `--help` 미기재(CP949 수동 스위치 JKTALK_INPUT_CP949 사용법 헬프 누락) | 전부 유예 — RunMain 주석·--help 문언 2건은 1행 수리성으로 probe 손때 아닌 문서 손때 대상 |
| T7(Minor 5) | 500 응답 reason phrase 미설정(HTTP 표준 문구 빈값 — curl 표시상 무차별) · 서러게이트(리뷰어 원장 용어 — 이스케이프 문맥 판정 항목: JsonEsc는 JSON 응답 문맥만 커버, 페이지 본문은 브라우저 textContent 무주입구 실측으로 방어됨) · CL 접두(Content-Length 접두 변형 헤더 대응 폭 — 본체는 대소문자 무시까지만 실장) · accept spin(연결 accept 비지 루프 — 30s 타임아웃 가드로 우회) · tar 표기(deploy list 주석의 산출물 개수 표기 관측) | 전부 유예 — probe 원격 pgrep 비브래킷 무해 nit 포함(실행 경로 접촉 0) |
| T8(M1-M4) | M1=probe RLOG에 콘솔측 영수증 미지속(폰 log에 Windows 측 근거 미수납 — 재확인이 원리포트 의존) · M2=열 스로틀 추정 라벨 분리(온도 미실측 — "열 스로틀"은 추정, 실측은 토큰간 간격 재확인만) · M3=probe EOF 개행 부재+헤더 주석 자가모순 · M4=OLLAMA-VERDICT vs 리포트 판정 분기 미봉합 | **M2·M4=본 원장 소각**(제3절·제6절) / M1·M3 유예 — **M3이 다음 probe 손때 1차 후보**(EOF 1바이트+주석 정합) |

## 6. 함정 원장 — 실측 신규(재발 방지 표준; 기존 원장=스펙 §3+docs/79 §3 승계 별기)

1. **병행 세션 존재 시 `git commit --amend` 금지(T6 사고).** T6 fix r1에서
   amend 시도 중 병행 컨트롤러 docs 커밋 7dfed70이 HEAD에 삽입돼 있어
   amend가 내 1바이트 수정을 **그 커밋에 흡수**(유령 커밋 047a8ed) —
   `git reflog` 전행 실측 후 `git reset --hard 7dfed70`로 병행 커밋을 원
   SHA 그대로 복원하고 본 수정을 **별도 fix 커밋**(35b00ab)으로 수리.
   교훈: amend는 "내 커밋이 HEAD"일 때만, 병행 세션이 있으면 처음부터
   별도 fix 커밋. 유령 커밋은 도달성만 남음(결함 아님).
2. **CMakeLists를 tar로 재밀 때 그 안이 참조하는 tools 소스 전량 동반
   배포(T7 사고).** T6 신설 jktalk 소스가 폰 트리에 없는데 CMakeLists만
   갱신 배포 → cmake 재생성이 `No SOURCES given to target: jktalk`
   (CMakeLists.txt:803)로 사망(NINJA-RC=1, duration=1s). 수리=배포 목록에
   `engine/tools/jktalk/main.cpp` 추가. 원칙: **타깃 정의는 소스에 우선한다**
   — CMake 재생성이 도는 환경은 신규 타깃 소스의 존재를 전제로 한다.
3. **srvx.log created 기준선은 boot·ping 후에 잡는다(T7 사고).** boot 전에
   기준선을 잡으면 tx4_boot가 로그를 돌려 부팅 후 카운트와 우연히 같아짐
   (3→3 유령 — 유효 증가 어설션이 FAIL). 기준선을 boot 후에 잡아 1→3
   유효 증가로 봉합.
4. **Content-Length 헤더 탐색은 대소문자 무시 규약(jkbridge::HeaderValue
   동형).** 첫판은 대소문자 감각 탐색이라 curl의 표준 `Content-Length:`
   오탈자 경로로 400 오판 → curl rc=56(연결 리셋) 관측. HeaderValue 규약
   교체로 수리 — 헤더 탐색 규약은 jkbridge 원문 그대로 승계한다.
5. **폰 0.5B 로컬 추론은 ≈0.29 tok/s(T8 실측 기준치).** qwen2.5:0.5b Q4_K_M,
   25 토큰 87.4s·발화→응답 101.8s — "폰 로컬 LLM 채팅"은 현 폰 CPU에서 UX
   불성립. 다음 실험의 기준선 값; 스트림 TTFB 4.1s는 개선 여지 있어도
   완결 턴은 수십초. 0.6B급 다음 후보도 이 값 대비 비교해야 한다.
   보조 함정: bare `ollama launch`(비TTY 메뉴 TUI)가 **rc=0**으로 돌아옴 —
   launch 계열 실패가 rc로 봉합되지 않는다(엔진 배선 시 fail-loud 계약에
   반드시 성공 마커 검사 병행).
6. **"열 스로틀=추정" 라벨 원칙(T8 M2 — 본 원장 채택).** 폰 온도는 미실측
   — "열 스로틀"이라고 쓰면 추정 라벨로만 쓴다. 실측은 토큰간 간격
   3-4초 재확인(C2)까지; 온도 센서 근거가 붙어야 인과 단정으로 승격.
   원장 문언에서도 추정·실측을 붙여 쓴다(예: "열 스로틀=추정(토큰간
   간격만 실측)").
7. **코드 수정 후 폰 배포는 항상 신선도 재배포+재기동(T7 fix r1 의무).**
   폰에 남아 있는 jkweb/jktalk는 이전 커밋 바이너리 — fix가 머지됐으면
   tar 재배포+리빌드+NINJA-RC=0+기존 프로세스 브래킷 pkill 후 nohup 재기동
   +재단정까지가 영수증(단면 리빌드 스킵=폰이 구판을 서빙). 본 라인에서
   `폰 배포 후 sha256 비교→전량 스킵` 경로(T8)가 신선도 확인 표준.

## 7. 커밋 원장 (전부 rev-parse 실측 값 — 본 원장 작성 시점 git log로 재확인)

| 커밋 | 내용 |
|---|---|
| 5074375109aaf9662016a767a65237f1b059011d | BASE — 스펙 결제판(stub MVP/ollama 실험 포함) |
| 24e0c5447bdb8080f50dfc69b7bb924556094a1a | 플랜 — T1-T9 구현 플랜(중반 개편 전판) |
| ad8258396a100878f4b3e450092e141f05dc3557 | T1 본체 — ChatRouter jkcore+셀프테스트 1n(17 check) |
| 1f3b89efc9cb55763a8d97502bd7c4bda7d90ae5 | T2 본체 — jkapp_chat 모듈+내장 카탈로그 등록(count 30 실측) |
| (T3) | Windows 축 영수증 전용 — 커밋 없음(ninja no-op rc=0·412 PASS·probe 4정판) |
| 5987b8990a956e94901a0b4faf3d216db16d5dab | T4 본체 — WSL 부팅·런치 probe(CHAT-BOOT-OK) |
| d8362507501ac1f4e377ab00aaecfb5011c1de60 | T5 본체 — 폰 실측 probe(CHAT-PHONE-OK·aarch64) |
| 52ca22ae1bc3ad5b5cc793e90135b285ff5acd15 | 중반 개편 — 사용자 결정 "입력=X11 밖" 스펙 §5+플랜 T6/T7/T8 재번호 |
| be8f008d7c4d23c089194735d9e3e6dfd90a8173 | T5 fix r1 — tetris 회귀 어설션 보강+헤더 오타(TETRIS-ASSERT-OK) |
| 7dfed70c55d94aa68f0e836e0bb3dd1ad4765afc | 룰링 3 — 폰 화면 웹 채팅 jkweb 플랜 반영(스펙 §5.1) |
| 93c20223665b4be2b898d6505b35f72ce5c1c88d | T6 본체 — jktalk CLI 대화 루프 4모드(332행) |
| 35b00ab94a8a100c9e61d864a47e322e05b558ff | T6 fix r1 — jktalk EOF newline 1바이트(amend 흡수 사고→reflog 복원 후 별도 커밋) |
| 591b1448d88dd1db9adde65353e781e0f65caa01 | T7 본체 — jkweb 폰 웹 채팅 HTTP 서버(535행+net adapter) |
| 74b67ea7f17b49f920de565ded5348afdeba2aca | T7 probe — 폰 웹 채팅 probe(WEBCHAT-PHONE-OK) |
| ba786b394bec57362f7e233e7008a8dbcce3a779 | T7 fix r1 — 서버 회신 정직 승계(ok/server 필드)+probe EOF(리뷰 I1/I2) |
| e8877032c8bb756385819a95bd63326499328f12 | T8 — 폰 ollama 설치 실험 probe(OLLAMA-TRY-OK)·승격 판정 stub 유지 |
| (병행) f5e720be69a3d35aa5a1c1b28f1c95850aa040e7 | 앱 커버리지 인벤토리 플랜 — 본 라인 밖(컨트롤러 병행 커밋, 체인 사이 삽입: 35b00ab..74b67ea 사이) |
| (본 커밋) | docs/80 as-built 원장+스펙 헤더 as-built 링크 |

전체 체인 = 5074375..e887703(본 라인 제품 커밋 15건 — 수정 2커밋 T6/T7 각
1회·T5 fix 1회·docs 개편 2건) + 병행 1건 + 본 커밋. 리뷰 판정 8태스크 전부
Spec PASS·Quality Approved/fix r1 경유 **CLEAN** — Critical 0, Important
총 3건(T5 tetris 어설션·T6 I1 EOF/I2 귀속·T7 I1/I2) 전부 fix r1로 소각.

## 8. 사용자 게이트 상태 — **대기 중 (결제 기록 없음 — 가짜 결제 금지)**

- **본 게이트(스펙 §5.1)**: 폰 브라우저 `http://localhost:8090/` 열기 →
  입력 박스 타자(안드로이드 소프트 키보드·음성입력 포함) → 회신 표시+DeX
  화면 반응 원단. 종료 상태=창 서버 UP+jkweb(fix r1판 pid 15974) 유지 중 —
  **서버·jkweb을 끄지 말 것**. 다른 기기에서는 `http://<폰IP>:8090/`.
- **보조 게이트(강등 — 룰링 2·3 준수)**: jkapp_chat IME — 폰 DeX 화면의
  Chat 창 입력 상자 탭→소프트 키보드→타자→ImGui 입력 도달·한글 IME 조합
  통과. 본 경로(jktalk/jkweb)가 성립한 뒤라도 UI 멤버로서 확인 대기.
- 결제 기록은 **사용자 선언을 받은 뒤에만** 본 원장에 기입한다 — probe
  출력만으로 "확인됨"이라 쓰지 않는다(계약 — T5/T7 리포트 게이트 조항 동일).

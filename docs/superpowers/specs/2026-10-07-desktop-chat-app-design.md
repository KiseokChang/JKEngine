# 스펙: 데스크톱 채팅 앱 (jkapp_chat) + 플러그형 턴 백엔드

상태: **결제 (2026-10-07 사용자 승인 — "빨리 구현하고 ollama 도 해봅시다")**
선행 조사: 2026-10-07 에이전트 턴 파이프라인 전수 조사(서브에이전트 보고 —
도구 표면 JKWindowServer.cpp ~3471-6500, jkbridge WS SessionRun :1424,
jkchat Win32 전용, Termux LLM 백엔드 부재) + as-built 원장 **docs/80**
(docs/80_desktop_chat_asbuilt.md — 실행 완결, 2026-10-07).

## 0. 비전 맥락

사용자 최종 그림(2026-10-07): **DeX 화면 꽉채운 jkdesktop + 아이콘 실행 +
폰에서 구두 조작**. 구두 입력 자체는 폰의 여러 입력 수단(안드로이드 키보드
음성입력 등)이 담당하는 것으로 확정 — **엔진은 STT를 만들지 않는다**. 엔진
몫은 "텍스트가 들어오면 에이전트 턴으로 앱을 조작하는 문";

턴 백엔드 결정(사용자 선택): **채팅 앱 먼저 + 플러그형 백엔드** — 실험 리스크
없이 즉시 사용 가능한 stub부터, cfg 승격으로 claude/ollama까지.

## 1. 아키텍처

### 1.1 앱: jkapp_chat (ImGui 클라, settings/library 가족 신규 멤버)

- 모듈: `engine/src/apps/JKAppModule_chat.cpp` + `ClientChatApp.h/.cpp`,
  meta `{"chat","Chat",720,540}` — **Windows·posix 양쪽 빌드 필수**.
- 형태: 상단 대화 기록(스크롤), 하단 입력 박스(InputText) + 전송 버튼(Enter
  전송), 한 줄 상태(연결·엔진·마지막 결과). 테마는 JKThemeImGui 가족 계약.
- 부팅: 앱 라이브러리에 `source=builtin`으로 등장(카탈로그 내장 3원 —
  JKLibraryCatalog 내장 판정에 chat 추가 필요 시 카탈로그 내장 목록에
  명기). 허브에서 실행.
- 앱 프로세스는 자기 서버(jkdesktop)가 이미 소유한 도구 실행기 위에서만
  동작 — **도구 실행은 채팅 앱이 직접하지 않고, 백엔드가 지시를 내보내면
  앱이 SendAgentQuery(ClientLibraryApp 선례)로 서버에 실행을 위임하고 그
  결과를 대화 기록에 반영**한다.

### 1.2 턴 백엔드 — 플러그형 (cfg 한 줄 승격 계약)

- 기존 `JKLlmEngine`(engine/agent)의 엔진 cfg(ollama/claude/stub)를 그대로
  재용. 채팅 앱은 JKLlmEngine을 자기 프로세스 내에 보유하거나 동형 계약으로
  봉합 — 구현 시 JKAppModule 계약(앱 객체가 DLL 내 전부 해결)과 충돌 없는
  형태로 결정하되, **cfg 전환으로 백엔드가 바뀌어도 채팅 앱 UI 무수정**이
  불변식.
- **stub (폰 기본값)**: 오프라인 명령 라우터. §2 어휘 → 도구 지시 → 앱이
  서버 실행. LLM 불요.
- **ollama (폰 ollama 실험 성공 시)**: Termux ollama(커뮤니티 빌드)로
  소형 모델 1-2B. 실험 태스크(T6)의 결과물로만 승격 — 안 되면 stub 유지.
- **claude (PC 전용)**: 기존 jkchat/jkbridge 선을 잇는 PC 방향은 이번 라인
  스코프 밖(별도 문).

### 1.3 stub 명령 어휘 (MVP — 사용자 승인 설계안)

- `<앱 이름> 켜줘/열어줘/실행` → `launch_app {"app":"..."}`
- `꺼줘/닫아줘` (포커스 창) → `close_window`(ask 게이트 있으면 서버 판정
  위임)
- `포커스/앞으로 가져와` → `focus_window`
- `창 목록/뭐 떠 있어` → `list_windows` 결과를 대화 기록에 인쇄
- 인식 불가 → 안내문("채팅에서 쓸 수 있는 명령: ...")

어휘는 확장 열린 패턴 테이블(single-source, 셀프테스트가 잠금) — 구현자가
코드로 표 만들 것.

### 1.4 IME 리스크 (실측 게이트)

Termux:X11 소프트 키보드(안드로이드 IME) → X11 앱 키 이벤트 → ImGui
InputText 도달이 이번 라인의 최대 불확실성. 폰 실측 태스크(T5)에서
**입력 도달 영수증**을 반드시 뽑는다(자동화 불가 분은 사용자 육안 게이트로
명시). 실패 시: 입력 박스에 폰 키패드 도구(send_input type) 우회나 X11
키보드 설정 진단 — 스펙 밖 수리는 룰링으로.

## 2. 계약 (불변식)

1. 채팅 앱은 창 서버의 도구를 **직접 실행하지 않는다** — SendAgentQuery
   위임 단발. 도구 루프(LLM↔MCP)는 claude/ollama 백엔드 전용.
2. 백엔드 cfg 승격 시 앱 UI·배선 무수정 — stub→ollama는 cfg+어휘 표만 건딘다.
3. 콘솔 lf/hx·library와 동일: 앱은 읽기 전용 상태를 서버에 의존, 자기
   진실원 일절 없음(대화 기록 1개 제외 — 앱 프로세스 메모리).
4. Windows 서버 스폰 금지 원장 유지 — Windows 축은 CLI/셀프테스트,
   GUI 실측은 WSL(부팅 수준)+폰(실물).
5. 도구 지시의 권한 게이트는 서버 기존 권한 행렬이 소유 — 채팅 앱이
   우회하지 않는다.

## 3. 함정 원장 승계 (구현자에게 전수)

- **ImGui Begin/End 본문 위치** — Begin 직후 End 후 본문 그리기=폴백
  "Debug" 창(imgui.cpp:7521 — T3 Critical 재발 방지). 본문은 Begin/End
  안.
- **NoDecoration=NoScrollbar 포함** — 스크롤 필요 창은
  AlwaysVerticalScrollbar.
- **posix** — 콘솔 lf/hx·MSYS_NO_PATHCONV=1·toybox pgrep -f 브래킷·폰
  tar -C ~/JKENGINE (engine/engine 중첩 금지).
- **probe 품질** — 수신=어설션, rc 전파 PIPESTATUS→exit, `|| true` 금지.
- **16ms 항시 렌더** — ImGui 가족 계약 준수(settings 선례), docs/78
  백로그 포인터 주석.

## 4. 다음 = 구현 플랜

T1 stub 라우터+셀프테스트 · T2 jkapp_chat 모듈+셀프테스트 · T3 Windows
축 영수증(CLI) · T4 WSL 실측(부팅+launch) · T5 폰 실측(IME 포함) ·
T6 jktalk CLI 대화 루프(§5) · T7 jkweb 폰 웹 채팅(§5.1) · T8 폰 ollama
설치 실험(성공하면 cfg 승격 검토) · T9 as-built docs/80.

## 5. 변경(2026-10-07 중반 — 사용자 결정): 입력 표면 = X11 밖 우선

사용자 문의 "채팅앱은 x11 과는 별도 앱이어야 의미가 있는거 아닌가요?" →
선택지 제시 → **사용자 결정: "X11 밖 입력 (Termux/네이티브)"**.

- **룰링 1**: T1-T5 산출(X11 창 채팅 앱 jkapp_chat+영수증)은 기각 아님 —
  **데스크톱 쪽 UI 멤버로 유지**(데스크톱 화면이 떠 있을 때의 대화
  인터페이스). 단, **본 입력 문은 X11 밖**으로 격상.
- **룰링 2**: X11 밖의 첫 형태 = **`jktalk` CLI 대화 루프**(Termux 셸에서
  자유 텍스트 → 라우터 → 도구 → 회신 인쇄; 여러 줄 세션). 이유: T1
  라우터가 pure 코드라 CLI에서 그대로 재용, 도구 실행은 폰에 이미 빌드된
  JKAgentClient→서버 소켓 경로(jkagentd 선준) 재용, **IME 미검증 리스크
  소멸**(Termux 터미널은 자체 키보드 — 안드로이드 음성키보드도 근원적
  도달), 구현 최소. 네이티브/웹 폰 UI는 다음 단계 후보.
- **계약 승계**: 스펙 §2 전부 jktalk에도 동일 적용(도구 실행은 서버 위임,
  권한 행렬 우회 없음). stub 어휘 §1.3 동일 — 진실원=ChatRouter 1개.
- **ollama 실험 승계**: ollama 백엔드 실험(T6)은 jktalk 턴 백엔드에
  JKLlmEngine cfg(engine=ollama)로 시도 — 성공하면 CLI와 창앱 양쪽 승격.
- `jkapp_chat`의 IME 육안 게이트는 **보조 게이트로 강등**(본 경로는
  jktalk라 이후에도 막혀도 라인 전체는 성립).

### 5.1 변경(2026-10-07): 폰 화면 웹 채팅 — jkweb (룰링 3, 사용자 결정)

사용자 문의 "폰 화면에서 도는 별도 앱을 만들면 안돼? 웹이라도" → 서버
위치·순서 확정(A): **Termux 웹 서버 `jkweb` — 폰 브라우저(localhost)에서
채팅. jktalk 완료 직후 끼워넣기(플랜 Task 7), ollama 실험은 그 다음.**

- 근거: 서버(jkdesktop)와 unix 소켓이 폰 로컬 — 웹 서버가 같은
  JKAgentClient 경로를 재용하면 폰 화면 채팅→X11 jkdesktop 반응이 성립.
  브라우저 키보드=안드로이드 네이티브(음성입력 포함) — IME 리스크 소멸.
  PC 브라우저에서 폰 IP 접속도 가능(LAN ssh 성립 선례).
- 형태: 신규 tool `jkweb` — 정적 채팅 페이지 1장(입력 박스+대화 기록,
  원문 문자열 내장) + `POST /talk` {text} → ChatRouterRoute → 서버 위임
  → 결과 JSON. jktalk의 턴 코어 재용(스펙 §2 계약 승계 — 도구 실행은
  서버 위임, 권한 행렬 우회 없음, 진실원=ChatRouter 1개).
- Android 네이티브 앱은 별도 문(NDK/패키징 설비 신설 — 백로그).
- ollama 승격(§1.2)은 jktalk·jkweb 양쪽이 같이 받는다.
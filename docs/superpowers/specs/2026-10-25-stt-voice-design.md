# 스펙 — #112 STT 구두 라인 v1 (폰 DeX 구두 조작) 설계 (2026-10-25, 초안)

상태: **확정 — 컨트롤러 재량 판정 4건(2026-10-25 사용자 승인 진행·폰 마이크
권한 사용자 확인 완료). §6의 미확정 5건 → §6b 확정 원장으로 폐곡.** 커밋·플랜 진행.

---

## 0. 확정 결정 원장 (사용자 2026-10-25 — 이 문서의 상위 진실원)

1. **STT 엔진 = whisper.cpp 로컬** (ggml 모델) — 네트워크 불요·폰 단독 작동.
   클라우드 STT·서버 전사는 v1 전부 비-목표.
2. **v1 스코프 = 조작+대화 포함** — 말 → STT → **기존 자연어 승격 파이프라인
   재용**(chat LLM 경로 → 창 제어·재생·앱 실행) + **문자 출력 대화**.
   새 자연어 판정 계층을 만들지 않는다 — ChatRouter/ChatRouteTurn이 단일 진실원.
3. **최종 그림 = 폰 DeX 꽉채움 jkdesktop + 구두 조작.** 사용자 피드백:
   **환경꾸미기 시간 최소화 — 경험 도달 가속 몫** (이 스펙도 그 결정을 따라
   스펙 재판정 0·토큰 산책 없는 최단 경로를 지향한다).

## 1. 조달 조사 결과 (저장소 원문)

### 1.1 whisper.cpp는 저장소에 없다

`git log --all -- engine/third_party` + 전수 목록: third_party는
cef·imgui·imgui_club·implot·miniz·quickjs-ng·stb 뿐 — whisper·ggml 관행
grep 전수 0건. 즉 STT 라인은 **처음부터 새 조달 대상**.

### 1.2 조달 규약 원문 (모델 파일은 커밋 금지 — 확정)

- docs/84_promotion_asbuilt.md §기록 5: "조달 바이너리 커밋도 기각
  (**기계만 커밋** 원칙)" — 조달 스크립트만 커밋.
- docs/81_app_coverage_asbuilt.md §4/§5: 조달기 스크립트만 커밋(멱등 — 부재
  시에만 네트워크)·조달 산출물은 무추적(`??` — docs/44 Windows 축 정책 계승).
- 소스 vendor 선형: third_party의 imgui·implot·stb·quickjs-ng는 **소스 커밋
  허용**(MB급·빌드 타깃 정적 링크). whisper.cpp 소스(~수 MB 소스)도 크기·성격상
  이 선형에 들어간다.
- 별도 저장소+env 선형: spatial-player(I:\progwork\spatial-player)는
  **SPATIAL_PLAYER_ROOT env-only, 경로를 source/commit에 never 씀** —
  MusicSpatialLeg.h 헤더 주석 원문 ("fail-closed — 미설정 빌드에서 leg는
  컴파일 자체가 생략").

### 1.3 GGML 한국어 품질 실측 (공개 벤치 — §7 근거)

| 모델(ggml) | 크기 | CER(원본 평균) | 비고 |
| --- | --- | --- | --- |
| tiny | ~75 MB | **36.6%** (Kspon clean 42.2) | 명령어 어휘용 최소판 |
| base | ~142 MB | 40.6% (Kspon clean 46.9) | tiny보다 나쁨 — 후보 탈락성 |
| **small** | ~466 MB | **17.5%** (Kspon clean 22.0) | Win 축 1순위 |
| medium | ~1.5 GB | 13.9% | 폰 불가·Win 선택지 |
| large-v3 | ~3.1 GB | 8.0% | v1 비-목표 |
| **tiny-ko (komixv2 파인튜닝·ggml 변환 존재)** | **74 MB** | 원본 대비 ~11.6% | **폰 축 1순위 후보** — whisper.cpp ggml로 공개 변환본 존재 |
| small-komixv2 (파인튜닝) | ~466 MB | 7.4% | Win 축 상향 후보 |

판정: 폰 CPU 실측(AOSP-계열 8코어) — tiny ~7× 실시간(11s→1.5s)·small ~0.25×
(15min→10min) — **폰은 tiny류만 현실적** (§7 근거). tiny은 CER 36.6%로
일반 대화엔 못 쓰지만, 라우터 어휘(켜줘/닫아줘/앞으로/창 목록+앱명)라는 **제한
발화** 도메인에선 실측 재판정 대상. komixv2 tiny-ko(11.6%)가 일반 tiny을
이기므로 폰 기본 후보.

## 2. 기존 파이프라인 원문 (접점 조사)

### 2.1 자연어 승격 파이프라인 — 3층 확정

- **ChatRouterRoute** (engine/src/apps/ChatRouter.cpp — 어휘 표
  kRouteTable/kAliasTable 단일 진실원): 정확 트리거 매치는 LLM 왕복 0 즉발
  — 닫아줘/꺼줘/앞으로/창 목록/열어줘/켜줘/실행해줘 + 앱 별명(지뢰찾기·테트리스).
- **ChatRouteTurn** (ChatRouter.h 설계 결정 4 배선 본체): (1) 정확 매치 즉발 →
  (2) 비매치 + chat.json engine 구성("ollama"|"claude"|"ollama-direct") →
  TurnSync 동기 턴 → 행동 JSON(ChatLlmActionParse) → (3) 실패 → stub 안내문
  폴백. 소비자 = jkweb HandleTalk(POST **/talk**)·jktalk ProcessTurn.
- **jkbridge WS `chat`** (engine/tools/jkbridge/main.cpp:1460): 폰 브라우저
  WebUI → engine_.StartTurn(text, resumeSession, ...) — 세션 재개형 LLM
  에이전트 경로(폰→PC 원격 조작 leg).
- 도구 지시 파이프라인: ChatAction(Launch/Close/Focus/ListWindows/Info) →
  launch_app·close_window·focus_window·list_windows → 창 서버.

**판정 — STT 결과 문자는 `text`와 완전 동형이다.** STT는 이 파이프라인의
발화 소스가 하나 늘 뿐이고, 접점 계약상 "STT 텍스트 == 사람이 타이핑한 텍스트"
(정규화·데이터 형태 동일 — 추가 파서 금지).

### 2.2 jkdesktop 링크 상태 (접점 비용 — 이미 0)

jkdesktop의 RunAppSelfTest가 이미 1n-f 계열(승격 배선·
ChatLlmActionParse)을 캐논 담당으로 단정(engine/src/main.cpp:4347 부근,
posix_selftest/main.cpp:1735 원문) — **jkdesktop은 ChatRouter+JKLlmEngine을
이미 링크 중**. in-process 접점(§3 ③ 안 A)은 신규 링크 0.

### 2.3 채팅 앱 (ClientChatApp.cpp)

`Submit`은 ChatRouterRoute(stub 라우터만 — LLM 배선 미연결 — ClientChatApp.cpp:
141) → launch_app/close_window/focus_window/list_windows 전송 → 답신을 기록에
원문 인쇄. 턴 구조(Turn{mine, text})·[나]/[시스템] 표기·스크롤 앵커 계약이
STT 대화 출력(§3 ④)의 그대로 재용 형태.

### 2.4 전사 품질 검증 문화 규약

whisper 전사는 **결정론이 아니다**(모델·시설·녹음마다 다름) — selftest가
전사 본문을 단정하게 뒀다가 CANON을 무너뜨리는 함정. selftest는 STT
파이프라인의 **순수 부품+fail-closed 판정부**(§3 ⑦)만 단정하고, 전사 품질
본판정은 사용자 EYES 게이트(§3 ⑧)로 봉인한다 — 폰 spatial 합산 "PARTIAL
정직" 선례(docs/90)의 계승.

## 3. v1 설계 아웃라인 (컨트롤러 검독 대상 — 권고안)

### ① 음성 수집 leg — 어디서 마이크를 캡처하나

- **Win 축: SDL2 오디오 캡처** (jkdesktop 안) — 엔진이 이미 SDL2를 소유
  (vplayer audio ring 선존). 캡처 장치는 재생 장치와 별개 SDL 장치 열기 →
  16 kHz 모노 PCM WAV 임시 파일. 신규 조달 0. WASAPI 공유 모드 →
  **재생과 입력 동시 성립**(music 재생 중 구두 조작 계약의 Win 축 근거).
- **폰(Termux/DeX) 축: Termux:API `termux-microphone-record`** — F-Droid
  Termux:API 앱(사용자 설치+마이크 권한 승인 필요 — §6 Q3) →
  `termux-microphone-record -f <wav> start` → stop → ffmpeg(-ar 16000 -ac 1)
  정규화 → whisper-cli. 실측 근거: 폰 SDL2 캡처가 안드로이드 오디오 스택에
  미통합(whisper-stream SDL 계통 실효성 부정 — §7), ffmpeg은 Termux
  bootstrap에 이미 실측 설치(termux_bootstrap.sh:21).
- **비권고 원장**: whisper.cpp의 stream 예제(SDL 실시간 캡처)는 폰에서 SDL
  캡처 계통이 막혀 실용 불가 — v1은 **파일 기반 전사**(push-to-talk) 원칙.
  Web/MediaRecorder 경로(jkbridge WebUI getUserMedia)는 원격 조작 모델
  전용 — DeX 로컬 데스크탑 조작에는 표면이 일치하지 않아 v1 비-경로.

- 수형: **push-to-talk** — 핫키(키보드)·[구두] 버튼(ImGui) 눌러 시작/끝 —
  VAD 상시 청취는 비-목표(v2 재판정). 환경꾸미기 최소화 원칙의 직접 귀결:
  상시 청취는 시작/끝 판정·오차 재판정이 라인 전체를 부풀린다.

### ② whisper.cpp 통합 — 별도 프로세스 바이너리 (권고)

- **권고: 별도 프로세스 `whisper-cli` 스폰** (WAV 경로+모델 경로 인자) —
  엔진 관행 원존 2개: ollama-direct 스폰(BuildOllamaDirectCmd —
  JKLlmEngine.cpp:618 "공개 계약 락")·spatial-player audio_core 별도 프로세스
  (SPATIAL_PLAYER_ROOT fail-closed). 별도 프로세스 = 크래시 격리(폰 실측
  #3117 segfault 원존 — 라이브러리 링크면 서버 자체 사망)·모델 교체가
  재링크 없이 가능·폰/Win 빌드 계열 분리.
- **비권고: libwhisper 정적 링크** — 폰 #3117(backend 공유 lib 탐색 segfault,
  2025-11 현재 open — static build이 완화)의 크래시 전파면 + ggml 상태
  전역화 + 폰 빌드 계열(#111 폰 컴파일 경합 원장) 부담.
- 배선 형태: **WHISPER_ROOT env(또는 설정 파일 키) + fail-closed** — 미설정/
  바이너리 부재/모델 부재 = 구두 leg는 **끔+정직 표기** (spatial leg 원존
  계약의 문자 그대로 계승; fail-closed 스펙 D2 선형).
- 응답 형식: whisper-cli 출력에서 최후 텍스트 절단(parse) —
  `ChatLlmActionParse`와 동형의 "방어 원료" 규약(펜스/해설 헤지) 승계 예정.

### ③ STT → chat 파이프라인 접점

권고(안 A): **in-process ChatRouteTurn 직호출** — 구두 표면이 먹은 문자열을
ChatRouteTurn(text, action, chatCfg)에 원문 전달 → ChatAction →
launch_app/close_window/focus_window/list_windows → 창 서버 위임.
jkdesktop이 이미 링크(§2.2) · 폰 DeX 로컬 모델과 정확히 일치 · IPC 0.
chat.json이 구성됐으면 LLM 승격 턴까지 같은 배선이 그대로 도는데(폰 ollama
cloud 모델 턴 실측 선존 — docs/84 §4), 미구성이면 라우터 즉발+stub 안내문
폴백 — **"LLM이 아니어도 구두는 조작된다"**(네트워크 불요 원존의 최후 방어).

대안 B: jkweb POST /talk(localhost) 재용 — 폰에선 jkweb을 동기 스폰
계열(폰 콘솔 앱)로. IPC 1검+HTTP 형태만 늘고 배선 동일 — in-process가
이기는 점은 "표기 즉시성"과 폰 콘솔 스폰 의존 소거. 대안 C: jkbridge WS
chat — 원격(PC 조작) v2 폭발 후보로 기록만.

### ④ 응답 출력 — TTS 없이

- **TTS는 v1 비-목표**(사용자 확정 원장 ②의 "문자 출력" 접시). 출력 =
  ① 턴 기록(채팅 앱 동형: [나]=발화(전사 원문)·[시스템]=안내문/도구 답신
  원문 — ClientChatApp 턴 구조 재용 ② 알림 표면(ClientNotifyApp 토스트
  선존 — "앱 실행"·"인식 못 함" 단문).
- 전사 본문은 **원문 그대로 기록에 남는다** — 오차의 정직 표기 문화
  (재판정 근거 남김).

### ⑤ 무음/오디오 충돌 계약

- **조그 무음 원존 무변조**: vplayer 조그 "세션 전 구간 무음"(사용자 지시
  원존 — 갱신 60)은 구두 라인이 건드리지 않는다. 재생 계통 계약 0 변경.
- **녹음 중 재생 동시**: Win = WASAPI 공유(§3 ①)로 자연 성립. 폰 =
  Termux:API 녹음은 Termux 소속 프로세스의 마이크 세션 — 재생(vplayer/
  spatial audio_core)과 별 세션. **risk 원장**: 재생음이 마이크로 스며
  전사 오차 — push-to-talk(짧은 명령 발화)이 완화. music 무음과의
  계약 충돌 원존은 **없다**(재생 아닌 캡처 — 다른 SDL 장치/별 프로세스).
- 재생 중 구두 조작의 실측 재판정(폰 EYES — §3 ⑧ ③)에서 정직 판정.

### ⑥ 폰 DeX 조작 사슬 (재용 경로)

발화 → Termux:API 녹음(WAV) → ffmpeg 16 kHz 정규화 → whisper-cli(폰 ggml
tiny-ko/ggml-tiny — 조달기 스크립트, §3 ②) → 텍스트 → **in-process
ChatRouteTurn** → ChatAction → 창 서버 도구 질의 → 로컬 창(launch/focus/
close/list) → 문자 표면(기록+토스트). 전 사슬 **폰 단독 완결·네트워크
불요**(llm 미구성 폴백이 기본 성립 — 폰 chat.json 부재 정직 지연 0 선존).
PC 조작(원격)은 jkbridge chat 폭발 후보 — v2.

### ⑦ selftest 계보 — 신설 **2v**

- `2u` 점유(#111 태그 쓰기 — engine/src/main.cpp:6447), `2t` 점유(배율) —
  신설 계열 = **2v**(결정론 0건 원칙 유지).
- 단정 대상(전사 비결정 단정 금지 — §2.4): 2v-a WAV 정규화 순수 부품(샘플레
  이트/모노/포맷 판정 — 헤더 파싱만)·2v-b fail-closed 판정부(env/바이너리/
  모델 부재 → 구두 끔+표기 — Unsupported 모양 데이터)·2v-c 전사 출력
  파서(최후 텍스트 절단 — 고정 입력 1종)·2v-d 접점(STT 텍스트 →
  ChatRouteTurn 왕복 = 기존 1n-f 계열의 입력 소스 치환 동형).
- 캐논 승격 계약: Win 691/WSL 668/posix 304/폰 668 + 2v N건 — 러스터
  표기는 갱신 65 4축 계보 표에 승수 행으로(2i 계열 표기 선례 — docs/88 §2.1).
- whisper 실전 전사 런(probe)은 CANON 밖 캡처 근거로만 — selftest에 녹이지
  않는다(mawk 14vs13 선례 — probe 자체 결함 원장 원존).

### ⑧ 사용자 게이트 (EYES — 사용자 선언만 결제)

1. 폰 DeX: "지뢰찾기 켜줘" 발화 → 앱 실행 체감.
2. 폰 DeX: 음악 재생 중 발화 조작 체감(⑤ 완화판 판정).
3. Win: 마이크 구두 조작 체감.
4. 전사 오차 체감(tiny-ko 판정 — 모델 등급 상향 요구 시 small/medium 표).
5. 대화(문자 출력) 체감 — 인식 실패 시 정직 표기 납득.

## 4. 조달 판정 (결정 재료 요약 — §1 원문 근거)

- **모델 파일(ggml, 74 MB~): 커밋 금지 확정** — "기계만 커밋" 원칙 문자
  그대로(docs/44·81·84). 두는 곳 권고 = 조달기 스크립트(멱등 — 부재 시에만
  네트워크, GitHub release/HF 조달·네트워크 명시·aarch64 자산 실측 —
  install_lf_helix_posix.sh 선형)이 **무추적 디렉터리**(buildwsl/stt-models
  계열)에 내려놓는다.
- **whisper-cli 바이너리: 무추적** — 빌드 산출물(buildwsl 계열).
- whisper.cpp 소스: vendor(소스 커밋 — third_party 선형) vs 조달기 clone —
  §6 Q1.
- 폰 조달: pkg(clang cmake ninja) 이미 실측(TX1 — docs/78 §4) — 신규 pkg
  = termux-api(+Termux:API 앱 — 사용자 손)만.

## 5. 비-목표 (v1)

- TTS 음성 응답·상시 청취(VAD)·클라우드 STT·PC 원격 구두 조작(jkbridge chat
  폭발)·large/medium 폰 배치·웹 브라우저 마이크 표면.

## 6b. 확정 원장 (컨트롤러 재량 4건+사용자 확인 1건 — 2026-10-25)

- **R1 (Q1) whisper.cpp 소스 = third_party vendor 커밋(imgui 선형)** —
  폰 배포가 engine HEAD archive 1줄로 완결(별도 저장소 스테이징 공비 소각)·
  모델/바이너리는 무추적+조달기 스크립트만 커밋("기계만 커밋" 유지).
- **R2 (Q2) 구두 표면 = 채팅 앱 [구두] 버튼+핫키** (별도 앱/전역 오버레이 아님
  — 턴 기록 재용·전역 핫키의 포커스 밖 수취 문제 회피).
- **R3 (Q3) 폰 Termux:API 설치+마이크 권한 = 사용자 완료 확인(2026-10-25)** —
  사용자 손 유계 소각.
- **R4 (Q4) 모델 등급 = 폰 tiny-ko(komixv2 74MB·CER ~11.6%)/Win small(466MB)
  확정** — medium/large는 비-목표(§5).
- **R5 (Q5) 전사 대기 UX = 상태 행 "전사 중…" 표기**(턴 기록에 전사 원문
  도착 시 치환 — 정직 원문 문화).

## 6. 미확정 질문 (폐곡됨 — §6b로 이행·전사 기록 보존)

1. **whisper.cpp 소스 조달 형태** — third_party 소스 vendor 커밋(imgui
   선형; 조달 0 강화) vs 별도 저장소+ROOT env fail-closed(spatial-player
   선형; IP 레포 경로 커밋 0). 권고는 vendor(§3 ②)지만 레포 크기·업데이트
   재동기화 빈도 몫 판정.
2. **구두 표면 위치** — 채팅 앱에 [구두] 버튼+핫키(권고 — 턴 기록 재용) vs
   별도 앱/전역 오버레이. 전역 핫키는 창 포커스 밖 수취 문제가 새로 열린다.
3. **폰 마이크 권한 승인** — Termux:API 앱 설치+마이크 권한(사용자 손 1회)
   승인 여부. 미승인이면 폰 leg는 fail-closed로 정직 표기(이 스펙의 정직
   문화에 맞음 — 단, "폰 구두" 목표 도달 자체가 이 승인에 걸린다).
4. **모델 등급 per leg** — 폰 = tiny-ko(komixv2, 74 MB, CER ~11.6%) 기본 +
   Win = small(466 MB, 17.5%/명령 도메인 상향 기대) 권고. small의 폰 실행
   비현실(0.25× 실시간) 원장 대로 등급 사전 확정.
5. **turn 비동기 UX** — 폰 tiny 전사 ~1-2s/5s 발화(7× 실측)·Win small
   즉각 수준 — 전사 대기 표기(상태 행 "전사 중…") 형태 확정.

## 7. 출처 (근거 링크)

- 저장소 원문: ChatRouter.h/.cpp·ClientChatApp.cpp:120-294·JKLlmEngine.cpp:
  92/231-237/618-623·jkweb/main.cpp:339-462·jkbridge/main.cpp:1460-1497·
  MusicSpatialLeg.h·MusicModel.h·ClientMusicApp.cpp(SPATIAL_PLAYER_ROOT)·
  engine/CMakeLists.txt:242/907·posix_selftest/main.cpp:1510-1735·
  docs/44·81(§4·§5 조달)·84(§5)·88(§2.1 캐논)·62·78.
- [seastar105/whisper-small-komixv2 (HF — 원본/파인튜닝 CER 표)](https://huggingface.co/seastar105/whisper-small-komixv2)
- [wabisabisocial/whisper-tiny-korean-ggml (HF — ggml 변환 74 MB)](https://huggingface.co/wabisabisocial/whisper-tiny-korean-ggml)
- [OpenKoASR Leaderboard — KsponSpeech/AIHub CER](https://gt-kim.github.io/open-korean-automatic-speech-recognition/leaderboard.md)
- [Running Whisper ASR on Android with Termux (HF blog — termux-microphone-record 실측)](https://huggingface.co/blog/Javedalam/wwhisper-cli)
- [whisper.cpp #3117 — Termux segfault(open)](https://github.com/ggml-org/whisper.cpp/issues/3117)
- [whisper.cpp #2370 — Termux Vulkan 불가/폰 CPU 실측(11s→1.5s tiny)](https://github.com/ggml-org/whisper.cpp/issues/2370)
- [whisper.cpp examples/stream README — SDL 스트리밍 계통](https://github.com/ggerganov/whisper.cpp/blob/master/examples/stream/README.md)
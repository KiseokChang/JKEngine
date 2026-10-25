# docs/91 — STT 구두 as-built (#112 라인 — T1-T6, 2026-10-25)

STT 구두 라인(#112) 원장 — **말 → whisper.cpp 로컬 전사 → 기존 자연어 승격 파이프라인
재용(ChatRouteTurn — 창 제어·재생·앱 실행) + 문자 출력 대화**·네트워크 불요·폰 단독
완결. 계획 문서=`docs/superpowers/plans/2026-10-25-stt-voice.md`(79a27d1), 스펙=
`docs/superpowers/specs/2026-10-25-stt-voice-design.md`(496a5e6 — 확정), SDD 원장=
`.superpowers/sdd/2026-10-25-stt-voice/`(progress+task-1..6 brief/report+review diff
— **전 태스크 review clean**·fix 커밋 4건(a5e65cf = 재심사 CLEAN·나머지 3건 =
런 중 자가발견 소각)). 라인 BASE=f33d41a(#111
music 태그 쓰기 최종). T7은 본 문서 — docs 전용(engine/probe 소스 무접촉)이라
selftest 재실행 불요 — 캐논 = 전 태스크 리포트/progress의 건네받은 수치 그대로 봉합.
서브에이전트 스폰 0건(T1-T6 각 리포트 선언)·worktree 미사용·main 직행·`git add -A`
미사용(전 태스크 명시 경로 staging). 코드 계보는 git log가 진실원.

- **룰링 승계** — 조달 원존=**기계만 커밋**(docs/44·81·84 — 모델 파일·바이너리 조달
  스크립트만 커밋·무추적)·**전사 비결정 원칙**(스펙 §2.4 — selftest는 순수 부품+
  fail-closed 판정부만 단정·전사 본문은 selftest에 녹이지 않는다 — CANON 밖 캡처/
  probe 원문으로만·사용자 EYES가 본판정)·probe/selftest stdout 수취 원장(WSL은
  WSL 안 리다이렉트 — T3 실측 797 소각 원장행)·폰 주소 기록 금지(PHONE_HOST env —
  known_hosts BatchMode 자기 판명).
- **부기(#112 — 하단 부기·변형 아님, docs/90 쪽)** — docs/90에 §4.1-6 신규 park 행
  (본 라인 커밋 체인 1행)+§7.10.2 캐논 정산 부기 1행(#111 종착 수치 818/795 → 현행
  계보 837/814/폰 814 — 계보 보존 방식·재작성 없음).

## §0. 체인 (rev-parse 전체 SHA 실측 — T7 측정 · git log가 진실원)

| 태스크 | 커밋(SHA) | 내용 |
| --- | --- | --- |
| 스펙 | `496a5e642a4452b58b0948da096539caf93949fa` | docs(apps): STT 구두 라인 설계 스펙 확정 (#112 — whisper.cpp vendor·[구두] 표면·모델 등급·폰 마이크 완료) — 사용자 결정 2건+R1-R5 폐곡 |
| 플랜(라인 BASE 지정) | `79a27d1def69a453136049baad4bce9c9d2553a4` | docs(apps): STT 구두 v1 구현 플랜 (#112 T1-T7) |
| T1 | `9512ba876934e8533bd7c00ed86591b1b191dad0` | feat(apps): whisper.cpp vendor+조달기+WHISPER_ROOT 판정부 — third_party/whisper.cpp 149파일(v1.9.5·~11.3MB)+라이브러리 0·2v-a 4건 (4계열) |
| T2 | `049b94e53e0e8c3c846756976e9ef105d308790a` | feat(apps): voice 전사 사슬 순수 부품 — wav+파서+스폰 — include/voice/SttVoice.h 신설+src/voice/SttVoice.cpp+2v-b/c 6건 (4파일) |
| T2 fix r1 | `a5e65cf83156849f7a839ec3ad521358991bf15a` | fix(apps): SttVoice 전체 마감 600s 판정 이동 — 리뷰 Important 1(idle-kill 퇴화) 소각 |
| T3 | `1b828e2512a4660bb9bf93a5895224a78db87803` | feat(apps): 구두 발화 수집(Win SDL)+ChatRouteTurn 접점 — VoiceCaptureSDL.cpp 신설+RunVoiceTurn 사슬+libgomp 스풀 target+2v-d 4건 (5파일) |
| T4 | `4279d967a613ddcafe46957b3d07950bbfd2358b` | feat(apps): 채팅 앱 [구두] 표면+전사 중 표기 — ClientChatApp(+h)·R5 placeholder 치환·비동기 워커+토스트 (3파일) |
| T5 | `688514f3c3a5649aa725b36cbf3ebe9c88cdfb58` | feat(apps): 폰 구두 leg — Termux:API 녹음+whisper-cli — VoiceCaptureTermux.cpp 신설+양캡처 컴파일 가드 (4파일) |
| T5(정산) | `6128bb46d18aa12539a9c4cc7a199b14937f88f0` | test(probes): music 폰 probe — TW 게이트 채용+폰 캐논 계보 정산 (#111 잔여 원장행 2건 겸행 소각) |
| T5 fix r1 | `e9d13caf5405754159f5b8c569cf4b8d2d032b6c` | fix(apps): 폰 구두 캡처 Termux prefix resolve 폴백 — TERMUX_PREFIX env 부재 실측 수형 |
| T5 fix r2 | `036b41e66367d52235a398665d51f1cb47ace7b7` | fix(apps): 폰 캡처 Termux:API 실측 계약 — -f 개방/-q 종착/-i 세션 판정 — 스펙 "start/stop" 표기 상쇄 |
| T6 | `58d89e6b52e5f9d9778293ab31b7cb5f670dffce` | test(probes): 구두 probe — E2E 사슬 원문 — wsl_voice.sh 1008행+phone_voice.sh 563행 신설 — **VOICE-OK 2런·VOICE-PHONE-OK** |
| T6 fix r1 | `7cd8ae60c99eafa3c40013b5fc498d01dd732340` | fix(probes): 폰 VOICE-VERDICT 게이트 fc3 채용 — 리뷰 Important 1(게이트 비대칭 phone 재발) 소각+Minor 3건 소정리 |
| T7 | 본 문서를 포함하는 커밋 | docs(apps): STT 구두 as-built (T7) — SHA는 git log가 진실원 |

- 라인 체인 = **496a5e6(스펙) → 79a27d1(플랜) → 9512ba8(T1) → 049b94e(T2) →
  a5e65cf(T2 fix r1) → 1b828e2(T3) → 4279d96(T4) → 688514f(T5) → 6128bb4(T5
  정산) → e9d13ca(T5 fix r1) → 036b41e(T5 fix r2) → 58d89e6(T6) → 7cd8ae6(T6
  fix r1)**+본 T7 커밋 — push 전부 origin main 직행·amend 없음.
- 폐곡 계보: T1 리뷰 **clean**(deferred minors 4건 — §5) → T2 리뷰 **Important 1 →
  fix r1 a5e65cf → 재심사 CLEAN**(deferred 4건) → T3 리뷰 **clean**(deferred
  5건) → T4 리뷰 **clean**(deferred 5건) → T5 리뷰 **clean**(fix r1/r2 =런 중
  실측 수행 소각·deferred 6건·함정 12건 — §4) → T6 리뷰 **Important 1 → fix r1
  7cd8ae6 소각(재런 불요 — Minor 3건 동커밋 소정리)**.

## §1. 사슬 원장 (사용자 결정 2건+R1-R5 — 스펙 §0/§6b = 상위 진실원)

- **사용자 결정 2건(2026-10-25)**: ① **STT 엔진 = whisper.cpp 로컬(ggml)** —
  네트워크 불요·폰 단독(클라우드 STT·서버 전사 = v1 전부 비-목표) ② **v1 스코프 =
  조작+대화 포함** — STT 결과 문자는 기존 승격 파이프라인에 원문 전달·**새 자연어
  판정 계층 0** — ChatRouter/ChatRouteTurn이 단일 진실원(최종 그림 = 폰 DeX 꽉채움
  jkdesktop+구두 조작·환경꾸미기 시간 최소화 — 경험 도달 가속 몫).
- **R1**: whisper.cpp 소스 = **third_party vendor 커밋**(imgui 선형) — 폰 배포가
  engine HEAD git archive 1줄로 완결(별도 저장소 스테이징 공비 소각)·모델/바이너리는
  무추적+조달기 스크립트만 커밋. **R2**: 구두 표면 = **채팅 앱 [구두] 버튼+핫키**
  (별도 앱/전역 오버레이 아님 — 턴 기록 재용·전역 핫키의 포커스 밖 수취 문제 회피).
  **R3**: 폰 Termux:API 설치+마이크 권한 = **사용자 완료 확인(2026-10-25)** — 사용자
  손 유계 소각. **R4**: 모델 등급 = **폰 tiny-ko(74MB·원본 대비 CER ~11.6%)/Win
  small(466MB급·CER 17.5%) 확정** — medium/large 폰 배치는 비-목표(small 폰
  ~0.25× 실시간 — 15min→10min 원장). **R5**: 전사 대기 UX = **상태 행 "전사 중…"
  표기**(턴 기록에 전사 원문 도착 시 placeholder 치환 — 정직 원문 문화).
  **R1-R5 = 재량 판정 — 기각 시 즉시 수리 계약.**
- **사슬(전체)**: 발화 → [Win = SDL 캡처 장치별도 개방 / 폰 = Termux:API
  `termux-microphone-record` + ffmpeg `-ar 16000 -ac 1` 정규화] → 16kHz 모노 s16le
  WAV → `jk::voice::turn::RunVoiceTurn` 5단 사슬(§2.3) — fail-closed 게이트(Voice
  LegVerdict) → wav::Validate16kMono 재독 → run::WhisperTranscribe(whisper-cli 스폰
  +ParseLastText 절단) → **in-process ChatRouteTurn 직호출(새 파서 0)** →
  ChatActionQuery → 표면 소유 dispatch(launch_app/close_window/focus_window/
  list_windows → 창 서버) → 문자 표면(턴 기록 [나]=전사 원문·[시스템]=안내문+토스트).
  **전 사슬 폰 단독 완결·네트워크 불요** — chat.json 미구성 = 라우터 즉발+stub 폴백
  원존 → **"LLM이 아니어도 구두는 조작된다"**(폰 실측 원문 채용 — §2.5).
- **무음/오디오 충돌 계약(스펙 §3 ⑤)** — vplayer 조그 "세션 전 구간 무음" 사용자
  지시 원존은 **무변조**(구두 라인의 재생 계통 계약 변경 0)·Win 재생 동시 = WASAPI
  공유(캡처 장치는 별개 SDL 장치 — VoiceCaptureSDL과 재생 백엔드가 물리적으로 다른
  TU+장치 — T3 구조적 보증)·폰 = Termux:API 녹음은 Termux 소속 프로세스 마이크
  세션(vplayer/spatial audio_core와 별 세션)·**risk 원장**: 재생음이 마이크로 스며
  전사 오차 — push-to-talk(짧은 명령 발화) 완화 — **실측 재판정 = EYES ②**.
  수형 = **push-to-talk**(핫키+버튼 — VAD 상시 청취는 v2 재판정 — 환경꾸미기
  최소화 원칙의 직접 귀결).

## §2. 라인 원장 (부품별 as-built — 원류 원문 1:1 수취 · 코드 재실측 0)

### §2.1 whisper.cpp vendor+조달기+WHISPER_ROOT 판정부 (T1 — 9512ba8)

- **v1.9.5 vendor 149파일(~11.3MB — 독립 재실측 등호)**: GitHub releases API
  `prerelease:false` 최신 안정 실측(v1.9.5·2026-10-06 — **브리프의 "1.8.x 계열"
  가정 상쇄** — "현시점 최신 안정" 우위·독립 재실측 등호 — 비용=업스트림 버전 이동
  재조달). commit 뒤 `cmp -s` 전수 — vendored 148파일 vs 업스트림 tag 체크아웃
  **DIFF_COUNT=0**(engine-owned CMakeLists.txt 1파일만 제외)·LICENSE 동봉(MIT —
  ggml authors). 폐기 근거 = 17 비-CPU ggml 백엔드 dir(bl·cuda·metal·vulkan 등)
  전부 `GGML_<NAME>` 옵션 기본 OFF — 폐기 후 도달 불가(무해)·kleidiai도 옵션 OFF
  (ON 시 FetchContent 네트워크 → 폰 네트워크 0 원존과 충돌 — 영구 OFF 가정)
  `miniaudio.h(4.1MB)·stb_vorbis.c는 보존` — common-whisper.cpp 무조건 include.
- **엔진 소유 CMake(유일 비-업스트림)**: `add_subdirectory(ggml)`(arch 선택은 ggml
  자신 — 726행 분기 재현 금지)·**`set(BUILD_SHARED_LIBS OFF)`** — WSL DrvFs(I:)
  symlink 금지 실측(`cmake_symlink_library: Operation not permitted` — T1 함정
  #1)·`whisper`(static)·`whisper-common`(상명 충돌 리네임)·`whisper-cli` 타깃 1종
  — server/quantize 등 폐기.
- **WHISPER_ROOT fail-closed(스폰 §3 ②·spatial-player 선형 계승)** —
  `if(DEFINED ENV{WHISPER_ROOT}) add_subdirectory(...)` — 미설정=빌드 off(분기
  사망)·설정=whisper-cli 타깃. 무변조 원문 = env unset scratch configure 뒤
  `grep -c "whisper-build" build.ninja` = **0(Win+WSL 동일)**. 2v 판정부는
  **env와 무관 매빌드 컴파일**(2n 선형 — 미설정 빌드에서도 판정부 산다).
- **조달기 `engine/tools/probes/install_stt_models.sh`(멱등 — 부재 시에만 네트워크
  — 실측 3런: 다운로드+sha/size 검증 OK→`ALREADY-PRESENT (no network use)`→cached
  OK)** 축 판정: Win=build/stt-models(small)·WSL=buildwsl/stt-models(small)·폰
  aarch64=buildterm/stt-models(tiny-ko). **sha256 핀(HF LFS oid — API tree 실측·
  다운로드·검증 재실측 등호)**: Win/WSL small = `ggerganov/whisper.cpp`
  `ggml-small.bin` **487,601,967 B(466MB급 — 스펙 표기~466·T1 리포트 표기 465 — 산법 차)**
  sha256 `1be3a9b2063867b937e64e2ec7483364a79917e157fa98c5d94b5c1fffea987b` /
  폰 tiny-ko = `wabisabisocial/whisper-tiny-korean-ggml`
  `ggml-tiny-ko.bin` **77,691,730 B(74MB)** sha256
  `dd6d9f46a7d292bb2ee4725ccb132bedde690ce2ee83e437a7786f8c67c41d08` — 불일치 =
  재조달(부분 다운로드 무음 수용 금지). tiny-ko 핀은 폰 미접촉 유계 속에서
  `STT_MODEL_DIR` 오버라이드로 WSL 쪽 조달기 실측으로 검증.
- **2v-a 판정부 4건**(`jk::stt::VoiceLegVerdict(envSet, binExists, modelExists)` —
  reason 원문 표: `구두 끔 (WHISPER_ROOT 미설정)`·`구두 끔 (whisper-cli 부재)`·
  `구두 끔 (whisper 모델 부재)`·`구두 켬 (whisper-cli)` — 스폰 0·TDD red 원문
  실측). 캐논: Win 827/WSL 804(+2v 4·산술 일치 — 823+4·800+4·2u 132 등호). CANON
  밖 실전사 런 원문: 합성 16k 440Hz 비프 → Win whisper-cli(small) = `(BEEPING)`
  encode 3.6s rc=0.

### §2.2 전사 사슬 순수 부품+스폰 (T2 — 049b94e+fix r1 a5e65cf)

- **배치 원존 실측 승계**: 순수 부품 = 헤더 인라인(GalleryModel.h/MusicModel.h
  "Header-inline on purpose" 원존)·도메인 dir 쌍 규약 → `engine/include/voice/
  SttVoice.h`+`engine/src/voice/SttVoice.cpp`(스폰 leg TU — JKLmEngine 선형).
  계약: `jk::voice::wav::Validate16kMono`·`jk::voice::transcript::ParseLastText`
  (헤더 인라인)·`jk::voice::run::WhisperTranscribe(wav, model, out, err)`(TU).
- **wav::Validate16kMono** — RIFF/WAVE 수동 파독(std만·라이브러리 조달 0): 4계약
  상수(PCM 1·모노 1·16000 Hz·s16le 16)+5규약+파생 정합 검증(blockAlign=ch*bits/8·
  byteRate=rate*align)·n<44 = 최소 캐논 미달 거부·보조 청크 스킵+2바이트 패딩·
  잘린/무한 청크 = 정직 거부·**리샘플 재합성 금지** — 정규화는 ffmpeg(폰)/SDL 개방
  지정(Win) 몫.
- **transcript::ParseLastText** — whisper-cli stdout 원문에서 `[ hh:mm:ss.mmm -->
   hh:mm:ss.mmm ]  텍스트` 세그먼트 구조행만 셈 → **최후 세그먼트 텍스트 절단
  반환**(\r 제거+트림)·비적합 행 무시·빈/이상 출력 = 빈 문자열·오류 없음
  (ChatLlmActionParse 동형 "방어 원료" 규약).
- **run::WhisperTranscribe** — jk::process 어댑터만 접점(SDL 접점 0·Win/폰 무분기):
  Spawn(inheritedStdioPipes=true — stdout/stderr 분리)·CreateKillOnCloseJob+
  AssignToJob·peek/read 드레인 루프(20ms·broken 109/232 판정)·TerminateJobTree
  마감·WaitForExit(5000)+rc 수확. 인자 = `-m <model> -f <wav> -l ko`(-o* 플래그
  금지 — T1 함정 #3 `-ot` stoi 크래시 봉인)·`-l ko` = 한국어 중심 고정 핀(cli 기본
  "en"은 한국어 발화 조용히 망각 — 재판정은 EYES). 바이너리 resolve = exe 형제
  `whisper-build/whisper-cli(.exe)`(Win build/·WSL buildwsl/·폰 buildterm 공통
  모양)+`WHISPER_BIN` 오버라이드. **타임아웃 = 전체 마감 1종(600s — JKLmEngine
  10분 kill 선형)** — **fix r1 원장**: 마감 판정이 드레인 루프 `if(!progressed)`
  분기 안에 있어 stderr 진행률이 흐르는 동안 마감이 불지 않는 **idle-kill 퇴화**
  (선언 계약 부정합 — 리뷰 Important) → **판정을 매 루프 무조건으로 이동**(idle-kill
  폐기 — 단일 마감 600s 1종·헤더 계약 표기 동일 갱신) — 재런 833/810 등호+스폰
  회귀 probe 등호. stdout 실측 원문(CANON 밖): 1.2s 440Hz 합성 →
  `[00:00:00.000 --> 00:00:01.260]   [구독 / 좋아요]` rc=0 — **T2 원문 = 이후
  전태스크 대차 원문**(probe chain==direct 등호의 강도 축).
- 2v-b/c 6건(⑤16k 합격·⑥44100 거부·⑦스테레오 거부·⑧고정 출력 2세그먼트 최후
  절단 등호·⑨빈 출력 빈 반환·⑩malformed 빈 반환 — 전사 비결정 스폰 어셜션 0).
  캐논: Win 833/WSL 810(+6·산술 일치).

### §2.3 캡처 수형(Win SDL)+RunVoiceTurn 접점 (T3 — 1b828e2)

- **VoiceCaptureSDL.cpp 신설 TU**(이 TU만 SDL 접점 — 캡처와 재생 코드가 물리적
  다른 TU+장치 = 무음 계약의 구조적 보증): `SDL_OpenAudioDevice(nullptr, 1,
  &want, &have, 0)` — want={16000·AUDIO_S16LSB·1·1024}·**allowed_changes=0** =
  SDL 변환까지 맡아 obtained가 계약 spec과 일치·`have` 이중 검증 — 비적합 = 즉시
  닫는 정직 거부(조용한 48k 수용 없음 — 리셈플 코드 0)·재생 장치와 별개(iscapture=
  1 기본 캡처 — WASAPI 공유로 음악 재생 중 구두 동시 성립의 Win 축 근거)·수확
  순서 = PauseAudioDevice(1)→LockAudioDevice(진행 중 콜백 완료 대기+배달 봉인)
  →pcm.swap(g_buf)→Unlock→Close — 수확 누락 0·부분 기록 = 절반 WAV 즉시 remove
  (잔산 금지 — 2u-⑦ REMNANT 선존 동형). 정직 거부 어휘: `구두 캡처 개방 실패
  (SDL: …)`·`구두 캡처 spec 부적합 (16 kHz 모노 s16le 불성립 — 정규화 사전 계약)`·
  `구두 캡처 이미 진행 중`·`구두 캡처 빈 결실 (PCM 0바이트)`·`구두 캡처 wav 쓰기
  실패(부분 기록)`.
- **RunVoiceTurn 5단 사슬(스펙 §3 ③ 안 A — in-process 직호출·IPC 0)**: ① fail-
  closed 게이트 `VoiceLegVerdict` — T1 reason 4행 원문을 그대로 err 승계·
  **WHISPER_ROOT env = 런타임 게이트 진실원**(모델 dir = 조달기 진실원 — exe 형제
  `stt-models/*.bin` 선두 .bin 결정론 핀+`STT_MODEL` 오버라이드 — T1 배선과 같은
  env의 두 축을 정합 배선한 수형이 리포트 근거로 원장) ② wav 재독 Validate16kMono
  ③ run::WhisperTranscribe ④ **`jn::ChatRouteTurn(전사 원문, cfg, &usedLlm)`
  in-process 직호출** — 정규화는 라우터 NormalizeText·행동 파싱은 ChatLlmAction
  Parse 몫 — **새 파서 0**("STT 텍스트 == 타이핑 텍스트" 접점 계약) ⑤
  `ChatActionQuery` = ClientChatApp::Submit 스위치 원문 수형(Launch 비어면
  `{"app":...}`·무앱 Close/Focus = argless `"{}"`·Info = 질의 0)·JsonEscapeStr
  쌍둥이 원문 수형. dispatch는 **표면 소유**(채팅 앱 SendQuery/jkdesktop
  JKAgentClient 각자 자기 통로)·실패 = `구두 도구 지시 전달 실패 (<tool>)`·
  **guide 유지+err 병기**(T4 리뷰 접선 — 소각행 삭제 — "지시의 의도와 결과를 둘
  다 남긴다").
- **libgomp 스풀 원장(런타임 수형 — 실측 3런)**: whisper-cli 는 ucrt64 PATH 없이
  rc=127(libgomp-1.dll 미해상 — T2 함정) → 더블클릭 jkdesktop 모사 stripped PATH
  런 = rc=127 재현 → **ucrt64 런타임 DLL 4종(libgomp-1·libgcc_s_seh-1·libstdc++-6
  ·libwinpthread-1)을 exe 형제 whisper-build/ 스폰 = rc=0·전체 전사 등호** → 수형
  = CMake `whisper_cli_runtime_spool` custom target(MINGW 한정 — DLL 로더 1순위 =
  application directory — PATH 의존 완전 소거·산출 무추적)·**T4 라이브 smoke에서
  stripped-env 계통(클라 호스트 프로세스 = 서버 스폰)의 실 전사 성립 = 스풀 규약
  최종 근거**.
- 2v-d 4건(⑪"지뢰찾기 켜줘"=Launch(minesweeper) 즉발·usedLlm=false·⑫launch_app
  질의 args 등호·⑬"창 목록"=list_windows·⑭"닫아줘"=argless close_window) —
  1n-f14/15/16 계열의 **입력 소스 치환 동형**. E2E scratch probe(CANON 밖): 전사
  원문 등호+fail-closed 3판 원문+launch-query 등호 — probe 자체 결함 1건
  self-fix(printf 인자 평가 순서 — 조립 이후 인쇄). 캐논: **Win 837/WSL 814/
  posix 304 실측**(+4·+4·304 등호 무접촉 재판정).
- **WSL selftest stdout 수취 함정(신설 원장행 — 실측 3런)**: Windows 쪽 리다이렉트
  `wsl.exe … > file` 형태 = PASS행 일부가 겉의 진단 행과 섞여 소각(797/814 — 0
  FAIL 동일) → **리다이렉트는 WSL 안 파일로**(… > /mnt/i/…/log 2>&1) → 814 등호
  원문. probe stdout 수취 5종 보존 원장의 승계행 — 이후 전태스크 계약.

### §2.4 chat [구두] 표면 (T4 — 4279d96)

- **[구두] 버튼 토글**(입력 창 전송 우측 SameLine): 캡처 비중="구두" 눌림=
  StartVoice()·캡처 중 라벨 "마침" 눌림=FinishVoice(). **핫키 F6 = hold-to-talk**
  (PreProcessMessage — KeyDown/KeyUp 직판): 누름=StartCapture·해제=StopCapture→
  WAV→비동기 전사. **핫키를 토글로 두지 않은 근거(repeat 원문 2점)**: 서버는 repeat를 승계
  전송(JKWindowServer.cpp:2146 `payload.detail = ev.key.repeat`)·TranslateSDLEvent
  는 그 플래그를 JKEvent에 승계하지 않아(JKEvent.cpp:59) 클라 토글은 지속 누름의
  자동재키 repeat KeyDown에 진동할 수 있다 → hold(캡처 중 repeat = no-op)가
  repeat를 구조적으로 무해화 — 토글 계약(R2 얼굴)은 버튼이 담당. 핫키 충돌 검사
  0(채팅 표 F키 실측 0·타 앱 F6 = 별도 창·별도 앱 프로세스라 병존 무해).
- **R5 치환(placeholder)**: FinishVoice가 `"(전사 중…)"` [나] 턴을 push+슬롯 번호
  큐잉 → 전사 도착(DrainVoice) 시 그 슬롯 text를 **전사 원문으로 치환(mine 유지)**
  ·실패 시 동일 슬롯을 `[시스템] [!] err` 원문으로 치환(append-only 벡터 — 슬롯
  안정)·캡처 정지 실패 = 턴 0건+상태행 정직 거부(기록 부풀림 금지). 상태행 2행:
  status_(기존)+voiceStatus_(신설 — 캡처 중 `녹음 중… (F6 해제 또는 [구두] 버튼으로
  마침)`→`전사 중…`→클리어).
- **비동기 사슬**: 단일 상시 워커 스레드(OnInit 1회 성 — Music ScanWorker 수형+
  파괴 join)·**FIFO 우편함**(voiceJobs_ — 연속 발화 순서 보존·버림 금지)·**질의는
  UI 축 전용통로** — 워커 dispatcher는 voiceOutbox_ 적립만, DrainVoice(OnIdle)가
  UI 스레드에서 SendQuery(**Surface를 워커 스레드에서 만지지 않는다** — 스레드
  계약 보전)·launch_app 시 launched 플래그 → 앱 실행 토스트. cfg는 워커 축에서
  조달 — **chat.json 구성 여부 무관 작동**(미구성 = 라우터 즉발+stub 폴백)·
  fail-closed 사전 판정(StartCapture 전 VoiceLegVerdict — 거부 시 reason 원문
  상태행+토스트 — 마이크 개방 낭비 전 정직 거부)·임시 WAV 정산 remove(표면 소유
  경로)+OnClose 캡처 중 종료도 StopCapture+remove.
- **토스트 재용**: publish_event → agent.notify(jkctl 원문 경로)·단문 2종(①인식
  실패 — err 원문 body ②앱 실행 지시 전달)·**id 0x40000000 대역 별도 카운터+
  pending_ 미적립** — PollReplies의 mine 체크 밖("남의 답신을 흘려보낸다" 계약
  재용 — 기록에 publish_event 답신 JSON 부풀리지 않음). idle 계약(#89 —
  더티조건화): 폴링 무더티(큐 빈 확인 1검 원자적 early-return)·더티 = 상태
  전이+결과 도착+기존 입력 — Timer 틱 배송일 뿐(구두 비활성 무소득 0).
- **라이브 smoke 원문(CANON 밖 — 2부)**: (A) probe — 실기기 마이크 개방+수확
  `capturing=1`·`wav=…\Temp\jkchat_voice_smoke.wav bytes=94252`(≈2.94s@16k)·
  RunVoiceTurn 전 사슬 성립 — 주변 무발화 3s에 whisper small이 **무음 환각**
  ("구독과 좋아요 부탁드려요!")을 내놓은 것 = **전사 비결정 원칙의 정직 결과** —
  실발화 게이트는 EYES 몫. (B) 표면 E2E — 라이브 jkdesktop+클릭 드라이버로 [구두]
  버튼 시동/마침 → 캡처 원문 3종(rec 상태행 "녹음 중…"+라벨 마침→transcribing
  "전사 중…"→done 기록 `[나] (구독 / 좋아요)`+[시스템] guide(usedLlm=1 — chat.json
  구성 기기 LLM 런 실측)+상태행 클리어) — **스풀 규약 최종 재판정 성립**. F6
  SendInput 주입 실패 런 1건(포그라운드 경합 핀점 — 클릭 드라이버 전환 수형) ·
  파괴 join 블록(전사 진행 중 창 닫힘 = 남은 전사만큼 join — v1 수용 · 폐곡 대상
  아님 원장). 캐논 837/814 유지.

### §2.5 폰 구두 leg (T5 — 688514f+6128bb4 정산+fix r1/r2)

- **VoiceCaptureTermux.cpp 신설** — capture 계약의 폰 수형: `#ifdef __ANDROID__`
  본체·SDL 수형은 `#if !defined(__ANDROID__)` — **컴파일 축 가드(한 바이너리 캡처
  수형 1개만 — 심볼 충돌 없음·그 밖 축은 빈 TU)** → Win/WSL 캡처 동형·2v 등호
  보존(WSL 814 등호 재판정 — 이 태스크 빌드 0). 스폰 leg = jk::process 어댑터만
  접점(동형 드레인 루프 — start/stop 15s·ffmpeg 60s). **MediaRecorder는 컨테이너를
  확장자로 결정해 .wav 녹음 불가** → 레코더 예약 경로 = `wavPath+".rec.m4a"`(정지
  후 소각 — 절반 잔산 금지 동형) → ffmpeg `-y -i <rec> -ar 16000 -ac 1 -c:a
  pcm_s16le <wav>` 정규화 → wav::Validate16kMono 재독 → RunVoiceTurn(동형 계약).
- **Termux:API 실측 계약(스펙 표기 상쇄 원장)**: 실측 usage 원문에 **start/stop
  어휘가 없다** → **-f 개방(`-f <rec> -l 60`)/-q 종착("Recording finished: <file>"
  finalize)/-i 세션 판정(`{"isRecording":…,"outputFile":…}`)** — fix r2 수형.
  `stop`은 usage를 인쇄해 세션이 생존·`-f <file> start`는 기존 세션 활성 시
  "Recording already in progress!"를 인쇄하고 **rc=0(성공 위장)** → 사전 판정(-i)+
  사후 판정 1눈(-i 재독 = isRecording+outputFile 대차)으로 소각. Termux 마이크
  세션은 **기기 전역 1개** — 같은 예약 경로 잔상 = 내것 정산(-q 후 재개방)·다른
  outputFile = 정직 거부(남의 세션 자동 -q 금지). **fix r1**: TERMUX_PREFIX env는
  ssh 셸에 부재($PREFIX만) → 2종 폴백 resolve.
- **폰 배포 원문(t112_t5_deploy.sh — 5런, 최종 T5-DEPLOY-OK)**: 전수 git archive
  HEAD — **third_party/whisper.cpp 149파일 = 폰 첫 조달(R1 vendor 실증)**·
  SWEEP-POST-DIFF=0(blob 등호)+**WHISPER_ROOT 신설 빌드** — CMAKE-RC=0·
  WHISPER-BUILD-NINJA-ENTRIES=297(분기 생존)·NINJA-RC=0 SECONDS=451~466(aarch64
  312 target — whisper-build/whisper-cli 포함·whisper-cli 17,948,320 B)·MUSP-LEG-SYM
  alcOpenDevice=3(폰 spatial leg WIRED 보존 — #110 계약)·**조달기 폰 재용(다운로드
  74MB 원문)**: `downloaded+verified file=ggml-tiny-ko.bin size=77691730 sha256=
  dd6d9f46…` — R4 핀 등호·STT-MODELS-SETUP-OK. **서버 재기동 = WHISPER_ROOT 보유
  부팅(구두 게이트의 클라 상속 계약)+Music 창 복원** — settings 213B/permissions
  33B 무변경 바이트 등호.
- **폰 [구두] 표면 E2E 원문**: chat 창 id=5(720×540)→ready 프레임 [구두] =
  **(835,602)**(btn_zoom 원문 — 이후 T6 5회 재측정 착탄 등호)→탭 = send_input
  (permissions 병합/핫리드 — T5 함정 #7)→버튼 라벨 **마침 전환**+상태행 "녹음
  중…"+`-i` = `{"isRecording": true, "outputFile": "…jkchat_voice_25993_0.wav.rec.m4a"}`
  — 표면→StartCapture→Termux:API 세션의 라이브 원문 삼점 → 마침 → 기록:
  `[나]` = 전사 원문(R5 치환 — 무발화 비결정 "click" 환각)+`[시스템]` = stub
  안내문(라우터 미매치+cfg 미구성 폴백 — **"LLM이 아니어도 구두는 조작된다"의 폰
  실측**)+isRecording=false.
- **#111 잔여 원장행 겸행(6128bb4)**: ①**spatial_mix 회수 성립 원문 — RECOVER-OK
  실측 소각**(recover_opt fix 후 첫 재런 원문 — #111 T6 Important 유계 종착 ·
  합산 본판정은 PARTIAL 원장행 유지 = #110 계보 등호) ②**TW 게이트 채용 정산** —
  MUS-TW-SUMMARY-2에 더해 **TW_OK 본조건에 cover_rt==1·apic_keep==1·md5 등호
  채용**+TW-FAIL 비등호 사유 열 — 런 원문 `MUSIC-TAGWRITE-PHONE-VERDICT:
  TW-OK(…게이트 채용(T5 정산)…)` ③**폰 캐논 승계 795→814** — 상승분 19 = #111
  최종 fix 5(795→800)+2v 14(#112 T1-T4) — `CANON-INCLUSION=MUSIC-FULL-2P2QRS-19
  (폰 캐논 795→814 상승…WSL 814 등호)`·selftest A/B = **rc=0 PASS=814 FAIL=0
  2u=132 2r=10 2s=20 2v=14 2런 등호**. 폰 probe 원문(t112_t5_voice.sh — clang++
  직링크 RC=0): gate 3판 원문 등호(WHISPER_ROOT unset·bin 부재·model 부재 =
  rc=3 usage — **STT_MODEL 오버라이드는 존재 검사 밖** 규약)+mic leg 사슬
  (mic-start=1→capturing=1→mic-stop=1→mic-wav-bytes=155726→mic-turn ok=1 —
  무발화 "." 환각 = 비결정 정직)+`fixture-direct=1` = **사슬==whisper-cli 직접
  스폰 등호(폰)** — 전사 축 원문.

### §2.6 구두 probe 2종 (T6 — 58d89e6+fix r1 7cd8ae6)

- **engine/tools/probes/wsl_voice.sh 신설(1008행 — 브리프 명시 6요소 전부 탑재)**:
  원존 수혀(wsl_music.sh 전체 수형 — #98 runner 가드·bracket pkill·stale 소켓 rm
  가드·permissions 스테이지/스냅샷·X11 캡처기 전문·REMNANT 가드)+fixture 양성
  대조 사슬+fail-closed 3판+**`VOICE-RUN-VALS` 요약 스트림+`VOICE-VERDICT` 1행**
  (MUSIC-TAGWRITE-PHONE-VERDICT 동형 — stdout 수취 부재 재발 방지 봉인 승계)+canon
  selftest 수취(WSL 814·2v 14)+캡처 원문(F6+클릭). **런 원문 2런 등호 — 두 런
  VOICE-OK**: `VOICE-RUN-VALS: run=2 canon=814 twov=14 fc3=3 direct=1 direct_t2=1
  stt_t2_canon=1 fixt=1 keyturn=2 clickturn=2 route=1 probe_rc=0 captures=…` —
  fc3 3판 등호·**chain==direct(자가 fixture+T2 승계 2축)**·**T2 원문 등호
  [구독 / 좋아요]**(Win 계보 → WSL 재실측)·F6 hold-to-talk 표면(키 축 — WSLg SDL
  캡처 개방 성립 `VOC-WAV /tmp/jkchat_voice…_0.wav 194604 bytes`+TRANS 기록 도착
  diff_pixels=1398)·[구두] 버튼 클릭 축(자가 캘리브레이터 — 상수 종속 제거 —
  버튼 = 클라 (664,478))·**launch_app 질의 소비 = 도구 경로 실창 원문**
  (`QUERY-DISPATCH {"ok":true}`→`{"id":14,"title":"Minesweeper",…}`.
- **engine/tools/probes/phone_voice.sh 신설(563행 — T5 원문의 형식화)**: probe leg
  (mic leg+gate 3판+fixture 사슬==직접 대조)+WHISPER_ROOT 보유 부팅 chat 개봉+
  [구두] 탭 절차(permissions 병합/원복)+캡처 md5 등호 회수+기록 도착 proxy(폰 잔상
  0) — **폰 배포 드라이버 신설 0(계약 — T5 t112_t5 계열 재용)**. 런 원문
  (검증+수동 재측정 — 총 5회 [구두] 좌표 착탄 전부 등호): **`VOICE-PHONE-VERDICT:
  VOICE-PHONE-OK`** — gate 3판 등호·Termux:API 마이크 leg·fixture 사슬==직접
  스폰·탭 표면 캡처 md5 등호+**기록 도착 바이트 증가(+21,275B = [나]/[시스템] 행
  렌더 영수증)**·chat close+Music 복원+permissions 33B 등호+폰 잔상 0.
- **fix r1(7cd8ae6 — 리뷰 Important 1)**: phone 축 VERDICT 게이트가 gate-model-
  missing 원문 등호(fc3)를 판정 부울에 미반영 — **fc3 미일치 런도 VOICE-PHONE-OK
  인쇄 가능**(WSL 축은 FC=3 — 게이트 비대칭 재발) → `FC3` 검사 채용+GATE_OK
  연쇄 — **재런 불요 근거**: 런 원문의 fc3 행 원문 존치 = 수정판 게이트 재판정
  등호. Minor 3건 소정리(죽은 rm 행 인용 glob 복원·wsl_voice.sh inert 폴백 좌표
  제거(CAL_OK=0 = 클릭 세그먼트 생략 — 상수 종속 소거 원장 정합)·VOICE-RUN-VALS
  헤더 실제 필드와 일치). 커밋 본문 오탈자 "Minesepper" 1건 = **amend 금지
  계약으로 원문 보존 원장행**.

## §3. 캐논 계보 (2v 계열)

| 커밋 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#111 종착 818/795 + 최final fix 5 f33d41a) | 823 | 800 | 304 | (795 — fix 5분 800 승계 대기) | 2u 132 |
| T1 `9512ba8` | **827** | **804** | (승계) | 795 | **2v 4** (①-④ fail-closed 판정부 — +4/+4 동증분·산술 일치) |
| T2 `049b94e`+fix r1 `a5e65cf` | **833** | **810** | (승계) | 795 | **2v 6** (⑤-⑩ — 누계 10·fix r1 등호 유지) |
| T3 `1b828e2` | **837** | **814** | **304**(실측) | 795 | **2v 4** (⑪-⑭ 접점 — 누계 14·posix 재판정 등호) |
| T4 `4279d96` | 837 | 814 | 304(승계) | 795 | 0 (UI 태스크 — 신설 어설션 0 계약·3축 등호 유지) |
| T5 `688514f`+`6128bb4`+fix 2건 | (무접촉 — 빈 TU) | 814 (등호 기준원) | (승계) | **814** (2런 A/B 등호 — 795→800+2v 14 승계) | 0 (폰 leg — 캡처 수형 __ANDROID__ 가드) |
| T6 `58d89e6`+fix r1 `7cd8ae6` | 837 (등호 재실측) | 814 (probe 내장 2런) | 304(승계) | 814 (T5 계보 무접촉 보존) | 0 (probe 전용 — CANON 밖 캡처 원문) |
| T7 본 커밋 | 837 | 814 | 304 | 814 | — (docs 전용 — 재실행 불요) |

- **캐논 = Win 837/WSL 814/posix 304/폰 814 — 2v=14·2u=132·2r=10·2s=20**. 기존
  계보 원수치 보존: 2m=26·2n=17·2o=16·2p=7·2q=7 변조 0. 캐논 증분은 전부 Win/WSL
  양축 동증분(4/6/4 — 플랫폼 조건 분기 0 계약)·posix는 하네스 TU 미링크(own
  main.cpp)라 신설 어설션 0(구조 무접촉 원존 — T3에서 304 실측 재판정)·**폰 =
  WSL 등호(A/B 2런 — 2P2QRS-19)**. whisper 실전 전사 원문(전사 본문)은 **전부
  CANON 밖 캡처/probe 원문** — selftest 녹임 0(스펙 §2.4 계약). 스펙 §3 ⑦의
  캐논 승격 계약("2v N건 — 계보 표 승수 행 표기")은 본 표로 이행.

## §4. 함정 원장 (T1-T6 종합 — 봉쇄 레슨 축 · 각 리포트 전량 수취)

1. **T1 5건**: ①WSL ggml SHARED 기본+DrvFs symlink 금지(symbolic link Operation
   not permitted) → BUILD_SHARED_LIBS OFF ②첫 엔진 빌드 jkagentd.exe 잠금
   Permission denied(잔류 exe — taskkill 후 재빌드 — **빌드 전 잔류 exe 점검
   계약 원장행**) ③whisper-cli `-ot tmp` stoi 크래시(출력 플래그 파싱 — 스폰
   계약은 -o* 플래그 전부 배제) ④du -sm 왜곡(vendor 판정은 find 바이트 합산) ⑤
   폰 tiny-ko 조달은 폰 미접촉 → STT_MODEL_DIR 오버라이드로 WSL 조달기 검증.
2. **T2 5건**: ①whisper-cli 직접 실행 rc=127(libgomp-1.dll — MinGW PATH 렛슨 →
   T3 스풀로 근본 수형) ②python wave float frames 인자 오류 ③bash 한 줄 eval
   redirect 확장 함정 ④MinGW scratch probe는 jkcore 정적 lib 없이 어댑터 TU
   직링크 수형 ⑤`grep -F -E` 동시 사용 = conflicting matchers(IP 스캔은 -E 1종).
3. **T3 7건**: ①add_custom_command POST_BUILD는 하위 dir 스코프 타깃에 부착 불가
   (was not created in this directory — custom TARGET+DEPENDS 수형) ②헤더 파일명
   오탈자 `JKLmEngine.h`(실물 = JKLlmEngine.h — 이 파일시스템 대소 구별 실측) ③
   WSL SHARED 재실측(STATIC 유지 무사) ④**WSL selftest stdout 수취**(WSL 안
   리다이렉트 — §2.3) ⑤probe printf 인자 평가 순서 미지정(self-fix) ⑥probe exe
   위치 = exe 형제 진실원의 함정 면(tmp 런 = 부재 판정이 정답) ⑦MinGW PATH
   렛슨 재실측(ucrt64 PATH 선행 표준).
4. **T4 6건**: ①**SDL_main 매크로**(`main`→`SDL_main` — `#undef main` 1행) ②
   GetCaptureState = 캡처 중 예약 경로 진실원(정지 후 g_wavPath clear — 표면/F6
   는 캡처 중 독해 순서) ③**F6 SendInput 주입 실패 런(포그라운드 경합) — 잔여
   원장행(EYES 재판정)** ④WSL 수취 함정 승계 ⑤파괴 join 블록(전사 진행 중 창
   닫힘 — v1 수용·폐곡 대상 아님 원장) ⑥placeholder 첫 그림 1프레임 지연(관측 —
   원존 — 라이브 원문(done 캡처)으로 정산 — #89 계약 내 동작).
5. **T5 12건(원문 축약 — 리포트 §4)**: ①원격 스크립트에 `pkill -f '[j]kdesktop'`
   브래킷이 "자기" ssh 세션을 즉사 — **원격 스크립트=파일 원존의 이유 재실측**(
   부트/탭 절차를 폰 측 파일로 발행+rm -f -- "$0" 자기 소각) ②Termux:API 어휘
   실측 상쇄(-f/-q/-i — §2.5) ③TERMUX_PREFIX env 부재($PREFIX — fix r1 폴백) ④
   whisper-cli 무인자 런은 version 행 없음(cold-run 게이트 수형) ⑤폰 트리에
   engine/tools 부재(git archive 커버) → 조달기 스크립트 별도 push ⑥Windows
   python은 MSYS 경로를 풀지 못한다(cygpath -w 수형 승계) ⑦send_input 파일 부재
   기본 = Ask(conquest ladder) — 승인 구속자 없는 세션 파킹 **무한 봉쇄**(
   permissions.json 병합(핫리드)+END 바이트 원복 수형 — PERM-RESTORED-OK 33
   bytes md5 등호) ⑧xtap(XTEST) = 폰 X 계통 결손 원장(#110) 재실측 → send_input
   폴백 ⑨**-l 60 상한 미발동 관측 — 상한 안전판 의존 금지, StopCapture의 -q가
   종착 진실원(잔여 원장행)** ⑩언포커스 창의 send_input 2번째 연속 클릭 미도달
   의심+폰 1fps 폴백 렌더 스타일이 캡처 판독을 흐린다(**상태 판독의 진실원 =
   termux -i+캡처 2장 이상 대조**) ⑪배포 재런 = git archive mtime 갱신 → 매
   재런 aarch64 전체 재컴파일(~8분 — RUN-DROPPED 증분 무성립 원장) ⑫**STT_MODEL/
   WHISPER_BIN 오버라이드는 verdict의 존재 검사 밖**(부재 경로를 통과 → 스폰
   leg rc=3+usage로 정직 실패 — 2v-③과 별개 축 — 수형 비-목표 = 게이트 계약
   변조 방지).
6. **T6 12건**: ①`[ -n "$OK_LINK" ]` 0 참 함정(-eq 수형) ②할당문의 glob 미확장
   (`pwd)/*.bin` 리터럴 → 모델 부재 rc=3 위장 — `ls | head -1` 수형 — §2.5 함정
   ⑫과 같은 계통 별판) ③한 줄 내 후방 참조 = set -u unbound 즉사 ④**좌표계 2종
   혼입(send_input = 데스크 논리 좌표 vs 캡처기 = 서버 X 창 전체 프레임 — 데스크
   원점 = 프레임 (39,59))** → window_move(0,0) 고정+전체 프레임 차분기 수형 ⑤
   [구두] 버튼의 WSL 좌표 = 폰 원문과 다른 축(폰트 폭 편차 ±20px) → 런 내 자가
   캘리브레이션(상수 종속 제거 — 정찰 원칙의 채팅 수형) ⑥스폰 뒤 첫 합성 클릭 =
   무음 소멸(2번째 클릭부터 착탄 — #97 렛슨 · F6 키는 첫 입력부터 도달) ⑦**채팅
   InputText의 합성 타이핑 = 캐럿만 성립(본문을 삼키지 않음 — typed route =
   도구 경로 소비로 대체 — 실사용자 타이핑 = EYES — 잔여 원장행)** ⑧WSLg SDL
   캡처는 독립 프로세스에서 무사(WSLg 마이크 leg 자체 성립 실측) ⑨폰 [구두] 탭
   2번째 연속 클릭 미도달 재현(런간 전이 귀속 수형 — 상태 판독 진실원 = termux
   -i) ⑩**폰 .rec.m4a 잔존 1건 — "정산 — 미들 컨테이너 잔산 금지" 계약의 1회
   미달(remove 경로는 존재 — 도달 못한 분기 규명 = 재측정 후보 — 잔여
   원장행)** ⑪스테일 소켓 잔존(서버 부재 확인 뒤 rm) ⑫원격 스크립트 자기 소각
   (rm -f -- "$0") → **push는 런마다 수행**(run2 push 누락 = "no such file")·
   원격 /tmp 쓰기 불가(TMPDIR 캐논) 재실측.
7. **fix 원장**: T2 fix r1(마감 판정 이동 —idle-kill 퇴화 소각 — §2.2)·T5 fix
   r1/r2(Termux 실측 계약 — §2.5)·T6 fix r1(fc3 게이트 채용+Minor 3건 — §2.6)·
   probe 자체 결함 self-fix 2건(T3 printf 인자 평가·T2 스폰 probe)·T6 커밋
   본문 오탈자 "Minesepper"(amend 금지 계약으로 원문 보존 — 원장행) — **런 중
   probe 파일 편집 금지 원장행**(#111 재발 방지
   봉인 승계 — T6는 런 사이 교정만: run1 종료 뒤 좌표계 교정).

## §5. Deferred/park (각 리포트 minors 전량 — 소등 후보)

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T1 minors ① | third_party AUTHORS 주석 오차(v1.9.5 표기 계열) | N | 다음 vendor 재동기화 커밋 몫 |
| T1 minors ② | parakeet-config template 잔존(폐기 누락 후보) | N | 다음 접촉 정산 |
| T1 minors ③ | Win build/stt-models dir 편차(정직 기록 — build/ gitignored) | N | 조달기 진실원 = exe 형제 — 표기 정산 |
| T1 minors ④ | WHISPER_ROOT 오경로 configure 실패 참고(에러 메시지 품질) | N | 다음 접촉 원장 행 |
| T1 원장행 | 빌드 전 jkagentd.exe 잠금 taskkill — 사용자 라이브 데몬이었으면 서버 재스폰 필요 | N | 차기 접촉 런 겸행 |
| T2 minors ① | WAV 헤더 말미 잔량 경계(데이터 정합의 경계 케이스) | N | 다음 파서 접촉 케이스 원장 |
| T2 minors ② | 파일 말미 개행 0(2파일) | N | 다음 접촉 1행 |
| T2 minors ③ | 2v 계보 명칭 표류 가능(문서 정산) | N | 본 문서 §3 표로 정산 |
| T2 minors ④ | GetExecutablePath 공반 err 표기(경로 수확 실패 시 진단) | N | 다음 접촉 |
| T3 minors ① | STT_MODEL 존재 검사 쌍둥이 정합 1행 | N | 함정 ⑫ 규약상 게이트 계약 변조 우려 — 원장 유지 |
| T3 minors ② | StartCapture 중복 검사 순서 | N | 무영향 — 다음 접촉 |
| T3 minors ③ | dispatch 실패 시 guide 소각 → T4에서 guide 유지로 소각 완료 | M | **본 문서 §2.3에 반영 — 소각** |
| T3 minors ④ | 폰 ffmpeg wav 부적합 표기 원인 혼동(정규화 원인 vs 파일 원인) | N | 다음 접촉 표기 정산 |
| T3 minors ⑤ | ChatActionQuery Focus dead 코드 | N | 도달 불가 경로 원장행 |
| T4 minors ① | 다중발화 상태행 공백(문구) | N | 다음 표기 접촉 |
| T4 minors ② | repeat 래치(토스트 러시) 후보 | N | 관측 무발생 — 발생 시 재판정 |
| T4 minors ③ | dispatch 실패 slot 전사 원문 소각(도달불가 — T3 잔여 ⑤와 동형) | N | park |
| T4 minors ④ | 리포트 라벨 캡처 원장 명기 | N | 본 문서 §2.4 (B) 원문으로 수취 |
| T4 minors ⑤ | voiceStatus_ 서술 가독성 | N | 리팩터 후보 — park |
| T5 minors ① | 리포트 [나] "." vs "click" 환각 문구 혼동 | N | 전사 비결정 원칙 — 문언 원장 |
| T5 minors ② | finalize 안정 루프 표기 2s vs 4s | N | 문언 정산 — 다음 접촉 |
| T5 minors ③ | JsonRecordingActive 토큰 계통(파서 상수 원장) | N | 다음 접촉 |
| T5 minors ④ | awk 2v 미포함 표기(배포판 selftest 캡처 필터) | N | 다음 런 몫 |
| T5 minors ⑤ | EOF 개행·TOCTOU 원장(스파이크 경합) | N | 원장 행 |
| T5 minors ⑥ | t5_chat_done 프레임 원장행(T6 계승) | N | **T6에서 캡처 원문으로 승계 수취 — 소각** |
| T6 probe 잔여 | 함정 ⑦(채팅 합성 타이핑 단절 규명)·⑩(.rec 정산 미달 분기) | M | §4 원장행 — 후속 probe 접촉 몫 |

## §6. 남은 원장행 (정산 3건 + 잔여 원장행)

- **①#111 TW 게이트 채용 정산완료 행** — 6128bb4: MUS-TW-SUMMARY-2 파싱행에 더해
  **TW_OK 본조건 채용(cover_rt==1·apic_keep==1·합성 cover md5 등호)+TW-FAIL 비등호
  사유 열 신설** — 런 원문 `MUSIC-TAGWRITE-PHONE-VERDICT: TW-OK(…게이트
  채용(T5 정산)…)`·`TW-RUN-VALS: run=1 stage=1 land=1 writes=1/1/1 cover_rt=1
  apic_keep=1 flac=keys=3 canon_phone=MUSIC-FULL-2P2QRS-19 captures=1` —
  **정산완료**(#111 지시 계통 종착).
- **②폰 캐논 814 = WSL 등호** — PHONE-SELFTEST-A/B = rc=0 **PASS=814 FAIL=0
  2u=132 2r=10 2s=20 2v=14** 2런 등호+CANON-INCLUSION 원문 — 기존 계보 승계
  (795 → +fix 5 = 800 → +2v 14 = 814) — **정산완료**(§3 표).
- **③#111 정산행(recover_opt 재런 원문)** — #111 T6 Important 유계("recover_opt
  fix 후 첫 재런의 spatial_mix 회수 성립 원문") = **#112 T5에서 RECOVER-OK 실측
  소각**(`RECOVER-OK: …mus_phone_spatial_mix.png` 144,329B 영수증+ANALYSIS/CROP
  원문 — 6128bb4) — **정산완료** — 합산 본판정은 PARTIAL 원장행 유지(MUSP-SPATIAL-
  MIX-VPLAYER MISS — #110 계보 등호 — 본 라인 소관 아님).
- **잔여 원장행(미해결 재판정 대기)**: F6 포그라운드 경합(실발화 = EYES 몫)·채팅
  합성 타이핑 단절(함정 ⑦)·폰 .rec.m4a 정산 미달 분기(함정 ⑩)·-l 60 상한 미발동
  관측(함정 ⑨ — StopCapture -q 진실원 유지)·T5 함정 ⑩ tap delivery 재현성·
  배포 재런 전체 재컴파일 비용(⑪ — 증분 계약 원장).

## §7. EYES 게이트 (사용자 선언만 결제 — EYES-PENDING · probe/본 문서는 결제를
기록하지 않는다 — §5/§7.9.5 원칙 동형 · **미결제(대기)**)

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **폰 DeX 발화 → 앱 실행+재생 중 조작** | "지뢰찾기 켜줘" **실발화** → 앱 실행 체감(+음악 재생 중 발화 조작 체감 — 무음 계약 §1의 실측 재판정) — T5/T6 실측은 무발화 환각 원문만(".","click" — 비결정 CANON 밖) | **대기(실기기·실발화)** |
| ② | **Win 마이크 구두 조작 체감** | F6 hold+[구두] 버튼을 사람 손으로 — T4 (B) 표면 E2E는 합성 입력(클릭 드라이버) 원문 — 키보드/마이크 실발화 = 사용자 몫 | **대기(체감)** |
| ③ | **전사 오차 체감(tiny-ko 판정)** | 폰 tiny-ko(CER 원본 대비 ~11.6%)·Win small(17.5%)의 오차 수용도 — **상향 요구 시 small-komixv2(7.4%·466MB) 표로 재판정** | **대기(체감)** |
| ④ | **대화(문자 출력) 체감** | 인식 실패 시 정직 표기([시스템] stub 안내문 — WSL 런 랩 15행 캡처 원문 보존) 납득 — 전사 원문은 기록에 그대로 남는다(오차 정직 문화) | **대기(체감)** |
| ⑤ | **결정 R1-R5 = 재량** | vendor·채팅 앱 표면·모델 등급·R5 치환 — **기각 시 즉시 수리** | **대기(유효)** |

## §8. 커밋 원장 + IP/호스트 경로 게이트 (#112 라인)

- 라인 체인 = §0 표(496a5e6..7cd8ae6+본 T7) — push 전부 origin main 직행·amend
  없음·**커밋 = 명시 경로만(T1-T6 각 리포트 선언 — `git add -A` 0)**.
- **IP·호스트 경로 게이트(전 태스크 리포트 실측 누계 — 0건)**: IPv4 정규식
  `(d+\.){3}d+`·내부 경로·사용자명 — 커밋 diff 전체 매치 0(T1 = miniaudio 문단
  넘버링 "10.3.1.1.1"·ggml quants 상수 전-외부·무해)·폰 주소 = PHONE_HOST env
  승계+known_hosts BatchMode 자기 판명(주소 미인쇄 계약)·조달 = **공개 변환본/
  공개 태그만**(HF·GitHub — 모델 파일 커밋 금지 = "기계만 커밋" 유지·모델
  sha256 핀은 원문 본표).
- **본 T7 커밋은 docs 2파일(docs/91 신설+docs/90 부기/정산)로 한정** — engine/
  probe 소스 무접촉·런 로그·캡처·probe cpp·fixture wav 전부 engine/tmp untracked
  (커밋 금지 충족 — wsl_voice_run1/2.log·phone_voice_validation.log·t5_* 영수증
  계보 포함).
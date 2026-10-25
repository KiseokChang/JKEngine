# docs/92 — JK APK as-built (#114 라인 — T1-T4, 2026-10-25)

JK APK 라인(#114) 원장 — **폰 홈 화면의 JK 아이콘 = 독립 안드로이드 앱**(APK·별도
설치) — **v1 = 얇은 타이핑 APK**: 텍스트 입력 1줄+[보내기]+append-only 기록 →
`POST http://localhost:8090/talk` — 전사(STT)는 안드로이드 IME 음성입력 등
**외부 앱 몫**(내장 whisper = **v2 백log** — 삭제 아님). 조작 사슬은 폰 jkweb
원존 재용(새 판정 계층 0 — ChatRouteTurn 동일 출구). 계획 문서=
`docs/superpowers/plans/2026-10-25-jkapk.md`(432f4d7), 스펙=
`docs/superpowers/specs/2026-10-25-jkapk-design.md`(확정 — §0 체인), SDD 원장=
`.superpowers/sdd/2026-10-25-jkapk/`(progress+task-1..4 brief/report). 라인
BASE=67e8288(#112 STT 구두 최종 — EYESfix r1). T4는 본 문서 — **probe는 T3에서
신설 확정**(`engine/tools/probes/phone_apk.sh` — progress.md 원장 룰링 — T4는
docs로 수형 축소)이라 engine/probe 소스 무접촉·selftest 재실행 불요 — 캐논 =
전 태스크 리포트/progress의 건네받은 수치 그대로 봉합. 서브에이전트 스폰 0건
(T1-T3 각 리포트 선언)·worktree 미사용·main 직행·`git add -A` 미사용(전 태스크
명시 경로 staging)·amend 0.

- **룰링 승계** — 조달 원존=**기계만 커밋**(docs/44·81·84·#112 — SDK·Gradle
  캐시·모델 ggml·빌드 산출물 APK 전부 무추적 — 조달기·소스만 커밋)·**엔진
  ninja/캔논 4축 무접촉**(APK는 별도 dir·별도 Gradle 빌드 — 스펙 D-원존 ·T1-T3
  각 리포트"접촉 0"선언)·폰 주소 일절 인쇄 금지(PHONE_HOST env — known_hosts
  BatchMode 셀프판명 — phone_voice.sh 원존 동형)·폰 잔상 0(probe 스폰 창
  close_window id 지명 소거·서버/jkweb kept 존중)·probe stdout 수취 원장
  (**APK-RUN-VALS 1행 — JOG-RUN-VALS 승계**)·**설치 = 사용자 손 1회 계약**(D5
  debug 서명 사이드로드 — probe/pm install 절사 금지 — T3에서 성립).
- **부기(#114 — 하단 부기·변형 아님, docs/90 쪽)** — docs/90에 §4.1-7 신규 park
  행(본 라인 커밋 체인 1행) 부기 — 본 문서 §8 커밋 원장과 등호.

## §0. 체인 (rev-parse 전체 SHA 실측 — T4 측정 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 |
| --- | --- | --- |
| 스펙 초판 | `6a7d6d7a0da4ac9ac684e7a7c313934d71c0ea26` | docs(apps): JK 안드로이드 APK 설계 스펙 (#114 — 전부 내장 전사·jkweb 조작 사슬·무추적 빌드) |
| 스펙 전환 | `32a74d5d8d2cc4ef526461f9610453a17c0ea426` | docs(apps): #114 APK v1 = 얇은 타이핑 APK로 전환 (STT=외부 앱·whisper 내장=v2) — 스펙 업데이트 |
| 스펙 정정+플랜 | `432f4d784083ba0595f45578b8296aa58b0c0b0c` | docs(apps): #114 APK v1 얇은 타이핑판 확정 — 스펙 §2/§4 정정+플랜 T1-T4 재작성 |
| T1 | `eb4de9635d2b462da109ab008226d92562449b8d` | feat(android): JK APK 빌드 사슬 — SDK 조달기+hello-world (T1) |
| T1 fix r1 | `ad3834ded5c5ff9b99faba0e62dc1520c9e65f6d` | fix(android): SDK 조달기 SIGPIPE 관용 (T1 fix r1) |
| T2 | `0f18157e2e7cbdb5f0ef041af99e328689b4949c` | feat(android): 타이핑 표면+/talk 조작 (T2) |
| T3 | `cce44204787212cb272e313873c6f19079e65fef` | test(android): 폰 APK 조작 사슬 실측 (T3) — probe `engine/tools/probes/phone_apk.sh` 신설 |
| T4 | 본 커밋 | docs(android): JK APK as-built (T4) |

## §1. 사슬 원장 (확정 원장 — 스펙 §0/D1-D7 = 상위 진실원 · 전환원장 포함)

- **D1 (C)**: 진짜 안드로이드 앱 — APK 별도 설치 — ImGui 창 표면과 별개.
- **D2 (B→v1 전환 — 전환원장)**: 사용자 2026-10-25 지시 원문 **"음성인식은
  다른 좋은 앱을 쓰면 되니 우선 타이핑하는 형태로"** → v1 = **얇은 타이핑
  APK**(전사 = 안드로이드 IME 음성입력 등 외부 앱 몫) — whisper NDK 내장+모델
  번들은 **삭제가 아니라 v2 백log 이동**(§5). 스펙 초판(6a7d6d7)의 전부 내장
  전사 → 전환 커밋(32a74d5) → 정정+플랜 재작성(432f4d7) 3단 계보.
- **D3 조작 사슬 재용**: 전사 텍스트 → `POST http://localhost:8090/talk`
  (jkweb — HandleTalk — ChatRouteTurn 원존·폰 서버가 chat.json 구성 시 cloud
  승격까지) — APK에 새 판정 계층 0("STT 텍스트 == 타이핑 텍스트" 규약 승계).
- **D4 무추적**: SDK/NDK·Gradle 캐시·모델 ggml·빌드 산출물 APK 전부 무추적+
  **빌드 산출물 커밋 금지** — 조달기 스크립트만 커밋.
- **D5 debug 서명 사이드로드**: v1·스토어 출고 비목표 — **설치 = 사용자 손
  1회 계약**(probe/pm install 절사 — T3에서 "설치 수행 0" 선언 원존).
- **D6 v1 모델 번들 = 비-목표**: tiny-ko asset 전개 절차는 v2 원장 행(§5).
- **D7 v1 품질 승계 = 비-목표**: 어휘 prompt 힌트 등 whisper 쪽 품질 수형은
  데스크톱 [구두](#112) 축에만 적용 — v1 APK는 전사 축 미접촉(마이크 권한 0).

## §2. 라인 원장 (부품별 as-built — 원류 원문 1:1 수취 · 코드 재실측 0)

### §2.1 SDK 조달기+무추적 빌드 사슬 (T1 — eb4de96+fix r1 ad3834d)

- **`engine/tools/probes/install_android_sdk.sh`** — 멱등 조달기: 마커 부재
  시에만 네트워크. 레이아웃 = `build/android-sdk/` 하에 `jdk17/`(Temurin
  17 — **AGP 8.x 필수·머신 java 부재 실측 `java: command not found`** 원장)·
  `cmdline-tools/latest/`·`sdk/`(platform-tools·platforms/android-34·
  build-tools/34.0.0)·`gradle-8.9/`·`wrapper/`·`downloads/`·
  `gradle-user-home/` — 전량 루트 `.gitignore` `build/` 룰 커버
  (`git check-ignore -v` 원문 = T1 리포트 §4). **NDK 미조달** = v1 얇은판
  계약(Kotlin/JVM — gradle 어셈블에 NDK 불요). sha256 기록 원문
  (`build/android-sdk/downloads/SHA256SUMS.txt` — jdk17 zip
  `e53a79c3…`·commandlinetools-win-11076708 `4d693120…`·gradle-8.9-bin.zip
  `d725d707…`).
- **wrapper 1종 결정 원장**: gradle-wrapper jar·gradlew 전량 무추적 — 조달기가
  pinned Gradle 8.9로 wrapper를 `build/android-sdk/wrapper/`에 생성해서 절차로
  참조(소스트리 바이너리 0). `distributionUrl` = **로컬 zip**
  (`file:/…/downloads/gradle-8.9-bin.zip`)+`validateDistributionUrl=false` —
  wrapper 재다운로드 없음. `GRADLE_USER_HOME=build/android-sdk/gradle-user-home`
  — gradle 캐시도 무추적. 플러그인 = AGP **8.5.2**+Kotlin **1.9.24**
  (`settings.gradle.kts` — google()/mavenCentral()/gradlePluginPortal()).
- **빌드 계약 원문**: `gradlew.bat -p engine/android/jkapk assembleDebug
  --no-daemon`(JAVA_HOME/ANDROID_HOME/GRADLE_USER_HOME env 주입 —
  `local.properties` 생성 안 함 — 추적 트리 머신 경로 0) → clean 최종 원문
  `BUILD SUCCESSFUL in 1m 10s / 36 actionable tasks: 35 executed, 1 up-to-date`
  · **CLEAN_BUILD_RC=0**(T1·T2 양쪽 — warm rerun 원문도 rc=0). 산출=
  `engine/android/jkapk/app/build/outputs/apk/debug/app-debug.apk`(T1
  795,390B → T2 후 **803,590B**) — 무추적·커밋 0. `apksigner verify
  --print-certs` 원문: `Verified using v2 scheme (APK Signature Scheme v2):
  true` — **D5 = Gradle debug 자동 서명**(`~/.android/debug.keystore` 자동
  생성 — keystore도 커밋 0).
- **수형 원장 1건(커밋 전)**: 초판 `compileDebugKotlin` FAILURE —
  `Inconsistent JVM-target compatibility … 'compileDebugJavaWithJavac' (1.8)
  and 'compileDebugKotlin' (17)` → `app/build.gradle.kts`에
  `compileOptions{VERSION_17}`+`kotlinOptions{jvmTarget="17"}` 명시 → 재빌드
  rc=0(clean 재확인).
- **fix r1 (SIGPIPE 관용 — 리뷰 I-1 소각)**: `set -euo pipefail` 상태의
  `( yes | sdkmanager.bat … )` — sdkmanager 종료 순간 yes가 SIGPIPE(141)로
  사망 → pipefail이 전체 실패화(신규 머신 첫 조달이 설치 성공 직후 FATAL —
  T1 원문은 마커 존재 재런이라 무영향이었다는 사실 정직 기록). 수형 =
  `run_sdkmanager()`로 141/130만 관용+최종 check 열 fail-closed 유지(§3
  원문은 커밋 ad3834d의 코드 주석 — T1 리포트 §7). 겸행 M-1 = `fetch()` 캐시
  히트 분기에서 기록 sha256 대조 불일치 FATAL(`M1_MATCH_PASS` 원문)·M-2 =
  이분 개행 소각. 재런 원문: 마커 전존 — 네트워크 0·PASS 9열 전부
  (`PROVISIONER_RC=0`).

### §2.2 타이핑 표면+/talk 조작 (T2 — 0f18157)

- **`app/src/main/java/chang/jkapk/MainActivity.kt`** — hello 1행 → 타이핑
  표면 본체(프로그래매틱 레이아웃 — XML res 추가 0·코루틴 의존 0 — framework
  전용·의존 gradle `dependencies` = none).
  - **입력행**: `EditText` 1줄(`setSingleLine`)+`[보내기]` 버튼 — IME action =
    `IME_ACTION_SEND`(Enter=전송) — **IME 음성입력이 그대로 유입되는 표면**(
    별도 권한 0 = D7 계약).
  - **대화 목록**: `TextView.append` 동형 append-only — [나]=보낸 텍스트·
    [시스템]=답신/오류(chat 앱 문화 승계)+`ScrollView` 스크롤 앵커
    (`fullScroll(FOCUS_DOWN)`).
  - **상태행**: 서버 연결 정직 표기 4형 — 초기 "서버: 미확인 — [보내기]로
    연결 시도"·전송 중 "서버: 연결 시도 (localhost:8090/talk …)"·성공
    "서버: 연결 (localhost:8090 · HTTP 200)"·부재 "서버: 미기동 — DeX 세션
    필요".
  - **조작**: `POST http://localhost:8090/talk` — `HttpURLConnection`+
    connect/read 타임아웃 각 10s — HTTP = 단일 스레드 실행기
    (`Executors.newSingleThreadExecutor`)·전송 중 입력행·버튼 disabled.
- **Manifest**: `uses-permission INTERNET` 1건만(aapt dump permissions 원문
  — 마이크/기록 0)+`android:usesCleartextTraffic="true"`(루프백 http 전용
  원존 — 8090 loopback bind 계약 승계).
- **jkweb HandleTalk 계약 원문 승계(새 파서 0)** — `engine/tools/jkweb/
  main.cpp` 실측(T2 리포트): 요청 = `{"text":"…"}`·JsonEsc 동형 이스케이프;
  응답 필드 ok/reply/kind/app/server — APK **최소 파싱 = ok/reply/kind/app**(
  `JsonGetString` 동형)·성공 200 `{"ok":true,"reply":"…","kind":"…"}`·실패
  "ok":false+"reply"(한국어 이유)+"kind":"Error"(400 빈 발화·500 창 서버
  파이프 미기동 fail-loud)·reply 키 탐색 불가 = 응답 원문 그대로 기록(숨김 0)
  ·빈 발화는 서버 400 전에 로컬 선-무거(서로소 — trim 후 빈 문자열 전송 안)
  ·연결 실패/타임아웃 공통 `IOException` → "서버 미기동 — DeX 세션 필요"
  (spec §3 계약 표기 원문).

### §2.3 폰 조작 사슬 성립 실측 (T3 — cce4420 · probe `phone_apk.sh` 신설)

- **프리플라이트(폰 원존 승계)**: ssh -p 8022 BatchMode termux_jkengine 키·
  u0_a4 — **폰 주소 인쇄 금지**(SELFRESOLVE: MATCH)·`aarch64`·wake-lock OK·
  **폰 창 서버 UP**(pids 절사 0 — 사용자 live 세션 존중)·`WHISPER_ROOT` 확인.
- **①반입(D5 반입까지 — 설치 시도 0)**: scp → `/sdcard/Download/jkapk-debug.apk`
  원문 —
  ```
  APK-DEPLOY-OK bytes=803590 md5=15cf74f4c79f7712af77d480fc4b0553 (양측 등호)
  ```
  3런 반복 scp 재전송 등호·FUSE 쓰기 권한 실측 성립. cat-over-ssh 페일오버
  경로는 스크립트 보존(본 런 발화 0). **pm install/adb install 미시도** —
  D5 사용자 손 1회 계약 원존.
- **②jkweb 폰 기동**: 런1·2 = fresh nohup(`./buildterm/jkweb --port 8090
  >~/tmp/jkapk_jkweb.log` — ssh 단절 생존)·런3 = **kept(재기동 금지 원존
  재실측)**. `GET / = 200`+한국어 페이지 마커 `JK 채팅` 3런 전원·폰 배너 원문
  `bind: 127.0.0.1 (루프백 기본)`.
- **③/talk 조작(서버 축) 원문 3런 동일**:
  ```
  TALK-REPLY: {"ok":true,"reply":"'지뢰찾기' 앱을 실행합니다.","kind":"Launch",
               "app":"minesweeper","server":"{\"ok\":true}"}
  HTTP=200
  ```
  `list_windows` 창 생성 원문(런2): `{"id":51,"title":"Minesweeper",…,
  "dw":320,"dh":380}` — jkweb→창 서버 위임 실측. **APK에서 실발화(설치→타이핑
  →보내기→DeX 창) = 사용자 설치 후 EYES**(스펙 §4).
- **④정직 케이스 2종(절사 0 — jkweb kill 없이)**: (a) 빈 발화 `POST /talk
  {"text":""}` → `HTTP=400`+`{"ok":false,"reply":"빈 발화입니다.",
  "kind":"Error","app":""}` (b) 무리슨 포트 8099 → curl `RC=7`(connection
  refused — APK IOException 수렴 통로 근거 원문).
- **⑤요약 스트림(APK-RUN-VALS — stdout 수취 계약)** 3런 원문:
  ```
  APK-RUN-VALS: run=1 | SRV-UP=1 | WHISPER-ROOT=1 | JKWEB-UP=1 | APK-DEPLOY=OK
    | APK-BYTES=803590 | APK-MD5-EQ=OK | TALK-LAUNCH=200/Launch/minesweeper
    | MS-WIN-ID=33 | WIN-DISPOSE=spawned-new | HONEST-400=OK | RESIDUAL=0
    | STATUS=APK-PHONE-OK
  ```
  런2 = fresh boot 등호(MS-WIN-ID=51)·런3 = kept 등호(`JKWEB-START=kept`·
  MS-WIN-ID=79·SPAWN-CLOSED=1) — **3런 APK-PHONE-OK(PROBE-EXIT=0·0·0)**.
- **종료 상태 3런 전원**: 서버 UP+jkweb UP(`END-SRV-UP`/`END-JKWEB-UP`)·폰
  프로세스 잔상 0·폰 트리 쓰기 0.

## §3. 성립 원문 (본 라인 결정론 게이트 — 스펙 §4 대사)

| 항 | 계약 | 원문 판정 | 상태 |
| --- | --- | --- | --- |
| ① 빌드 | gradle assembleDebug rc=0 | clean 재확인 2회(§2.1 원문 — CLEAN_BUILD_RC=0) | **성립** |
| ② 반입 | scp bytes+md5 등호 | 803,590B·md5 15cf74f4… 양측 등호·3런 반복(§2.3) | **성립** |
| ③ 서버 축 조작 | POST /talk kind=Launch | TALK-REPLY 원문+list_windows 창 실측 — 3런 APK-PHONE-OK | **성립** |
| ④ 정직 2종 | 빈 발화 400·서버 부재 refused | HONEST-400=OK·RC=7 원문 | **성립** |
| ⑤ 설치·런치·APK 실발화 | adb install/런치 rc·APK에서 발화→DeX 창 | **T3에서 수행 0(D5 사용자 손 1회)** — 사용자 EYES | **대기(EYES)** |
| ⑥ 캐논 4축 | 무접촉 = 등호 유지 | 본 라인 engine/ 소스 접촉 = probes 스크립트 2종(조달기·probe — selftest 어셜션 축 아님) — APK 별도 dir·별도 Gradle 빌드 — ninja 계보 무변조. **캐논 = #112 계보 그대로(Win 837/WSL 814/posix 304/폰 814 — 2v=14) — 본 라인에서 재측정 0(정직 표기)** | 유지(표기) |

## §4. 함정 원장 (T1-T3 리포트 concerns 전량 수취 — 봉쇄 레슨 축)

1. **`yes |` SIGPIPE 파이프라인 사망(T1 I-1)** — set -uo pipefail에서 피더
   죽음(141)이 전체 실패화 → 관용 수형(§2.1) — 캐논 각종 "피더 파이프" 조달
   스크립트 공통 레슨.
2. **`cmd //c "quoted .bat"` 인자 전개 함정(T1)** — msys 인자 전개로 따옴표
   붕괴(파일명 미식별) → **.bat 직접 실행** 형태 수형.
3. **downloads dir 선두 생성 누락(T1)** — curl `(23) Failure writing output`
   RC=1 — mkdir 선행.
4. **JVM-target 불일치(T1)** — Kotlin 17 vs javac 1.8 → 17 명시(재발 방지
   원장 — 모듈 접촉마다 확인).
5. **grep 실패 시 set -e 무음 사망(T1 — progress M-3급 유계)** —
   install_android_sdk.sh:75 zip 전존+SUMS 미기록 조합 — `|| true` 1행 원장행
   (다음 probe 접촉 시 겸행 소각 — progress 원장 그대로).
6. **probe 창 disposition 절단 불량 → 기존 창 close 세션 위배(T3 런1)** —
   `grep '"id":…'` | cut 조합이 `33,"title"` 절단 불량 → focus-existing을
   spawned-new로 오판 → **측정 이전 존재 Minesweeper 창(id 33, pid 28120)
   폐쇄**. 수형 = 창 오브젝트 통째 수령 후 선두 `"id":[0-9]+` 추출×
   after−before 집합차(phone_web_chat ⑧ 원존) — **사후 복원 완료**(launch_app
   minesweeper → id 42 창 확정 — "Minesweeper 1창 열림" 상태 등호·계보 id 33
   → 42 원장)·런2·3에서 기존 창 무접촉 receipts. **세션 위배 = 사후 복원+
   수리 원장으로 존중**(선원장 관행).
7. **/talk 스폰 창의 지연 착지(T3)** — 15s list에 부재·40s+ 후 등장(런1)·스윕
   후 등장(런2) → **다중 라운드 지연 스윕(20s 간격·이중 무지정까지 최대
   6라운드)** 수형 — 런3 SWEEP-ROUND-CLEAN → RESIDUAL-0-OK.
8. **ssh 원격 nohup 반환 지연(T3)** — `sleep` 원호는 2s 반환, nohup 데몬은
   ssh 반환 미도달(timeout 40 — **세션 fd 회수 지연 원장행**) → 기동 ssh
   timeout 정상 통과로 보는 `JKWEB-BOOT-SSH-DETACHED` receipt+pgrep 2pids
   원문+curl(ROOT-CODE=200)로 기동 성립 판정 대체.
9. **probe `$ROUND` unbound(set -u — T3 런3 중간판)** — `$round` 소각 후 재런
   완성 — probe 자체 결함 원장 관행 소각.
10. **probe ROOT 부정합(교차 라인)** — phone_voice.sh(#112 — eyes-fix r1
    원장)의 ROOT 2단 재산정으로 캡처 착지 = engine/engine/tmp(엔진 내부
    중복 dir) — **#112 라인 미소각 원장행 승계**; 본 라인 T3 신설 probe
    `phone_apk.sh`는 ROOT 3단(`../../..` — engine/tools/probes에서 repo
    root 등호) 재산정 원문 = 원존 수형이 **암묵 전파된 것 — phone_voice.sh
    ROOT 수형 시 이 계보 참조**.
11. **"server" 응답 필드 미파싱(T2 concern 1)** — jkweb은 서버 회신을
    "server" 필드로 원문 승계하나 APK 최소 파싱은 ok/reply/kind/app만 —
    "APK 기록에도 답신 원문" 계약의 server 원문 승폭 여부 = 후속 재판정 행.
12. **IME dictate 편차(T2 concern 2)** — dictate 앱 일부는 IME action 없이
    \n을 넣는 모양(singleLine이라 \n 본문 유입은 안 됨 — 실기기 dictate 2종+
    재판정 = EYES 몫).
13. **부재 문제 2종(T2 concern 3)** — "서버 프로세스 부재" vs "jkweb 살아
    있으나 창 서버 파이프 부재"(500 fail-loud)를 앱이 구분하지 못함 — 둘 다
    정직 표기로 수렴 — 표기 정밀화 = v2 여유.
14. **executor 종료 정직(T2 concern 4)** — onDestroy에서 `shutdown()` —
    진행 중 POST 완료 후 스레드 정리(비-인터럽트 — 정직 완주).

## §5. v2 원장행 (비-목표 보존 — D2/D6/D7 — 접촉 금지 원존 유지)

- **whisper NDK JNI+마이크**: vendored whisper.cpp 상대 참조
  (`../third_party/whisper.cpp` — #112 vendor 149파일 v1.9.5 계보 = docs/91
  §2.1)·whisper JNI lib 1종·마이크 `AudioRecord` **16k 모노 정규화**·
  **#3117 격리 근거 원문 승계**(앱 자체 프로세스 = 격리 본질 동일) — 전부
  v2 리포트 소관. v1에서 NDK 미조달·whisper 자체 접촉 0(T1-T3 전 리포트).
- **모델 asset 전개 절차(D6)**: tiny-ko(폰 캐논 모델 — 77,691,730B —
  eyes-fix r1 원문) → APK asset 번들 절차 원장행. **small-komixv2 핀 승계**
  (docs/91 T7 부기 — eyes-fix r1 §2.1 원문): `ggml-small-komixv2.bin`
  sha256 `a428f6462d65c8f8942a2e1b9684910e5ff3a6873cc2b80856d549739627bba4`
  ·487,601,984B(≈466MiB — 2repo 교차 등호)·양자 변형 후보(q5_0
  175,209,680B·q8_0 264,464,624B) — **폰 지연 8-10s = SMALLKO-PHONE-BORDER
  원장(승계 판정 = 사용자 몫 — docs/91 §6/eyes-fix r1 concerns 1)** — v2
  번들 결정 시 이 BORDER 재판정과 결합.
- **STT 품질 승계(D7)**: 어휘 `--prompt` 힌트(`kVoicePrompt` — ChatRouter
  kRouteTable/kAliasTable 원문 — eyes-fix r1 §1.1)·전사 품질 수형은
  **데스크톱 [구두](#112) 축에만** 적용 — v1 APK는 전사 축 미접촉(마이크
  권한 0). **참고 원장행**: 외부 dictate 앱의 환각도 v1 조작 사슬에는 그대로
  유입될 수 있다(jkweb 라우터 트리거 원존 — APK 쪽 필터 0 = D3 "텍스트 ==
  타이핑" 규약) — 환각→launch 트리거 risk는 #112 축 원장(eyes-fix r1
  concerns 3)의 v1 변형으로 원장 유지.
- **v2에서도 유지할 계약(전승)**: 무추적(모델 ggml 커밋 금지)·"기계만 커밋"
  ·엔진 ninja 무접촉·정직 표기 문화(§2.2 상태행 4형·§2.3 정직 케이스).

## §6. 자연어(LLM) 축 — 스파이크 원문 봉인 (리포트 수취 — 후속 보강 1절)

- **phone-llm-respike 스파이크 원문 수취 완료** — 리포트
  =`.superpowers/sdd/2026-10-25-jkapk/phone-llm-respike-report.md` — 엔진
  바이너리/코드 접촉 0·커밋 0·chat.json 조정 0·폰 주소 원문 미인쇄(계약).
  이하 = 원문 보강 1절. 현재 원존(스펙 D3) 유지: /talk의 판정 = 문자열
  트리거(kRouteTable)+별명(kAliasTable) — 자연어(LLM) 승격 = **폰 서버가
  chat.json 구성 시 ChatRouteTurn cloud 경로**(jkweb 원존) — APK 쪽은 판정
  계층 0(텍스트 통과) — 수혜 재판정(표기 문구·답신 반영)은 이 자리 말미.
- **측정 원문(기준선 2상 — POST /talk 폰 루프백, 각 2회)**: **serve DOWN**
  = 3발화 전부 stub 안내문 즉발 **0.010-0.195s**(발화 1건 `kind=Launch
  app="다시"` = 라우터 즉발 경로 — LLM 미개입) / **serve UP** = 같은 2발화
  **2.35-10.49s** — 결과도 **동일 stub 안내문**(파스 실패 폴백). 즉 파스
  수형 없는 구성 살리기는 지연만 증가하고 UX는 악화된다(리포트 §2 —
  원문 `engine/tmp/plr_baseline.log`·`plr_llm.log`).
- **진원 원장(모델 무죄)**: 폴백 원인 100% = **ollama CLI 렌더층 혼입** —
  `ollama run` stdout이 `Thinking...` 헤더+추론 문단+`^[K`·`^[2D` ANSI
  커서 시퀀스(워드랩)로 시작, thinking 본문 안의 JSON 골자(ActionSchemaLine
  예시)가 `ChatLlmActionParse` (b) **최초`{`..마지막`}` 절단
  (ChatRouter.cpp:330-341)을 오염** → AgentJson 파스 실패 → (3) stub 폴백
  계약(ChatRouter.cpp:431). docs/84 §2.4(2026-10-08) 동일 모양 **오늘
  재실측 — D1 유예 유효**. **대조 = 모델 건강**: `POST /api/chat`
  stream=false 직행(같은 모델·폰 루프백) **1.58s** — thinking 별도 필드
  분리·`message.content` = 완건 행동 JSON — 혼입 전부 CLI 렌더층 사안.
- **수형 후보 원문(D1 재료)**: 0.23.2 CLI에 렌더 억제 플래그 2건
  (`--hidethinking`·`--nowordwrap`). **단독 불충분** — thinking 제거는
  확인하나 CLI 워드랩이 JSON 문자열 값 안쪽에 `^[K`+개행 혼입
  (`...확인합니[K\nd다."}` — python json.loads `Invalid control character`
  파스 실패 원문). **결합 = 2턴/2턴 성립** — **2s/3s(2-4s)·순수 단일행
  완건 JSON**(첫`{`..마지막`}` 절단+표준 파스 둘 다 통과·enum 내 값) →
  **≤5s UX 문턱 통과 실측 성립**. 대조 실험부: 모델 교체 후보 4종(cloud)
  전원 서버 은퇴(retired)·`qwen2.5:0.5b` 품질·속도 불성립·`direct_cmd`
  주입은 계약 락(JKLlmEngine.h)이 닫음 — **state-only 최선 경로 불성립
  판정**(리포트 §4). 원장행: stdin 파이프 개방 시 0.23.2 CLI가 응답 완료
  후 stdin EOF 대기로 매달림(엔진 스폰 무영향 — fd0=/dev/null 전달).
- **수형 실장 원장(ad9aa81 — #113 소형 수형·계약 락 갱신)**:
  `BuildOllamaDirectCmd`에 `--hidethinking --nowordwrap` 결합 상설
  (`kOllamaRunRenderFlags` — 엔진 리터럴 단일 근원)+**opt-out 밸브**
  (`ChatConfig::renderFlags` — chat.json `"render_flags":0` → 계약 변경
  이전 형태 바이트 동일 — 플래그 미지원 ollama 축 unknown-flag 사망을
  끄는 fail-closed 재량·LoadChatConfig 유일 독자·키 부재=기본 true).
  selftest 계약 락 단정 갱신(1n-d5·llm15 — 어셜션 수 등호)·캐논 승계
  (**Win 841/WSL 818/posix 304 등호 유지** — ad9aa81 커밋 원문). **Win
  0.40.11 실측: `ollama run -h` 플래그 2건 전원 존재 + 라이브 턴 0.9s
  순수 단일행 JSON 원문.** **폰 = BLOCKED 원장행** — 결합 수형의 폰 실기기
  판정(0.23.2 원문 재빚음)은 미실측·**다음 폰 접촉 배포에서 승계**.
- **판정**: 스파이크 시점 기준 **폰 자연어 UX ≤5s 미성립**(stub 폴백에
  지연만 증가) — 결합 수형으로 CLI 렌더층 원인은 소각됐으나 **잔여 = 폰
  원문 승계 대기**(APK 측 수혜 재판정과 동일 자리 — §7 ③).

## §7. EYES 게이트 (사용자 선언만 결제 — EYES-PENDING · probe/본 문서는 결제를
기록하지 않는다 — docs/91 §7 원칙 동형 · **미결제(대기)**)

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **홈 아이콘 → 명령 → DeX 창 실행 체감** | 홈 JK 아이콘 터치 → 입력행 텍스트(또는 IME dictate)→[보내기]→**DeX에 앱 창 실행** 체감 — 스펙 §4 사용자 게이트 1. **원장행: 사용자 "지뢰찾기 창은 실행됨" 실측 보고가 컨트롤러 전언으로 존재(2026-10-25 — T3 설치·기동 원문 뒤 계통)** — 그러나 **EYES 결제(사용자의 확정 선언)는 본 문서 기준 미접수** — 결제 표기는 사용자 선언이 있어야 한다(원장행 ≠ 결제) | **대기(확정 선언)** |
| ② | **APK 모양·기록 체감** | 타이핑 표면(입력행+[보내기]+append-only [나]/[시스템] 기록+상태행 정직 표기)·앱 라벨 "JK" 모양·기록 체감 | **대기(육안)** |
| ③ | **자연어(LLM) 축** | 스파이크 원문 봉인(§6) — 결합 수형 Win 실측·**폰 실기기 판정 = 승계 대기**(서버 축 — APK 수혜 재판정 동석) | **대기(폰 원문 승계)** |
| ④ | **결정 D1-D7 = 재량** | 전환원장(D2) 포함 — **기각 시 즉시 수리** | **대기(유효)** |

## §8. 커밋 원장 + IP/호스트 경로 게이트 (#114 라인)

- 라인 체인 = §0 표(6a7d6d7..cce4420+본 T4) — push 전부 origin main 직행·
  amend 없음·**커밋 = 명시 경로만(T1-T3 각 리포트 선언 — `git add -A` 0)**.
- **IP 리터럴 게이트(전 태스크 리포트 실측)**: T1·T2 = `grep -F`
  (`Licensed under the Apache`·`Copyright`·`All rights reserved`) **히트 0**
  (전부 자작 — T2 측 2파일 RC=1 원문)·T3 = 커밋+probe 스크립트+리포트 전량
  `grep -F` 재판정 **0 히트**(폰 주소·IP 0 — 주소 인쇄 금지 계약 동형). 본
  T4 커밋도 동일 게이트로 커밋 전 staged 재실측(원문은 T4 리포트에 봉인).
- **무추적 게이트(D4)**: 본 라인 커밋 전체 = 조달기 1종+APK 모듈 소스
  (.kts 3종+manifest+Kotlin+strings.xml+.gitignore)+probe 1종 뿐 —
  `build/android-sdk/**`·gradle 캐시·wrapper jar·APK 산출물·
  `engine/android/jkapk/.gradle`·`app/build` 전부 gitignore 커버
  (`git check-ignore` 원문 = T1 리포트 §4)·루트 .gitignore 무변경.
- **본 T4 커밋은 docs 2파일**(docs/92 신설+docs/90 §4.1-7 부기 1블록)로
  한정 — engine/probe 소스 무접촉(probe = T3 신설 확정·재용)·selftest
  재실행 불요(엔진 어셜션 축 무접촉 — §표기 ⑥ 정직).
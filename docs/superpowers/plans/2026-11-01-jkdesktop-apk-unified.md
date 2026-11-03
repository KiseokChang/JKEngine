# jkdesktop 단일 APK (완전 대체 v1) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 아이콘 하나로 Termux 없이 기동하는 jkdesktop 단일 APK — DeX 풀스크린 데스크탑+exec 클라합성+음성(SpeechRecognizer·시스템 TTS)+ChatRouter in-process+ollama cloud/HTTP+뮤직(OpenAL) v1.

**Architecture:** SDLActivity=서버(SDLMain 스레드·GL 렌더)·클라/모듈=lib*.so 포장 exec+dlopen(nativeLibraryDir)·클라 렌더=SW 렌더러 표면 직결→공유메모리→서버 합성(docs/103 실측 승계). Java(JNI) 계층 = 음성·수명·에셋 조달. ChatRouter 스레드 in-process(jkweb 폐기).

**Tech Stack:** NDK 28.2 clang·gradle AGP 8.6.0·SDL2 2.32.10(벤더)·OpenAL soft(OpenSL)·Android SpeechRecognizer/TextToSpeech(SDK)·HTTP(HTTPURL/jk::net — ollama cloud 계약).

**Spec:** `docs/superpowers/specs/2026-11-01-jkdesktop-apk-unified-design.md` (이 플랜은 스펙과 함께 읽는다 — 수형 원문 인용 = docs/103).

## Global Constraints

- 스파이크 수형 승계 원존(docs/103 §3): lib*.so 포장 exec·모듈 lib 접두·JK_EXE_PATH(dladdr)·SDL_SetMainReady+dummy+SoftwareRenderer 표면 직결·SDL_FINGER→Mouse·재진입 가드 우회+endpoint unlink·mixer 심(실물로 교체될 때까지).
- 엔진 편집 = 전부 플랫폼 게이트(`__ANDROID__` 또는 JK_ANDROID_* env) — Win/WSL 캐논 회귀 접촉 0 원칙.
- 셀프테스트 4축 캐논 유지(Win/WSL/posix/폰) — APK 라인의 산출은 캐논 라인과 별도(캐논은 기존 빌드로 측정).
- 조달 = 무추적 `build/android-sdk/` (jkapk 조달기 계약 승계)·핀=sha256·fail-closed.
- jkapk(#123/#114 세션 소유)·jkapk-빌드 파일 비접촉 — 완전 별도 project `engine/android/jkdesktop-apk/`.
- 빌드 명령 표준: `cd engine/android/jkdesktop-apk && ANDROID_HOME/JAVA_HOME/GRADLE_USER_HOME env + gradle-8.9/bin/gradle.bat assembleDebug` (cd 누락 함정 원존 — gradle은 프로젝트 루트에서).
- 폰 주소 = 매번 스캔/기록 금지 원존; adb 무선 페어링 상존 사용.

---

### Task 1: APK 프로젝트 골격 신설(`engine/android/jkdesktop-apk`) — 스파이크 정착

**Files:**
- Create: `engine/android/jkdesktop-apk/{settings.gradle.kts,app/build.gradle.kts,app/src/main/AndroidManifest.xml}`
- Create: `app/src/main/java/chang/jkdesktop/{MainActivity.java,ForegroundService.java}`
- Create: `app/src/main/jni/{CMakeLists.txt,jksrv_main.cpp,mixer_shim.c,include/SDL_mixer.h,iconv_shim/iconv.h}`
- Modify: `engine/tools/probes/install_android_sdk.sh`(SDL2+SDL2_mixer+openal 조달기 섹션 추가 — sha256 핀·무추적 조달 반복 가능)

**Interfaces:**
- Consumes: spike-b 커밋 체인(7f9c3f5..c858e9f)의 검증 코드 — 복사 후 정제(폐기 라벨·계량기 프로브 전부 철수, 수형 본문만 유지)
- Produces: `app-debug.apk`(chang.jkdesktop) 빌드+폰 반입+기동 — 이후 모든 태스크의 기저

- [ ] **Step 1: SDL2 조달기 섹션 추가**(install_android_sdk.sh — sdl2-2.32.10.tar.gz sha256 실측 핀 → `build/android-sdk/sdl2-src` 전개·재실행 멱등. openal은 Task 9에서 추가)
- [ ] **Step 2: 골격 복사+정제** — spike-b에서 gradle/manifest/jni 파일 복사, `chang.spikeb`→`chang.jkdesktop` 전 갱신, 계량기 프로브([spikeB] commit/FillRect/comp/texture/event) 전량 철수, srv.err 로그 채널(수취 채널)은 유지, versionName="v1.0", `ForegroundService` 골격(startForeground+notification 채널 — spec §3 수명)
- [ ] **Step 3: 빌드 성립** — gradle assembleDebug RC=0(기저 계약)
- [ ] **Step 4: 폰 반입+기동** — adb install -r+am start → 로그 수취: `JKWindowServer: spawned … taskbar`+srv.err에 Init·fit 원문 → 배경+태스크바 육안(EYES 항목)
- [x] **완료** f135553 — Samsung FGS 함정 2건 원존(부팅 원문 jk-err-3) + 계량기 철수+라벨 전환 동봉

### Task 2: exec 클라+전체 모듈 lib*.so 포장

**Files:**
- Modify: `app/src/main/jni/CMakeLists.txt`(모듈 전체 — minesweeper/taskbar/tetris/gallery/music(태그 전용 leg)·chat·terminal·settings/notes/files/agentmgr/workshop/script·filedlg·snap 등)
- Modify: `engine/src/main.cpp`(ClientModulePath lib 접두 유지 재검 — 스파이크 커밋본 채용 확인)

**Interfaces:**
- Produces: 네이티브 libN개(libSDL2/libjkdesk/libjkc/libjkapp_*) — 패키징 검증(unzip -l)+기동

- [ ] Step 1: 모듈 타깃 일괄 신설(엔진 CMakeLists 소스 목록 흡수 — spike CMake 패턴 동형·JKAPP_MODULE_BUILD·PREFIX lib)
- [ ] Step 2: 빌드+패키징 검증(unzip -l로 lib/arm64-v8a에 전 실물)
- [ ] Step 3: 폰 기동→그리드 셀 탭 3종 실측(지뢰찾기+갤러리+채팅 창 등장 원문+육안)
- [x] Step 4: 완료 13f0118 — 조건부 혼입 함정 원존(music/filedlg)

### Task 3: 입력 계약 — MouseChord+챠드 지뢰찾기 — **as-built 2026-11-04**: 아래 MouseChord EVENT 신설 설계는 미실행(스텝 미체크 원존) — 실착 = 챠드 발화 계약 3계(실물 마우스 좌우 동시 = 앱 코어 leftDown_/rightDown_ 원존·폰 화면 두 손가락 터치 = spike SDL_FINGER held==2→right 합성 원존·**DeX 트랙패드 = 동시 버튼 도달 불가 실측([mouse] button=3 부재·clicks 1..4 흡수) → 더블클릭 챠드 신설(IsDoubleTapChordGesture — clicks는 JKEvent::keyCode 실계)** — docs/phone-input-resize-chord-report.md §2. 셀프테스트 케이스 4건 — Win 965 PASS.

**Files:**
- Modify: `engine/include/JKEvent.h`(JKEventType::MouseChord 신설 — enum 말미 배치(와이어 상호호환 — serial 검증))
- Modify: `engine/src/JKEvent.cpp`(TranslateSDLEvent — MouseDown 챠드 판별: 버튼1 보유 중 버튼2 도착→Chord 발사·U에서 해제. 스파이크 FINGER 블록 위)
- Modify: `engine/src/apps/ClientMineSweeperApp.cpp`(PollInputEvent MouseChord 소비 → 기존 둘레 열기 로직 연결)
- Test: `engine/tools/selftest` 케이스 신설(챠드 판별 순수함수 — 버튼 보유 표만)

**Interfaces:**
- Produces: `JKEventType::MouseChord`(detail=1) — 앱 계약("두 버튼 동시")

- [ ] Step 1: 셀프테스트 실패 케이스(챠드 판별: L보유+R도착=chord·L단독=no) 작성
- [ ] Step 2: 실행→FAIL 확인
- [ ] Step 3: TranslateSDLEvent 최소 구현
- [ ] Step 4: 셀프테스트 PASS+4축 캐논 등호 확인(posix/Win)
- [ ] Step 5: 폰 실측(챠드로 둘레 열림 육안 — EYES 항목)
- [ ] Step 6: 커밋

### Task 4: DeX 창 모드+이행 계약+재진입 정리

**Files:**
- Modify: `engine/src/server/JKWindowServer.cpp`(android 드라이버 분기 — DeX에서 SDL_SetWindowFullscreen(SDL_WINDOW_FULLSCREEN_DESKTOP)·설정 `jk.window_dex_freeform`=true면 스킵)
- Modify: `engine/src/main.cpp`(재진입 zombie 정리 — StartAcceptor 전 남은 acceptor 조인/차단·가드 우회 유지)
- Modify: `app/src/main/java/chang/jkdesktop/ForegroundService.java`(표면 재구성 생존 계약)

**Interfaces:**
- Produces: DeX 풀스크린(시스템 타이틀바 소멸)+폰↔DeX 이행 재-fit+연속 재진입 무성사망 0

- [ ] Step 1: DeX 풀스크린 수형+이행 재-fit 검증(스파이크 원문 흐름 — desktop size changed 행)
- [ ] Step 2: 재진입 3회 반복(닫음→오픈×3) — 무성사망 0+srv.err에 RE-ENTRY 원문
- [ ] Step 3: 폰 실측(외형 동일성 — 새싹 vs 이행 — EYES 항목)
- [x] Step 4: 완료 13f0118 — 조건부 혼입 함정 원존(music/filedlg)

### Task 5: ChatRouter 스레드 in-process(jkweb 폐기)+음성 STT(SpeechRecognizer)

**Files:**
- Modify: `engine/android/jkdesktop-apk/app/src/main/jni/CMakeLists.txt`(채팅 HTTP 스레드 목표 — ChatRouter+소량 소켓 서버)
- Create: `engine/src/voice/VoiceService_Jni.cpp`(__ANDROID__ 전용 — JNI에서 SpeechRecognizer 결과를 ChatRouter 턴으로 주입·[구두] 표면 계약 유지)
- Create: `app/src/main/java/chang/jkdesktop/JkVoiceService.java`(RECORD_AUDIO 권한 흐름+SpeechRecognizer 세션+JNI 콜백)
- Modify: `engine/src/apps/JKAppModule_chat.cpp`([구두] 버튼→JNI 훅 호출 — 기존 F6 계약 유지)

**Interfaces:**
- Produces: `POST /talk {"text":...}` = 200+Launch/Reply(기존 jkweb 계약 유지)+[구두] 버튼 → 인식 → 턴 → 앱 실행

- [ ] Step 1: ChatRouter HTTP 스레드(localhost :8090 — 기존 라우트 표 원용) + selftest 케이스(POST /talk 근사 매칭)
- [ ] Step 2: 폰 curl /talk → 200+kind 원문(기존 phone_apk ④ 계약 동형)
- [ ] Step 3: JkVoiceService+JNI 주입 — [구두] → 결과 턴
- [ ] Step 4: 폰에서 "지뢰창 찾기를 켜줘" 발화 → 실행 원문+육안(EYES 항목)
- [ ] Step 5: 커밋

### Task 6: TTS 시스템 API leg

**Files:**
- Create: `engine/src/voice/JkTtsAndroid.cpp`(__ANDROID__ — JNI TextToSpeech speak/stop — 15s 마감·실패무시 계약 유지; Win 축 SAPI 무접촉)
- Modify: `app/src/main/java/chang/jkdesktop/JkTtsService.java`

- [ ] Step 1: JNI 브리지+앱 콜백 배선
- [ ] Step 2: 폰 실측(응답 음성 — EYES 항목)
- [ ] Step 3: 커밋

### Task 7: ollama cloud/HTTP 어댑터 통일

**Files:**
- Modify: `engine/src/agent/JKLlmEngine.cpp`(HTTP 어댑터 — (a)`ollama_host` 설정 재용 (b)cloud 모드: `https://ollama.com/api/chat`+Bearer `OllamaApi` 설정·모델 `*` 접미 표기 원존)
- Modify: `engine/src/apps/ChatRouter.cpp`(LLM 지정 턴의 어댑터 분기 — 근사 매칭 우선 유지)

**Interfaces:**
- Produces: `llm.mode=off|local|cloud`(settings) — local=ollama_host·cloud=ollama.com+API키

- [ ] Step 1: 어댑터+settings 키(폰 내부 저장 — 민감값)
- [ ] Step 2: 폰 실측(cloud 턴 성립 원문 — API키 사용자 준비)·로컬은 BLOCKED 유계 허용(라인 재개 몫)
- [ ] Step 3: 커밋

### Task 8: 저장 권한+뮤직 라인

**Files:**
- Modify: `app/src/main/AndroidManifest.xml`(MANAGE_EXTERNAL_STORAGE+isExternalStorageManager 흐름)
- Create: 조달 — OpenAL soft 1.23.1(sha256 핀 — install_android_sdk.sh §openal)+openal CMake4 거절 함정(원존: CMAKE_POLICY_VERSION_MINIMUM 3.5)
- Modify: `app/src/main/jni/CMakeLists.txt`(spatial-player 2파일+openal 정적 링크 — SPATIAL_PLAYER_ROOT 계약 재용)

**Interfaces:**
- Produces: 뮤직 앱(스캔·재생·태그·커버아트) 폰 실측

- [ ] Step 1: 권한 흐름(설정 유도 1회) + 잔여 원장행 봉질: `text.font_path` 설정키(files/fonts/HANMOON.VFT 유도 — 벡터 폰트 결선 — docs/103 원문 원존)
- [ ] Step 2: openal 조달+빌드(OpenSL 백엔드)
- [ ] Step 3: 뮤직 모듈 링크+폰 재생 실측(전사 EYES 항목)
- [x] Step 4: 완료 13f0118 — 조건부 혼입 함정 원존(music/filedlg)

### Task 9: 터미널 leg

**Files:**
- Modify: `engine/src/terminal/JKConPtyBridge_posix.cpp`(셸=/system/bin/sh=mksh 계약 — 스파이크 ShellPath 수형 흡수)
- Modify: CMake(jkapp_terminal lib*.so 포장 — Task 2에 이미 포함되면 재검만)

**Interfaces:**
- Consumes: lf/helix 조달(static aarch64) — lib*.so 포장 재용

- [ ] Step 1: 터미널 창 mksh 실제 동작 폰 실측
- [ ] Step 2: lf/helix 포장 배치(조달 핀 유지)
- [ ] Step 3: 커밋

### Task 10: 설계 문서 as-built+캐논 정산+EYES 정리

**Files:**
- Create: `docs/104_jkdesktop_apk_unified_asbuilt.md`
- Modify: memory MEMORY.md(갱신 계보)

- [ ] Step 1: as-built(커밋 체인·핀·함정 원존·측정표)
- [ ] Step 2: 캐논 4축 등호 검산(Win/WSL/posix/폰 — APK 라인은 캐논 미변죄 원존)
- [ ] Step 3: EYES 목록 확정본 정리(사용자 결제 대기 표기)·커밋

## v2 명시(별도 라인 — 이 플랜 밖)

vplayer(MediaCodec+자체 MP4 디먹스 — "길 2")·Whisper 재봉·BOOT 자동 기동·SAF·스토어 서명·폰 내장 LLM.
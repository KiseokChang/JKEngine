# 101. JKDesktop을 APK로 — 조사 정리와 새 세션 인계 (2026-11-01)

> 상태: **조사 완료 · 착수 전**. 사용자 지시: "JKDesktop을 termux 기반이 아니라
> APK로 만들 수 있나요? 조사만" → "이 작업은 새로운 세션에서 하고 싶어요".
> 이 문서가 새 세션의 출발점이다. 아직 스펙(설계 승인)이 아니다 — 새 세션은
> 브레인스토밍(아키텍처 경로)부터 시작해 사용자 승인을 받는다.

## 0. 왜 하려는가 (사용자 맥락)

- 최종 그림: DeX 큰 화면의 "나만의 OS"를 말로 조작 (기억: DeX+voice vision).
- 지금 폰 구성은 Termux 안의 jkdesktop 서버 + jkweb + ollama + Termux:X11(화면)
  + 별도 JK 앱(APK, 음성 인식·브라우저). 재부팅마다 sshd·X11·서버·jkweb을
  손으로 켜야 하고(2026-10-31~11-01 반복 사고: X 서버 소멸, 태스크바 중복,
  jkweb 구판 잔존), JK 앱과 데스크톱이 따로 놀아 브라우저도 JKDesktop 창이
  아니라 DeX 별도 창이다.
- 한 APK로 묶으면: 아이콘 하나로 시작, 음성·브라우저·데스크톱이 한 화면.

## 1. 결론

가능하다. 단 "포장"이 아니라 **이식(port)** 이다. 막히는 핵심은 두 가지:
1. 앱마다 별도 프로세스로 띄우는 구조(fork+exec 자기 자신) — Android 10+
   W^X 규칙이 앱 데이터 폴더의 exec를 막는다.
2. 각 클라이언트 프로세스가 숨김 SDL 창을 만든다 — Android에선 화면은
   Activity 하나만 가진다(SDL Android도 창 1개 제한).

추천 방향은 **B (서버만 화면, 앱은 화면 없는 별도 프로세스)**. 근거: JKDesktop은
원래 서버가 모든 앱 그림을 공유 메모리로 받아 합성하는 구조라, 클라이언트가
실제로 화면에 그릴 필요가 없다.

## 2. 현재 의존 지도 (코드 근거, 경로는 engine/ 기준)

| 영역 | 지금 | 근거 |
|---|---|---|
| 화면 | Termux:X11 `:1`, Termux의 SDL2가 X11 클라이언트로 연결 | docs/78 §145-155, tools/probes/termux_bootstrap.sh:21 |
| 렌더 | `__ANDROID__`에서 software 렌더러 강제(llvmpipe present 140ms 회피), fit-to-display | src/server/JKWindowServer.cpp:161-166, 186-201, 216-224 |
| 클라이언트 창 | 클라이언트마다 SDL_Init(VIDEO)+숨김 창+ACCELERATED 렌더러 | src/client/JKClientApplication.cpp:96-116 |
| 입력 | X11이 터치를 마우스로 변환, `SDL_FINGER*` 처리 0 | JKWindowServer.cpp:255 |
| 앱 실행 | 서버가 `/bin/sh -c '<exe-dir>/jkdesktop --client <app>'` 또는 `--jkx`로 재실행 | JKWindowServer.cpp:9339-9368, 9403-9430 |
| 프로세스 | pipe+fork+dup2+execl("/bin/sh"), kill(-pgid), waitpid | src/process/JKProcess_posix.cpp:100,129,202,338-360 |
| 합성 | 클라이언트는 오프스크린 → 공유 메모리 + CommitSurface(더티 사각형) | src/client/JKClientSurface.cpp:80,114-117,165-180,330,448-452 |
| IPC | AF_UNIX 파일 소켓 `TempDir()+name.sock` | src/ipc/JKPipeTransport_posix.cpp:32-68,162-169 |
| TempDir | `$TMPDIR` 또는 `/tmp/` | src/fs/JKFs_posix.cpp:43-63 |
| 공유 메모리 | Android: `$TMPDIR/jkshm_<name>` open+ftruncate+mmap | src/ipc/JKSharedMemory_posix.cpp:14-95 |
| 앱 모듈 | `<exe-dir>/jkapp_<app>.so` dlopen / `.jkx`의 MODL을 TempDir에 풀어 dlopen | src/main.cpp:482-600 |
| exe 경로 | readlink /proc/self/exe | src/fs/JKFs_posix.cpp:29 |
| 마이크 | termux-microphone-record + ffmpeg 16k 변환 | src/voice/VoiceCaptureTermux.cpp |
| TTS | termux-tts-speak | src/apps/ClientChatApp.cpp:26-40,106-195 |
| 음성 인식 | whisper-cli 자식 프로세스 | src/voice/SttVoice.cpp |
| LLM | `ollama run` 셸 실행(ollama-direct 등) | include/agent/JKLlmEngine.h:49,100-115 |
| 오디오 | SDL2 audio + SDL2_mixer, spatial = 정적 OpenAL(OpenSL ES) | CMakeLists.txt:38,723-779 |
| 터미널 앱 | openpty+fork+setsid+exec /bin/sh (Termux bash·lf·helix) | src/terminal/JKConPtyBridge_posix.cpp |
| 라이브러리 | Termux pkg: sdl2, sdl2-mixer, ffmpeg, libiconv, CJK 폰트 | termux_bootstrap.sh:17-24, CMakeLists.txt:281-300 |
| jkweb | 별도 바이너리 :8090, JK 앱이 루프백 HTTP로 호출, **ChatRouter 내장** | CMakeLists.txt:990-1001 |
| 기존 Android 빌드 | jkdesktop 자체는 없음. jkapk(얇은 앱 + libwhisper_jni.so)만 있음 | engine/android/jkapk |

## 3. Android 쪽 사실 (출처)

- Android 10(targetSdk 29+)부터 앱 홈 폴더 파일의 execve 금지(W^X).
  [developer.android.com](https://developer.android.com/about/versions/10/behavior-changes-10)
- 우회: 실행 파일을 `lib<name>.so`로 jniLibs에 넣으면 설치 시 nativeLibraryDir
  (읽기 전용·실행 가능)에 풀린다. `extractNativeLibs=true`(또는 AGP
  `useLegacyPackaging = true`) 필요. Termux도 같은 방식을 계획했다.
  [termux wiki](https://github.com/termux/termux-packages/wiki/Termux-and-Android-10/5d899145ab70caa6e484609baf6c354651150230),
  [W^X 우회 정리](https://yadavyashvant.medium.com/how-to-run-a-real-web-server-on-your-android-phone-no-root-required-73cbb1630255)
- dlopen(mmap PROT_EXEC)은 막히지 않는다는 실측 보고(Android 15).
  [issue #288](https://github.com/logos-fleet/logos-workspace/issues/288)
- SDL2 Android = SDLActivity(SurfaceView) + JNI, DeX/자유 창 크기 변경 지원,
  **창 1개 제한**("Android only supports one window").
  [SDL README-android](https://github.com/libsdl-org/SDL/blob/main/docs/README-android.md),
  [SDL Discourse](https://discourse.libsdl.org/t/request-support-multiple-windows-on-android/27641)
- 기존 원장(whisper_jni.c:10-12): "jniLibs .so는 exec 불가"라고 적혀 있으나
  위 우회(이름 바꿔 nativeLibraryDir에서 exec)와 충돌 — **새 세션의 스파이크로
  실기기 확인 필요**(이 폰 SM-F926N, Android 버전·targetSdk 조합).

## 4. 방향

| 방향 | 내용 | 평가 |
|---|---|---|
| A | X 서버(Termux:X11 상당)까지 APK에 넣고 지금 바이너리를 거의 그대로 실행 | Termux를 다시 만드는 셈 — 크고 복잡 |
| **B (추천)** | 서버 = SDLActivity의 유일한 화면. 클라이언트 = nativeLibraryDir에서 exec되는 별도 프로세스, SDL 비디오 없이(오프스크린/더미) 공유 메모리로만 그림 | 프로세스 격리 유지, 합성 구조와 맞음 — 중대형 |
| C | 모든 앱을 한 프로세스(스레드/모듈)로 | 구조 변경 최대, 앱 하나 죽으면 전체 사망 |

B에서 함께 바꿔야 하는 것: TempDir/소켓/공유 메모리 경로 → 앱 캐시 폴더,
exe-dir 기준 경로 → nativeLibraryDir + 풀어 둔 assets, `/bin/sh -c` 재실행 →
직접 exec(nativeLibraryDir), 터치 입력(SDL_FINGER 또는 touch-as-mouse 힌트),
마이크·TTS → Android API(JNI), whisper → 프로세스 안 링크(jkapk JNI 재사용),
ollama → HTTP, SDL2/SDL2_mixer/OpenAL/iconv → NDK 빌드, 폰트·assets → APK,
수명 → 포그라운드 서비스.

잃는 것: 터미널 앱의 Termux 도구(bash·lf·helix). 개발용 sshd·probe 경로도
Termux 의존이라 시험 방식 재설계 필요.

얻는 것: 아이콘 하나로 시작, JK 앱(음성·브라우저)과 통합, 브라우저를
JKDesktop 창 위치에 WebView로 겹쳐 띄우는 "JKDesktop 안의 브라우저" 가능성.

## 5. 새 세션의 첫 단계 — 시험판(스파이크) 범위

질문 하나만 답한다: **"방향 B가 이 폰에서 실제로 되는가?"**

1. 최소 APK: SDLActivity로 jkdesktop **서버**를 띄워 DeX 화면에 데스크톱 배경
   +태스크바 수준까지 그린다.
2. 지뢰찾기 클라이언트 1개를 `libjkdesktop.so`(실행 파일을 이름만 바꾼 것)
   형태로 nativeLibraryDir에서 exec → SDL 비디오 없이 공유 메모리 합성으로
   서버 화면에 보이는지.
3. 터치로 지뢰찾기 칸이 눌리는지.
4. 결과를 표로: exec 성공 여부(에러 원문), 합성 성공 여부, 터치, 프레임 시간.

이 넷이 되면 방향 B 확정 → 스펙 → 플랜(SDD). 안 되면 원인 원문과 함께 A/C
재검토. 스파이크 코드는 버리는 코드로 표시한다.

## 6. 새 세션이 알아야 할 현재 상태 (2026-11-01)

- HEAD는 main. 캐논: Win 937 / WSL 914 / 폰 914 (2y 44).
- 음성 라인 최근 커밋: e0aef54(-O3), 0fcfac5(armv8.2·ctx 재사용·audio_ctx),
  74409f2(/talk 60s), 7d585d5(근사 매칭), e9a790c(초기 프롬프트 철회).
- GPU(Vulkan, Adreno 660)는 모델 로드 중 SIGSEGV·회피판도 CPU보다 3배 느림
  → CPU 고정(.superpowers/sdd/2026-10-31-apk-gpu/gpu-spike-report.md).
- jkapk는 병행 세션(#123 폰 브라우저, docs/100)이 빌드 전담 중. 같은
  `engine/android/jkapk`를 만지면 반드시 조율(시험용 APK는 jkapk-test.apk,
  덮어쓰기 전 통지 — 합의 완료).
- 폰 접속: Termux sshd 8022, 키 ~/.ssh/termux_jkengine, 주소는 대역이
  바뀌므로 매번 스캔(문서·커밋에 주소 기록 금지).
- 폰 서버 배포 함정: jkweb도 ChatRouter를 내장 → 서버 배포 후 jkweb 재기동.
- 빌드 함정: MSYS2 도구(vulkan-shaders-gen 등)는 `PATH=/c/msys64/ucrt64/bin`
  선행 필수, gradle은 SDK 경로를 절대 경로로.

## 7. 아직 안 끝난 것 (이 세션에서 넘김)

- 사용자 체감 대기: 새 JK 앱(md5 91bb9a93)에서 음성 인식 속도·정확도.
- 다음 후보(음성): LLM 프롬프트에 설치 앱 전체 목록 + 행동 종류(app_tool·창
  번호) 보강, ① Android SpeechRecognizer 전환, Whisper 모델 양자화(APK 480MB
  중 모델 488MB).
- 이 APK 이식을 하면 위 음성 항목들의 위치(어느 APK에 들어가나)가 바뀐다 —
  새 세션 브레인스토밍에서 함께 정할 것.

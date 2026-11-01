# 103. #125 스파이크-B 리포트 — 방향 B 폰 실측 (2026-11-01)

> 질문(docs/101 §5): **"방향 B(서버=SDLActivity의 유일 화면 + 클라=nativeLibraryDir
> exec·SDL 비디오 없이 공유메모리 합성)가 이 폰에서 실제로 되는가?"**
> 답: **된다 — 4질문 전원 폐곡(측정표 §1)**. 스파이크 코드 전량 폐기 라벨
> (engine/android/spike-b — 본판 이식에 병입 금지·개념만 승계).

## 1. 측정표 (사용자·probe 원문)

| 질문 | 판정 | 근거(원문) |
|---|---|---|
| exec 성립 | ✅ | `/data/app/…/lib/arm64/libjkdesk.so X_OK=1` — nativeLibraryDir exec 허용(SM-F926N·Android 15). bionic 링커 기동 = `CANNOT LINK…`·`Illegal instruction` 상전이 원문 둘 다 실측. |
| 합성 성립 | ✅ | 클라 커밋 원문 `commit id=2 nonzero=1024/1024 …=ffc0c0c0`(셀 회색 착지) → 서버 `texture id=2 …=ff444444/ff202020`(콘텐츠 수취) → **사용자 육안**: 배경·그리드·지뢰찾기 보드·글자·태스크바 전부 보임. |
| 터치 | ✅ | 좌표 수형(SDL_FINGER→Mouse) 후 **사용자 육안 탭 반응 결제**("탭도 잘 되요"). |
| 프레임 시간 | ⏳ 정밀 미측정 | commit-stall 마크(kCommitStallMs=100ms) 부재 → <100ms·본판의 정밀 측정(4)으로. |

## 2. 결함 사슬 진원 원문 (블랙 화면 4층)

1. **exe-dir 계약 붕괴** — SDLActivity는 argv0에 `app_process`를 주고
   `/proc/self/exe`=/system/bin/app_process64(zygote 승계) → 엔진의 exe-dir
   규약(exe 옆 모듈·에셋 탐색) 전체 어긋남. 원문: `[spikeB] SDL_main
   argv0=app_process selfExe=/system/bin/app_process64 args=--server`.
   수형: 심(jksrv_main)이 dladdr로 진입 물리 경로 역산 → JK_EXE_PATH →
   GetExecutablePath의 Android leg(JKFs_posix).
2. **ET_DYN exec 불가** — 공유 객체는 PT_INTERP 부재라 커널이 e_entry로 점프
   → 부팅당 `Illegal instruction` 즉사. 수형: 진짜 PIE 실행파일을
   lib*.so로 포장(jniLibs 규약 통과 — W^X 우회의 정석 수형).
3. **SDL 게이트 2종** — execed 클라는 SDLActivity 사다리가 없어
   SDL_Init main-ready 게이트(원문: `did you include SDL_main.h...`) →
   SDL_SetMainReady 1행; offscreen 드라이버는 내부가 GBM/EGL(원문:
   `eglQueryDevicesEXT is missing`) → dummy 드라이버 1행.
4. **SDL SW 렌더러 원시 미착지(오래 걸린 것)** — SDL_CreateRenderer(더미
   창 경로)+SW 렌더러의 큐잉 원시(FillRect·DrawLine·DrawPixel) 전부
   미래입, Clear만 착지. **고립 분절 원문**(같은 libSDL2.so·폰 adb shell):
   `set(192,192,192,255)→get=192,192,192,255`(상태 정상)·`fill rc=0 →
   read=ff202020`(미착지)·**`G: SDL_CreateSoftwareRenderer(표면 직결) →
   00646464 착지`**. → 수형 = 클라 렌더러를 SDL_CreateSoftwareRenderer(
   윈도우 표면)로 교체 + 타깃 NOP(윈도우 표면 일직선 렌더). **엔진 무죄
   — SDL 만듦 경로 의존 결함**(동일 버전 Termux 빌드는 정상 운용 중).
5. **터치→마우스 공백** — X11이 대행하던 변환 지대(서버 SDL_FINGER 처리 0).
   수형: TranslateSDLEvent의 SDL_FINGERDOWN/UP/MOTION→Mouse(정규화 좌표×
   창 크기·__ANDROID__ 게이트).
6. **재진입 무성 사망** — 닫았다 다시 열기(같은 프로세스 SDL_main 재진입)
   선행 부트 leaked fd(flock·소켓 경로)가 guard/StartAcceptor를 침묵사망
   시킴(logcat Fatal 0건 원문). 수형: 재진입 판 JK_SPIKE_NO_GUARD+
   endpoint unlink 재바인드.
7. **에셋 CWD** — zygote 계승 CWD에서 상대 `assets/…` 전부 미검출(원문:
   JKImageLoader fail 열) → files/ chdir+APK 에셋 추출.

## 3. 수형 총 목록 (커밋 체인: 7f9c3f5→…, 전부 __ANDROID__/스파이크 게이트)

- `engine/android/spike-b/` — 스파이크 APK(폐기): SDL2 2.32.10 벤더+
  SDLActivity 글루·libjkdesk.so(SDL_main 심·TMPDIR 시딩·srv.err 로그
  채널·MediaStore 내보내기)·libjkc.so(exec 전용 PIE)·libjkapp_minesweeper/
  taskbar.so·mixer 음소거 심 20종·bionic iconv 승계(iconv.h 심).
- 엔진 소수행: JKFs_posix(JK_EXE_PATH)·JKProcess_shell(/system/bin/sh
  폴백)·main.cpp(SetClientHostExe·가드 우회·endpoint unlink·
  JK_SPIKE_AUTOSPAWN)·JKWindowServer(모듈 lib 접두·android 드라이버 SW
  힌트 제외)·JKClientApplication(더미+SetMainReady+CreateSoftware
  Renderer 수형)·JKEvent(SDL_FINGER→Mouse)·JKCompositor(계량기→철수).

## 4. 잔여 원장행 (본판 이식의 몫 — 아키텍처 블로커 없음)

1. 벡터 폰트: `text.font_path` 설정 비움(원문) → 설정 봉질 필요. 비트맯
   글리프 폴백은 운용(사용자 육안: 글자 보임).
2. 프레임 정밀 시간 측정(commit-stall 마크만으로 <100ms 추정).
3. terminal 스폰 leg(lf/helix — Termux 바이너리 비팩)·오디오(mixer 심
   음소거)·ollama/jkweb/음성(STT) — 본판 이식 범위.
4. DeX fit — fit-to-display 원문 성립(1920x1080 수용·리사이즈 이벤트
   구간 실측 복수) — 본판의 fit-scale 재판정.
5. 재진입 사후 정리 — leaked acceptor/서버 스레드의 zombie는 남는다
   (링크 단계 원문에서 무해 확인) — 본판에서 정리.

## 5. 결론

**방향 B 확정 권고.** docs/101의 추진 근거(공유메모리 합성 구조·SDL 창 1개
제한·W^X 우위설)가 전부 실측으로 확정됐고, 예상 밖의 실측 결함 1건(SDL SW
렌더러 창 경로)도 표면 직결 수형으로 폐곡. 다음 단계 = 스펙(설계 승인) →
플랜(SDD).
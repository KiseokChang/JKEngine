# docs/78 — Termux 패키징·폰 실기기 도달 워크플랜 (2026-10-05)

상위 스펙: docs/62 §2·§3(2단계 후반부), docs/70 §6 #6 잔여. 선행 완결:
리눅스 1~3단계 W1-W9(docs/68~70) + 플랜 G/H(docs/72~73) + vplayer WSL
재생 실측(docs/70 §8). 이 원장의 달성 대상 = **ARM aarch64 폰(Termux)에서
엔진 전체 기동 + 앱 실측 + 출하 팩 눈확인**.

## 0. 근거 요약

- 스펙 핵심 발견(docs/62 §1): 데스크톱 전체 = OS 창 1개 + 내부 창 관리자 →
  폰에서도 OS 창 1개(Termux:X11)만 띄우면 셸 전체 이식. 새 셸 설계 불요.
- 시스템 의존 = SDL2·SDL2_mixer·ffmpeg(pkg-config)뿐(WSL 실측 동일);
  imgui/implot/quickjs-ng/stb/miniz는 repo 내장. cef는 Windows 전용(제외).
- vplayer 표시가 이미 SDL 일반(docs/62 §8.5 — NV12 텍스처+ImGui::Image) →
  폰 소프트 디코딩 경로 그대로 존재. 목표 720p/1080p(docs/62 §6.1).
- LLM: 폰 단독 모드 + PC 백엔드 연계 모드 둘 다(docs/62 §3 — jkbridge
  PC↔폰 연계가 자동 성립 설계).

## 1. 단계

| 단계 | 내용 | 게이트 |
|---|---|---|
| TX0 | 준비 — F-Droid Termux+Termux:X11 설치, openssh+passwd+sshd, PC→폰 SSH 도달 | PC에서 ssh 접촉 응답 |
| TX1 | 툴체인+소스 도달 — `pkg install`(clang cmake ninja pkg-config sdl2 sdl2-mixer ffmpeg git openssh rsync font 패키지 실측), PC→폰 rsync(소스 부분집합) | `clang --version`+pkg-config 3종 조사 |
| TX2 | 첫 ARM 빌드 — cmake configure+`ninja -j` | 빌드 에러 0(WSL 회귀와의 차이 목록화) |
| TX3 | 헤드리스 실측 — 서버 부팅(X11 불요)+`test`(셀프테스트 인수 표기)+jkctl agent(list_windows/ping) | AppSelfTest 0 failure(s) — **aarch64 첫 영수증** |
| TX4 | 눈 부팅 — Termux:X11 + setsid 서버 부팅 + taskbar 자동 스폰 | 폰 화면에 데스크톱 셸 육안 |
| TX5 | 앱 실측 — launch_app: terminal(pty)·vplayer(엔진 소유클립 720p)·채팅/에이전트 PC 백엔드 연계(jkbridge) | 상태 폴링 영수증+육안 |
| TX6 | 출하 팩 — slot-pack 산출물을 폰으로, `--jkx` 부팅 | 슬롯 창 모양 눈확인(대기 중인 눈확인 항목 결제) |

## 2. 환경 교훈 선승계 (docs/70 §5·§8 표준)

- 부팅 표준: `(env DISPLAY=:0 setsid ./jkdesktop --server >/tmp/srv.log 2>&1 &)`
  — **wsl.exe 세션 detach 소각 우회는 Termux sshd 세션에서도 동일 원리**가
  의심됨: TX0에서 ssh 세션 detach 시 서버 생존 여부 실측(안 되면
  nohup+termux-wake-lock·setsid 재검증).
- 진단은 스크립트 파일로(wsl 인라인 따옴표 소각 레슨 — ssh도 명령 2줄 넘어가면
  파일 경유).
- 테스트 클립: 엔진 소유 `tmp/vpt2_test.mp4`(i:\@keep 금지 — docs/70 §8
  수사 원장 동일).
- 리소스: 폰 CPU 소프트 디코딩이면 ffmpeg 라이브러리는 Termux ffmpeg 패키지
  (NEON 활성) 사용 — 폰 셀프빌드 불요 추정(TX2 실측).
- 배터리 최적화 예외+termux-wake-lock 필수(안드로이드 백그라운드 킬 레슨 —
  이번 "세션 소각" 이슈와 별개).

## 3. 보안 원칙 (재명시)

- repo는 사내 전용(내부 IP 포함) — 폰 복사는 scp/rsync(PC→폰 LAN)로만,
  공개 원격에 올리지 않는다. 폰에 git 원격 두지 않음(TX1 rsync만).
- 폰 SSH는 LAN 내 PC↔폰 한정, 공용망 노출 금지.

## 4. TX2·TX3 as-built — 첫 ARM(aarch64) 빌드 + 3축 selftest 0 failure (2026-10-06 실측)

### 4.1 성취

- **폰(aarch64/clang 21/libc++) 풀빌드 완결**: `ninja -C buildterm` —
  jkdesktop+jkwinserver+jkserver/jkclient/jkcore/jkdesktop_shell+앱 .so 22종+
  jkctl/jkbridge/jktriggers/jkagentd 131/131, 빌드 에러 0.
- **AppSelfTest: 0 failure(s) 3축 동시 달성** — Windows(MinGW)·WSL(g++
 /libstdc++·glibc)·폰(clang/libc++·bionic) 전부 0. aarch64 첫 영수증.
- TX0/TX1 게이트값: sshd 8022 키 인증(u0_a4), clang 21.1.8, cmake 4.4.4,
  ninja 1.13.2, sdl2 2.32.10, SDL2_mixer 2.8.2, **libavcodec 62.28.103**
  (WSL 60.x·Windows 63과 3면 API — vplayer 소프트 디코딩 API 호환은 TX5
  실측 대기), aarch64 6코어. rust/rust-std는 apt-mark hold(133MB 절약).

### 4.2 봉합한 플랫폼 결함 6종 (TX2/TX3 — 전부 원장 커밋)

| 결함 | 원인 | 봉합 |
|---|---|---|
| ① legacy/ 미전송 | 전송 리스트에 디렉터리 누락 → "No SOURCES: jkcore" | tar 재전송 |
| ② `wancode.h` 스펠 | **대소문자 실측** — WSL /mnt/i drvfs가 대소문자 무시해 잠복, 폰 native fs 민감 | `WANCODE.H`(실재 파일명)로 스펠 교정 4파일 |
| ③ POSIX shm 부재 | **bionic에는 shm_open/shm_unlink가 없다** — 서버↔클라이언트 픽셀 경로의 지주 | `JKSharedMemory_posix.cpp` `__ANDROID__` 폴백: `$TMPDIR/jkshm_*` 네임드 파일(플래시 유백 — tmpfs 아님, TX5 성능 실측 대기) |
| ④ getdtablesize | bionic 미실장 | `sysconf(_SC_OPEN_MAX)`(glibc 동일값) |
| ⑤ `clock_cast` | **libc++는 C++20 clock_cast를 미실장**(libstdc++/MSVC만) | `jk::fs::FileTimeToSys` 단일 헬퍼로 3 사용처(JKThemeConfig 폴링·FilesListOpJson·EntryMtimeSecs) 봉합 — libc++ branch: file_clock epoch=Unix(1970 ns)라 duration 재구성이 전체 변환. **표준 time_point_cast는 아예 다른 clock을 거부**(같은 clock 전용) |
| ⑥ libiconv | bionic libc에 iconv 없음 — Termux libiconv 패키지가 기호를 `libiconv_open` 등으로 개명 | CMakeLists `check_cxx_source_compiles`(iconv_open 빈 링크) — 실패 기기만 `-liconv`. **Termux cmake는 CMakeCache에 CMAKE_SYSTEM_NAME을 안 심는다 — 시스템명 게이트는 안 걸린다**, 감지 방식이 정답 |

### 4.3 libc++ 관용 대비 실측 2건 (Windows/WSL은 우연히 살아 있던 것)

- **`std::vector<TrustRecord> g_trust` 전방선언+불완전형 전역** — GCC는 파일
  말미까지 인스턴스화를 미뤄 통과, libc++는 전역 소멸자를 즉시 인스턴스화해
  거부. jktriggers main.cpp: 정의 뒤로 전역 이동.
- **1j-d 시딩 계약 뒤집힘(진짜 엔진 이슈)**: libc++ ifstream이 *디렉터리*를
  열어 성공(bionic open(dir,O_RDONLY) 허용) → "외부 진실원 존재" 오판 →
  쓰기 실패(-1) 계약이 무변(0)으로 회귀. `JKWorkshopSeed.cpp ReadWhole`에
  is_directory 가드 — 모든 플랫폼에서 외부=파일 동형 고정. **폰 실측이
  잡아낸 엔진 결함 1건 — TX3 실측의 진짜 산출.**

### 4.4 셀프테스트 리터럴 포터빌리티 (win32 전용 케이스 가드)

- `C:/Windows/Fonts/malgun.ttf·consola.ttf` 리터럴, ConPTY powershell.exe,
  stub 자식 cmd.exe — 전부 Windows 기기에서만 존재. **WSL은 interop 덕에
  우연히 통과하던 것**(cmd.exe/powershell.exe 부트) — interop 유무로 판정이
  흔들리지 않게 case wide-glyph A/B/C + case 14를 `#ifdef _WIN32` 고정
  (case 14는 원래 주석도 "win32-only by convention").
- 실측 전력: 폰 최초 실행 350 PASS/10 FAIL → scripts/ 누락 보충(전송 리스트
  누락 제2종)+1j-d 수리+가드로 **0 failure**. 가드 1차 실수(`#ifndef`를
  써서 posix에서만 켜짐+`#endif`가 리플로우 블록 닫는 괄호까지 덮어 RunAppSelfTest
  미닫힘) — `#ifdef` 정정 후 3축 확인. 셀프테스트 명령은 `test`(아님
  selftest — 잘못 부르면 MyApp 루프 스핀 후 멈춤, TX3에서 1회 소각).

### 4.5 절차 교훈 (기존 원장 승계)

- tar 전송 경로: 폰 루트 `-C ~/JKENGINE`로(아니면 `engine/engine/` 중첩
  실측 — 2회 소각). 로그는 라운드 센티넬(TX2-ENDn)로 구역 절삭 후만 grep —
  전체 로그 grep엔 옛 라운드 에러가 섞여 오판한다.
- pgrep -f 자기 매칭 오판 재실측: probe의 bash -c 커맨드라인이 검색어를
  포함하면 자기 자신과 매칭 — pgrep -x + ps aux 파싱 병용.
- 폰 방전은 빌드 중단(중단 지점 78/132) — 재기동 후 ninja가 이어받음(체크포인트
  성질 그대로). termux-wake-lock+충전 케이블이 장시간 빌드 표준.
- 배터리 방전→재부팅 후 sshd는 재실행 필요(사용자 수동 — 표준 부트 시트에
  추가: `sshd` + `termux-wake-lock`).

### 4.6 폰 헤드리스 서버 부팅 영수증 (TX3 잔여 — 2026-10-06)

- **서버+taskbar 자동 스폰 성공**: `(setsid nohup ./buildterm/jkdesktop
  --server > ~/srv.log 2>&1 &)` — surface 1 "shell" 등록 + `[uistall] commit`
  커밋까지(docs/72 WSLg 셸 부팅 표준의 폰 재현; X11 불요 헤드리스).
- **ssh 세션 detach 생존 PASS**: 부팅 ssh 종료 후 신규 세션에서 서버+taskbar
  살아 있음 — docs/70 §8 setsid 표준이 Termux ssh 세션에서도 성립(워크플랜
  §2의 의심 항목 결제 — 의심 아니었음, 동일 원리).
- jkctl agent ping `{"ok":true,"pong":true}`, list_windows `{"ok":true,
  "windows":[]}` (앱 미런치 — 실앱 윈도우는 TX5).
- 이 과정에서 봉합 2건 더: **단일 인스턴스 가드**가 `/tmp` 하드코딩(폰 EACCES)
  → `JKInstanceLock_posix.cpp` `jk::fs::TempDir()` 기반, **유닉스 소켓 경로**
  동일 문제 → `JKPipeTransport_posix.cpp::MapEndpointName` `/tmp/` →
  `jk::fs::TempDir()`(가드 봉합과 같은 지주). WSL 회귀 재GREEN(0 failure).

### 4.7 남은 것

- TX6 출하 팩 `--jkx` 부팅 눈확인.
- docs/70 §8.4 부수 관측 2건 판정(ended 조기 플립·서버 .ttc) — PC 쪽 대면.
  (조기 플립은 §5 TX5에서 **WSL 24.4 / 폰 24.66 양쪽 재현** — 플랫폼 공통
  현상으로 격상. 디먹스 EOF~프리젠테이션 괴리 추정은 유지.)
- TX5 성능 메모: 폰 shm이 파일 유백($TMPDIR)인 상태의 vplayer 지연 관측.
- 관측(비막음): X11 화면 부팅 상태 서버 CPU ~99%/클라 ~70-90% — 컴포지터
  소등(vsync/pacing) 조사 후보. TX6 성능 메모.

## 5. TX4·TX5 as-built — 눈 부팅 + 앱 실측 (2026-10-06 실측)

### 5.1 TX4 눈부팅 영수증

- **폰 화면에 데스크톱 셸 육안 확인(사용자 "보여요")** — Termux:X11 앱 열기
  (사용자 조력) → X 서버 `termux-x11 com.termux.x11 :1`(CLI) → 소켓
  `$TMPDIR/.X11-unix/X1` → `DISPLAY=:1` 서버 부팅 → 태스크바 자동 스폰
  (surfase 1 셸 등록, 1280x800/40) → **런처 아이콘 직접 탭으로
  minesweeper 창 생성(사용자 조작)** — G 표준 셸 부팅의 폰 완결.
- 절차: Termux 메인 repo에 CJK 폰트 패키지 부재(`pkg search noto` 빈 결과)
  → **PC malgun.ttf(13.4MB)를 폰 `buildterm/state/fonts/`로 복사 +
  settings.json `text.font_path` 오버라이드**(리졸버 1순위,
  JKTextAtlas.cpp:59). Noto .ttc는 docs/70 §8.4의 init 실패 종류라 .ttf
  선택. 폰 배치 노트: exe=`~/JKENGINE/engine/buildterm/jkdesktop` —
  settings/terminal.json은 exe 옆 `buildterm/state/`·`buildterm/`.
- 부팅 스크립트 `~/tx4_boot.sh`(pkill+setsid+`DISPLAY=:1`) — TX3 헤드리스
  서버(부팅 후 수시간 생존 재확인)는 정상 절차로 교체.

### 5.2 TX5 앱 실측 — 3앱 생존 + vplayer 재생 첫 영수증

- **terminal**: 폰에서 창 생성 성공(800x500, list_windows 확인).
- **minesweeper**: 사용자 런처 탭으로 창 생성+focused — 클라 3프로세스
  (server+taskbar+app) 전부 생존.
- **vplayer 재생(libavcodec 62.28.103 — 3면 API 진화의 폰면)**:
  `launch_app` ok → `app_tool open`(클립 `~/tmp/vpt2_test.mp4`) →
  `{"ok":true,"windowId":28,"accepted":true}` → get_status 폴링: pos 실시간
  진행(3s 폴링당 ~3.4s), `dur:30.000, error:"", ended:true` 수령, pos 최종
  **30.000 도달**(WSL 29.954보다 온전). **디코딩·프리젠테이션·EOF 전 정상 —
  ARM 폰 소프트 디코딩 첫 영수증.**

### 5.3 봉한 결함 1건 + 와이어 진단

| 항목 | 내용 | 봉합 |
|---|---|---|
| 터미널 폰 즉사 | `JKTerminalConfig.h` 기본 셸이 `"powershell.exe -NoLogo"`(Win32 리터럴) — 폰에선 `mksh -c "powershell.exe -NoLogo"` = not found → 자식 127 → 클라 사망. **WSL은 interop으로 powershell.exe 실존 — 우연 생존 케이스 3번째** | posix branch 기본 셸 = `$SHELL` env(Termux 항상 설정), 미설정 `/bin/sh` 폴백 |
| jkctl agent 와이어 | 도구 키는 `tool`(아님 name), 도구 인자는 `args`(아님 arguments) — `JKWindowServer.cpp:3361` GetStr("tool") | 원장 기록 (잊으면 bad_request/missing_app 진단 소모) |
| vplayer 조작 와이어 | `{"tool":"app_tool","args":{"app":"vplayer","tool":"open","args":{"path":...}}}` — docs/70 §8.2 사이클 재사용 | probe `wsl_vp_cycle.sh` 선례 승계 |
| 서버 소스 원장 | `/bin/sh`는 폰에도 동작(Android `/bin→/system/bin` 심볼릭링크, mksh) — `jk::process::ShellPath` 수정 불요 | 관측으로 정정(진단 중 오판 회피) |

- 배포 레이어: 폰 `buildterm/terminal.json` 시딩 — shell=Termux bash 절대경로,
  font/fontFallback=앞서 심은 malgun.ttf(Consolas 리터럴은 폰 부재 →
  placeholder 열화 방지). **코드 아닌 배포 문인 이유: 지뢰는 죽음(셸)만
  엔진 결함이고 폰트는 열화** — terminal.json이 이 폰트를 담당하는 문서화
  스팟(docs/26 단계 5).
- 좀비 관측: 죽은 클라가 서버 자식으로 `[jkdesktop] <defunct>` 잔류
  (ppid=server) — CleanupDisconnectedClients 재aping 타이밍 후보, TX6
  판정. 비막음.
- 셀 스크립트 함정 추가: 유저딘 heredoc에서 `$CLI`가 원격 파싱 시점에
  미리 확장 — **quoted heredoc(`<<"EOF"`) 표준**(wsl 인라인 따옴표 소각
  레슨의 폰 재현).

## 6. 커밋 원장

| 커밋 | 내용 |
|---|---|
| 24be351 | (TX1) 워크플랜+termux_bootstrap.sh |
| 47e4c9f | (TX2/TX3) ARM 봉합 12파일+as-built §4 — 3축 selftest 0 failure |
| fd257ed | (TX3 잔여) 가드/소켓 /tmp → TempDir 폴백 2건+폰 헤드리스 서버 영수증 §4.6 |
| (본 커밋) | (TX5) 터미널 posix 기본 셸 $SHELL + TX4·TX5 as-built §5 |
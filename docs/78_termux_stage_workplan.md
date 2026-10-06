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
| TX6 | 출하 팩 — slot-pack 산출물을 폰으로, `--jkx` 부팅 | 슬롯 창 모양 눈확인 — **기계 영수증 완결 §5.4(게이트 해체+SideFilePath dladdr 봉합, 폰 workshop 창 수령), 육안 결제만 대기** |
| TX7 | CPU 소등(부산물 — §5.5·§5.7): 페이싱+활동 게이트+좀비 reaping+폴백 커밋 스킵+폰 SW 렌더러 | WSL 서버 124→6.7%+3축 selftest, 폰 99→34.4%(잔여=진짜 변화 3합성/s · present ~70ms — 더티프리젠트 백로그) |
| TX8 | jkbridge 폰↔PC 연계(§5.6) | PC 게이트웨이+WSL 도달 영수증 완료; 폰→PC는 네트워토폴로지(환경) 불도달 — 폰 Wi-Fi 전환 후 사용자 몫 |

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

### 5.4 TX6 as-built — 출하 팩(--jkx) 게이트 해체 + 폰 워크숍 수령 (2026-10-06 실측)

- **게이트 해체 원리**: 4종 라우트(--jkx·slot-pack·jkx-list·jkx-extract)는
  순수 stdio+JKJkxFile(TOC 파서 어댑터리)이어서 개방 — RunJkxPack만
  LoadLibraryA 메타 소싱이라 win32 전용 유지(`#endif` 이동; 옛 trailing
  `#endif` 삭제 — 삭제 누락 한 번으로 `'#endif' without '#if'` 실측,
  지역 이동 수술의 함정). server SpawnClient posix leg `--jkx '<path>'`
  단일인용(설치 dir 공백 케이스와 동 계약). slot-pack posix 레그 —
  슬래시 경로+`std::filesystem::create_directories` 치환(존재-ok 동형),
  MODL 엔트리 이름 `"jkapp_script.dll"`은 매니팟 키 계약으로 유지,
  **파묻히는 바이너리만 팩 기기의 `jkapp_script.so`**(전제 소멸 —
  팩 기기 자체가 MODL을 파묻는다).
- **★폰 실측 발각 신규 결함(봉합)**: `JKAppModule_script.cpp`
  `SideFilePath()` posix leg가 exe 경로를 주는데 RunClientFromJkx는
  사이드카를 **추출 모듈 옆**에 떨어뜨린다 → --jkx 부팅에서 MANI
  미독 → 기본 meta(320x240 "Script App") 열화 → 워크숍 분기 미진입 →
  `cannot open script '...buildterm/jkdesktop.app.js'`. 옛 주석의
  "build .so가 exe와 동거하므로 동치" 가정이 jkx temp 추출 순간 파열 —
  플랜 G2가 posix leg(temp 추출·dlopen)를 이미 깔아 둔 순간부터 잠복한
  논리 결함. **수리 = `dladdr(&SideFilePath)`** =
  GetModuleFileNameA(FROM_ADDRESS)의 정확한 posix 쌍, 실패 시 exe 경로
  폴백(회귀 없음).
- **함정 2건 기록**: ① 라우트에 `agent`라는 서브커맨드는 없다 —
  **`agentctl '<json>'`이 정답**. `agent`로 치면 demo 앱 폴스루 →
  200% CPU 무한 루프(진단 소모 3회 — 와이어 원장 §5.3에 합류).
  ② **slot-pack은 파묻는 순간의 모듈 바이너리를 파묻는다** — 수리 뒤
  .jkx 재생성 없이 재런치하면 옛 모듈(결함 포함)이 temp로 추출되어
  같은 증상 재현 — 재팩(`slot-pack`) 필수, 그래야 새 manifest에
  수리된 jkapp_script.so가 묻힌다.
- **폰 영수증**: `slot-pack phoneprobe` RC=0(caps=widget,timer 자동
  분석, MODL=aarch64 14,837,744B) → `launch_app {"jkx":"phoneprobe"}`
  ok → **workshop 창 id 7 `title=phoneprobe 360x280`(MANI 표기 전승)**
  + 로그 `[client] module '.../jkapp_phoneprobe_17101.so' loaded:
  app='phoneprobe' title='phoneprobe' size=360x280` — 신규 `start
  failed` 없음(Start 성공). 수리 전 시도의 창 id 4(기본 meta 열화본,
  start-failed 라벨)는 agent close가 permission_denied라 잔존 —
  수신 기기 reboot 시 소각(유저 자산 아님). WSL·Windows·폰 3축
  selftest 전부 0 failure(s).
- 셀 스크립트 함정 추가(폰): /tmp 직접 로그 파일 불가(Permission
  denied) — **폰 리다이렉트는 ~/tmp/ 표준**.

### 5.5 CPU 소등 as-built — 프레임 페이싱+활동 게이트+좀비 reaping (2026-10-06 실측)

- **발각 경로**: TX5 폰 실측의 서버/클라 풀코어 스핀(~99%/~90%)을 WSL에서
  재현·국소화. 3차 국소화: ① `SDL_Delay(1)`만 있는 루프(SW 렌더러는 present
  블록 없음) → ~1000fps 스핀. ② 60fps 페이싱 결합 → 터미널 89→7.6%, 그러나
  태스크바 89.6%/서버 103~124% 잔여. ③ `/proc/<pid>/task` 스레드별 Δ측정
  (ps %cpu는 **수명 평균**이라 idle 판정 불가 — 측정 렛슨) → 서버 llvmpipe
  풀 12스레드 × ~11% = ~125%(합성이 계속 돌고 있다는 신호), 본체 5.2%.
- **계측 기법**: `JK_CPU_TRACE=1` → 서버가 `[cpustat] sdl/N msg/N
  composites/N`, 클라가 `[cpustat] timer/N input/N agent/N tool/N theme/N
  frames/N`을 초당 stderr로. 범인 확정: 태스크바 클라 `frames=63/s`인데
  활동 소스 전부 0 — **`IsFrameDirty()` 기본 true가 게이트 OR 첫 항으로
  무력화**돼 idle 63fps 렌더 → 커밋 63/s → 서버 합성 63/s → llvmpipe 풀점유.
- **봉합 4종**(플랫폼 공통 — vsync 플랫폼은 무관측):
  ① 서버 Run 60fps 하한 스트라이드(`frameWorked < 16 ? 16-frameWorked : 1`,
  늦은 프레임 흘려보냄). ② 클라 Run 동일 스트라이드. ③ **활동 게이트
  (계약 변경)**: 클라 `JKClientApplication::IsFrameDirty()` 기본값
  **false** — Run은 이번 이터레이션에 버스 활동(타이머/입력/에이전트/
  툴콜/테마)이 있었거나 IsFrameDirty(오버라이드 앱)거나 마지막 렌더에서
  1s 폴백이 지났을 때만 RenderAndCommit. 이벤트 구동 네이티브 앱(태스크바
  등 13종 무오버라이드 앱)은 idle 1fps 폴백으로 조용함 — 스크립트 앱의
  캔버스 애니메이션은 `setInterval`(api 캐탈로그 동기 갱신 완료)로 활동을
  만든다. 서버도 같은 게이트+1s 폴백(부팅 첫 프레임은 루프 진입 전 1회 합성,
  클라는 lastRenderMs = frameStart-1000 기점으로 첫 이터레이션 즉시 렌더 —
  Uint32 랩 산술). 서버 활동 원천=SDL 이벤트+`workTick_`(ProcessClientMessage
  선두++·신규 접수) — 클라 메시지 1건 수필이 곧 활동. ④ **좀비 reaping**:
  posix `CloseHandleLike`는 일발 WNOHANG이라 disconnect 정리 시 생존 자식을
  놓치고 회수 사각지대(pid 불일치 스폰 포함) — 서버 Run 루프 5s 주기
  `ReapSpawnedChildren()` 스윕(GetExitCode의 WNOHANG reap 내장 활용,
  win32는 핸들 닫힘=커널 정리라 no-op).
- **WSL 영수증(Δ 3s /proc utime+stime)**: 서버 124→**8.7%**, 태스크바
  15.5→**1.3%**, 터미널 **1.7%** — idle 3프로세스 합계 ~135% → ~12%.
  Windows·WSL·폰 3축 selftest 0 failure(s).
- **폰 재측정 영수증(재배포 — b82c0f6 소스, 5s Δ/proc)**: 서버
  99%→**44.6%**, 태스크바 90%→**2.8%**, 터미널→**6.6%**. 클라 소등 완전.
  잔여 서버 44.6%의 성질: 폰 SW 합성 자체 코스트 — 폰 화면의 full-desktop
  합성+present가 1회 ~200ms 수준이라 클라 폴백 커밋(2/s)만으로 40%대가
  성립. 더 내리려면 **컴포지터 더티프리젠트(dirty-rect blit)** 별도 과제
  — TX7 범위 밖, 백로그 등재.
- 재팩 룰 재실측: CPU 수리 코드 재배포 후 `slot-pack phoneprobe` 재팩없이
  재런치하면 옛 .so — 재팩 후 `launch_app {"jkx":"phoneprobe"}` 부팅
  재확인(360x280 meta 전승, start-failed 없음). §5.4 함정 ②의 반복 확인.

### 5.6 TX5 잔여 — jkbridge 폰↔PC 연계 실측 (2026-10-06, **환경 결함으로 부분 완결**)


- 계약 재확인(docs/57): jkbridge.exe = PC 콘솔 게이트웨이(HTTP 8790 기본,
  본 PC는 8899 지정) — 폰 브라우저가 WS로 접속, 세션당 JKAgentClient(에이전트
  허브=PC 창 서버 파이프 ∖∖.\pipe\JKWindowServerPipe)+JKLlmEngine(LLM 서브
  프로세스). 즉 **실 LLM 턴·툴 릴레이는 PC 창 서버(사용자 콘솔 소유)를
  필요로 한다** — 클로드 기동 불가 제약(§3)과 마찬가지.
- **기계 영수증(성립 분)**: ① PC `engine/build/jkbridge.exe` 기동(메인
  빌드, 토큰 state/jkbridge.json 32hex, port 8899) — 콘솔에 URL+QR 인쇄 ✓
  ② PC localhost /health="ok" ✓ ③ **WSL→PC:8899 /health="ok"** ✓ (외부
  인터페이스 리슨+방화벽 통과 양쪽 확인 — 폰 실패가 PC 측 문제 아님을
  분리하는 통제 실험).
- **불도달 영수증(폐곡선 문서화)**: 폰→PC curl(8s) rc=28 타임아웃 +
  폰→PC ping 100% loss. PC→폰은 ping 79ms·ssh 정상 — **비대칭**. 폰은
  192.168.219.x(핫스팟 대역) 인터넷만 44ms로 도달, 192.168.11.x(home
  router 대역)로의 신규 발신이 전부 무응답. 엔진 결함 아님(WSL 통제
  실험에서 같은 URL ok). 해소는 사용자 물리 조작: **폰을 공유기
  (192.168.11.x)와 같은 Wi-Fi에 붙인 뒤** 브라우저로
  `http://192.168.11.130:8899/?token=<state/jkbridge.json의 token>` —
  브리지는 기동 상태로 남겨 두었다(15h 자율 세션 말 기준).

### 5.7 CPU 소등 잔여 — 잔여 분해+폰 SW 렌더러 전환 (2026-10-06 실측, 푸시 7661b66)

- **잔여의 실체([compst] 신설 계측 — JK_CPU_TRACE에서 초당 아님, 8회마다 1인쇄)**:
  폰 서버 idle 44.6%는 ①터미널 커서 블링크(진짜 변화, 2 커밋/s) ②기타
  클라 폴백 커밋(무변화 1fps)이 유발한 full 합성이었고, 합성 1회의 비용
  분해 = **레이어 blit 0.5ms vs present ~140ms** — 93%가 SDL_RenderPresent
  (폰 X11 전체 프레임 업로드). 셸 draw(shell=)는 llvmpipe 비동기 인큐 탓에
  0.0ms로 위장, SW 전환 후 실체 8-12ms.
- **봉합 4종**:
  ① **클라 폴백 커밋 스킵** — 폴백만 유발한 렌더는 서브트리 더티가 남아
  있을 때만 이어간다. `JKControl::HasDirtyWindows()` 신설(dirtyRects_의
  소유자는 JKWindow라 자기 mainWindow_만 보면 틀린다 — 컨트롤 트리 전수
  dynamic_cast 검사). 무변화 폴백 커밋이 서버 full 합성을 유발하던 것
  소각. 첫 렌더는 `renderedOnce` 플래그로 항상 강제(부팅 더티 상태
  불보증). 스킵은 더티를 소각하지 않는다 — 소각은 페인트 몫.
  ② **서버 합성 폴백 1s→5s watchdog** — 서버 화면의 실변화는 전부 클라
  메시지(workTick_)+SDL 이벤트로 도달하므로 폴백은 안전망뿐. ③ 클라
  disconnect 정리에 `++workTick_` — 고스트 창이 watchdog까지 생존하지
  않게(레이어 소탕도 화면 변화). ④ **폰 SW 렌더러 강제**(`#ifdef
  __ANDROID__` SDL_HINT_RENDER_DRIVER=software): ACCELERATED가 폰에서 GL
  llvmpipe로 "성립"하는데 SwapBuffers에서 밀린 래스터를 워커 몰아처리해
  present가 ~140ms — SW(memcpy blit+윈도우 서피스 업로드) 전환 후
  present ~70ms. Windows·WSL은 ACCELERATED 유지(접촉 0).
- **영수증**: WSL 서버 8.7→**6.7%**(taskbar 1.3·terminal 1.7) — idle
  클라 cpustat `frames=0` 관측(스킵 동작 증명). 폰 서버 47.2→**34.4%**
  (taskbar 2.4·terminal 8.2·probe 2.6) — 잔여 합성 3/s는 전부 **진짜
  변화**(블링크 2+probe 시계 1). 3축 selftest 0 failure(s).
- **잔여의 남은 본질**(백로그 정밀화): 합성당 present ~70ms(SW 전환 후)는
  SDL SW 렌더러 present=X11 전체 업로드 구조 — 이를 더 내리려면 **더티
  rect 부분 present**(SDL_UpdateWindowSurfaceRects 또는 X11 직접 경로)가
  필요. 렌더러 백버퍼는 윈도우 서피스와 분리돼 있어 "렌더→부분 blit→
  부분 업로드" 재구성이 필요 — 별도 과제.
- **함정 원장**: fit-scale 레이어(1920x1080 surface 축소 표시)는 SW
  렌더러에서 선형 필터(SDL_ScaleModeLinear)가 무시될 우려 — 폰 레이어는
  전부 1:1이라 실측 무영향. 폰에서 fit-scale 앱(대형 surface 창) 쓸 때
  눈확인 필수. llvmpipe 경로는 선형 보장.

## 6. 커밋 원장

| 커밋 | 내용 |
|---|---|
| 24be351 | (TX1) 워크플랜+termux_bootstrap.sh |
| 47e4c9f | (TX2/TX3) ARM 봉합 12파일+as-built §4 — 3축 selftest 0 failure |
| fd257ed | (TX3 잔여) 가드/소켓 /tmp → TempDir 폴백 2건+폰 헤드리스 서버 영수증 §4.6 |
| (본 커밋) | (TX5) 터미널 posix 기본 셸 $SHELL + TX4·TX5 as-built §5 |
| (TX6 본 커밋) | 출하 팩 게이트 해체(라우트 4종 posix 개방+SpawnClient --jkx 승계)+slot-pack posix 레그+SideFilePath dladdr 봉합+TX6 as-built §5.4 — 폰 워크숍 수령 영수증 |
| (CPU 본 커밋) | CPU 소등 4종 봉합(페이싱 2+활동 게이트 2)+좀비 reaping+IsFrameDirty 계약 변경+as-built §5.5 + §8.4 ttc/CFF 가드(docs/70) |
| 7661b66 | CPU 소등 잔여(§5.7): 폴백 커밋 스킵(HasDirtyWindows)+서버 watchdog 5s+disconnect workTick+폰 SW 렌더러+[compst] 계측 |
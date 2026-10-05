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
| TX3 | 헤드리스 실측 — 서버 부팅(X11 불요)+`selftest`+jkctl agent(list_windows/ping) | AppSelfTest 0 failure(s) — **aarch64 첫 영수증** |
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

## 4. 커밋 원장

| 커밋 | 내용 |
|---|---|
| (본 문서) | 워크플랜+termux_bootstrap.sh |
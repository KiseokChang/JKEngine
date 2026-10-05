# docs/72 — 리눅스 셸 진입 경로 봉합 플랜 G (as-built)

> 플랜: docs/superpowers/plans/2026-10-05-linux-shell-entry-seal.md · 커밋 원장
> 7811f09 → f1ffe38(+docs) · 2026-10-05 · 상위: docs/71 §5 "F7 신규 잔차 5종"
> 전량 소각 — SDD(subagent-driven): 구현 G1-G5+수리 R1·R2+게이트 G6.
> **이 문서가 as-built이며 최종리뷰 판정은 §6.**

## 0. 성과 요약

**WSLg 리눅스 서버가 이제 데스크톱 셸을 갖는다.** 서버 기동 즉시
`jkapp_taskbar.so`가 dlopen으로 스폰되어 태스크바가 뜨고, agent 도구
`launch_app {"app":"minesweeper"}`가 실제 앱 창을 만들어 `list_windows`의
windows 배열이 비어 있지 않게 됐다(docs/70 §6 #8의 원래 왕복 증명 — F7
체계 완성). docs/71 §5의 5종 잔차와 stub 리터럴 3처 복제를 전부 소각했고,
재시작 leg(크래시 후 stale 소켓 위에서의 셸 재등록)도 실측 소각했다.

```text
윈도우 공식 9항목 게이트  GREEN (1회차 contiguous, @f1ffe38)
WSL 빌드+posix_selftest   GREEN (ninja RC=0, 0 failure(s))
WSLg 스모크 v3            GREEN (O-A~O-G: 소켓·taskbar 스폰·창 왕복·재시작 leg·정리)
diff 크기                 8 파일 (+docs 1) — win32 동작 변화: stderr 문구 %s 접미 전환뿐(바이트 동일)
```

## 1. docs/71 §5 잔차 → 소각 매핑

| docs/71 §5 | 플랜 G 태스크 | 커밋 | 소각 실측 | 남은 것 |
|---|---|---|---|---|
| #1 .dll/.so 접미 프로브 (plain route :4234 + taskbar :516) | G1 | d54f3d5 | `jk::server::AppModuleSuffix()` — win32 `".dll"` / posix `".so"`; stderr 문구는 `no jkapp_taskbar%s` 포맷으로 win32 바이트 동일 유지 | 없음 |
| #2 posix --client route (main.cpp "Windows-only in this prototype") | G2 | 308b6e6+R1 72407f4 | `RunClientModule` dlopen(RTLD_NOW\|RTLD_LOCAL)+dlsym — **닫지 않음**(FreeLibrary 힙 손상 선례 계승); exe-dir 절대 경로 헬퍼 `ClientModulePath()`(bare 이름이 exe-dir을 검색하지 않는 실측 결함 — "cannot open shared object file"에서 R1으로 소각); --client/--filedlg 게이트 `#else` leg 개통 | --jkx route는 v1 게이트 유지(docs/70 §4) |
| #3 taskbar 자동 스폰 `#ifdef _WIN32` 블록 | G3 | fa32565(+4467c38 수리, R1/R2) | 블록 개통 — posix 대기 leg는 `jk::ipc::DefaultServerEndpointPath()`(**신설, unix socket fold 규칙의 결정론 결과** /tmp/JKWindowServerPipe.sock — JKPipeTransport_posix.cpp:33-43 인용) | 없음 |
| #3b jkx 핸들러 SpawnClient false 무시 → 정직 답변 | G4 | 827b3f4 | posix `{"ok":false,"error":"spawn_failed","jkx":"…"}` — 단 v1에서 **ScanJkxApps 갭**이 선행해 unknown_jkx(:4356)가 먼저 거부 → spawn_failed는 갭 해소 전 unreachable(스모크 §5 실측, 거짓 성공은 어느 경로도 0) | ScanJkxApps posix 갭(docs/71 #3 계열 — 냉동) |
| #4 launch_chat jkchat.exe 하드코드 | G4 | 827b3f4 | posix `{"ok":false,"error":"unavailable_on_platform"}`; agentd 스모크에서는 `unknown_tool` — **launch_chat이 jkagentd 등록줄에 없음**(tools/jkagentd/main.cpp:97·148, 클라이언트 사이드 등록만 :58 launch_app); 실 경로는 팔레트/폰(ClientPaletteApp:268 SendTool) | agentd 미등록은 agentd 전용 냉동 잔차(§5.2) |
| #5 jkx 인수 `'/'→'\'` 폴드 | G4 실측 판정 | 827b3f4 내 기록 | **무재현 종결**: 폴드로 MISS 되는 입력 조합 집합은 공집합 — 후보 (1) 원문·(2) exeDir+원문이 posix 슬래시를 보호 | 수정 없음, 판정만 영속 |
| (docs/71 리뷰 MEDIUM) stub 셸 리터럴 3처 복제 | G5 | ae376a2 | `jk::agent::kStubShellCmdWin32/kStubShellCmdPosix` inline constexpr(JKLmEngine.h, Llm — 파일 표기 주의 [[le저]] 참조); JKLmEngine.cpp :175·:180+posix_selftest 케이스 10 전부 상수 참조 | 없음 |

**수리 2건(셸 경로의 은밀한 부작용):**

- **4467c38/R2 f1ffe38 — fork fd 상속 소각.** fork는 exec 대상과 무관하게
  열린 fd 전부를 복사한다. 대표 사고: taskbar 자동 스폰 직후 자식이 서버의
  unix listener를 물려쥐어 (a) 소켓 파일이 "살아" 보이고 (b) 서버의 다음
  acceptor 이터레이션이 "a live server already holds that socket"으로
  영원히 실패하고 (c) 이후 클라 connect가 상속 listener의 만석 백로그에서
  블록(agentd rc=124 실측). 자식 분기가 dup2 이후
  `for (fd = STDERR_FILENO+1; fd < getdtablesize(); ++fd) close(fd);`
  스윕으로 소각. **R2(오프바이원):** 첫 판 `STDOUT_FILENO+1`(=2)은 stderr를
  희생 — dup2 배선 2 직후 close(2)로 닫아 끊어져 posix_selftest 2 FAIL
  (자식 stderr 공백, `echo >&2`가 EBADF로 sh exit 1). STDERR_FILENO+1로
  교정. 불변명: 스윕은 0/1/2를 건드리지 않는다. 상한은 getdtablesize()(리뷰
  R1-2, 고정 4096은 RLIMIT_NOFILE 소프트 상한 밖 상속 fd를 계속 누출).
- **R1 72407f4 — stale 소켓 판정 교정.** 파일 존재 폴링은 사망한 서버의
  .sock 파일(JKInstanceLock 미삭제 — 크래시 후 재기동에서 영영 스태일)을
  "산 자"로 오판한다. 교정: connect-once 생존자 프루브 — connect 성공=산
  자(중단·스폰 스킵), ECONNREFUSED=스태일(계속 폴링); 스태일 소각 자체는
  CreateServer의 기존 triage(JKPipeTransport_posix.cpp:104-119) 소관으로
  유지. 스모크 O-F leg가 이 교정을 실측: 재기동 서버의 신규 acceptor에
  taskbar가 접속, 스큐류팅 스톨 없음.

## 2. 태스크 실측 기록 요약 (G1→G5, 레저 전문은 .superpowers 원장)

**G1 (d54f3d5, sonnet APPROVE).** 두 헬퍼 신설 —
`jk::server::AppModuleSuffix()`(JKWindowServer.h)·
`jk::ipc::DefaultServerEndpointPath()`(JKWireEndpoints.h, posix 경로는
MapEndpointName fold 규칙과 함께 관리하라는 주석). stderr 문구 "no
jkapp_taskbar%s — desktop runs without a shell"은 win32에서 접미가 .dll이므로
자동 바이트 동일.

**G2 (308b6e6, sonnet APPROVE → R1).** dlopen 트리오 신설. 실제 발견
결함: bare `jkapp_<name>.so` dlopen 실패 — 리눅스 dlopen은 exe-dir을
검색하지 않는다. `ClientModulePath()`(exe-dir 절대 경로)로 소각.
RTLD_LOCAL은 모듈 심볼의 전역 네임스페이스 유출 방지 원칙. dlerror 문구는
dlerror()가 NULL 가능 → 주석으로 기록(포맷 보호). RunClientFromJkx의 temp
합성은 getpid 기반으로 플랫폼 중립화(DeleteFileA→std::remove,
temp_directory_path ec).

**G3 (fa32565, sonnet APPROVE → R1/R2).** 블록 개통+대기 leg 신설.
**리뷰 R1-1 판정:** 파일 존재 폴링은 스태일 소켓 오판 — connect-once
생존자 프루브로 교정(위 수리 2건 참조). **리뷰 R1-2 판정:** fd 스윕
상한 getdtablesize(). Hello read 실패 유령 진단 인쇄 분할(:565-581 —
type 필드는 기본 멤버 초기자 MsgType::Close(=9)를 가지므로 "미초기화"
가설은 틀렸고, 진짜 픽스는 짧은 읽기/EOF와 진성 불일치의 인쇄 분리).

**G4 (827b3f4, sonnet APPROVE).** 정직 답변 2종 — **posix 한정**
(win32 관측 바이트 보존이 글로벌 제약). launch_app jkx 2곳+launch_chat
1곳. 인수 폴드는 4종 후보 전수 대조 무재현 — 종결.

**G5 (ae376a2, sonnet APPROVE).** 상수 2개 신설+소비 3곳. win32 셸
`cmd.exe /c echo {...}` 원문 불변(F2 선계약 보존), posix는 셸 접두 없는
sh echo.

## 3. 게이트 (2026-10-05, @f1ffe38)

**(1) Windows 공식 9항목 ×1 연속 GREEN** (docs/70 §3 형식): ①ninja RC=0
②AppSelfTest 0 failure(s) ③hangul 44/44 ④app_tools ALL PASS ⑤jkbridge
PASS ⑥events 8/8 ⑦semantic ALL PASS(스택 복원 완료) ⑧agentd
list_windows ok:true(win32 빈 = 정상) ⑨HTTP root/health 200.

**(2) WSL 전체 빌드+posix_selftest:** ninja RC=0 → POSIX selftest 재빌드
(sh build.sh — CMake 외부 아티팩트 스태일 함정 재예방) → 케이스 0 failures.

**(3) WSLg 스모크 v3** — §5 표.

## 4. posix_selftest 케이스 원장

케이스 0-9 기존+10(stub sh 왕복, F2/G5)+11(FmtStamp 계약, F4).
변동 없음 — fd 스윕 R2에서 케이스 전체가 0 failures로 돌아온 것이 이 플랜의
회귀 망이다(첫 스윕은 stderr를 죽여 케이스 2건 FAIL — 리그레션 감지 성공 사례).

## 5. WSLg 스모크 v3 전사 (f7_smoke/f7_honest @f1ffe38)

| O | 관측 | 결과 |
|---|---|---|
| O-A | `/tmp/JKWindowServerPipe.sock` 존재 + 서버 로그 `spawned jkdesktop --client taskbar` + `[client] module '/…/jkapp_taskbar.so' loaded` | OK |
| O-C | `launch_app {"app":"minesweeper"}` → ok:true; list_windows windows:[Minesweeper id=3 focused] **비어 있지 않음** | OK — **docs/70 §6 #8 왕복 증명 최종 완성** |
| O-E | pkill 정리 — 남은 jkdesktop 프로세스 없음 | OK |
| O-F | 재시작 leg: stale .sock 의도 방치 → 재기동 서버의 acceptor에 taskbar 신규 접속, "live server already holds" 스큐류팅 없음 | OK (R1 교정 실측) |
| O-G | 정리 완료 | OK |
| 정직 | `launch_app {"jkx":"workshop"}` → `{"ok":false,"error":"unknown_jkx"}` (posix .jkx 스캔 갭 선행 — G4 spawn_failed는 갭 해소 전 unreachable) | 정직 거부 확인 |
| 정직 | `launch_chat` → agentd `unknown_tool` (agentd 등록줄 미포함 — §5.2) | 정직 거부(agentd 층) 확인 |

### 5.1 거짓 성공 소각 결론

posix에서의 도구 결과는 이제 (a) unknown_app/unknown_jkx(존재부), (b)
unknown_tool(agentd 등록부), (c) unavailable_on_platform(플랫폼부),
(d) ok:true(실스파 왕복) 4경로만 나온다 — posix 전 경로에서 거짓 ok:true는
F7 왕복(plain launch_app 성공)과 (불가능 지점의) jkx/launch_chat에서
전부 소각되었다.

### 5.2 agentd 전용 냉동 잔차 (신규 기록)

`launch_chat`은 서버 권한 테이블(:2508 "allow")·서버 핸들러(:5991)·
ClientPaletteApp 경로(:268)에는 존재하나 jkagentd 도구 등록줄(:97·:148)에는
없다. agentd로 launch_chat을 필요로 하는 워크플로가 생기면 등록줄 1줄로
개통 가능 — 단, 등록줄은 client 사이드 공용(win32/posix 동일)이므로
win32 등록줄이 실제 변화하는 것(신규 도구 노출)이라 승인 관측 편차가 된다.
**v1 유지·냉동** — 사용자 판정 시 해동.

## 6. 최종리뷰 (whole-branch, opus, 7811f09..f1ffe38+docs 패키지)

- **(대기 — 판정 확정 시 이 절에 기록)** VERDICT 형식: APPROVED /
  APPROVED WITH RIDERS / CHANGES REQUIRED + severity. 라이더 있으면
  반영 후 커밋.
- 리뷰 체크 관측 목표: (a) win32 stderr문 %s 접미 전환의 바이트 동일
  관측 유지·(b) RTLD_NOW|RTLD_LOCAL 상수·(c) fd 스윕 경계(STDERR_FILENO+1)
  ·(d) DefaultServerEndpointPath의 fold 규칙 연동 주석·(e) 정직 답변의
  posix-한정 #ifdef·(f) JKLmEngine 표기(Llm — 대소문자 주의).

## 7. 커밋 원장 (이 플랜, BASE a842533→)

| 커밋 | 내용 |
|---|---|
| 7811f09 | 플랜 G 문서 (docs/superpowers/plans/2026-10-05-linux-shell-entry-seal.md) |
| 4467c38 | fd 스윕 1판(결함 포함 — STDOUT_FILENO+1) |
| fa32565 | G3 taskbar 자동 스폰 posix 개통+Hello 인쇄 분할 |
| d54f3d5 | G1 접미 플랫폼화+DefaultServerEndpointPath |
| 308b6e6 | G2 dlopen 로더+--client/--filedlg 개통(G3의 fd 스윕 공용 파일 기초) |
| 72407f4 | R1: exe-dir 절대 경로+connect-once 프루브+fd 스윕 주석+`./` 폴백 |
| 827b3f4 | G4 정직 답변 2종+jkx 인수 폴드 무재현 판정 |
| ae376a2 | G5 stub 셸 리터럴 공용화 |
| f1ffe38 | R2: fd 스윕 오프바이원( STDERR_FILENO+1 교정 — selftest 2 FAIL에서 발각) |
| (이 문서) | G6 as-built |

## 8. 환경 교훈 (레저 → 기억)

- **fork-상속 fd 소각은 dup2 *이후* 자식 분기에서** — 부모 분기의 close는
  이미 fork 순간 복사된 자식 fd 사본을 못 지운다(사건 실측: agentd
  rc=124 백로그 블록).
- fd 스윕의 경계는 **STDERR_FILENO+1**(STDIN/STDOUT/STDERR 보존) —
  STDOUT+1(=2)로 쓰면 stderr가 죽는다(케이스 실측 FAIL 2건).
- 리눅스 dlopen은 bare 파일명에서 exe-dir을 검색하지 않는다 — 항상
  절대 경로(jk::fs::GetExecutablePath 기반).
- unix socket 파일의 존재 폴링은 "산 자/스태일"을 구분 못 한다 —
  connect-once 생존자 프루브가 필요하다.
- getdtablesize()가 fd 스윕의 상한(RLIMIT_NOFILE 대응, 리뷰 R1-2).
- **남은 docs/70 §6 계**(플랜 F+G로 해소된 5건+F7 5건 외): #5 job/stdin/
  좀비/phantom·#6 vplayer 실측/Termux·#7 selftest 강화 잔여
  (RecvAll 패턴·Accept 무한블록 방어) — 사용자 판정.
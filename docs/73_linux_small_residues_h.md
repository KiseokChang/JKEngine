# docs/73 — 리눅스 소액 잔여 봉합 플랜 H (as-built)

> 플랜: docs/superpowers/plans/2026-10-05-linux-residue-h.md (커밋 c5b3f26+c8920b3)
> · BASE c8920b3 → HEAD 80a4370 · 2026-10-05 · 상위: docs/70 §6 항목 #5·#7
> SDD(subagent-driven): 구현 H1(080efc2+R1 bfcc84a)·H2(71b0f63)·H3(80a4370)
> +게이트 H4. **이 문서가 as-built이며 최종리뷰 판정은 §6.**

## 0. 성과 요약

docs/70 §6 잔여 중 Windows/WSL에서 가능한 소액 계(#5 job·stdin·좀비·phantom,
#7 selftest 강화)를 전부 봉합했다. posix job은 이제 win32 JobObject처럼
**복수 멤버**를 적산해 한 번의 킬로 전 트리가 죽고, posix 스폰 자식의 stdin은
win32 `hStdInput 미설정`과 같은 관측(/dev/null EOF)으로 봉합됐다. 셀프테스트는
12(job 복수 멤버·stdin 패리티)·13(RecvAll 320B 7청크·부분읽기 timeout 경계·
Accept 상한) 신설, unix socket backlog는 1→8로 phantom 슬롯 점유 방어.

```text
윈도우 공식 9항목 게이트  GREEN (@80a4370, 라이브 스택 정지→복원)
WSL 빌드+posix_selftest   GREEN (ninja RC=0, 13케이스 0 failures)
WSLg 스모크 f7_smoke      GREEN (taskbar 스폰·Minesweeper 왕복·재시작 leg)
diff 크기                 posix TU 2 + selftest 1 — win32 관측 변화 0
```

## 1. docs/70 §6 #5·#7 → 소각 매핑

| docs/70 §6 | 플랜 H | 커밋 | 실측 | 남은 것 |
|---|---|---|---|---|
| #5 job=단일 멤버(posix pgid 덮어쓰기 — 트리 킬 누락) | H1 | 080efc2 | `JobState.pgids: vector<pid_t>` — AssignToJob 적산(중복 assign 멱등), TerminateJobTree/CloseHandleLike가 전 멤버 `kill(-pgid, SIGKILL)`. win32 JobObject 복수 멤버 계약과 패리티. 셀프테스트 12a: 자식 2명 assign → 킬 → 양쪽 3s 내 사망 관측 | 없음 |
| #5 자식 stdin 부모 상속(패리티 원하면 open("/dev/null")+dup2(0)) | H1 | 080efc2+bfcc84a | 자식 분기가 stdin을 `/dev/null`(O_RDONLY)로 상시 dup2 — win32 `hStdInput 미설정`(JKProcess_win32.cpp:77)과 같은 관측(자식이 stdin에서 블록하지 않고 부모 stdin 탈취가 구조적으로 불가). 12b: `read x; echo got:$x` 자식이 2s 내 EOF로 종료 | 없음 |
| #5 조기 CloseHandleLike 좀비(init 회수 — 누수 아님) | H4 | 판정만 | 코드 변경 없음 — CloseHandleLike의 ProcState 분기는 WNOHANG 1회 리랩(조기 사망 시 즉시 회수). **판정 유지+정밀화(최종리뷰 MEDIUM 라이더):** "init 회수"는 부모 종료 시 성립 — 엔진 전역에 SIGCHLD 처분 0건이라 장수 부모 경로(JKLmEngine.cpp:403-411: WaitForExit 미스→proc close→job close-kill 뒤)에서는 ProcState 소멸 후 브리지 수명까지 좀비가 잔존한다. 리소스 누수(프로세스 표 항목 수개)가 아니라는 판정 자체는 유지 | 없음(판정 확정) |
| #5 전송 phantom 연결(probe connect가 backlog 슬롯 소비) | H3 | 80a4370 | `listen(listener, 8)` — 스태일/phantom connect 한 개가 단일 대기 슬롯을 점유해 이후 클라 connect가 블록하는 상황 방어(서버 부트 직후 taskbar 스폰+프루브 connect 겹침 커버). **(최종리뷰 LOW 라이더)** 이 상향은 R-D3 "one client at a time" 코멘트를 대체 — serial accept 루프가 서빙 계약을 그대로 유지하므로 계약 파손 아니고 posix 전용 관측 직렬화 완화 | 없음 |
| #7 수백 바이트 RecvAll 패턴 | H2 | 71b0f63 | 케이스 13a: 320B를 7청크(13·47·64·1·128·33·34, 청크 사이 10-20ms) 전송 → RecvAll true+memcmp 일치. 기존 케이스 3은 5바이트 단발이었음 | 없음 |
| #7 Accept 무한블록 방어 | H2 | 71b0f63 | 케이스 13c: listening 소켓 SO_RCVTIMEO(300ms) → 무피어 Accept가 kInvalidSocket로 ~315ms 반환 실측(Linux는 accept에도 SO_RCVTIMEO 적용) + alarm(20) hang 방어막. 13b: 부분읽기 도중의 EAGAIN도 RecvAll을 false로 끝내는 **fail-closed 계약 봉합**(§3 룰링) | 없음 |
| #6 vplayer 리눅스 실측·Termux/폰 실기기 | — | 이월 | libav-on-Linux 런타임·폰 실기기 필요 — docs/70 §6 유일 잔여로 이월 | 이월(사용자 판정) |

## 2. 태스크 실측 기록 요약 (레저 전문은 .superpowers 원장)

**H1 (080efc2, sonnet APPROVE → R1 bfcc84a).** 리뷰의 MEDIUM 1건:
fd-0 재사용 엣지 — 부모의 fd 0이 닫힌 채(데몬화 서버·agentd 체인 스폰 —
플랜 G fd 위생 구역 그 자체) 스폰되면 `open("/dev/null")`이 fd 0을 돌려받아
`dup2(0,0)` no-op(등가-fd 규칙, FD_CLOEXEC 미소거) 뒤 `close(nullFd)`가
자식 stdin을 EBADF로 완전히 닫아버린다. "stdin=/dev/null" 계약이 맥락에서
무음 붕괴. **R1 교정(둘 다 필요):** O_CLOEXEC 제거(원본 fd는 즉시 닫히므로
무의미) + `nullFd != STDIN_FILENO` 조건화(등가-fd 경로는 무조건 close가
여전히 깨뜨림). LOW 3건은 비차단 기록: 복수 멤버 킬 루프의 재사용 pgid 노출
(기존 단일 pgid 코드와 동일 노출 — 회귀 아님, 향후 패스), 12b의 판별력은
러너 stdin이 열린 fd일 때 성립(블랙박스 관측 한계), 12a의 exit-code 0 수용
관용(단언력은 WaitForExit 쌍이 검). 구현자 룰링 2건: killed=ok 시맨틱
(비-ESRCH 실패는 close-kill로 재시도 가능한 fail-correct), /dev/null open
실패=무음 fail-open — **표현 정밀화(최종리뷰 LOW 라이더):** 이 경로에서
자식은 부모 stdin을 그대로 물려받는다. win32 패리티가 아니라 봉합 대상 그
자체의 잔존이고, 실전 도달은 /dev/null이 없는 비-Linux 뿐이라 사실상
불가(판정: 치유 대상 실패, 무음 경로).

**H2 (71b0f63, sonnet APPROVE)·H3 (80a4370, sonnet APPROVE).** 케이스 13의
13b는 플랜 작성 중 계약 실측(JKNet_posix.cpp:104-118 `r<=0 → false`, EAGAIN
포함·EINTR만 리트라이)에 맞춰 **반전 룰링**: "timeout 후에도 RecvAll true"가
아니라 부분읽기 이후 EAGAIN → RecvAll false 계약 봉합(무데이터 timeout만
보던 기존 케이스 3 D의 부분읽기 경로 확장). 13c는 alarm(20) 방어막(hang은
조용한 붙잡힘이 아니라 요란한 적색). 구현자 룰링(리뷰 ACCEPTED): 13b 서버
티어다운을 clientB.join() 이후로 — raw client send()에 MSG_NOSIGNAL이 없어
400ms close vs 700ms send 경합 RST→SIGPIPE로 하네스 자체가 사망하는 실해저
방어(~2s 비용). reviewer NIT 4건 비차단 기록(부분송신 재시도 없음=정직 FAIL,
13b 마진 300ms=플레이크 시 클라 갭 1000ms로, 13a/13b Accept 무한블로커=
케이스 3 선재 패턴, 13c elapsed 하한 없음=의도).

## 3. 룰링 원장 (컨트롤러)

- **13b 반전** (c8920b3, 플랜 수정): posix RecvAll은 fail-closed 계약이고
  win32가 동형 — "EAGAIN 다음 데이터로 완성"은 계약 위반이므로 테스트가
  아니라 플랜을 계약에 맞춤. 비용: 없음(계약 자체가 승계 목적).
- **자식 stdin 무조건 /dev/null**: inheritedStdioPipes 케이스도 포함 —
  win32는 hStdInput을 두 계열 모두 미설정. terminal pty는 JKConPtyBridge
  소관이라 미영향.
- **빈 job TerminateJobTree=true**: win32 TerminateJobObject 빈 job TRUE 패리티.
- **윈도우 빌드 이월**: H1/H2/H3 파일들은 모두 win32 CMake 의존 그래프 밖
  (posix TU·selftest) — 구현 중 라이브 스택 exe 잠금 사유로 전체 빌드를
  H4 게이트(위 §0, RC=0)에서 일괄 수행.

## 4. posix_selftest 케이스 원장 (docs/72 §4 승계)

케이스 0-9 기존 + 10(stub sh 왕복) + 11(FmtStamp) + **12(job 복수 멤버·
stdin 패리티, H1)** + **13(RecvAll 청크·부분읽기 timeout 경계·Accept 상한,
H2)**. 메모리 원장의 "케이스 13/14/15"는 플랜 D 문서 번호였고 실제 구현은
케이스 2(job trio)·3(net)에 흡수됐다 — 이 문서가 원장 캐리오버.

## 5. 게이트 (2026-10-05, @80a4370)

**(1) Windows 공식 9항목 ×1 연속 GREEN** (docs/70 §3 형식): ①ninja RC=0
②AppSelfTest 0 failure(s) ③hangul 44/44(소스 재컴파일 — 구성 확립:
`-I../include -I../legacy -I../legacy/wancode -ISDL2` + JKHangulAutomata·
TerminalHangulInput·JKHangulUtil·JKTextConv_win32·legacy/wancode/WANCODE.CPP)
④app_tools ALL PASS ⑤jkbridge PASS ⑥events 8/8 ⑦semantic ALL PASS
⑧agentd `tools/call list_windows` → `{"ok":true,"windows":[]}` (win32 빈=
정상 — MCP jsonrpc tools/call 형태가 정식 요청; bare list_windows라인은
"invalid request"가 스펙대로) ⑨HTTP root=200/health=200(:8899).
**(2) WSL** ninja RC=0 + posix_selftest 13케이스 0 failures.
**(3) WSLg f7_smoke** GREEN(플랜 G 스탠딩 스모크 — taskbar 스폰·Minesweeper
왕복·재시작 leg).
**(4) 라이더 웨이브 재검증** — 라이더는 posix TU 코멘트 2처+docs뿐이지만
해당 TU는 win32 그래프(jkcore)에도 존재하므로 스택 정지→재빌드(RC=0)→
복원 후 ②AppSelfTest 0 failures ⑧list_windows ok:true(windows 빈=정상)
⑨root/health 200 재확인(③-⑦는 코멘트 전용·전처리 소거 TU라 동일 코드
상태 — pre-rider 판정 승계).

## 6. 최종리뷰 (whole-branch, opus, c8920b3..31aab6c 패키지)

- VERDICT: **APPROVED WITH RIDERS**. 전문은
  .superpowers/sdd/…/final-review-report.md. 로직 전부 올정동(kill 루프·
  fd-0 엣지 픽스·12/13 단언 강도·소비자 무영향 — JKLmEngine.cpp:315-320·
  main.cpp:2990 계약-b 스모크의 복수 멤버 영향 본검증·win32 diff 0 본검증).
- 라이더 반영(라이더 커밋):
  - MEDIUM — JKPipeTransport_posix.cpp backlog 코멘트의 "§6 #4" 오인을
    "#5"로 교정(플랜 H3 근거 인수도 동일 오인이었으나 docs/73 §1 표는 정확).
  - MEDIUM — 좀비 판정의 회수 시점 정밀화(§1 — 부모 종료 시 init 회수;
    장수 부모 경로는 좀비 잔존하나 누수 판정은 유지, 코드 변경 없음 —
    플랜 냉동 룰링 준수).
  - LOW 2건 — §1 phantom 행에 R-D3 대체 기록, §2 fail-open 표현 정밀화.
  - NIT 2건 — CloseHandleLike job 분기 코멘트 사문 개소 명시(코드 주석
    라이더 반영), docs/73 끝 개행.

## 7. 커밋 원장 (이 플랜, BASE c8920b3→)

| 커밋 | 내용 |
|---|---|
| c5b3f26 | 플랜 H 문서 |
| c8920b3 | 플랜 H2 브리프 룰링 반전+13c alarm 방어막 (실행 전 수정) |
| 080efc2 | H1: posix job 복수 멤버+자식 stdin /dev/null 패리티+selftest 케이스 12 |
| bfcc84a | H1 R1: /dev/null 블록 fd-0 재사용 엣지 — O_CLOEXEC 제거+nullFd==0 조건화 |
| 71b0f63 | H2: posix_selftest 케이스 13 — RecvAll 청크+SetTimeouts 상한 계약 |
| 80a4370 | H3: unix socket backlog 1→8 — phantom connect 슬롯 점유 방어 |
| (docs) | docs/73 as-built (이 문서) |
| (라이더) | 최종리뷰 라이더 — §6 #4→#5 인수 교정(backlog 코멘트)+close 분기 retryable 사문 주석+docs 라이더 정직화(§1·§2·§6·§7) |

## 8. 잔여와 승계

- docs/70 §6 남은 것: **#6뿐** — vplayer 리눅스 실측(libav 런타임 필요)·
  Termux/폰 실기기(하드웨어 필요) — 사용자 판정 이월.
- 냉동 잔차(docs/72 §5.2) 불변: agentd launch_chat 등록줄(agentd 층),
  ScanConsoleApps posix 갭.
- 리뷰 LOW의 향후 패스 후보: 복수 멤버 킬 루프의 재사용 pgid 가드
  (KillProcess류의 waitpid 재배치 노출 — 극좁은 창).

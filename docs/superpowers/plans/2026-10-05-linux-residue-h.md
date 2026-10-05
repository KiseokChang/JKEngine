# 리눅스 소액 잔여 봉합 (플랜 H — docs/70 §6 #5·#7) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/70 §6 잔여 중 Windows/WSL에서 가능한 소액 계 — #5(job 단일
멤버·자식 stdin 상속·좀비·phantom)와 #7(selftest 강화 RecvAll 청크·timeout
경로)을 플랫폼 패리티 계약으로 봉합한다.

**Architecture:** 모든 픽스는 posix leg 또는 posix 어댑터 한정. win32 분기는
바이트 보존(diff 0). #5의 좀비 항목은 이미 문서화된 "init 회수 — 누수 아님"
상태를 코드 검증으로 소각판정(코드 변경 없음).

**Tech Stack:** jk::process posix 어댑터(JKProcess_posix.cpp)·jk::net
(JKNet_posix.cpp)·JKPipeTransport_posix.cpp·posix_selftest(WSL2)·플랜 G 스택.

**Spec/상위:** docs/70 §6 항목 5·7 (원문 그대로 인용):
- `job=단일 멤버(posix pgid 덮어쓰기 — 트리 킬 누락)`, `자식 stdin 부모 상속
  (패리티 원하면 open("/dev/null")+dup2(0))`, `조기 CloseHandleLike 좀비
  (init 회수 — 누수 아님)`, `전송 phantom 연결(probe connect가 backlog 슬롯
  소비)`
- `selftest 강화 후보(docs/68 승계): 수백 바이트 RecvAll 패턴·Accept 무한블록
  방어`

## Global Constraints

- **win32 무변동 원칙** — JKProcess_win32.cpp·JKNet_win32.cpp·JKPipeTransport
  win32 TU는 diff 0여야 한다. 모든 픽스는 posix TU/posix leg.
- **ec 중립형** — 새 std::filesystem 호출 없음(이 플랜은 fs 접촉 0).
- **dlclose 금지·플랜 G 불변식 존중** — fd 스윕(STDERR_FILENO+1)과 connect-once
  프루브는 손대지 않는다(플랜 G 소각물).
- **posix_selftest는 CMake 외부 아티팩트** — 검증 전 항상
  `sh tools/posix_selftest/build.sh` 재빌드 후 실행(스테일 바이너리 함정).
- 기존 셀프테스트 케이스는 1-11 — 신규 케이스는 12번부터(번호 연속성,
  docs/70 §7 교훈 "케이스 12부터 신규 자리").
- **빌드 명령**: 윈도우 `cd /i/progwork/JKENGINE/engine && export
  PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" && ninja.exe -C build -j3`
  (라이브 스택 정지 확인). WSL `wsl.exe -d Ubuntu-24.04 -e sh -c "cd
  /mnt/i/progwork/JKENGINE/engine/buildwsl && ninja -j4"` + selftest
  `sh ../tools/posix_selftest/build.sh && ./posix_selftest`.
- 커밋 트레일러 `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

---

### Task H1: posix job 복수 멤버 + 자식 stdin /dev/null + 셀프테스트 케이스 12

**Files:**
- Modify: `engine/src/process/JKProcess_posix.cpp` — JobState 구조체(:46),
  AssignToJob(:343), TerminateJobTree(:355), CloseHandleLike의 kMagicJob
  분기(:322), Spawn 자식 분기(:137-175)
- Modify: `engine/tools/posix_selftest/main.cpp` — Case 12 신설(파일 끝,
  Case 11 뒤), main의 케이스 목록에 등록

**Interfaces:**
- Produces (계약 — 헤더 무수정, 시그니처 불변): `jk::process::AssignToJob(job,
  proc)`은 이제 복수 호출 가능 — 각 proc의 pgid를 job에 **적산**(덮어쓰기
  금지, 같은 proc 중복 assign은 무시). `TerminateJobTree(job)`는 job에 속한
  **모든 pgid를 kill(-pgid, SIGKILL)**. `CloseHandleLike(job)`(close==트리
  사망)도 전 멤버에 같은 킬. win32 관측(JobObject 복수 멤버)과 패리티
- Spawn 내 자식 stdin: posix 자식 분기에서 stdin을 `/dev/null`(O_RDONLY)로
  dup2 — win32가 `hStdInput` 미설정으로 자식 입력을 절대 주지 않는 것
  (JKProcess_win32.cpp:77)과 같은 관측(자식이 stdin에서 영원히 블록하지
  않는다)

- [ ] **Step 1: JobState 복수 멤버화** —

```cpp
struct JobState : HandleState {
    std::vector<pid_t> pgids;  // 복수 멤버 — win32 JobObject 복수 계약 패리티
    bool killed;
};
```
`AssignToJob`: `j->pgids`에 `p->pid`가 없으면 push_back (중복 assign 무시).
`TerminateJobTree`: loop —
```cpp
for (pid_t pgid : j->pgids) kill(-pgid, SIGKILL);  // ESRCH=이미 사망 멤버 — 무시
```
ok 반환 계약은 **판정 필요·룰링**: 모든 멤버가 이미 사망해 ESRCH만 돌아온 경우
win32 `TerminateJobObject`는 빈 job에서도 TRUE — ESRCH는 성공 취급, ok=true.
kill이 ESRCH 외 오류인 경우에만 false. `killed=true` 설정으로 이중 킬 방지.
`CloseHandleLike` kMagicJob 분기도 동일 루프(조건 `bound`는 소멸 — `killed`만).

- [ ] **Step 2: 자식 stdin /dev/null** — Spawn 자식 분기, dup2·close 블록
  **뒤**, fd 스윕 **앞**:
```cpp
// win32 parity (JKProcess_win32.cpp:77 hStdInput 미설정): 자식 stdin은
// 데이터 원이 없다 — 부모 stdin 상속은 posix 전용 이탈이었다(플랜 E 잔여
// docs/70 §6 #5: 패리티 원하면 open("/dev/null")+dup2(0)). /dev/null EOF는
// win32 NULL stdin 즉시 오류와 같은 관측 — 자식이 read에 영원히 블록하지
// 않고 부모(agentd 등)의 stdin을 훔치는 사고가 구조적으로 불가능해진다.
if (!options.inheritedStdioPipes) {
    const int nullFd = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (nullFd >= 0) { dup2(nullFd, STDIN_FILENO); close(nullFd); }
}
```
주의: `inheritedStdioPipes` 케이스에서도 win32는 hStdInput 미설정이므로
분기 없이 항상 적용하는 것도 옳다 — **룰링: 무조건 적용**(win32 스폰의 자식
stdin은 두 계열 모두 데이터 원 없음; terminal: 접두 pty는 이 어댑터가 아니라
JKConPtyBridge 소관이라 영향 0).
- [ ] **Step 3: 셀프테스트 Case 12 신설** — `tools/posix_selftest/main.cpp`,
  Case 11 뒤, 케이스 목록(main의 runner에) 등록:
  - 12a job 복수 멤버: `jk::process::Spawn({"sleep 30", .inheritedStdioPipes=true})` 2회
    (실측용 마커 출력 불요) → `CreateKillOnCloseJob` → `AssignToJob` 2회 →
    `TerminateJobTree` → ok:true + `WaitForExit(3000)` 양쪽 true + GetExitCode
    → 코드는 정상 종료(0) 또는 128+9(137) 둘 다 수용(SIGKILL 도착 타이밍
    무관 — sleep은 신호 사망 137이 상통, ok만 단정) → `CloseHandleLike` 정리
  - 12b stdin 패리티: `sh -c 'read x; echo got:$x'` 스폰(pipes) →
    `WaitForExit(2000)` **true**(블록하지 않음) + stdout이 `got:` 행
    (EOF 빈 읽기로 read 즉시 끝남 — 전사 트림 후 검증). 12b가 실패하면
    posix 자식이 부모 stdin을 여전히 상속 중이라는 신호.
  - 케이스 헤더 주석: `// Case 12 (plan H — docs/70 §6 #5): posix job 복수
    멤버 + 자식 stdin /dev/null 패리티.`
- [ ] **Step 4: WSL 빌드+셀프테스트** — buildwsl ninja RC=0 +
  `sh ../tools/posix_selftest/build.sh && ./posix_selftest` →
  케이스 12 포함 0 failures(기존 11케이스 회귀 포함).
- [ ] **Step 5: 윈도우 빌드 RC=0** — JKProcess_win32.cpp diff 0 확인(필요시
  `git diff --stat src/process/JKProcess_win32.cpp` 빈 출력 증명).
- [ ] **Step 6: 커밋** `fix(h1): posix job 복수 멤버+자식 stdin /dev/null 패리티+selftest 케이스 12`

### Task H2: net selftest 강화 (RecvAll 청크·timeout 경로·Accept 방어) — 케이스 13

**Files:**
- Modify: `engine/tools/posix_selftest/main.cpp` — Case 13 신설
  (Case 12 뒤) + 케이스 목록 등록

**Interfaces:**
- Consumes: `jk::net` 9심볼(Startup/ListenTcp/Accept/RecvAll/Send/
  SetTimeouts/ShutdownBoth/Close/PrimaryIp) — 변경 0. 순수 셀프테스트 태스크.

- [ ] **Step 1: Case 13 신설** (Case 12 뒤, runner 등록):
  - 13a RecvAll 청크: `ListenTcp("127.0.0.1", 0, 4, &boundPort)` → 자기
    클라 connect → **수백 바이트(예: 320B)를 7개 불규칙 청크**(13·47·64·1·
    128·33·34, 청크 사이 ~10-20ms sleep)로 전송 → `RecvAll(s, buf, 320)` →
    true + 내용 전부 일치(memcmp). 부분 recv 루프의 정확성 봉합(docs/68
    승계 "수백 바이트 RecvAll 패턴"). 기존 케이스 3은 5바이트 단발 — 320B로
    확장하는 의미만 있다(컨트롤러 룰링: 중복 아님).
  - 13b 부분읽기 이후 timeout 경계(**컨트롤러 룰링으로 반전** — posix
    RecvAll은 `r<=0 → false` fail-closed 계약(KJNet_posix.cpp:104-118,
    EAGAIN 포함)라 "timeout 후에도 성공"은 계약 위반이다): 피어가 64B 중
    절반(32B) 전송 → 700ms sleep(**SO_RCVTIMEO 400ms 만료 유도 → recv
    EAGAIN**) → 그 뒤 나머지 전송 → `RecvAll(acc, buf, 64)` → **false**
    (부분 읽기 도중의 EAGAIN도 RecvAll을 정직하게 거부한다 — 기존 케이스 3 D의
    "무데이터 timeout"과 대비되는 부분읽기 경로 봉합). elapsed < 2500ms.
  - 13c Accept 무한블록 방어 실측: 신설 listener에 `SetTimeouts(listener,
    300)` 후 **아무도 connect하지 않은 상태**로 `Accept` → Linux는 listening
    소켓의 SO_RCVTIMEO를 accept에도 적용 — kInvalidSocket이 ~1초 내 반환되면
    통과. hang 방어막으로 13c 시작 전 `alarm(20)` + 통과 후 `alarm(0)`
    (타이머 누출 방지, SIGALRM 기본 동작=프로세스 사망이므로 hang은 조용한
    붙잡힘이 아니라 요란한 적색). 만약 WSL 환경에서 accept가 bound하지
    않으면 구현자는 실패를 보고하고 컨트롤러가 룰링(계약 문서화로 소각) —
    실패를 숨기지 않는다.
  - 케이스 헤더 주석: `// Case 13 (plan H — docs/70 §6 #7): RecvAll 청크
    패턴+부분읽기 timeout 경계+Accept 상한 실측.`
- [ ] **Step 2: WSL 셀프테스트** — 재빌드+런 → 케이스 13 포함 0 failures.
- [ ] **Step 3: 윈도우 빌드 RC=0** — selftest TU는 win32 빌드 미포함이지만
  변경이 없음을 상태로 확인(ninja up-to-date RC=0).
- [ ] **Step 4: 커밋** `test(h2): posix_selftest 케이스 13 — RecvAll 청크+SetTimeouts 상한 계약`

### Task H3: unix socket backlog 상향(phantom 방어)

**Files:**
- Modify: `engine/src/ipc/JKPipeTransport_posix.cpp:148` — `listen(listener, 1)`
  → `listen(listener, 8)`

**Interfaces:**
- Consumes: 없음. posix 전용(윈도우 named-pipe 전송은 listen 개념 없음 —
  win32 diff 0).
- 근거(docs/70 §6 #4): backlog 1이면 시리얼 connect 사이 accept가 지연될 때
  후속 connect가 큐에 못 들어간다 — 특히 플랜 G의 connect-once 프루브·프로브
  connect(stall 중인 phantom 연결)가 단일 슬롯을 점유하면 다른 클라가 블록.
  8은 실측 서버 스폰 동시성(서버 부트 taskbar 즉시 스폰+에이전트 접속 겹침)
  를 커버하는 넉넉값.

- [ ] **Step 1: 상향+주석** —
```cpp
    // backlog 8 (plan H — docs/70 §6 #4): 1이면 스태일/phantom connect 한
    // 개가 단일 대기 슬롯을 점유해 이후 클라 connect가 블록한다. 서버는
    // accept를 빠르게 소진하지만 부트 직후 taskbar 스폰+프루브 connect가
    // 겹치는 순간을 커버한다.
    if (::listen(listener, 8) != 0) {
```
- [ ] **Step 2: 빌드+스모크** — WSL ninja RC=0 + 셀프테스트(케이스 5·9 회귀)
  + f7_smoke.sh GREEN(백로그에 겹치는 스폰이 정상 소진되는지 taskbar 스폰
  관측 유지).
- [ ] **Step 3: 커밋** `fix(h3): unix socket backlog 1→8 — phantom connect 슬롯 점유 방어`

### Task H4: 좀비 항목 코드 검소각판정+마감(게이트·docs/73·최종리뷰·푸시·메모리 28)

- [ ] **Step 1: 좀비 판정 기록** — docs/70 §6 #5의 "조기 CloseHandleLike 좀비":
  현재 CloseHandleLike는 WNOHANG 1회 리랩 시도 후 delete — 이미 실행 중인
  자식은 미래 좀비가 되지만 init(WSL)이 회수(누수 아님 — 원문 판정 유지).
  **코드 변경 없음·판정만 docs/73 기록**(플랜 G의 CloseHandleLike ProcState
  분기 주석 "one cheap WNOHANG reap"이 이미 근거).
- [ ] **Step 2: 게이트** — 윈도우 공식 9항목 1회차+
  WSL 빌드+selftest(케이스 12·13 포함)+f7_smoke 재실행.
- [ ] **Step 3: docs/73_linux_small_residues_h.md** — docs/70 §6 #5·#7 소각
  매핑(커밋·실측)+좀비 판정+phantom/backlog 룰링+남은 #6 상태(vplayer
  리눅스 실측·Termux — 폰 실기기 이월)+승인 편차 유무 정직화.
- [ ] **Step 4: 최종리뷰** — BASE..HEAD 패키지, opus, 판정 형식
  VERDICT+severity. 라이더 있으면 반영.
- [ ] **Step 5: 푸시(사전 승인)+메모리 갱신 28**+원장 갱신.

---

## Self-Review

1. **Spec 커버리지**: docs/70 §6 #5 4관(복수 멤버=H1, stdin=H1, 좀비=H4
   판정, phantom=H3)+#7 2관(RecvAll 청크=H2, Accept 무한블록 방어=H2 13c).
   #6은 폰 실기기/vplayer 리눅스 런타임 필요 — 이월 명시.
2. **플레이스홀더 스캔**: 모든 코드 블록 실문 제공. 12a의 exit-code 수용
   (0 또는 137)은 SIGKILL 타이밍 실측 변수 — 둘 다 수용이 정직.
3. **타입 일관성**: JobState 필드명·AssignToJob 시그니처(JKProcess.h 불변)·
   케이스 번호 12·13 연속성 확인(posix_selftest 현재 Case 1-11 실측
   완료 — 컨트롤러 grep 실측).
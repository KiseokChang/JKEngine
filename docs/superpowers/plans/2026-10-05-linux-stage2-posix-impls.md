# Linux stage-2 posix 실구현 (어댑터 6종) Implementation Plan — plan D

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** docs/68에서 봉합한 어댑터 경계의 posix 실구현을 채운다 — jk::fs·jk::process·jk::net·JKConPtyBridge(pty)·JKPipeTransport(전송)·싱글인스턴스 가드. **검증은 이 머신의 WSL2 Ubuntu-24.04에서 실 linux g++ 컴파일+런타임** (리눅스 머신 켜기 불요 — WSL 실측 확보: `wsl.exe -u root -d Ubuntu-24.04` 루트 실행 가능, apt-get 가능, g++/make/cmake는 이 플랜에서 설치).

**Architecture:** 플랜 A/B/C에서 만든 헤더 계약을 바꾸지 않는다(win32 TU불문). 모든 작업은 `*_posix.cpp` 본체 교체 + 새 어댑터 2건(instance guard, guard는 win32 TU도 신설). 검증 하네스는 CMake 밖(`engine/tools/posix_selftest/` — 어댑터 TU만 직접 g++로 묶어 SDL 등 엔진 의존 없이 WSL에서 컴파일·실행). 윈도우 스택은 전혀 건드리지 않는다 — **동작 변화 0은 "win32 TU diff 0"으로 증명**.

**Tech Stack:** POSIX(/proc/self/exe, fork/exec, pipe, openpty, unix socket, flock), WSL2 Ubuntu-24.04(g++ 13.x), ucrt64 g++(윈도우 회귀 유지), SDD.

**Spec:** docs/62_linux_port_and_dex.md §3(2단계 정의)·§8(4본체: posix 전송·pty·flock 가드·폰트 탐색) + docs/68(1단계 as-built, 어댑터 계약 원본) + docs/22 §8.2(pty close 순서 원문).

## Global Constraints

- **win32 TU diff 0** — 이 플랜의 "동작 변화 0" 증명은 모든 `*_win32.cpp`·소비 TU가 무변경임으로. 단, instance guard 어댑터는 신설이므로 WindowServer 콜사이트 치환 1건 예외(원문 CreateMutexA 호출이 어댑터 TU로 이동하는 순수 치환 — as-built 명문화).
- **헤더 계약 원문 유지** — JKNet.h/JKFs.h/JKProcess.h/JKConPtyBridge.h/JKPipeTransport.h의 공개 시그니처·계약 주석은 확장만(계약 문장 추가) 가능. posix 구현이 계약을 만족 못 하면 판정할 것(레저에 ruling).
- **posix 본체는 계약 fail-closed** — 스텁이 실패 응답을 했다면 실구현도 지원 안 하는 경로는 같은 실패 응답.
- **UTF-8/CP 계약:** win32 fs는 CP_ACP, posix는 native 바이트(UTF-8) — JKFs.h 헤더에 posix 쪽 문장 보강. 소비자가 A-API 전제인 것은 win32만 해당 — posix 바이트는 그대로 fopen에 들어가므로 무해.
- **검증 하네스는 두 개:** (1) 윈도우 공식 게이트(플랜 C와 동일 세트 — win32 불변 증명), (2) WSL posix 게이트(`posix_selftest` — 새 하네스, WSL에서 실체험). 각각 ×2.
- **파일명은 `git ls-files` 실측**(플랜 B/C 레슨).
- **커밋 트레일러:** `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

## 사전 실측 (컨트롤러 — 구현자 신뢰 가능, contact 시 재확인)

- WSL2: `Ubuntu-24.04`(24.04.4 LTS), 커널 6.18.33.2-microsoft-standard-WSL2, 디스크 여유 955G, 기본 사용자 kiso, `wsl.exe -u root` 패스워드리스 루트 가능. g++·make·cmake 미설치 → **설치는 컨트롤러가 배경 작업으로 진행 중**(task brief 시점 완료 확인 필요).
- posix 스텁 현황: JKFs_posix.cpp 8행·JKProcess_posix.cpp 32행·JKNet_posix.cpp 62행(가드된 스텁)·JKConPtyBridge_posix.cpp 29행(관성 스텁+ReaderThread no-op)·JKPipeTransport_posix.cpp 62행(CreateServer/ConnectClient "not implemented", Send/Read는 fd 실증 코드일 가능성 — T5에서 실측).
- 싱글인스턴스 가드: JKWindowServer.cpp:414 CreateMutexA(이름 가드) — :73 dllimport 수기 선언.
- pty 헤더 계약: Start(commandLine, cols, rows)/ProcessExited/ShellExited/DrainOutput/WriteInput/Resize/Stop + ReaderThread private; close 순서 계약 docs/22 §8.2.
- 전송 헤더 계약: CreateServer(name) 블록-대기, ConnectClient(name), Write/Read/Close/CancelPendingIo; NativeHandle=int(posix).

## Rulings

- **R-D1(폰트·셸 추상·CRT 진입 미포함):** docs/62 §8 본체 4가지 중 "폰트 탐색"은 W5 as-built의 2단계 결정 대기 건과 설계 논의(docs/63 통합)가 필요 — 플랜 E에서. cmd.exe 셸 리터럴·wmain도 동일. 이 플랜 D는 어댑터 본체 6종+검증 하네로 한정(스코프 과적 방지).
- **R-D2(posix pty commandLine 해석):** posix Start(commandLine)는 `/bin/sh -c commandLine`으로 실행한다 — Windows의 "커맨드라인 문자열" 시맨틱과 등가. ReaderThread는 master fd 읽기; Stop은 close 순서 원문(docs/22 §8.2) 승계: pty master close→bounded wait(waitpid WNOHANG 폴링)→SIGKILL→정리.
- **R-D3(전송 소켓 경로):** unix domain socket 파일은 `<name>.sock`을 state 디렉터에 두지 말고 `/tmp` 고정? — 아니, 서버가 이미 쓰는 state 디렉터 컨트롤이 우선. 판정: 이름 그대로 `name`을 상대 경로로 받아 그대로 소켓 파일로 bind하고, 스텁시절 관습은 버린다(호출자가 절대 경로를 준다 — win32 named pipe 이름과 호환되는 문자열). 런타임 연결 오류 처리는 win32와 동일(null 반환+stderr 경고 원문 유지).
- **R-D4(selftest 케이스는 어댑터별 신설):** posix_selftest 케이스는 각 태스크가 자기 어댑터 분을 추가(T0에서 하네스+fs 케이스부터). win32 셀프테스트(main.cpp)에는 이 플랜에서 case 신설 없음 — win32 TU 무변경이라 케이스도 무변경.

## Files

- Create: `engine/tools/posix_selftest/main.cpp` — WSL 검증 하네스(케이스: fs/process/net/pty/transport/instance)
- Create: `engine/tools/posix_selftest/build.sh` — 어댑터 posix TU+main을 g++로 정적 묶음(윈도우 미빌드)
- Modify: `engine/src/fs/JKFs_posix.cpp`, `engine/include/fs/JKFs.h`(posix 문장 보강만)
- Modify: `engine/src/process/JKProcess_posix.cpp`
- Modify: `engine/src/net/JKNet_posix.cpp`
- Modify: `engine/src/terminal/JKConPtyBridge_posix.cpp`
- Modify: `engine/src/ipc/JKPipeTransport_posix.cpp`
- Create: `engine/include/fs/JKInstanceLock.h` + `engine/src/fs/JKInstanceLock_win32.cpp` + `engine/src/fs/JKInstanceLock_posix.cpp`
- Modify: `engine/src/server/JKWindowServer.cpp:414`(가드 어댑터 치환 1건 — R-D 예외)
- Modify: `engine/CMakeLists.txt` — instance lock 2행 등록
- Modify: `docs/69_linux_stage2_posix_impls.md`(as-built 신설) — 마지막 태스크

---

### Task 1: WSL 하네스 + jk::fs posix 실구현

**Files:**
- Create: `engine/tools/posix_selftest/build.sh`, `engine/tools/posix_selftest/main.cpp`
- Modify: `engine/src/fs/JKFs_posix.cpp`, `engine/include/fs/JKFs.h`

**Interfaces:**
- Consumes: `jk::fs::GetExecutablePath()` existing contract.
- Produces: posix_selftest exe(WSL 실행)·`jk::fs::GetExecutablePath()`가 posix에서 실제 exe 경로 반환.

- [ ] **Step 1: 하네스 스켈레톤** — main.cpp에 케이스 러너(체크 함수+`PosixSelfTest: %d failure(s)` 형식 플랜 B/C 호환), build.sh(리포 루트 기준 `/mnt/i/progwork/JKENGINE`). 케이스 1(fs): `GetExecutablePath()`가 `/mnt/i/.../posix_selftest`로 끝나는 실재 경로 반환+읽기 가능. RED: 현재 스텁은 빈 문자열 → FAIL.
- [ ] **Step 2: 구현** — JKFs_posix.cpp에 readlink("/proc/self/exe") 256→배증 루플(읽어온 버퍼 그대로 반환 — NUL 종료 불확실 방지). JKFs.h에 posix 쪽 바이트 주석 보강(native UTF-8).
- [ ] **Step 3: WSL 게이트** — `wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE && sh engine/tools/posix_selftest/build.sh && ./engine/build/posix_selftest"` (빌드 산출 위치는 build.sh 결정 — engine/build 밖 posix scratch 디렉터 권장; CMakeLists 무접촉) → PASS.
- [ ] **Step 4: 윈도우 게이트(가벼움)** — 전체 빌드 0에러+셀프테스트 0 failure(win32 TU 무변경 증명).
- [ ] **Step 5: 커밋** `feat(posix): WSL 검증 하네스+jk::fs posix 실구현 — readlink /proc/self/exe (플랜 D 태스크 1)`

---

### Task 2: jk::process posix 실구현

**Files:**
- Modify: `engine/src/process/JKProcess_posix.cpp`

**Interfaces:**
- Consumes/Produces: 헤더 계약 전부 원문 — Spawn(SpawnOptions: commandLineUtf8/workingDir/hideWindow/inheritedStdioPipes)·PeekPipeAvail(brokenError)·ReadPipeData·CloseHandleLike·CreateKillOnCloseJob·AssignToJob·TerminateJobTree·KillProcess·GetExitCode.
- posix 매핑: commandLineUtf8→`/bin/sh -c`(R-D2와 동일 해석)·pipe() fork 전 생성/자식 dup2/부모는 READ 끝만+write 끝 즉시 close(계약 a)·자식 setpgid(0,0)·job=pgid 포인터(literal pgid를 heap에)·AssignToJob(job,spawned)=pgid 재설정·TerminateJobTree=kill(-pgid,SIGKILL)·GetExitCode=waitpid WNOHANG 폴링으로 STILL_ACTIVE(=259) 보존 — **posix용 STILL_ACTIVE 대응은 job/exit 상태를 heap에 캡처해 waitpid 결과로 259 계약 유지**(또는 리뷰어에게 계약 대안 판정 요청 — brief에 명시).

- [ ] **Step 1: 케이스 2 RED(posix_selftest)** — sh -c echo 왕복+stdout/stderr 분리+부모 write 끝 EOF 관측(계약 a)+TerminateJobTree로 `sh -c sleep 30 && echo` 죽임+GetExitCode 0/비0 — 현재 스텁 FAIL.
- [ ] **Step 2: 구현** — 위 매핑. close-on-exec 주의(pipe fd는 CLOEXEC 불필요 — 자식이 소유, 부모 write 끝만 조심).
- [ ] **Step 3: WSL 게이트**(+윈도우 가벼운 게이트) → 커밋 `feat(posix): jk::process posix — fork/exec+pgid 트리 kill+계약 a·b 실측 (플랜 D 태스크 2)`

---

### Task 3: jk::net posix 실구현

**Files:**
- Modify: `engine/src/net/JKNet_posix.cpp`

**메모:** POSIX 소켓은 AF_INET TCP라 win32와 API 형태가 거의 동일 — 실구현이 작다. PrimaryIp UDP 트릭 동일(10.255.255.254). SO_RCVTIMEO/SO_SNDTIMEO는 timeval(초+마이크로초 — `timeoutMs`→`{ms/1000, (ms%1000)*1000}`).

- [ ] **Step 1: 케이스 3 RED** — ListenTcp(127.0.0.1, 0, 1, &boundPort)→원시 클라(POSIX용 — posix_selftest TU는 헤더 자유, <sys/socket.h> include)→Accept→RecvAll 5바이트→Send 에코→Close. bad bindIp "999.999.999.999" → kInvalidSocket.
- [ ] **Step 2: 구현 + WSL 게이트**(+윈도우 가벼운 게이트) → 커밋 `feat(posix): jk::net posix — POSIX TCP 왕복+bind 실패 관측 (플랜 D 태스크 3)`

---

### Task 4: posix pty (JKConPtyBridge)

**Files:**
- Modify: `engine/src/terminal/JKConPtyBridge_posix.cpp`

**메모:** openpty+fork+setsid+TIOCSCTTY+dup2(0,1,2)+execl("/bin/sh","sh","-c",commandLine). ReaderThread는 master fd 읽기(가드 시작 전 블록 주의 — started_ 후 스레드 가동). Resize=TIOCSWINSZ ioctl+SIGWINCH는 셸이 알아서. Stop: master close→waitpid WNOHANG 폴링(2s)→SIGKILL pgid→회수. util.h 링크(-lutil, WSL 빌드 스크립트에 추가).
- [ ] 케이스 4 RED→구현(계약 원문: ProcessExited=자식 종료, ShellExited=pty 읽기 EOF, DrainOutput=버퍼 덤프, Resize/WriteInput)→WSL 게이트(bash -c 'echo HANGUL_TEST; exit 7'로 출력 수집+exit 코드 관측)→커밋 `feat(posix): pty 실구현 — openpty+fork/exec+리더 스레드+docs/22 §8.2 close 순서 (플랜 D 태스크 4)`

---

### Task 5: posix 전송 (JKPipeTransport)

**Files:**
- Modify: `engine/src/ipc/JKPipeTransport_posix.cpp`

**메모(R-D3):** CreateServer(name)=unix socket bind+listen(1)+accept(블록)·ConnectClient=name connect. 스텁시절 "not implemented" stderr 경고 원문 유지(성공 경로에서 사라짐). CancelPendingIo=shutdown(fd, SHUT_RDWR) — Read() 박힌 스레드 깨움(계약 원문). close 순서: CancelPendingIo→reader join→Close(win32 계약 승계).
- [ ] 케이스 5 RED(서버 스레드+클라 왕복 100바이트 wire 무관 순수 전송+CancelPendingIo로 박힌 Read 깨우기)→구현→WSL 게이트→커밋 `feat(posix): 전송 real — unix domain socket+CancelPendingIo 계약 (플랜 D 태스크 5)`

---

### Task 6: 싱글인스턴스 가드 어댑터 신설

**Files:**
- Create: `engine/include/fs/JKInstanceLock.h`, `engine/src/fs/JKInstanceLock_win32.cpp`, `engine/src/fs/JKInstanceLock_posix.cpp`
- Modify: `engine/src/server/JKWindowServer.cpp:414`(치환 — win32 디퍼런스 순수 치환 1건, as-built 예외 기재), `engine/CMakeLists.txt`

**Interfaces (신설 계약):**
```cpp
// jk::fs::InstanceLock — single-instance guard adapter (docs/62 §8 flock 봉쇄).
// Acquire: true=owned (process exits release it — Win32 mutex / flock lifetime)
// false=another instance holds it. Release: drop early (optional).
namespace jk::fs {
bool AcquireInstanceLock(const std::string& lockName);
void ReleaseInstanceLock();
}
```
- posix: lockName→`/tmp/<name>.lock`, open(O_CREAT|O_RDWR, 0600)+flock(LOCK_EX|LOCK_NB). win32: CreateMutexA(lockName, TRUE) — WAIT_OBJECT_0|WAIT_ABANDONED 취득, ERROR_ALREADY_EXISTS 실패. **win32 본체는 원문과 동일 API+취득 조건**(WindowServer:417-419의 실패 분기 메시지 원문 유지).
- [ ] Step 1: 케이스 6 RED(같은 프로세스에서 2회 취득 — 2번째 false; 풀고 다시 true — WSL에서 자식 프로세스 2개로도 검증 가능하면 그렇게) → Step 2: 어댑터 3파일+WindowServer 치환+CMake 등록 → Step 3: 게이트(WSL 케이스+윈도우 빌드/셀프테스트+probe_app_tools — WindowServer 접촉이므로) → 커밋 `feat(posix): instance lock 어댑터 — CreateMutexA→flock 계약 (docs/62 §8 flock 봉쇄, 플랜 D 태스크 6)`

---

### Task 7: 전체 게이트 ×2 + docs로 as-built 완결 (컨트롤러 직접)

**Files:** docs/69_linux_stage2_posix_impls.md 신설(as-built+2단계 잔여 목록화 — 폰트·셸 추상·CRT 진입·Termux 패키징은 플랜 E), docs/68에 2단계 착수 항목 1줄 링크.

**게이트:** (1) **윈도우 공식 세트 ×2**(플랜 C T5와 동일 9항목 — win32 TU 무변경 증명), (2) **WSL posix_selftest ×2**(케이스 1-6 전부, 리빌드 포함), (3) `git diff 9618195..` 이후의 win32 TU(플랜 목록 제외) 변경 0 실측 — `git diff --stat` 검토로.

- [ ] Step 1: 게이트 ×2 → Step 2: docs/69 as-built(각 태스크 실측+R-D 판정 이행+레슨) → Step 3: 커밋 `docs(spec): docs/69 2단계 posix 실구현 as-built — 어댑터 6종 실측 완결 (플랜 D)`

---

## 2단계 이관 목록 (플랜 D 뒤, 플랜 E로)

- 폰트 탐색 체계(docs/63 폴백 통합 설계 — 데스크탑 벡터 폰트와 한 어댑터)
- cmd.exe 셸 리터럴(jk::process 셸 추상 — engine별 cfg)+wmain CRT 진입
- 전체 엔진 WSL CMake 빌드(SDL2·SDL2_mixer 등 패키지 설치 후 jkdesktop --server 런타임 게이트)
- Termux 패키징·폰 실기기 도달(docs/62 §3 흐름 유지)

## Self-Review

1. **스펙 커버리지:** docs/62 §8 본체 4: posix 전송(T5)·pty(T4)·flock(T6)·폰트(E로) — D는 명시적 분할(R-D1). 어댑터 3종(T1-T3)은 docs/68의 stage-2 승계. 검증 WSL 실측은 사용자 승인 라인(①)+WSL 실측 확보로.
2. **플레이스홀더:** 없음 — 케이스/구현 지시 전부 구체(코드 레벨은 brief 단계에서 플랜 B/C 원문 스타일로 전개).
3. **타입 일관성:** 계약 전부 기존 헤더 원문 — 신설은 instance lock뿐(3파일+win32 TU). posix selftest 케이스 추가는 T1-T6 각자.
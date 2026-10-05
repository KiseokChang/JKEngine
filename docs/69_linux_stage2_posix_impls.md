# docs/69 — 리눅스 2단계 posix 실구현 as-built (2026-10-05, 플랜 D)

상위: docs/62 §3·§8(2단계 정의) + docs/68(1단계 as-built — 어댑터 경계의 원본
계약) + docs/22 §8.2(pty close 순서 원문). 이 문서는 플랜 D 실행 원장:

SDD 플랜 docs/superpowers/plans/2026-10-05-linux-stage2-posix-impls.md
(docs/plan commit 313e5b4)을 태스크 1-7로 실행한 as-built 기록.

**플랜 D의 정의:** 1단계(docs/68)가 봉합한 어댑터 경계의 posix 본체 6종 실구현
+jk::fs::InstanceLock 신설. **검증은 이 윈도우 머신의 WSL2
Ubuntu-24.04에서 실 linux g++ 컴파일+런타임** — 별도 리눅스 머신 불요.
동작 변화 0은 "win32 TU diff 0"으로 증명한다(아래 §3 실측).

## 1. 검증 하네스 — `engine/tools/posix_selftest/` (태스크 1)

- `main.cpp` — 어댑터 posix TU만 직접 링크하는 스탠드얼론 셀프테스트(win32
  in-app selftest 관습 승계: 케이스별 `[PASS]/[FAIL]` 행 + `PosixSelfTest: <n>
  failure(s)` 푸터 + 실패 시 비0 종료). 케이스: 1 fs·2 process·3 net·4 pty·
  5 transport·6 instance-lock — 전부 posix 네이티브 헤더(sys/socket.h 등)
  허용(윈도우 컴파일 대상 아님, windows.h-clean 제약 없음).
- `build.sh` — `g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread -lutil`
  무조건 플래그(컨트롤러 러링: -lutil은 T4 openpty 시점부터 실사용이지만
  T1부터 고정 — 태스크 간 build.sh 흔들림 방지). 산출은
  `engine/build/posix_selftest`(윈도우 CMake 디렉터 내부 — 리뷰어 LOW 판정
  수용, gitignored+문서화).
- **레슨(WSL 실측 확보 경로):** 별도 리눅스 머신 없이도 이 머신의 WSL2
  (Ubuntu-24.04.4, `wsl.exe -u root` 패스워드리스)에 build-essential+cmake
  설치로 어댑터 TU만 묶는 스탠드얼론 하네스가 SDL 등 엔진 의존 제외하고 즉시
  실 linux 컴파일+런타임 검증을 제공. 2단계 이후(전체 엔진 SDL 빌드)는
  플랜 E 몫.

### 태스크 1 — jk::fs posix (커밋 134a88b+1b7bd5d)

- `JKFs_posix.cpp` — readlink("/proc/self/exe") 256→32KiB 배증 루프.
  **NUL 종료 불확성 처리:** resize(len)로 읽어온 바이트 수 그대로 반환
  (strlen 스캔 금지 — /proc/self/exe는 NUL 미삽입), len==buf.size()는 절단
  가능성으로 재시도, 소진 시 빈 문자열 fail-closed(스텁 계약 유지).
- `JKFs.h` 계약 보강(주석만): posix 쪽은 native 바이트(UTF-8) — win32 CP_ACP
  문장과 병기.
- 리뷰: Spec YES / Quality APPROVE. NIT(파일 끝 개행) 픽스 1b7bd5d.
- **발견(도중):** autocrlf=true 환경에서 .sh가 재체크아웃 시 CRLF로 변환되어
  WSL sh 실행이 깨질 함정 → **.gitattributes `*.sh text eol=lf` 신설
  (65882a6)** — 리포 전역 표준 보강. CJK/개행 민감 파일의 플랫폼 간 이동
  점검은 앞으로 .gitattributes 우선.

### 태스크 2 — jk::process posix (커밋 847ceeb+bc1eeda)

- fork+exec("/bin/sh","-c",commandLineUtf8)(R-D2)·pipe() fork 전 생성/자식
  dup2/부모는 READ 끝만(계약 a 원문)·setpgid(0,0) 자식+부모 대행(레이스 폐구)
  ·job=heap pgid·TerminateJobTree=kill(-pgid,SIGKILL)·GetExitCode=
  waitpid(WNOHANG) 폴링으로 **STILL_ACTIVE(259) 계약 완전 보존**
  (Heap ProcState{pid, reaped, killed, exitCode}).
- 리뷰 픽스(bc1eeda): F-2 PID 재활용 킬 위험 — KillProcess가 kill 전에 WNOHANG
  reap 시도(reaped면 false)·F-3 PipeState alloc을 nothrow+fail-closed로
  (void* ABI에서 throw 금지)·F-4 계약-a close 후 fd 슬롯 -1 정리(이중 close 소각)
  ·F-5 read() EINTR 리트라이.
- 리뷰 후 레저: job-path pgid 킬의 reap-first 가드는 proc 링크 구조 변경이
  필요해 미적용(~32k pid 재활용 확률 — 실소비자 무영향)·TerminateJobTree
  이중 호출 false(win32 TRUE와 패리티 어긋, 관측 동일)·exitCode 미사용(SIGKILL
  사망은 137 관습)·ProcState 무락(win32 같음, 스레드 소비자 도입 시 재판정).

### 태스크 3 — jk::net posix (커밋 5820d93+240ea67)

- 9함수 전수 실구현, win32 패리티 fn-by-fn: ListenTcp(SO_REUSEADDR+getsockname
  호스트오더 R-C4+실패 리스너 close 소각)·Accept/RecvAll EINTR 재시도·
  **Send는 EINTR 미재시도(원시 1호출 R-C2 원문 유지)**·SetTimeouts timeval
  {ms/1000,(ms%1000)*1000}(ms=0={0,0} 비활성 — 양 플랫폼 동등 실측)·PrimaryIp
  UDP 트릭 동일.
- **★실측 발견(플랜 가설 반론):** glibc inet_addr("999.999.999.999")는
  win32와 같이 INADDR_NONE이지만 **Linux bind(255.255.255.255)는 성립한다**
  (윈도우는 거부). 단순 미러링이면 브로드캐스트 바인딩 리스너가 조용히 열렸을
  것 → posix ListenTcp에 INADDR_NONE fail-closed 검사 추가(윈도우 관측 결과의
  재현 — widening 동반 등재). 셀프테스트 케이스 3B가 실측.
- 리뷰: Spec YES / APPROVE —픽스 240ea67(중복 include 제거·발견 반영 주석·
  개행·Monotonic 주석).

### 태스크 4 — posix pty (커밋 d2e66b6+aacaa66)

- openpty+fork+setsid+TIOCSCTTY+dup2(0/1/2)+execl("/bin/sh","-c",cmd).
  ReaderThread는 **poll(master, POLLIN|POLLHUP|POLLERR, 100ms)** 루프.
- **★실측 발견(플랜 가설 2번째 반론):** 플랜은 "마스터 close가 blocked read()를
  각성"을 전제했으나 WSL 실측 30초 무각성 — Linux close()는 다른 스레드의
  blocked read를 깨지 않는다(POSIX 미정의). 마스터 read는 EOF(0)가 아니라
  **EIO**로 끝난다(마지막 슬레이브 소멸 후). → 리더 정지는 `started_` 플래그
  +poll 타임아웃으로 봉합: Stop= flag-down→join(≤100ms 실측 11ms)→마스터
  close→waitpid(WNOHANG) 2s 유창 창(SIGHUP 유예)→SIGKILL→회수.
- 픽스 라운드 1(aacaa66): 마스터 close를 join 뒤로 이동(fd 재사용 경주 격차
  구조 소거 — 원문 주석 과잉보장 정직화)·케이스 4A 멱등 검사 실체화
  (`!IsValid()` 동성 반복(tautology) 제거)·자식 스폰 하드실패(setsid/TIOCSCTTY/
  dup2/exec)가 slave로 진단 1행 기록 후 _exit(126/127)(win32는 CreateProcess
  실패 false — posix는 계약상 true 반환 불가피, 실패를 스트림 관측 가능화)
  ·usleep→nanosleep.
- 리뷰 결론: Linux 특성(close 무각성·EIO) 2건의 플랜 가설 반론이 모두 실측
  검증 — 리뷰어 독립 확인. ShellExited 탐지 ≤100ms(윈도우 즉시) — UI 틱 미영향
  레저.

### 태스크 5 — posix 전송 (커밋 fdaa4be)

- unix domain socket으로 R-D3 계약: name을 그대로 소켓 경로로 사용(호출자가
  경로 문자열 공급). CreateServer=bind+listen(1)+accept 블록. **stale-socket
  파일 치징(triage): stat으로 S_ISSOCK인 잔여 파일만 unlink, 비소켓 충돌은
  unlink 없이 거부(fail-closed), 살아있는 서버 보유 중 bind 실패 역시 거부**
  (다른 프로세스의 락을 절대 제거하지 않는다 — D-T6 flock과 같은 철학).
- Write/Read 완전페이로드 루프(EINTR, 0x7FFFFFFF 청크 캡 win32 동일)+
  **MSG_NOSIGNAL**(windows에 없는 SIGPIPE 위험 소각). CancelPendingIo=
  shutdown(SHUT_RDWR) — 박힌 Read가 실패로 리턴(계약: pending ops fail now).
- 리뷰: Spec YES / **APPROVE as-is(픽스 0)**. 레저: posix cancel은 파기형
  (win32 CancelIoEx는 재사용형) — 엔진 소비자 전부 Cancel→join→Close 규약
  (JKClientConnection.cpp:116-118·JKClientSurface.cpp:198-200)이어서 무영향.
  윈도우가 cancel 후에도 쓰기를 계속하면 어긋나니, 그런 소비자는 금지.
  소켓 파일은 Close 후 잔존(헤더 동결 — 다음 CreateServer의 triage 회수).
  단일 인스턴스(listen(1)+listener close) — 실소비자 JKWindowServer::AcceptorLoop
  순차 루프 확인, 클라이언트 관측 동등(핸드셰이크 창 내 제2 클라는 win32도
  ECONNREFUSED 동급).

### 태스크 6 — jk::fs::InstanceLock 신설 (커밋 bfd16f5)

- 신설 어댑터: `engine/include/fs/JKInstanceLock.h` +
  JKInstanceLock_win32.cpp(CreateMutexA 명명 뮤텍스, WAIT_OBJECT_0|
  WAIT_ABANDONED 취득) + JKInstanceLock_posix.cpp(open(O_CREAT|O_RDWR|
  O_CLOEXEC, 0600)+flock(LOCK_EX|LOCK_NB), /tmp/<name>.lock, `/`→folding,
  **unlink 일체 금지**(타 인스턴스 락 파일 절대 삭제 안함), TU-static fd
  lifetime, O_CLOEXEC= brief-plus — 자식 상속 시 재시작 불가 함정 소각).
- JKWindowServer.cpp:414 치환 — **plan D 유일의 win32 소비 TU 접촉**(순수
  치환 총 8 hunk 전부 허용 영역: include 1행+dllimport 이전+가드 본체 5 hunk+
  파괴자 치환). 실패 메시지 byte-for-byte 보존 + 어댑터가 실패 경로
  SetLastError(183) 보유로 `!m && GetLastError()!=183` 합동 판정의 결정성
  확보(리뷰어 word-by-word 트레이스: 어댑터 실패 후 소비자 GetLastError 읽기
  사이에 clobber 가능 연산 0 — 결함 없음 확정).
- 파괴자도 ReleaseInstanceLock() 치환(마지막 핸들 close=해제 의미 승계) —
  brief 지점 밖의 첫 번째 필수 종속으로 as-built 기재.
- 리뷰: Spec YES / **READY TO MERGE(결함 0)**. 레저: slot-held 조기 리턴에
  SetLastError 보완 1행(옵션)·serverGuardMutex_ = 보유 마커 `void*(1)`로 의미
  전환(핸들 API 오용 위험 — 헤더 동결로 주석으로만 경고)·JKWindowServer.h:57
  stale 주석 소각(T7 스웹에서 수행).

## 2. R-D 판정 이행

- **R-D1:** 폰트 탐색·cmd.exe 셸 추상·wmain CRT 진입 — 플랜 E로 이관(스코프
  과적 방지). docs/62 §8 본체 4가지 중 posix 전송·pty·flock 가드는 이 문서에서
  완결, 폰트 탐색만 잔여.
- **R-D2:** posix pty commandLine = `/bin/sh -c` — d2e66b6로 이행. close 순서는
  실측 반론(§1 태스크 4)으로 flag→join→close 순서로 조정됐으나, 소비자 관측
  순서(pty가 유계 셸 대기보다 먼저 닫힘)는 docs/22 §8.2와 동일 — 리뷰어 확정.
- **R-D3:** name-as-path — fdaa4be로 이행.
- **R-D4:** 케이스 신설은 각 태스크가 자기 것만 — 이행됨(케이스 1-6). win32
  셀프테스트 case 신설 0.

## 3. 게이트 (2026-10-05)

**(1) WSL posix_selftest: `PosixSelfTest: 0 failure(s)` ×2 연속 쾌적 완료(케이스
1-6, 빌드+런 — 구현 과정 누계 ×6 이상).** **(2) 윈도우 공식 세트(스토탑 후
공식 런) ×2 연속 전부 GREEN:** ① 전체 빌드 71/71 에러 0(런2는 무변경
재빌드 exit 0으로 2연속 증명) ② jkdesktop 셀프테스트 `AppSelfTest: 0
failure(s)`(케이스 13·14·15 포함) ③ terminal_hangul_probe **44/44** ④
probe_app_tools **ALL PASS** ⑤ probe_jkbridge **PASS** ⑥
probe_agent_events **8/8** ⑦ probe_semantic_cursor **ALL PASS** ⑧
jkagentd 라이브니스(stdio list_windows → `"ok":true`) ⑨ jkbridge HTTP
스모크(root=200+토큰 페이지). **(3) win32 TU diff 0 실측:**
`git diff --stat 313e5b4..7b24808` — 기존 win32 어댑터 5종(JKFs·JKNet·
JKProcess·JKConPtyBridge·JKPipeTransport의 _win32.cpp) diff **0행**,
CMakeLists +6행(instance lock 등록만), JKWindowServer.cpp 59행(R-D 예외
순수 치환 8 hunk — 리뷰어 전 hunk 검증), 그 밖 소비/테스트 TU 접촉 0.

## 4. 남는 것(3단계로 — 플랜 E 후보)

- 폰트 탐색 체계(docs/63 폴백 체인과의 통합 설계 — §8 본체 4중 유일 잔여)
- cmd.exe 셸 리터럴(jk::process 셸 추상)+wmain CRT 진입(JKWindowServer 콘솔
  잔여 8종은 docs/68 residual ①-⑤와 합쳐 3단계 결정)
- 전체 엔진 WSL/리눅스 CMake 빌드(SDL2·SDL2_mixer 등 패키지 후
  jkdesktop --server 런타임) — 현재는 어댑터 TU 단위 실측까지
- Termux 패키징·폰 실기기 도달(docs/62 §3 흐름)
- selftest 강화 후보(docs/68 기재 ② 승계: 수백 바이트 RecvAll 패턴,
  Accept 무한블록 방어)
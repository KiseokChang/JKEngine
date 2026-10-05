# Linux Stage 3 — 전체 엔진 WSL 빌드 (플랜 E) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** WSL2 Ubuntu-24.04에서 엔진 전체 CMake 빌드를 에러 0으로 만들고, `jkdesktop --server`가 WSLg에서 실제로 기동되게 한다.

**Architecture:** spike(2026-10-05, 커밋 전)가 실패 TU 21개로 범위 확정 — 전부 유틸리티 계층(CRT `_s` 함수, FindFirstFile/CreateDirectory류 fs ops, 수기 dllimport, 콘솔 API, charset 변환 2함수) 잔여이며 렌더/컴포지트/클라 코어(jkclient·jkdesktop_shell·JKDC·터미널 렌더 층)는 이미 POSIX 컴파일 통과. fs ops는 **std::filesystem**으로, 프로세스 스캔은 jk::process 어댑터 확장으로, charset은 신설 `jk::text` 어댑터(posix leg=glibc iconv CP949 실측 확인)로 흡수. Linux 제외(명문 v1): jkchat(Win32 GUI), jkapp_browser+cefosr(CEF win 바이너리), .ps1 하네스.

**Tech Stack:** C++20(CMake 기존), Ninja, g++ 13.3, SDL2 2.30(apt)+SDL2_mixer 2.8+FFmpeg libav 60.x(apt), WSLg(X11/Wayland) 기동.

**Spec:** docs/62 §8(2단계 정의) + docs/69 §4(소비자 미접봉 — 이 플랜이 그 2건(flock·unix socket 경로)을 소각) + docs/68 residual. Spike 관측 원장: docs/70(이 플랜과 같은 세션에서 작성).

## Global Constraints

- **동작 변화 0(Win32 관측 동일):** 공유 TU 편집은 수반되므로 "win32 TU diff 0"은 이 단계에서는 쓸 수 없다 — 증명은 **Windows 공식 게이트 세트 ×2 GREEN**(플랜 D와 동일 9항목)으로 대체. posix 편차가 win32 쪽 관측을 바꾸지 않는 것은 각 태스크 게이트가 확인.
- posix TU는 `#ifndef _WIN32` 가드 필수(docs/69 레슨). 공유 TU의 플랫폼 편차는 `#ifdef _WIN32` 게이트 또는 플랫폼 중립 API 치환만.
- **빌드 명령 표준:** `wsl.exe -d Ubuntu-24.04 -e sh -c "cd /mnt/i/progwork/JKENGINE/engine/buildwsl && ninja ..."` — build 디렉은 `engine/buildwsl`(drvfs, 슬로우 허용; Windows `engine/build` 접촉 금지).
- **라이브 win32 스택 보호:** 윈도우 빌드(engine/build)는 라이브 스택 중 링크 Permission denied — WSL 작업은 engine/buildwsl만 건드리고, Windows CMake 재빌드가 필요한 태스크(CMakeLists 편집류)는 컨트롤러가 태스크 종료 시점에 윈도우 정지→빌드→복원을 수행(하네스 기존 관례). WSL만 컴파일 검증하는 태스크는 스택 무정지.
- **이미 설치됨(WSL apt, spike):** libsdl2-dev 2.30.0, libsdl2-mixer-dev 2.8.0, libavformat/codec/swscale/swresample/avutil-dev 60.x, pkg-config, ninja-build, build-essential, cmake 3.28.
- **spike CMakeLists 변경(커밋 안 됨, T1 승계):** :2-8 project 언어 조건부 RC+`if(WIN32) enable_language(RC)`, jkdesktop sources의 `src/jkdesktop.rc`→`$<$<PLATFORM_ID:Windows>:src/jkdesktop.rc>`.
- **Linux 제외 v1(명문):** jkchat(WIN32 GUI — docs/68 W8 마킹), jkapp_browser+cefosr(third_party/cef는 Windows 바이너리 — 기존 CMake도 cef 폴더 존재 시에만 빌드하는 가드가 있음, 여기에 WIN32를 더한다), vplayer는 유지(FFmpeg libav는 apt로 해소됨).
- 커밋 트레일러 `Co-Authored-By: Claude Code <noreply@anthropic.com>`; 사내 전용 repo — 외부 공개 금지.

---

### Task 1: CMake 플랫폼 조건화

**Files:** Modify `engine/CMakeLists.txt`

**Interfaces:** Produces: Linux에서 제외되는 타깃 집합 = {jkchat, jkapp_browser(+cefosr), .rc 언어}; 나머지 전 타깃이 Linux configure/컴파일에 참여.

- [ ] **Step 1:** spike의 2건(RC 조건부+rc genex)이 그대로인지 확인하고 보완: `JKPlatform_win32.cpp`·`JKImeHook_win32.cpp`·`JKCrashHandler_win32.cpp` 세 TU(:141-143)를 `$<$<PLATFORM_ID:Windows>:...>` genex로 감싼다(jkcore 리스트 내 — 전 TU가 수기 dllimport/windows.h 소유 TU라 Linux에서는 편집 대상 아님).
- [ ] **Step 2:** WIN32 전용 exe 옵션 게이트: `set_property(TARGET jkdesktop APPEND_STRING PROPERTY LINK_FLAGS " -municode")`(:331)과 `target_link_options(jkchat PRIVATE -municode -static-libstdc++ -static-libgcc)`·jkctl의 `-municode -static-libstdc++ -static-libgcc`를 `if(WIN32)`/genex로 게이트. jkchat exe와 jkapp_browser/cefosr 타깃 전체를 `if(WIN32)` 블록으로 감싼다(cefosr의 기존 `third_party/cef` 존재 가드 안에 WIN32 조건을 AND). agentd/bridge/triggers는 유지.
- [ ] **Step 3:** WSL configure 재실행 → `-- Generating done` 확인.
- [ ] **Step 4:** Windows 쪽: 컨트롤러가라이브 정지 후 `engine/build` 전체 빌드 에러 0 확인(윈도우 정지-빌드-복원은 컨트롤러 몫 — 태스크 종료 후 보고에 명기), 복원. WSL configure 재확인만으로 커밋 가능하다면 WSL 게이트로 대체해도 좋다.

### Task 2: CRT shim 집합 신설 + 소비 접촉

**Files:** Create `engine/include/port/JKCrtShim.h`; Modify `src/apps/ClientFilesApp.cpp:52`·`src/apps/ClientNotesApp.cpp:52`·`include/apps/ClientScriptApp.h:917,928`·`src/apps/ClientTaskmgrApp.cpp:44-54,297`

**Interfaces:** Produces: `jk::crt::FopenS(FILE**,...)`/`jk::crt::LocaltimeS(...)`/`jk::crt::Stricmp(...)` — `_WIN32`에서는 CRT 원본 위임(win32 패리티 100%), `!_WIN32`는 표준 CRT 조합(fopen+errno)·localtime_r·strcasecmp. 헤더 온리(인라인)가 가능한 것만 헤더에; 실험 후 확정.

- [ ] **Step 1:** shim 헤더 작성 — 각 함수 1행 주석에 원본 대응 명기. `FormatBytes`는 ClientTaskmgrApp.cpp:44의 `#ifdef _WIN32` 게이트를 **제거**(순수 포맷팅, 플랫폼 중립).
- [ ] **Step 2:** 소비 6부위 접촉 — shim include로 원문을 대체.
- [ ] **Step 3:** WSL: `ninja jkapp_files jkapp_notes jkapp_script jkapp_taskmgr` 에러 0.
- [ ] **Step 4:** 커밋 `feat(port): CRT shim header (_WIN32 delegates to CRT originals)`.

### Task 3: mouseLog 디버그 설비 유니폼화

**Files:** Modify `include/JKApplication.h:82`·`src/JKApplication.cpp`(:300,:320,:342,:569 부근)

**:Ruling:** mouseLog_(마우스 검증 로그 — 레슨 문서 출처: docs 승인 배너/마우스 검증 세션)는 디버그 설비라 Linux에서도 **nullptr 반환+개방 시도 생략**으로 유니폼화한다. `#ifdef _WIN32` 게이트 제거, 개방 호출부만 `_WIN32` 게이트 유지(개방 안 되면 로그 null → 소비 무동작 — 기존 null 분기 존재 확인 후 이행).

- [ ] **Step 1:** JKApplication.cpp의 mouseLog_ 개방/사용 4부위와 멤버 선언 확인 → 개방부만 `_WIN32`로, 멤버+GetMouseLog()는 무조건으로.
- [ ] **Step 2:** WSL `ninja jkcore` 통과(에러 0 — JKButton 두 소비처 자동 해소 포함).
- [ ] **Step 3:** 커밋 `feat(port): mouse debug log facility platform-uniform (null log on non-win32)`.

### Task 4: jk::text charset 어댑터 신설 (Utf8↔Utf16, Utf8↔CP949)

**Files:** Create `engine/include/text/JKTextConv.h` + `src/text/JKTextConv_win32.cpp` + `src/text/JKTextConv_posix.cpp`; Modify `src/JKHangulUtil.cpp`(선언 2건 :19-25 + 소비 :41-48,203-211 치환), `engine/CMakeLists.txt`(등록 2행 — JKInstanceLock 선례 위치)

**Interfaces:** Produces: `jk::text::Utf8ToUtf16(std::string_view)->std::wstring`(실패/부적합=빈 문자열 fail-closed — MBTW MB_ERR_INVALID_CHARS 관측 승계), `jk::text::Utf16ToUtf8`, `jk::text::Utf8ToCp949`, `jk::text::Cp949ToUtf8` — win32 impl=MultiByteToWideChar/WideCharToMultiByte 원문 위임(codepage 상수 소유), posix impl=`iconv_open`("UTF-8"/"UTF-16LE"/"CP949") — **glibc CP949 실측 확인 완료(spike)**. 유효성 계약: 부적절 시퀀스 실패=빈 문자열(win32 MBTW err flag와 동일 관측).

- [ ] **Step 1:** 어댑터 3종 작성 — win32 TU가 windows.h를 소유(windows.h-first 규약), posix TU `#ifndef _WIN32` 가드+iconv.h. posix는 iconv 실패 부분-변환(E2BIG/EINVAL/EILSEQ) 전부 빈 문자열 fail-closed.
- [ ] **Step 2:** JKHangulUtil의 extern "C" __stdcall 선언 2건 삭제, 두 함수의 UTF-8↔CP949 leg를 어댑터 호출로 치환 — **EUC-KR(2바이트 완성형)→KSSM 결합형 wCodeTable leg는 플랫폼 중립 본체로 유지**(이것이 이 파일의 실제 지식).
- [ ] **Step 3:** WSL: `ninja jkcore` 에러 0 + **posix_selftest에 케이스 7 신설**(posix_selftest는 스탠드얼론 — build.sh TU 리스트에 JKTextConv_posix.cpp 추가, 케이스: UTF-8→CP949→UTF-8 왕복 3문장(한글/한자/기호)+부적합 바이트 빈 문자열 단언; `PosixSelfTest: 0 failure(s)`).
- [ ] **Step 4:** Windows 게이트: terminal_hangul_probe(44/44) — KSSM 경로 회귀.
- [ ] **Step 5:** 커밋 `feat(text): jk::text charset adapter (iconv posix leg, glibc CP949 verified)`.

### Task 5: JKWindowServer 수기 Win32 소각 + posix 소비자 접봉 (flock·unix socket)

**Files:** Modify `src/server/JKWindowServer.cpp`(:317-383 프로세스 스캔, :3000-3025/:4564-78 FindFirstFile, :3175/:3766/:5008/:5099/:7330 CreateDirectoryA, :4209 GetFileAttributesA, :7876 kStillActiveExit, :414 부근 가드) + jk::process/fs 어댑터 헤더·구현 + `src/server/JKWindowServer.cpp` posix 가드 분기

**Interfaces:** Consumes: Task 4 이후 상태. Produces: docs/69 §4 소비자 미접봉 2건(flock 가드 posix wiring+`\\.\pipe\`→unix socket 경로) 소각 — **이것이 이 플랜의 봉인 태스크: Linux에서 서버가 실제로 단일 인스턴스+실제 파이프로 살아나는 길.**

- [ ] **Step 1 (fs ops → std::filesystem):** FindFirstFile/FindNextFile/FindClose/kInvalidFindHandle·FindFileDataA(2곳)→`std::filesystem::directory_iterator` 루프(name/sort/order 관측 동형 확인 — FindFirstFileA는 와일드카드 패턴 사용부가 있으면 `std::filesystem::path::compare` 필터로 관측 동형화); CreateDirectoryA 5곳→`std::filesystem::create_directory`(이미 존재=무동작, 실패=false — 원문 bool 관측 동형); GetFileAttributesA/kInvalidFileAttributes(:4209)→`std::filesystem::exists`(존재 bool만 소비인지, attribute 비트 소비인지 원문 확인 후 치환).
- [ ] **Step 2 (프로세스 스캔 → jk::process 어댑터 확장):** :317-383 Toolhelp32 스캔(CreateToolhelp32Snapshot/Process32FirstW/NextW/CloseHandle)을 어댑터 신설 함수로 승계 — **win32 impl은 해당 본문의 이동**(Toolhelp 코드 그대로), **posix impl=`/proc/<pid>/cmdline`+`/proc/<pid>/comm` 스캔**(이미지명 매칭 계약: exe 이미지명 하위 문자열/LIKE 동형 — 원문 매칭 논리 정독 후 계약 명문화). :7876 kStillActiveExit 상수 → 어댑터가 소유(STILL_ACTIVE 259 계약 승계 — JKProcess_posix가 이미 보유). :380-383 OpenProcess/TerminateProcess/CloseHandle → 기존 어댑터(TerminateJobTree/HandleLike) 흡수 정합 확인.
- [ ] **Step 3 (posix 소비자 접봉 — docs/69 §4 2건):** (a) JKWindowServer의 posix 가드 분기(`#else` no-op true)를 `AcquireInstanceLock("jkwinserver")`+실패 메시지 원문 경로로 교체 — win32 분기가 adapter 치환된 것과 논리 동형(플랜 D T6 소비자 측 마무리); (b) 와이어 경로: kWindowServerPipeName이 `\\.\pipe\...` 문자열을 그대로 unix socket 바인드에 쓰고 있는지 확인(현재 posix CreateServer는 name-as-path) — **posix에서만 적용되는 이름-경로 매핑 신설**(예: `\\.\pipe\X` → `X` 이름 fold('/'→'_' 관례 승계) 후 `/tmp/<folded>.sock`, `\\.\pipe\` 접두 제거 로직은 _WIN32 아니면 미동작을 어댑터 수준에서 명문화; 상수 그대로 소비하므로 호출부 무수정). JKClientConnection/JKClientSurface 클라 접속도 같은 매핑을 타는지 확인(Connect 부위의 posix 분기).
- [ ] **Step 4:** WSL: `ninja jkserver` 에러 0 + posix_selftest(프로세스 스캔 posix impl 케이스 — /proc 스캔 자기 PID 발견 실측) `0 failure(s)`.
- [ ] **Step 5:** Windows 게이트: 전체 빌드 에러 0+probe_app_tools+probe_agent_events(스폰 경로 스모크)+jkdesktop 셀프테스트 0 failure(Toolhelp 이동 검증은 jktriggers/jkbridge 스폰 소비자 스모크로 대체 가능 — probe_jkbridge).
- [ ] **Step 6:** 커밋 `feat(server): win32 residue → std::filesystem + process adapter; posix consumer wiring (flock+unix socket path)`.

### Task 6: jkcore 잔여 (LlmEngine dllimport·WorkshopStore·ThemeConfig mtime)

**Files:** Modify `src/agent/JKLlmEngine.cpp:25,370`·`src/script/JKWorkshopStore.cpp`(:100 FindFirstFile, :143-146 CreateDirectory, :155 WriteFile, :174,:181 ReadFile — windows.h) ·`src/theme/JKThemeConfig.cpp:62-65`(GetFileAttributesExA mtime) — JKHanjaDict는 통과 실측(에러 목록 밖).

**Interfaces:** jk::fs 어댑터에 파일 mtime 신설(`ModTimeNs(path)—win32=GetFileAttributesExA 원문(FILETIME 100ns — exFAT 서브초 레슨 승계), posix=stat st_mtim`)+Write/ReadFile류는 플랫폼 중립 CRT(파일 I/O는 어댑터가 아니라 stdio로) — 원문이 windows.h CreateFile을 쓰는지 stdio를 쓰는지 정독 후 결정, CreateFile이면 `std::filesystem`+`std::ifstream/ofstream` 치환.

- [ ] **Step 1:** JKLlmEngine.cpp:25의 "expected constructor" — 원문 정독(수기 dllimport 잔여 블록). 어댑터 W4 흡수 잔여(마킹문자 주석 docs/68 W4)로 판정 → 어댑터 소화(스폰 경로는 이미 adapter) or `#ifdef _WIN32` 게이트(정당 잔여면 게이트+주석 갱신). :370 WaitForSingleObject — 마킹된 잔여: PeekPipeAvail 타임아웃 대기인지 — 어댑터 소화 가능하면 소화, 아니면 `#ifdef _WIN32`+posix 대응(poll) 어댑터 함수(WaitPipe?) 신설로 fail-closed 흡수. **룰링: 간단 쪽 우선 — 소화가 원문 시맨틱 수정을 강제하면 게트만.**
- [ ] **Step 2:** JKWorkshopStore/TJKThemeConfig fs ops 치환(위 인터페이스) — ThemeConfig mtime은 jk::fs::ModTimeNs로(win32 원문 GetFileAttributesExA 이동, 관측 동일; theme 폴링 500ms 관측 불변).
- [ ] **Step 3:** WSL `ninja jkcore` 에러 0; posix_selftest ModTimeNs 케이스(파일 생성→mtime>0 단언) 1체크 추가.
- [ ] **Step 4:** Windows 게이트: probe_jkbridge(LLM 턴 경로)+theme 스모크(probe_theme_swap). 커밋 `feat(port): jkcore residue — llm engine dllimport, workshop store fs ops, theme mtime adapter`.

### Task 7: 클라 앱 + main.cpp 진입/임시 경로

**Files:** Modify `src/apps/ClientNotifyApp.cpp`(windows.h, :109 localtime_s, :239 CreateDirectory)·`src/apps/ClientShotApp.cpp`(windows.h, :108 FindFirstFile)·`src/apps/JKAppModule_script.cpp`(windows.h, :14 GetFileAttributes, :35 GetModuleFileName, :55,:66 fopen_s, :76 CreateDirectory)·`src/main.cpp`(:3283 SetCurrentDirectoryA, :3405 GetTempPathA, :3408 GetCurrentProcessId, :3459 wmain)

**Interfaces:** GetModuleFileName→`jk::fs::GetExecutablePath`(기존). GetTempPathA→jk::fs 신설 `TempDir()`(win32=GetTempPathA 원문, posix=TMPDIR/env 폴백 /tmp). GetCurrentProcessId→`#ifdef _WIN32`+!(posix getpid). wmain→**이중 진입**: `#ifdef _WIN32 int wmain(...) { ...argv UTF-8 변환 후 RunMain } #else int main(int argc, char* argv[]) { RunMain(argv UTF-8 그대로) }` — RunMain 분리 원문 확인(:3452 주석 블록) 후 최소 봉합. SetCurrentDirectoryA→`#ifdef _WIN32`+`chdir`(posix).

- [ ] **Step 1:** 클라 앱 3종 fs ops 치환(T5 승계 패턴 — std::filesystem)+CRT shim(T2 승계).
- [ ] **Step 2:** main.cpp 5부위+b1 봉합(위 인터페이스 원문 그대로).
- [ ] **Step 3:** WSL: `ninja jkapp_notify jkapp_shot jkapp_script jkdesktop` 에러 0 — **jkdesktop 링크까지 포함**(이 플랜 최초의 Linux 링크 성공 지점; undefined reference는 본 태스크에서 전수 해소 — __imp_ 잔여는 수기 dllimport 본체 소각 누락).
- [ ] **Step 4:** 커밋 `feat(port): client apps + main entry dual (wmain/main), temp/chdir/pid shims`.

### Task 8: tools 3종 (jkbridge·jktriggers·jkctl)

**Files:** Modify `tools/jkbridge/main.cpp`(windows.h :35 — 콘솔 API 8종+기타 잔여)·`tools/jktriggers/main.cpp`·`tools/jkctl/main.cpp`(windows.h, :786 wmain, CreateProcessW — docs/68 W8 마킹)

**Interfaces:** 콘솔 색/커서 API(SetConsoleTextAttribute 등)→posix는 **ANSI 이스케이프 출력 소형 로컬 헬퍼**(파일 유틸 함수 — 신설 어댑터 아님; 툴별로 겹치면 tools 공용 헤더 `tools/ConsoleShim.h`로). CreateProcessW(jkctl 자식 스폰)→jk::process 어댑터(SpawnProcess 계열 실존 — inheritedStdioPipes 흡수 대상). jkctl wmain→이중 진입(Task 7 승계 패턴). jkbridge QR 출력·색 코드 원문 유지(posix ANSI 동등 출력).

- [ ] **Step 1:** jkbridge windows.h 잔여 전수 원문 정독 → std::filesystem/stdio/ANSI 치환, `#ifdef _WIN32` 잔여 최소화(콘솔 8종은 shim).
- [ ] **Step 2:** jktriggers 동형 처리, jkctl 어댑터 흡수+이중 진입.
- [ ] **Step 3:** WSL: `ninja jkbridge jktriggers jkctl jkagentd` 에러 0+링크 OK.
- [ ] **Step 4:** 커밋 `feat(port): console shim (ANSI posix) + jkctl spawn adapter absorb + dual entry`.

### Task 9: 게이트 전부 + WSL 런타임 스모크 + docs/70

**Files:** Create `docs/70_linux_stage3_full_build.md`; Modify 없음(게이트만)

- [ ] **Step 1 (WSL):** `ninja -k 0` 전체 빌드 에러 0 **×2 연속**(제외 대상: jkchat/jkapp_browser/cefosr — 타깃 자체가 게이트되어 목록 밖). posix_selftest `0 failure(s)`.
- [ ] **Step 2 (WSLg 스모크):** `DISPLAY=:0 ./jkdesktop --server` WSLg 기동 → 서버 로그 생존+taskbar 자동 스폰 확인. **캡처는 Windows 쪽 WSLg 창 스무거(WSLg Weston 윈도우가 Windows 데스크톱에 뜸 — Win32 스크린샷/WindowList로 실측 가능)**; 터미널 앱 스폰(`terminal:` 접두)의 posix pty 경로 동작 관측. 실패해도 진단 기록이 산출 — 성공이 목표.
- [ ] **Step 3 (Windows 공식 ×2):** 플랜 D 9항목 세트(빌드 0 에러, 셀프테스트 0, hangul 44/44, app_tools, jkbridge, events 8/8, semantic, agentd ok:true, curl 200) ×2 GREEN — 동작 변화 0 증명(TU diff 0 대체).
- [ ] **Step 4:** docs/70 as-built — spike 인벤토리(21 TU)·각 태스크 실측·WSLg 기동 스크린샷 판독·제외 대상 명문·잔여(레저: 채팅 GUI 비이식 확정 유지, job 단일 멤버, 자식 stdin /dev/null, phantom probe, vplayer 리눅스 실재 동작, Termux).
- [ ] **Step 5:** 커밋 docs/70 → opus 최종리뷰 → 픽스 웨이브 → 푸시.

---

## Plan Self-Review (스캔 테이블 — spike 기록)

| 접촉 | 소비/생산 | 판정 |
|---|---|---|
| T2 shim ↔ T5/T6/T7 fs 치환 동시에 같은 TU | ClientTaskmgrApp(T2)와 Taskmgr 프로세스 스캔(T5 어댑터) 같은 파일 | T2는 CRT만 — 충돌 없음; T5는 server TU — 분리 |
| T3 mouseLog ↔ jkcore 어느 태스크도 | 3개 태스크가 jkcore 편집 | T4·T6가 jkcore 접촉 — 병렬 금지(SDD 규칙), 순차(T3→T4→T6)로 흔들림 최소화 |
| T4 charset ↔ terminal_hangul_probe | KSSM 경로 win32 회귀 | T4 게이트에 명기 |
| T5 어댑터 확장 ↔ posix_selftest | build.sh TU 리스트 grow | T5 Step 4에 포함 |
| T7 jkdesktop 링크 ↔ T5/T6/T8 미해소 심볼 | 링크는 마지막 | T7이 최초 링크 성공 지점 — 미해소 시 T5/T6 소각 누락으로 판정, 되돌림 |
| Windows 빌드 접촉(스택 정지) | T1(CMakeLists)·T5·T6 등 공유 TU 편집마다 | 각 태스크 컨트롤러 복원 책임 — Global Constraints 명문 |

무충돌 판정. T5가 최대 난이도/최대 접촉(가장 큰 검증 표면) — 실패 시 룰링 후보: T5를 fs(process)로 2분할.
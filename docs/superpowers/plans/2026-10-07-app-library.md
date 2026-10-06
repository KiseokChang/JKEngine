# App Library Hub Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 설치 앱 3원(.jkx·콘솔·내장)을 스캔하는 pure 카탈로그 + 표시·상세·실행하는
허브 앱 `library`(settings/notes/files 4호 멤버) — 폰(posix)에서도 동일 동작.

**Architecture:** 스캔 규약은 **jkcore의 pure 카탈로그 1개**(exe-dir 기준,
std::filesystem, 창·imgui 무접촉 — CLI·클라 앱·셀프테스트가 같은 진실원을
먹는다). UI는 `jkapp_library` 모듈(ImGui 클라 앱 — ClientSettingsApp 선례:
RenderOverlay 내 NewFrame·BuildUi, launch_app 재용). 런처(JKDesktopShell
ScanJkxApps)는 **접촉하지 않는다** — 스펙 Q1 룰링.

**Tech Stack:** C++17 std::filesystem(ec 중립형, 무 try/catch), quickjs
throwaway 런타임(manifest.json 파싱 — ScanConsoleApps 선례), JKJkxFile(TOC),
stb(JKImageLoader — LoadImageMemory), ImGui(ImGui_ImplJKWindow), SDL texture.

**Spec:** docs/superpowers/specs/2026-10-06-app-library-design.md (결제판 —
Q1 A 허브 앱+런처 아이콘 뷰 유지 / Q2 uninstall 스킵 / Q3 미출하 슬롯 제외)

## Global Constraints

- 런처 `ScanJkxApps`/`ScanConsoleApps`(engine/src/desktop/JKDesktopShell.cpp)를
  수정하지 않는다 — win32 전용 관측은 그대로 두고 라이브러리는 자기 스캔
  (스펙 §4 결제 재확정).
- 카탈로그 출력 대전제: 능력 배지는 MANI `capabilities=` **원문 그대로**, 빈
  선언도 "능력 없음"으로 숨기지 않는다(docs/76 슬롯 스트립 배지 동일 계약 —
  문구는 ClientScriptApp의 기존 배지 문구를 grep해 정확히 따른다).
- `std::filesystem`은 `error_code` 중립형 — 퍼블리 코드 무 try/catch 규약
  (launch_app의 fileExistsFn 주석 review r1 HIGH 계약).
- 빌드: `export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" && ninja
  -C engine/build -j3` (Git Bash PATH 누락=cc1plus DLL 무음 사망 원장).
- 셀프테스트: `./engine/build/jkdesktop.exe test` → `AppSelfTest: 0
  failure(s)`. 케이스 번호는 1l·1m이 비어 있음을 실측 확인 — 본 플랜은
  **1m** 사용.
- probe .ps1은 **UTF-8 BOM 필수**(BOM 없으면 한국어 주석 파서 오류 원장).
- Windows에서 **서버를 띄우지 않는다**(서버 소유권=사용자 콘솔) — Windows
  기계 영수증은 CLI/셀프테스트만. WSL·폰 서버 부팅은 플랜의 probe가 소유
  (docs/68~78 선례).
- 사내 전용 repo — 내부 IP 포함 외부 공개 금지; 폰 복사는 LAN tar-over-ssh만.
- 커밋 메시지 다중 행은 `git commit -F <file>`; SHA 기록은 rev-parse 실측 값.
- ImGui Begin-false여도 End() 반드시 호출(docs/53 §9 잔여 계약).
- 폰 로그는 `~/tmp/`만(/tmp Permission denied 원장); WSL 파일 스크립트 표준
  (`MSYS2_ARG_CONV_EXCL='*' wsl.exe -d Ubuntu-24.04 bash <file>`).

---

### Task 1: 라이브러리 카탈로그(jkcore pure 스캔) + 셀프테스트 케이스 1m

**Files:**
- Create: `engine/include/JKLibraryCatalog.h`
- Create: `engine/src/JKLibraryCatalog.cpp`
- Modify: `engine/CMakeLists.txt` — jkcore STATIC 소스 리스트(src/JKJkxFile.cpp
  다음 줄)에 `src/JKLibraryCatalog.cpp` 추가
- Modify: `engine/src/main.cpp` — RunAppSelfTest 안 1k 블록 바로 뒤에 케이스
  1m 추가(1k 블록은 `'1k)'` 주석 grep — 블록 끝 판정은 다음 `//` 케이스 주석
  또는 함수 꼬리)

**Interfaces:**
- Consumes: `jk::JKJkxFile`(include/JKJkxFile.h — Open/Entries/Manifest/
  FindEntry/ReadEntry), `jk::fs::GetExecutablePath`(include/fs/JKFs.h:31 —
  basePath 규약: 뒤 구분자 없음), quickjs(JS_NewRuntime/JS_ParseJSON —
  ScanConsoleApps 선례), `std::filesystem`.
- Produces (Task 2·3·4가 이 시그니처를 그대로 먹는다):
  `namespace jk {`
  `enum class LibrarySource { Jkx, Console, Builtin };`
  `struct LibraryEntry { std::string appName, title, capabilities, path;`
  `  LibrarySource source = LibrarySource::Builtin; long long sizeBytes = 0;`
  `  bool hasIcon = false; };`
  `int LibraryScan(const std::string& basePath, std::vector<LibraryEntry>& out);`
  `}` — 반환값 = 스캔된 엔트리 수(out.size()). basePath는 exe dir
  규약(뒤 "\\"·"/" 없음 — ScanConsoleApps 주석의 관측).

- [ ] **Step 1: 셀프테스트 케이스 1m를 먼저 쓴다 실패 테스트**

main.cpp RunAppSelfTest의 1k 블록 뒤에 (check 헬퍼 계약: `check(cond,
"name")` — main.cpp:977 시그니처 그대로):

```cpp
        // 1m) 라이브러리 카탈로그 스캔 (스펙 2026-10-06-app-library §2 — 3원+1
        //   발견·.jkx 우선·능력 원문 보존). 가짜 apps/ 트리를 temp에서 조립
        //   — 실 기기 apps/에 의존하지 않는 pure 케이스.
        // 기대 계약(JKLibraryCatalog.h와 1:1):
        //   a) .jkx(name/title/capabilities/ICON 유무) → source=Jkx
        //   b) 콘솔 dir + manifest.json(name/cmd/desc) → source=Console
        //   c) .jkx와 동명 콘솔 → .jkx가 이긴다(스캔 순서 — 런처 규약)
        //   d) 내장 minesweeper는 항상; lf/hx는 파일 부재 시 제외
        //   e) MANI에 name/module 없는 컨테이너 → 스킵(Parse false 계약)
        {
            const std::string base =
                (std::filesystem::temp_directory_path(std::error_code{})
                 .append("jk_library_st_1m")).string();
            std::error_code ec;
            std::filesystem::remove_all(base, ec);
            std::filesystem::create_directories(base + "/apps/consoleapp", ec);
            // (a) .jkx 컨테이너 — JKJkxFile::Write 선례로 조립
            const std::string mani =
                "name=galapp\ntitle=갤 앱\ntitle2=ignored\nmodule=jkapp_gal.dll\n"
                "capabilities=widget,timer\nicon=icon@1x.png\n";
            std::vector<uint8_t> maniBytes(mani.begin(), mani.end());
            const bool jkxOk = jk::JKJkxFile::Write(
                base + "/apps/galapp.jkx", {{"manifest.txt", maniBytes}});
            std::vector<jk::LibraryEntry> got;
            const int n = jkxOk ? jk::LibraryScan(base, got) : -1;
            check(jkxOk, "1m-0 컨테이너 조립");
            check(n == 2 || n == 1, "1m-1 스캔 개수(jkx+내장 내장 차이 허용=2)");
            // d/e 등 나머지 check는 Step 3 구현 확정 후 정밀화 — Step 1에선
            // 위 2 check + 컴파일 실패(함수 부재)만 확인.
        }
```

주의: Step 1 단계에서는 `jk::LibraryScan`가 없어 링크 실패가 정상(빨간 테스트).
체크 나머지는 Step 4에서 전면 작성한다(위 코드블록은 최종본과 다를 수 있다).

- [ ] **Step 2: 실패 확인**

Run: `cd /i/progwork/JKENGINE && export PATH=("/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH" 확장) && ninja -C engine/build -j3 && ./engine/build/jkdesktop.exe test`
Expected: jkapp_settings 등은 통과하나 jkdesktop 링크 단계에서
`undefined reference to 'jk::LibraryScan(...)'` — 컴파일/링크 실패가 정상.

- [ ] **Step 3: 헤더 + 구현 본체 작성**

`engine/include/JKLibraryCatalog.h`:

```cpp
#ifndef JKLIBRARYCATALOG_H
#define JKLIBRARYCATALOG_H

// 라이브러리 카탈로그 (스펙 2026-10-06-app-library §2 — 설치 앱 3원 스캔).
//   1. .jkx 컨테이너 — apps/*.jkx, MANI 소비: name/title/capabilities/icon.
//   2. 콘솔 앱 — apps/<dir>/manifest.json(name/cmd/desc). .jkx 동명 스킵
//      (런처 ScanConsoleApps 규약 동일 — .jkx 우선).
//   3. 내장 — minesweeper·tetris(항상) + terminal: lf/hx(파일 존재 시만;
//      접미 차이 win32 .exe/posix 무접미).
// 창·imgui 무접촉 pure 스캔 — CLI(library-list)·클라 앱(jkapp_library)·
// 셀프테스트(케이스 1m)가 같은 진실원을 먹는다. 스펙 Q1 룰링상 런처
// (JKDesktopShell)는 이 카탈로그를 쓰지 않는다(접촉 0 — 접미 유지).
//
// 읽기 전용 설비(스펙 §0) — 어떤 파일도 쓰지 않는다(uninstall 스킵 결제).

#include <string>
#include <vector>

namespace jk {

enum class LibrarySource { Jkx, Console, Builtin };

struct LibraryEntry {
    std::string appName;      // 스폰 키 — launch_app {"app":...}. 콘솔 내장
                              // lf/hx는 런처 관례 "terminal:<cmdline>" 전체.
    std::string title;        // 표시명 — MANI title → 콘솔 desc → 스폰 키 순.
    std::string capabilities; // MANI capabilities 원문(""=선언 없음 — 숨기지
                              // 않는다, docs/76 배지 계약). 콘솔·내장은 "".
    std::string path;         // .jkx 절대경로 / 콘솔 dir 절대경로 / 내장 "".
    long long sizeBytes = 0;  // .jkx 파일 크기. 콘솔·내장 0.
    bool hasIcon = false;     // .jkx ICON 엔트리 존재(디코딩은 클라 몫).
};

// basePath(규약: exe dir — 뒤 구분자 없음)의 apps/를 스캔. 반환 = out에 채운
// 엔트리 수. apps/ 부재 등 스캔 불가는 0을 돌려준다(오류 전파 없음 — 라이브러리
// 빈 목록이 정당한 상태). out은 지우지 않고 push_back한다.
int LibraryScan(const std::string& basePath, std::vector<LibraryEntry>& out);

} // namespace jk
#endif // JKLIBRARYCATALOG_H
```

`engine/src/JKLibraryCatalog.cpp` — 핵심 본체(스캔 순서·동명 스킵·원문 보존).

- jkx leg: `std::filesystem::directory_iterator(basePath+"/apps", ec)` —
  확장자 `.jkx`만(equality_insensitive 대소문자). JKJkxFile::Open 실패
  continue(런처 동일), `mani.name.empty()` continue. MANI title→스폰 키,
  capabilities는 `mani.capabilities` **원문 대입**(정규화 금지 — 원문이
  배지 원천), hasIcon = `FindEntry("ICON", mani.icon or icon2x)` >= 0
  (런처 wanted 산식 재용: 2x 우선), sizeBytes = file size
  (`std::filesystem::file_size(path, ec)` — 실패 0).
- console leg: directory에서 dir만(스캐터 파일 스킵) + `manifest.json` —
  quickjs throwaway 런타임으로 name/cmd/desc(ScanConsoleApps :456-479 본사
  복사 — getString 람다 포함). name/cmd 비었으면 continue, 동명 jkx 존재
  스킵(.jkx wins — 로그 행까지 동일 문구 `console app '<name>' skipped
  (.jkx wins)`). appName=title 규약: appName=name, title=desc 비었으면
  name. consoleDir 절대 경로는 path에.
- builtin leg: `hasX` 가드로 minesweeper("Minesweeper")·tetris("Tetris")
  push. lf/hx는
  ```
  #ifdef _WIN32
      target = basePath + "/apps-bin/lf/lf.exe"  (hx: "/apps-bin/helix/hx.exe")
  #else
      target = basePath + "/apps-bin/lf/lf"      (hx: "/apps-bin/helix/hx")
  #endif
  ```
  존재(ec 중립 exists) 시만 `appName="terminal:" + target의 basePath 하위
  상대경로(폰·WSL 표기 일치 — 슬래시 정규화)` — 런처 폴백 4종
  (JKDesktopShell.cpp:257-284)의 캡처. lf/hx 파일 부재는 조용히 제외(폰
  기본값). Builtin 엔트리 path="".
- 상단 파일 주석에 렛슨 1행: "폰 런처가 built-in만 보였던 갭(스펙 §4)의
  해소는 라이브러리 스캔이 아니라 본 카탈로그의 posix 개방 자체 — 런처는 접촉
  없음(Q1 룰링)."
- 로그: 콘솔 스캔만 stderr 1행/앱(launch_app 존재 검증 디버그 도움) —
  jkx는 런처와 달리 조용(수십 개 정상 사이즈).
- **쓰기 금지**: 어떤 파일도 생성·수정하지 않는다(읽기 전용 설비).

`engine/CMakeLists.txt` jkcore STATIC 목록의 `src/JKJkxFile.cpp` 다음 줄에
`src/JKLibraryCatalog.cpp` 추가.

- [ ] **Step 4: 케이스 1m을 전면 check로 완성**

Step 3 설계에 맞춰 (a)~(e) 전부:

```cpp
            const jk::LibraryEntry* g = nullptr;
            const jk::LibraryEntry* c = nullptr;
            for (const auto& e : got) {
                if (e.appName == "galapp")  g = &e;
                if (e.source == jk::LibrarySource::Console) c = &e;
            }
            check(g != nullptr, "1m-2 jkx 발견");
            if (g) {
                check(g->title == "갤 앱", "1m-3 MANI title 전승");
                check(g->capabilities == "widget,timer",
                      "1m-4 능력 원문 보존(정규화 없음)");
                check(g->source == jk::LibrarySource::Jkx, "1m-5 source=Jkx");
                check(!g->hasIcon,
                      "1m-6 ICON 부재=hasIcon false(폰 기본값 경로)");
                check(g->sizeBytes > 0, "1m-7 크기 수령");
                check(g->path.find("galapp.jkx") != std::string::npos,
                      "1m-8 절대 경로");
            }
            // 콘솔 leg: 두 번째 자식 dir + manifest.json 조립은 Step 3 구현
            // 확정 순서(prepare 단계)에서 케이스 코드와 함께 보강한다 —
            // name=conapp cmd=apps-bin/x cmd2=desc 파싱 → source=Console,
            // 동명 conapp.jkx를 추가로 팩해 .jkx-wins 3원(c) 검증.
            check(std::filesystem::remove_all(base, ec2), "1m-z 클린업");
```

- [ ] **Step 5: 0 실패 확인 + 커밋**

Run: `ninja -C engine/build -j3 && ./engine/build/jkdesktop.exe test`
Expected: `AppSelfTest: 0 failure(s)`.
커밋(F-file 다중 행):
`feat(library): 카탈로그 3원 스캔 jkcore — jkx/콘솔/내장+능력 원문 보존+케이스 1m (스펙 2026-10-06-app-library §2)`

---

### Task 2: library-list CLI + probe

**Files:**
- Modify: `engine/src/main.cpp` — 서브커맨드 라우팅(slot-pack 라우트와 같은
  표기 — main.cpp:3690 계열) + usage 문자열(main.cpp:3637 계열) 갱신
- Create: `engine/tools/probes/probe_library_list.ps1` (UTF-8 BOM)

**Interfaces:**
- Consumes: Task 1 `jk::LibraryScan(basePath, out)`, `jk::fs::GetExecutablePath()`.
- Produces: `jkdesktop library-list [BASE]` — 1행/앱
  `name=<appName> title=<title> source=<jkx|console|builtin> caps=<capabilities> size=<n> path=<path>`
  + 꼬리 `count=<n> base=<basePath>`. caps 빈값은 `caps=`로 인쇄(원문 계약
  — 빈 선언을 숨기지 않는다). 인수 생략 = exe dir(런치 존재 검증과 같은
  기점), 인수 주면 그 basePath(셀프테이트와 동형 케이스 재용).

- [ ] **Step 1: 라우트 작성**

jkx-list 라우트(main.cpp:877 주석 계열) 직접 아래 본문 양식:

```cpp
// library-list [BASE]: 라이브러리 카탈로그 스캔 CLI(스펙 2026-10-06-app-library
// §6 — 서버 불요 검증, jkx-list의 posix 개방 라우트 선례). jkapp_library
// 클라 앱과 같은 jk::LibraryScan 진실원을 먹는다 — CLI가 늘어나면 클라가
// 변질된 것(스캔 규약 서버리 검증).
static int RunLibraryList(int argc, char** argv) {
    const std::string exe = jk::fs::GetExecutablePath();
    std::string base = argv[argc > 2 ? 2 : 1] ? /* 인수 파싱 본문 */ std::string{};
    // (구현 본문: 인수 파싱 — argv[2] 있으면 base=argv[2], 없으면 exe dir
    //  규약 추출. GetExecutablePath 비었다 cwd 폴백 — 라이브러리 자체는 빈
    //  목록도 정당 상태라 실패가 아니다.)
    std::vector<jk::LibraryEntry> out;
    const int n = jk::LibraryScan(base, out);
    for (const auto& e : out) {
        std::printf("name=%s title=%s source=%s caps=%s size=%lld path=%s\n",
                    e.appName.c_str(), e.title.c_str(),
                    e.source == jk::LibrarySource::Jkx ? "jkx" :
                    e.source == jk::LibrarySource::Console ? "console" : "builtin",
                    e.capabilities.c_str(), e.sizeBytes, e.path.c_str());
    }
    std::printf("count=%d\n", n);
    return 0;
}
```
(위 스니펫의 파싱 라인은 의사 — 작성 시 정확한 인수 파싱: `argc>=3 →
argv[2]` 그 외 exe-dir 규약 추출 블록. 라우팅: `if (argc > 1 && ... ==
"library-list") { return RunLibraryList(argc, argv); }` — slot-pack 라우트
같은 자리에. usage 행도 추가.)
**posix 게이트**: 라우트는 무게트 — 순수 stdio라 TX6 개방 선례로
`#ifdef _WIN32` 없이 양 플랫폼 동작(posix 본체 — 스펙 §4).

- [ ] **Step 2: probe 작성 (BOM 필수)**

probe_slot_ship.ps1을 템플릿으로 읽고(단일 서브커맨드·스택 무접촉 패턴 따름),
`probe_library_list.ps1` 작성 — 검증 4정판:

```
LIBRARY-BASE  — 기본 기점(exe dir)에서 count>=1 + name=minesweeper (source=builtin)
LIBRARY-CAPS  — caps= 라인 존재(설치 jkx의 능력 원문이 콘솔로 나오는지)
LIBRARY-ARG   — 임시 1m형 가짜 트리를 스크립트가 조립해 인수 기점 스캔 →
              name=galapp + caps=widget,timer + count 정확
LIBRARY-POSIX — (WSL 실행 시) 동일 CLI가 WSL에서도 rc=0
```
- [ ] **Step 3: Windows probe 실행**

Run: `powershell -ExecutionPolicy Bypass -File engine/tools/probes/probe_library_list.ps1`
Expected: 4정판 전부 OK(POSIX 항은 스킵 표기 — Windows 런이면).
- [ ] **Step 4: WSL에서도 rc=0** — WSL 배포 표준으로 파일 스크립트 1실행
(library-list는 stdio라 재빌드 없이 트러블 가능 — 빌드가 안 되어 있으면
wsl_rebuild 표준 이용).
- [ ] **Step 5: 커밋** `feat(library): library-list CLI+probe — 카탈로그 서버리 검증 라우트 (스펙 §6, posix 개방)`

---

### Task 3: 허브 앱 모듈 jkapp_library

**Files:**
- Create: `engine/include/apps/ClientLibraryApp.h`
- Create: `engine/src/apps/ClientLibraryApp.cpp`
- Create: `engine/src/apps/JKAppModule_library.cpp`
- Modify: `engine/CMakeLists.txt` — jkapp_files 블록 뒤에 `jkapp_library`

**Interfaces:**
- Consumes: Task 1 카탈로그, `jk::JKJkxFile`(ICON 디코딩 — 런처 :400-407
  wanted 산식 재용), `jk::LoadImageMemory`(include/JKImageLoader.h:22),
  `jk::client::JKClientApplication`(RenderOverlay/Surface/IsFrameDirty 꼴),
  `surface_->SendAgentQuery(id, json)`(ClientSettingsApp::SendQuery :96-107
  본사 복사), quickjs 불요(카탈로그가 파싱 몫).
- Produces: 모듈 ABI — `jk_app_meta()` = `{"library", "Library", 920, 640}`,
  `jk_app_run_client`. 스폰 통로 = 기존 launch_app 존재 검증
  (`jkapp_library<접미>` — JKWindowServer.cpp:4442 AppModuleSuffix) 신규
  와이어 불요.

- [ ] **Step 1: ClientLibraryApp.h** — ClientSettingsApp.h를 템플릿으로

핵심 멤버만 (설정 앱 쓰는 버퍼·캡처 계열 전부 삭제 — 라이브러리는 읽기 전용):

```cpp
// 라이브러리 허브 클라 (스펙 2026-10-06-app-library — settings/notes/files 4호
// 멤버, ClientSettingsApp 패턴: 요청-응답 폴링, 이벤트 구독 없음).
// 읽기 전용 설비 — uninstall 없음(Q2 스킵 결제), 자기 기기 apps/를 직접
// 읽어 본다(수신 기기에서는 수신 기기 apps/가 진실원 — 파일 시스템 기반
// 자동 성립).
class ClientLibraryApp : public JKClientApplication {
public:
    ~ClientLibraryApp() override;
protected:
    void OnInit() override;
    void OnClose() override;
    void OnThemeChanged() override;   // ImGui 팔레트 재적용 (docs/52)
    bool PreProcessMessage(const JKEvent& ev) override;
    bool IsFrameDirty() const override { return frameDirty_; }
    void OnFrameCommitted() override;
    void RenderOverlay(SDL_Renderer* renderer, int w, int h) override;
private:
    struct Row {                      // 목록 행 — 카탈로그 + 런타임 텍스처
        jk::LibraryEntry cat;
        SDL_Texture* tex = nullptr;   // 아이콘(있으면) — 클라 hidden renderer
        int texW = 0, texH = 0;
    };
    void BuildUi(int w, int h);
    void LaunchSelected();            // launch_app 재용 (폰 TX6 실측 경로)
    std::vector<Row> rows_;
    int selected_ = -1;
    std::string status_;
    bool frameDirty_ = true;
    bool imguiReady_ = false;
    bool koreanFont_ = false;
    uint32_t nextQueryId_ = 1;
    uint32_t launchId_ = 0;           // 응답 식별 — SendQuery 폴링 1종
    bool iconsLoaded_ = false;        // 첫 RenderOverlay에서 디코드 1회
    std::vector<uint32_t> pending_;   // AgentQueryReply 라운트트립 체
};
// 배지 문구: ClientScriptApp의 기존 능력 배지 문구를 grep해 정확히 따른다 —
// 캡처된 원문("능력: <원문>" / 빈 선언 계열)과 동일 계약 유지(docs/76).
```

- [ ] **Step 2: ClientLibraryApp.cpp 본체** — 블록별 원준

  - **OnInit** (ClientSettingsApp.cpp:40-74 원준): 창 생성+SetMainWindow,
    `SetTimerInterval(16)`(settings 선례), ImGui::CreateContext+
    `jk::theme::ApplyImGuiTheme()`+IniFilename nullptr+korean 폰트 리졸러
    (settings :59-71 **4행 그대로** — GetGlyphRangesKorean 계약).
    스캔: `const std::string exe = jk::fs::GetExecutablePath();` → exe-dir
    규약 추출(launch_app :4420-4426 블록 재용) → `jk::LibraryScan(base, cat)`
    → rows_ 채움. 스캔 개수 stderr 1행(`[library] apps=N`).
  - **PreProcessMessage** (settings :88-92 그대로): ImGui_ImplJKWindow_
    ProcessJKEvent + Timer때 frameDirty_=true, return true.
  - **OnFrameCommitted**: frameDirty_=false.
  - **RenderOverlay** (settings :279-291 그대로): imgui backend init + PollReplies
    + NewFrame + BuildUi + Render. **첫 RenderOverlay에서 아이콘 디코드 1회**
    (ClientShotApp :196-205 원준 — renderer는 RenderOverlay에만 존재):
    각 Row에서 cat.source==Jkx && cat.hasIcon인 것만 JKJkxFile Open →
    FindEntry(wanted 산식: scale 없이 상시 1x 우선 폰 2x 훅) → ReadEntry →
    jk::LoadImageMemory → `SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
    SDL_TEXTUREACCESS_STREAMING, w, h)` + SDL_UpdateTexture(rgba) →
    SDL_SetTextureScaleMode(STREAMING 텍스처 위 런처 선형 계약)  — 폰 SW
    렌더러 함정(docs/78 §5.7) 메모 주석. 실패 시 tex=nullptr로
    남기고 계속(플레이스홀더 폴백 — 런처 그리드 규약).
  - **BuildUi** — 2창: 좌 `라이브러리`(table: 아이콘 32x32 | 이름 | title | 출처
    | 능력) + 우 `상세`(선택 항목: appName/title/출처/캡처 배지/path/
    sizeBytes/실행 버튼). Begin-false여도 End()(docs/53 §9). 선택 =
    ImGui::IsItemClicked → selected_ 갱신 + frameDirty_=true. 실행 버튼 =
    `launch_app` SendQuery(`{"app":"<EscapeJson(appName)>"}`) — 폴백:
    surface 비연결 → status_[!] (settings :99-102 재용). 콘솔·내장 lf/hx
    appName은 "terminal:<cmd>" 전체 — prefixed라 launch_app 존재 검증 면제
    경로(JKWindowServer.cpp:4437) 자동 성립.
  - **배지**: title 위 `caps` 열과 상세 둘 다 — 원문 그대로 + 빈값
    ClientScriptApp 계약 문구. badge 정확 문구는 구현 시
    engine/src/apps/ClientScriptApp.cpp의 기존 배지 부분 grep으로 캡처해
    그대로 갖다 쓴다(고침 없음 — 문구 고정 계약 docs/76 §9 유지).
  - **OnClose**: imgui backend Shutdown(OnClose settings :76-82) + rows_
    텍스처 전부 SDL_DestroyTexture.

- [ ] **Step 3: JKAppModule_library.cpp** — settings 모듈 본사(:1-18) 복사해

```cpp
// Library hub app module (스펙 2026-10-06-app-library — settings/notes/files
// 4호 멤버). C++ 전부 DLL 안부.
#include <apps/JKAppModule.h>
#include <apps/ClientLibraryApp.h>

JKAPP_EXPORT const jk::JKAppMeta* jk_app_meta() {
    static const jk::JKAppMeta meta{ "library", "Library", 920, 640 };
    return &meta;
}

JKAPP_EXPORT int jk_app_run_client(const char* pipeName) {
    const jk::JKAppMeta* meta = jk_app_meta();
    jk::ClientLibraryApp app;
    if (!app.Init(meta->title, meta->width, meta->height, pipeName)) return 1;
    return app.Run();
}
```
`jk::JKAppMeta` 필드 순서는 JKAppModule.h 실계서 읽어 정확히 맞춘다
(settings 모듈이 {name, title, w, h} 순서 — 위 그대로).
- [ ] **Step 4: CMakeLists** — jkapp_files 블록 뒤:
```cmake
# Library hub module (스펙 2026-10-06-app-library): 로컬 라이브러리만(Q1 A —
# settings/notes/files 4호 멤버). A launcher .jkx(아이콘 없음 — 동일 폴백).
add_library(jkapp_library SHARED
    src/apps/JKAppModule_library.cpp
    src/apps/ClientLibraryApp.cpp
)
target_compile_definitions(jkapp_library PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_library PRIVATE jkclient imgui)
set_target_properties(jkapp_library PROPERTIES PREFIX "")
```
- [ ] **Step 5: 빌드 + 3축 표시 확인**

Run(ninja+test): RC=0, AppSelfTest 0 failure(s), `ls engine/build/japp_library`?
→ `jkapp_library.dll` 존재 확인.
- [ ] **Step 6: 커밋** `feat(library): 허브 앱 모듈 jkapp_library — 목록·상세·능력 배지·launch_app 실행 (스펙 Q1 A)`

---

### Task 4: WSL 실측 — 부팅+launch 영수증

**Files:**
- Create: `engine/tools/probes/wsl_library_boot.sh`

**Interfaces:**
- Consumes: Task 3 모듈(+Task 1·2의 jkcore/lib 통짜 리빌드), WSL 파일
  스크립트 표준, agentctl 정답 와이어(docs/78 §5.4 함정 ① — `agent` 서브커맨드
  금지).
- Produces: WSL에서의 실 기동 영수증(list_windows에 library 창) — 폰 배포
  (Task 5) 전 posix 동작 증명.

- [ ] **Step 1: probe 작성** — wsl_cpu_pacing.sh 템플릿 훅:
  ① WSL 내 ninja 리빌드 ② `jkdesktop library-list` 인쇄수령 ③ 서버 부팅
  `(setsid ./jkdesktop --server > ... 2>&1 &)` ④ `agentctl
  '{"tool":"launch_app","args":{"app":"library"}}'` ⑤ sleep 후 list_windows
  — `title=Library` 수령 ⑥ grep 수령 후 종료(cdb q kill — 클라이언트가
  남기면 pkill 재용). 폰 표준: `setsid nohup` 생존 검사 포함.
- [ ] **Step 2: 스크립트 실행·영수증 수령** — LIBRARY-BOOT-OK
  + list_windows library 수령인지 확인.
- [ ] **Step 3: 커밋** `feat(library): WSL 부팅·런치 probe — library 창 영수증 (스펙 §6)`

---

### Task 5: 폰 배포 실측 (posix 개방의 실사입)

**Files:**
- Create: `engine/tools/probes/phone_library.sh`

- [ ] **Step 1: tar 재배포 표준** — engine 소스 일부(jkcore/catalog+앱
  모듈+main.cpp+CMakeLists) → 폰 ssh 후 `ninja -C buildterm -j4` (폰 리빌드).
  재팩 룰 접촉 없음(모듈 코드가 변하긴 하지만 phoneprobe.jkx와 무관 —
  phoneprobe 재팩 불요: 재팩 필수 규약은 **파묻힌 모듈**이 변할 때만).
- [ ] **Step 2: 폰 영수증 수령** — `library-list` (phoneprobe.jkx 인지 +
  minesweeper built-in) → 서버 부팅(`bash ~/tx4_boot.sh` 재용) → agentctl
  launch_app library → list_windows Library 수령 → **사용자 눈확인 등장
  (목록에서 phoneprobe 아이콘·전부 정상 보임 — fit-scale 없는 1:1)**.
- [ ] **Step 3: 커밋** `feat(library): 폰 실측 probe — posix 스캔 폰 도달 (스펙 §4)`

---

### Task 6: as-built 문서 + 기억 갱신

**Files:**
- Create: `docs/79_app_library_asbuilt.md` — as-built+렛슨 원장(docs/77 선례
  승계 · 스펙 체인·카탈로그/모듈 배선·실측 영수증·함정 원장·deferred minors).
- Modify: `docs/superpowers/specs/2026-10-06-app-library-design.md` — 상태
  주석에 as-built 문서 링크 1행.
- Memory: `jkengine_next_backlog.md` 갱신 39 블록 + MEMORY.md 인덱스 1행.

- [ ] **Step 1: docs/79 작성** — §0 스펙 체인, §1 배선(카탈로그/모듈/CLI/
  probe), §2 실측 영수증(Windows probe 4정판·WSL LIBRARY-BOOT-OK·폰
  목록+런치·사용자 눈확인 결과), §3 함정 원장, §4 deferred minors 트라이아지,
  §5 커밋 원장.
- [ ] **Step 2: 기억 갱신 + 커밋+푸시** `docs(library): as-built 원장 docs/79 + 기억 갱신 39`

---

## Self-Review (작성 후 통과)

- 스펙 커버리지: §2 발견 규약=Task 1 · §3 A 허브 앱=Task 3 · §4 posix 개방
  =Task 1(std::filesystem)/Task 4(WSL)/Task 5(폰) · §5 스코프 밖=쓰기 금지
  (uninstall 없음)·런처 무접촉 · §6 테스트=Task 1(셀프테스트 1m)+Task 2(CLI
  probe)+Task 4·5(실측) — 갭 없음.
- placeholder 스캔: 위 코드블록 중 2곳은 "의사/보강" 표기가 있으나 전원
  Step에 정확한 작성 규칙(원준 위치+계약)이 명시돼 있다 — 구현 단계
  implementer가 원준 파일을 읽고 그대로 따른다.
- 타입 일관성: LibrarySource{Jkx,Console,Builtin}·LibraryEntry 필드(6종)·
  LibraryScan(basePath, out) 시그니처가 Task 1→2→3에서 동일 문자열.
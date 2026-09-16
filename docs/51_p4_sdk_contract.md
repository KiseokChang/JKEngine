# 45 — P4 SDK 계약 as-built (콘솔 앱 + 에이전트 쌍방)

- 날짜: 2026-09-16
- 스펙: docs/superpowers/specs/2026-09-16-p4-sdk-contract-design.md
- 플랜: docs/superpowers/plans/2026-09-16-p4-sdk-contract.md
- 커밋: a263bff(Task1) b8eb307(Task1b) 4e48404(Task2) f2b2136(Task3) f0f687a(Task4)

## As-built

### 콘솔 앱 kind (스펙 §3, Task 1)

- `apps/<name>/manifest.json` (name/cmd 필수, desc 선택) — 런처 셀로 등록.
- JSON 파싱: quickjs throwaway 런타임 (`JKTerminalConfig::Load` 패턴) —
  `JKDesktopShell::ScanConsoleApps()`. `.jkx` 이름 충돌 시 `.jkx` 우선(명시
  스킵 + 로그). 1MiB 상한.
- 스폰: `ShellHost::spawnConsole` 콜백 → `JKWindowServer::SpawnConsoleApp()` →
  `jkdesktop.exe terminal --cwd "apps\<name>" --shell "<cmd>"` (docs/44 CLI
  재사용). **cwd는 앱 폴더, cmd는 상대경로** — 인용 겹침 방지(453a327)를
  상대경로 규칙이 보장.
- 아이콘: `apps/<name>/icon@{1x,2x}.png` (출력 스케일 ≥1.5면 @2x) — 없으면
  placeholder 사각형. `jk::LoadImageFile` → `host_.makeTexture`.
- `ConsoleAppInfo(name, &cmd, &dir, &fingerprint)` — 런처 셀의 원본을
  에이전트 도구가 재사용하는 조회 인터페이스.

### trust 지문 (스펙 §3.4, Task 1b)

- `ConsoleAppFingerprint(cmd)` = bcrypt SHA-256, `"sha256:" + 64hex`
  (jktriggers `Sha256Hex`와 동일 형식). `jkdesktop_shell`이 bcrypt 링크.
- `EnsureTrustRecord()` — `state\trust.json` upsert. **지문이 이미 있으면
  파일을 건드리지 않는다**(읽기-수정-쓰기 경쟁 최소화) → 재시작 멱등 확인
  (ts 불변). **파손 스토어는 절대 덮어쓰지 않는다**(기록 보존 우선).
- ts는 steady_clock ms (jktriggers `NowMs`와 동일 — epoch 아님, 문서화).
- 확인: sdkprobe 지문 기록, sampletodo 추가 시 append, 재시작 무변경.

### run_console_app (스펙 §5, Task 2)

- 서버 도구: `run_console_app {name}` → `{"ok":true}`. **ask 기본 게이트** —
  close_window/trust_request와 같은 inline-approval 파이프라인 재사용.
  `AgentToolAllowed`: askCapable + 기본 Ask 추가. jkagentd broker는 기본
  deny(permissions.json에 `"run_console_app":"ask"`가 승인 행위 — close_window
  동일).
- **승인 시점에 매니페스트를 다시 읽는다** — 파킹된 cmd를 신뢰하지 않음.
  승인 대기 중 매니페스트가 바뀌면 최신 cmd가 스폰된다.
- jkagentd: `kToolsListJson` + `IsKnownTool` + `LoadPermissions` 기본 false +
  **MCP args 전달 분기 추가**(tools/call이 argsJson을 도구별로 재구성하는
  구조였다 — 분기 누락 시 `{"args":{}}`가 서버로 가서 `bad_name`. e2e에서
  포착 → 수정).
- e2e: agentctl(ask→`approval_unavailable` / allow→스폰) + MCP tools/call
  (`{"ok":true}` → 터미널 스폰) 양쪽 확인.

### jkctl (스펙 §4, Task 3)

- `jkctl notify "<msg>"` — `publish_event` topic `agent.notify` (jktriggers와
  동일 경로, jkapp_notify가 띄운다). 서버에 별도 notify 도구를 만들지 않았다.
- `jkctl agent '<json>'` — `QueryRaw` 통과 (agentctl 형식).
- `jkctl ask "<q>"` — `ollama launch claude --model <chat.json model> -- -p "<q>"`,
  CreateProcessW + 핸들 상속, 응답은 stdout 통과. **wmain + UTF-8 정규화** —
  docs/48 CP949 argv 레슨 적용 (std::system은 한글 프롬프트를 파손).
- 동기 원컷만 (스트리밍/세션 YAGNI). `--attach`는 미구현 — 파일 첨부 질의는
  프롬프트에 경로를 넣는 것으로 대체 (스펙 §4 예제에서 YAGNI로 내림).
- e2e: notify exit 0 / agent `{"ok":true,"pong":true}` / ask 한글 응답
  ("2", sampletodo 검토 요약).

### C 씨앗 (스펙 §6, Task 4)

- `engine/templates/console-app/` — manifest.json + README(규칙 + jkctl 사용법).
- `engine/apps/sampletodo/` — todo.txt 뷰어 + `jkctl ask` 에이전트 검토.
- `.cmd`는 **ASCII 전용** — cmd는 .bat을 OEM 코드페이지로 읽는다(docs/48
  레슨의 배치 파일 버전). UTF-8 todo.txt는 `type`으로 그대로 출력된다.
- `build_with_temp.sh`: `apps/*/` 디렉터리를 `build/apps/`로 동기화(.jkx 산출물
  과 같은 자리).

### lf 교체 (Task 5)

- `%APPDATA%\lf\lfrc` `cmd agent` → `jkctl ask` (직접 ollama 호출 임시해법
  → SDK 표준 경로). 모델은 `state\chat.json`의 model을 따른다.
- `probe_lf_ops.ps1`: jkctl 존재 + lfrc ask 규칙 검증 추가.

## 계획과 다른 점

1. **MCP args 전달 분기 누락** — 플랜은 "IsKnownTool+kToolsListJson 추가"로
   끝났지만 jkagentd는 tools/call에서 argsJson을 도구별로 재구성한다.
   전달 분기가 없어 `{"args":{}}`가 서버로 갔고(`bad_name`), e2e에서
   포착 후 추가했다. **교훈: 새 도구 추가 시 브로커의 4곳(list/known/
   permissions/args-rebuild)을 모두 고쳐야 한다.**
2. **경로 버그(`apps\` 누락)** — 스캔이 `basePath\<name>\manifest.json`을
   읽어 `err=2`. 절대경로 실패 지점 진단 로그로 1회 확인 후 수정.
3. **moved-from 로그** — `push_back(std::move(icon))` 후 이동된 객체에서
   필드를 찍었다(`dir=''`). `back()`에서 읽도록 수정.
4. **JS_IsArray 시그니처** — quickjs 빌드는 `JS_IsArray(val)` 1인자.
5. **notify 도구 없음** — 플랜은 "notify 도구"를 가정했으나 서버에는
   `open_notify`(센터 토글)만 있다. `publish_event` agent.notify 경로로
   해결 (jktriggers와 동일 — 별도 도구를 만들지 않는 것이 맞았다).
6. **.bat OEM 코드페이지** — 한글 배치 파일은 파싱이 깨진다. ASCII 전용 +
   데이터 파일(todo.txt)은 UTF-8 유지.

## DLL 계약 (문서화 — 현재 동작이 계약)

`jkapp_<name>.dll` 모듈은 docs/43 ShellHost 계약을 따른다:

- 빌드: `add_library(jkapp_<name> SHARED src/apps/JKAppModule_<name>.cpp)`
  + `target_compile_definitions(... JKAPP_MODULE_BUILD)` + 링크 `jkclient`
  (+ imgui 등) + `set_target_properties(... PREFIX "")`. CMakeLists의
  기존 모듈 블록이 템플릿.
- 스폰: 서버가 `jkdesktop.exe --client <name>` 스폰 → 클라가
  `jkapp_<name>.dll` 로드 → `CreateApp` 진입. 런처 셀 appName이 곧 모듈명.
- 컨테이너 없는 로컬 모듈은 `build/jkapp_<name>.dll`이 있으면 동작
  (빌드 스크립트가 `jkapp_*.dll`을 build로 복사).
- 파일 열기 등 특수 모듈은 `filedlg:<json>` 같은 appName 접두 관례
  (docs/48 — 접두는 shell-adjacent, 런처에 안 나옴).
- 이번 변경 없음 — 계약 고정은 코드 변경 불필요(현행 동작 기술).

## .jkx 계약 (문서화 — 기존)

- docs/21 컨테이너(ICON/MANIFEST 엔트리) + docs/37 trust(pack 소스 자기
  증명). 스캔은 `apps/*.jkx`, 스폰은 `--jkx <path>`.
- 이번 변경: 콘솔 kind와 이름 충돌 시 `.jkx` 우선(§3 규칙).

## C 후보 (풀 SDK로 가는 다음 단계)

이번에 심은 씨앗: `engine/templates/console-app`(수동 복사 설치),
`sampletodo`(살아있는 예제), `jkctl`(에이전트 원컷).

- 템플릿 생성기 — `jkctl init <name>`: 템플릿을 복사해 이름 치환
  **→ 2026-09-16 완료.** 템플릿에 실행 본체 `main.cmd` 추가(ASCII 전용
  레슨 준수, manifest `cmd`를 `main.cmd`로), `build_with_temp.sh`가
  `templates/console-app`을 런타임 루트로 동기화(`/.` 중첩 방지 복사).
  jkctl init은 `<cwd>\<name>\` 생성 + `"myapp"` 토큰 치환, 이름은
  `[A-Za-z0-9_-]{1,64}` 검증. probe_jkctl_init.ps1 10/10.
- 패키지 매니저 — 콘솔 앱 폴더의 zip 배포 + trust 지문 검증 설치
  **→ 2026-09-16 폴더 MVP 완료: `jkctl install <folder>`** — manifest.json의
  `"name"` 문자열 스캔 → `<exeDir>\apps\<name>\` 재귀 복사, 기존 설치 거부.
  스캔/지문은 서버 재시작 시 1회(EnsureTrustRecord — docs/51 §3.4 그대로).
  **→ 2026-09-17 zip 배포 완결: `jkctl pack <folder>` + `jkctl install
  <folder-or-.zip>`** — miniz 3.0.2 벤더링(`engine/third_party/miniz`,
  `miniz_export.h` 수동 스텁). pack은 deflate + '/' 구분자 통일, install은
  tmp 스테이징 → zip-slip 가드(절대경로/드라이브문자/'..' — **'\'도 구분자로
  검사(리뷰 MAJOR: `"..\..\x"` raw 항목이 스테이징 밖에 적재되는 우회를
  실증 후 봉쇄**) → copy 설치(rename은 AV 스캔 공유 위반 실측). 설치 시
  trust 지문 선기록 — manifest cmd의 SHA-256 = 서버 ConsoleAppFingerprint와
  동일 규약, 스플라이스는 배열 머리 삽입(빈 배열/공백 경계 처리; 쉼표 누락
  시 fail-closed 서버가 절대 못 고치는 실측 결함 픽스 포함). 매니페스트
  상한 1MiB = 서버 kMaxManifestBytes 동일. probe_jkctl_init.ps1 17/17.
  주의: 선기록 source는 "user" — zip 파생 지문이지만 현행 trust는 표시
  이상의 역할이 없어(run_console_app은 승인 시점 재조회) 안전 실패 설계.
- 샘플 라인업 — Python/Rust/Go 예제 + jkctl agent 실전 예제
  **→ 2026-09-17 완료: `engine/samples/`** — py-todo(즉시 실행, stdlib),
  rust-todo/go-todo(소스 + 첫 실행 1회 빌드, 툴체인 부재 시 안내),
  agent-notify(jkctl notify/agent/ask 3종 실전 데모). 공통 계약: 무인자 =
  todo.txt 목록, 인자 = jkctl ask 우선순위 정리. .bat 본체 ASCII 전용
  (docs/48). 배포는 `jkctl pack samples/<name>`.
- `--attach` — jkctl ask 파일 첨부(스펙 §4 예제에 있었으나 미구현) (잔여)
- 콘솔 앱 → `.jkx` 승격 도구(매니페스트→컨테이너 변환) (잔여)

## 검증 요약

- 스캔: sdkprobe/sampletodo 매니페스트 → 런처 등록(서버 로그) ✓
- 지문: 신규 기록 + 재시작 멱등 + 다중 앱 append ✓
- 도구: agentctl ask(`approval_unavailable`)/allow(스폰) + MCP tools/call ✓
- jkctl: notify/agent/ask 세 경로 e2e ✓
- sampletodo: 목록 경로(type) + ask 경로(한글 LLM 응답) ✓
- 런처 셀 클릭 스폰: 사용자 확인 대상 (최종 리뷰 항목)
## 부록 — worktree 빌드 워크플로 (build_wt.sh)

worktree 세션의 빌드는 커밋된 build_with_temp.sh로 부족하다:

- `SRC`가 본 트리 경로로 박혀 있다 — worktree 경로로 트윈 스크립트 필요
- **third_party/cef는 gitignored** — worktree 소스 복사본에 없어
  `jkapp_browser missing`이 뜬다. 본 트리에서 `cp -R third_party/cef/.`
  (tracked README 위 내용 겹침 — `/.`로 중첩 방지)
- temp를 본 트리와 **별도 디렉토리**(`temp_jkdesktop_wt`)로 — 상호 파괴 방지
- 빌드 산출 복사는 exe/dll뿐 아니라 **runtime DLL 141개**도 필요
  (`CopyRuntimeDlls` 산출 — 없으면 SDL2_mixer 로드 실패로 즉사)

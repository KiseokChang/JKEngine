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
- 패키지 매니저 — 콘솔 앱 폴더의 zip 배포 + trust 지문 검증 설치
- 샘플 라인업 — Python/Rust/Go 예제 + jkctl agent 실전 예제
- `--attach` — jkctl ask 파일 첨부(스펙 §4 예제에 있었으나 미구현)
- 콘솔 앱 → `.jkx` 승격 도구(매니페스트→컨테이너 변환)

## 검증 요약

- 스캔: sdkprobe/sampletodo 매니페스트 → 런처 등록(서버 로그) ✓
- 지문: 신규 기록 + 재시작 멱등 + 다중 앱 append ✓
- 도구: agentctl ask(`approval_unavailable`)/allow(스폰) + MCP tools/call ✓
- jkctl: notify/agent/ask 세 경로 e2e ✓
- sampletodo: 목록 경로(type) + ask 경로(한글 LLM 응답) ✓
- 런처 셀 클릭 스폰: 사용자 확인 대상 (최종 리뷰 항목)
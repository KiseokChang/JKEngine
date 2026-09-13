# 44 — Phase A: TUI 흡수 as-built (lf/helix)

- 날짜: 2026-09-13
- 스펙: docs/superpowers/specs/2026-09-13-killer-app-absorption-design.md
- 플랜: docs/superpowers/plans/2026-09-13-phase-a-tui-absorption.md

## As-built

### 흡수 결과

- **lf r42**, **helix 25.07.1** — `engine/build/apps-bin/{lf,helix}/`에 설치
  (스크립트 `engine/scripts/install_lf_helix.ps1`, git 밖 바이너리).
- **터미널 CLI**: `jkdesktop.exe terminal [--shell <cmdline>] [--cwd <dir>]`
  — terminal.json의 shell을 CLI로 오버라이드. lf/helix가 터미널 창 안에서 실행된다.
  사용자 GUI 검증 완료 (lf 창에서 마우스/키 동작 + `q` 종료까지).
- **런처 관례**: appName `"terminal:<cmdline>"` → 서버가 `jkdesktop.exe terminal
  --shell <cmdline>`을 스폰 (JKWindowServer::SpawnClient). 데스크탑 셸 fallback
  셀로 lf/helix가 등록된다. 상대경로는 jkdesktop cwd(engine/build) 기준.
- **lfrc** (`%APPDATA%\lf\lfrc`, git 밖):
  - 파일 `o` → helix, 디렉토리 `o` → 새 엔진 터미널(--cwd 그 폴더), `T` → 현재
    폴더에 터미널
  - **개조 커밋 1**: `A` → 선택 항목을 LLM CLI에 전달
    (`ollama launch claude --model glm-5.3-flash:cloud -- -p ...` — jkchat의
    claude_wrapper 관례와 동일)
- 프로브: `engine/tools/probes/probe_lf_ops.ps1` — lfrc 필수 명령 존재 확인, PASS.

### 실행 이력

- 워크트리(`.claude/worktrees/phase-a-tui`)에서 개발 → main 머지(b3e3955).
  테마 세션(P2 phase 1, JKTheme 토큰 리팩터)과 텍스트 무충돌, 병합 검증 빌드
  254/254 통과.
- 계획 대비 수정 3건:
  1. helix zip이 최상위 폴더 하나에 담겨 와서 **플래튼 단계 추가** (hx.exe가
     runtime/ 옆에 있어야 자동 탐색).
  2. SpawnClient 관례 코드의 `string + size_t` 연산자 오류 → `substr`로 수정
     (플랜 코드의 연산자 우선순위 버그, 컴파일에서 잡힘).
  3. CEF 바이너리 배포판이 gitignored라 워크트리 빌드에서 `jkapp_browser` 타깃이
     정의되지 않는 문제 → 빌드 스크립트가 메인 체크아웃에서 CEF를 복사하도록 수정.
     (CMakeLists의 CEF 게이트(:502)는 EXISTS 검사인데 jkx 패킹(:799)은 무조건
     DEPENDS — CEF 미보유 환경에서 빌드가 깨진다는 것도 여기서 확인.)

## C 후보 적립

- **런처 전용 아이콘 아트** — lf/helix 셀이 현재 tetris fallback 아트를 씀.
  아이콘 2개 생성 + JKDesktopShell 매핑 추가면 끝, 포크 포팅 불필요 (폴리싱 항목).
- **터미널 안에 갇힌 GUI 감각** — 파일 아이콘/썸네일 프리뷰, GUI 다이얼로그.
  lf를 써보고 체감되면 부분 포크 포팅(C) 후보로 승격.
- **helix 시작 폴더** — 런처에서 띄운 helix는 cwd 기본값으로 시작. 홈 디렉토리
  시작이 자연스러울 듯 (런처에서 `--cwd %USERPROFILE%` 관례 추가 시 해결, 코드
  변경 불필요).

## P4 갭

- **터미널 shell 설정은 전역 단일값** (terminal.json `shell`) — 창별 shell은 CLI
  arg로만 가능. 창별 설정 모델이 SDK 계약에 필요할 수 있음.
- **콘솔 앱 흡수는 `terminal:` 문자열 관례** — 정식 계약이 되려면 appName
  네임스페이스(예: `terminal:` 스킴)를 문서화하고, `--shell`에 전달되는 cmdline의
  신뢰 경계(스폰 권한)를 정의해야 함.
- **PTY 시작 디렉토리** — `--cwd`는 프로세스 cwd를 바꾸는 것; PTY 자식의 시작
  환경(env 상속) 규칙은 정의되지 않음.
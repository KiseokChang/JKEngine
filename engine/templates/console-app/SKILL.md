---
name: console-app-sampletodo
description: sampletodo 콘솔 앱을 에이전트로 구동/조작하는 방법 (jkdesktop P4 SDK). run_console_app으로 스폰하고 jkctl로 상호작용한다. Use when the user asks to run, inspect, or delegate work to the sampletodo console app.
---

# sampletodo 콘솔 앱

sampletodo는 P4 SDK 콘솔 앱(docs/51)의 살아있는 예제다 — `apps/sampletodo/`
의 `manifest.json` + `sampletodo.cmd` 배치 본체. GUI 앱이 아니라 터미널 창에서
돌아가는 배치 스크립트라, 에이전트가 할 일은 "스폰 → 출력 읽기 → (선택) 앱 쪽
jkctl로 되물어보기" 3단계다.

## 구동

서버 도구 `run_console_app`:

```json
{"tool": "run_console_app", "args": {"name": "sampletodo"}}
```

- 성공 응답 `{"ok":true}` — 서버가 `jkdesktop.exe terminal --cwd
  "apps\sampletodo" --shell "sampletodo.cmd"` 로 스폰한다(cwd = 앱 폴더).
- **승인 게이트**: 기본 `ask` — permissions.json의 `run_console_app` 키로
  조정(파킹 → 다른 연결(채팅/알림 표면)의 승인 → 스폰). 승인 시점에
  매니페스트를 다시 읽으므로 파킹 중 cmd가 바뀌면 최신 cmd가 실행된다.
- 앱이 설치돼 있는지 먼저 확인하려면 `installed_list {}` — 콘솔 앱과 .jkx가
  kind와 함께 나온다.

## 앱 동작 계약 (무엇을 기대할 수 있는가)

`sampletodo.cmd`는 인자 1개로 두 가지 동작만 한다 — 이것이 앱 전체 계약이다:

- **무인자** — `todo.txt`(앱 폴더)를 `type`으로 출력. 항목은 한 줄에 하나.
  파일이 없으면 안내 문구.
- **인자 있음** — `jkctl.exe ask "Review this todo list and set priorities:
  <인자>"` 로 로컬 LLM 질의를 날리고 응답을 stdout에 통과한다.
  (chat.json의 model을 따른다 — docs/51 §4)

## 출력 확인

스폰된 터미널 창의 출력은 다음 경로로 관찰한다:

1. **`terminal.output` 이벤트** — 터미널 앱이 250ms 코얼레싱으로 publish하는
   토픽(`{"data":{"text":...}}`, VT 시퀀스 제거됨). `read_events`로 배출하거나
   구독 채널로 받는다.
2. **`terminal_exec {"command":"...", "timeoutSec":N}`** — 독립 ConPTY 세션에서
   명령을 실행하고 출력을 즉시 돌려준다. sampletodo 자체의 재출력(무인자
   `type`)이나 앱 폴더 상태 확인에 쓴다.
3. **`read_log {"lines":N, "match":"..."}`** — jkdesktop 서버 로그 테일.
   스폰 실패/게이트 거부 사유가 여기 남는다.

## 앱 쪽에서 에이전트로 되물어보기 (jkctl)

앱 본체가 에이전트 채널로 말을 거는 3종 — 콘솔 앱 안에서 실행된다
(`<엔진 빌드 폴더>\jkctl.exe`):

- `jkctl notify "메시지"` — 데스크탑 알림(agent.notify 경로)
- `jkctl agent '<json>'` — 서버 도구 원 요청(agentctl 형식 통과). 예:
  `agent '{"tool":"list_windows","args":{}}'`
- `jkctl ask "질문"` — 로컬 LLM 질의, 응답은 stdout

에이전트가 sampletodo에 작업을 시키고 싶으면: run_console_app으로 띄운 뒤
필요한 인자를 알 수 없으므로(콘솔 앱은 도구를 등록하지 않는다 — 앱 도구 허브는
창 클라 전용, docs/58) terminal_exec로 앱 cmd를 인자와 함께 재실행하거나,
앱 안의 jkctl ask 응답을 terminal.output/read_events로 수신한다.

## 종료

프로세스 종료는 앱 자체 명령으로(배치는 끝나면 터미널이 남는다). 에이전트가
강제 종료하지 않는다 — 터미널 창을 닫는 일은 `close_window`(별도 권한 게이트)
의 영역이다.

## 설치/등록 규칙 (앱 제작자용)

- `manifest.json`의 `name`은 폴더명과 같게, `cmd`는 상대경로(앱 폴더가 cwd).
- `.cmd` 본체는 **ASCII 전용** — cmd는 .bat을 OEM 코드페이지(CP949)로 읽는다
  (docs/48 레슨). 한글 데이터는 별도 UTF-8 파일(todo.txt)로 분리.
- 스폰 cmd의 SHA-256 지문이 `state\trust.json`에 기록된다(docs/37, source
  "user").
- 같은 이름의 `.jkx` 컨테이너가 있으면 `.jkx`가 우선한다.
- 이 템플릿 폴더를 복사하거나 `jkctl init <name>`으로 스캐폴드한다.

## 배치 (이 파일의 설치 위치)

이 SKILL.md는 claude skill(문서형 래퍼) 샘플이다. 에이전트가 읽으려면:

- 채팅 LLM worker cwd(`state\chat.json`의 `directory`)의
  `.claude\skills\console-app-sampletodo\SKILL.md`, 또는
- 유저 레벨 `~\.claude\skills\console-app-sampletodo\SKILL.md`

배치 절차/근거는 docs/58 앱 도구 허브 as-built 참조.
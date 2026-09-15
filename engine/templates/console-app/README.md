# 콘솔 앱 템플릿 (P4 SDK)

이 폴더를 복사해 `<엔진 빌드 폴더>\apps\<name>\`에 넣으면 런처에 앱으로
등록된다 (엔진 재시작 시 스캔).

## 규칙

- `manifest.json`의 `name`은 폴더명과 같게, `cmd`는 터미널에서 실행할 명령줄.
  앱 폴더가 작업 디렉토리 — 상대경로를 사용한다.
- 콘솔 앱 본체는 아무 언어든 가능 (exe, py, cmd, …)
- 에이전트 호출: `<엔진 빌드 폴더>\jkctl.exe notify "메시지"` / `ask "질문"`
  - `notify` — 데스크탑 알림 (jktriggers와 같은 agent.notify 경로)
  - `agent '<json>'` — 서버 도구 원 요청 (agentctl 형식 통과)
  - `ask "<질문>"` — 로컬 LLM 질의, 응답이 stdout으로 나온다
- 아이콘(선택): `icon@1x.png`(16px), `icon@2x.png`(32px) — 없으면 placeholder
- 스폰 cmd는 설치 시 SHA-256 지문으로 `state\trust.json`에 기록된다
  (docs/37 스킴, source "user")
- 같은 이름의 `.jkx` 컨테이너가 있으면 `.jkx`가 우선한다

## 예제

살아있는 예제는 `engine/apps/sampletodo/` — 배치 파일 + `jkctl ask` 조합.
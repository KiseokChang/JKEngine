# agent-notify

P4 SDK 샘플 콘솔 앱 — jkctl 에이전트 채널 3종 실전 예제 (본체는 순수 batch).

| 데모 | jkctl 호출 | 경로 |
|---|---|---|
| notify | `jkctl notify "<msg>"` | 단발 알림 — 채팅 창(jkchat)에 표시, 승인 불필요 |
| ask | `jkctl ask "<question>"` | LLM 질의 — 이 터미널에 스트리밍 응답 |
| agent | `jkctl agent '{"tool":...,"args":{...}}'` | 서버 도구 직접 호출 — 권한 매트릭스(allow/ask/deny)를 따른다 |

`run_console_app` 같은 ask-게이트 도구를 agent로 호출하면 승인 요청이
채팅 창으로 가고, 콘솔은 승인/거부 결과를 받아 끝난다.

## 설치

데스크탑 빌드 루트에서:

```
jkctl install "I:\progwork\JKENGINE\engine\samples\agent-notify"
```

런처에서 클릭하거나 에이전트가 `run_console_app name=agent-notify`로
스폰한다. 채팅 창이 떠 있어야 notify가 화면에 보인다(구독자 무결).
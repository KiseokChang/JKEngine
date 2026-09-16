# samples — P4 SDK 콘솔 앱 샘플 라인업 (docs/51 C 후보)

`apps/sampletodo/`(살아있는 예제)와 `templates/console-app/`(jkctl init
스캐폴드)와 함께 배포용 샘플 4종. 전부 manifest.json 계약(docs/51 §3)을
따른다 — `jkctl install`로 데스크탑 빌드 루트의 `apps/`에 설치한다.

| 샘플 | 언어 | 특징 |
|---|---|---|
| py-todo | Python 3 (stdlib) | 설치 즉시 실행 — 목록 뷰 + `jkctl ask` |
| rust-todo | Rust (std 전용) | 소스 + 첫 실행 1회 `cargo build` |
| go-todo | Go (stdlib) | 소스 + 첫 실행 1회 `go build` |
| agent-notify | batch | jkctl notify/agent/ask 에이전트 채널 3종 실전 데모 |

공통 계약: 인자 없음 = `todo.txt` 목록 출력, 인자 있음 = 목록을 에이전트에
넘겨 우선순위 정리 (agent-notify는 채널 데모 전용). .bat 본체는 ASCII 전용
(docs/48 CP949 레슨).

## zip 배포

```
jkctl pack samples/py-todo     # → <cwd>\py-todo.zip (zipPath는 현재 디렉터리 기준)
jkctl install py-todo.zip      # 받는 쪽: tmp 스테이징 → apps/ + trust 선기록
```

참고: jkctl.exe는 libwinpthread-1.dll을 동반하므로 배포는 빌드 루트 째로
가정한다(단일 exe 복사로는 실행되지 않는다).
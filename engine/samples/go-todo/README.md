# go-todo

P4 SDK 샘플 콘솔 앱 — Go (표준 라이브러리만 사용).

- 인자 없음: `todo.txt` 목록 출력
- 인자 있음: 목록을 `jkctl ask`로 에이전트에 넘겨 우선순위 정리 요청
- 첫 실행 시 `go build` 1회 빌드 (go 부재 시 안내 메시지)

## 설치

데스크탑 빌드 루트에서:

```
jkctl install "I:\progwork\JKENGINE\engine\samples\go-todo"
```

툴체인 없이 배포하려면 미리 빌드한 `go-todo.exe`를 포함해
`jkctl pack samples/go-todo`로 포장한다.
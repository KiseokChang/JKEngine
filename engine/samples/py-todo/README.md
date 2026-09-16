# py-todo

P4 SDK 샘플 콘솔 앱 — Python 3, 표준 라이브러리만 사용 (실행 가능).

- 인자 없음: `todo.txt` 목록 출력 (UTF-8, 한 줄에 한 항목)
- 인자 있음: 목록을 `jkctl ask`로 에이전트에 넘겨 우선순위 정리 요청

## 설치

데스크탑 빌드 루트에서:

```
jkctl install "I:\progwork\JKENGINE\engine\samples\py-todo"
```

설치 후 데스크탑을 재시작하면 런처에 `py-todo` 셀이 등록된다. 패키지(zip)로
배포하려면 `jkctl pack samples/py-todo` → 생성된 `py-todo.zip`을 배포하고
받는 쪽에서 `jkctl install py-todo.zip`.
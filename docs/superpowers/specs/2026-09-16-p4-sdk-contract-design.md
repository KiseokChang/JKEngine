# P4 SDK 계약 — 콘솔 앱 정식화 + 에이전트 쌍방 (설계)

- 날짜: 2026-09-16
- 상태: 승인 (브레인스토밍 합의)
- 선행: docs/43 (engine/desktop split P1, §5 P4 연결), docs/44 (Phase A TUI 흡수 as-built — C 후보/P4 갭 레저)

## 1. 문제와 목표

엔진 밖에서 누구나(외부 개발자, 본인, 에이전트) 앱을 만들어 올리고, 에이전트가
그 앱들과 쌍방으로 동작하게 만드는 계약 집합. 궁극에는 풀 SDK 프레임워크(템플릿
생성기, 패키지 매니저, 샘플 앱 라인업)까지 가되, 이 설계는 그 **1단계 마일스톤**:
콘솔 스킴 실구현 + 계약 문서 + C의 씨앗(템플릿·샘플 앱).

**고객(우선순위)**: 서드파티/오픈소스 개발자 + 에이전트(jkagentd). 쌍방 계약.

## 2. 앱 계약 3층위

| 등급 | 형식 | 정식화 정도 | 고객 |
|---|---|---|---|
| 1. 콘솔 스킴 | `apps/<name>/` + JSON 매니페스트 | **이번에 실구현** | 외부 개발자, 에이전트 |
| 2. DLL 모듈 | `jkapp_<name>.dll` | 문서화만 (현재 동작이 사실상 계약 — docs/43 ShellHost) | 파워 개발자 |
| 3. 스크립트 | `.jkx` 컨테이너 | 문서화만 (docs/21/37 기존) | 경량 앱 |

근거: 콘솔 스킴은 언어 무관(Rust/Go/Python/무엇이든)으로 서드파티 진입장벽이 0이고,
lf/helix 흡수(docs/44)로 검증된 길. 표현력은 터미널로 제한 — GUI가 필요한
앱은 DLL 계약으로, 웹 앱은 OSR 브라우저(B 경로)로.

## 3. 콘솔 앱 kind (실구현)

### 3.1 폴더 구조

```
apps/<name>/
  manifest.json    # 필수: name, cmd / 선택: desc
  icon@1x.png      # 아이콘 (선택 — 없으면 placeholder)
  icon@2x.png
  <바이너리/스크립트>  # 콘솔 앱 본체 (아무 언어)
```

### 3.2 매니페스트 (JSON)

```json
{
  "name": "mytodo",
  "cmd": "python mytodo.py",
  "desc": "할 일 목록 뷰어"
}
```

- `name`: 런처 라벨 + 식별자 (폴더명과 일치 권장)
- `cmd`: 터미널에서 실행할 명령줄. 앱 폴더가 작업 디렉토리 — 상대경로 사용.
- `desc`: 런처 툴팁용 (선택)

### 3.3 스폰 흐름

- 런처 셀 클릭 → 서버가 `jkdesktop.exe terminal --shell <cmd>`를 앱 폴더를
  cwd로 스폰 (기존 `terminal:` 관례 위에 매니페스트 계층 얹음)
- 기존 lf/helix 셀은 문자열 관례(`terminal:<cmdline>`) 유지 — 하위호환
- 새 매니페스트 앱이 정식 계약. 이후 셀들을 점진 이관 가능

### 3.4 신뢰/권한

기존 trust 모델(docs/37) 스킴 재사용 — 새 권한 모델을 만들지 않는다:

- 로컬 `apps/` 폴더 설치 앱은 신뢰 기본
- 스폰은 기존 서버 승인 경로를 따름
- `cmd` 필드의 신뢰 경계: 매니페스트가 설치될 때 검토 대상 (trust ledger에
  스폰 cmd 지문 기록 — docs/37 스킴 재사용)

## 4. 앱→에이전트: `jkctl` CLI

콘솔 앱이 엔진 에이전트를 원컷으로 호출하는 CLI. jkchat/jktriggers/jkagentd가
이미 쓰는 control-client 패턴(window-server pipe)의 CLI 형태.

```
jkctl ask "선택한 파일 요약해줘" --attach <path>   # 에이전트 질의 → 응답 stdout
```

> **as-built (2026-09-17)**: `--attach` 구현 완료 — 텍스트만(NUL 거부),
> basename 헤더 블록 결합, 16KiB 컷(UTF-8 연속바이트 후퇴) + 명령행 30000
> 가드, UTF-8 실패 시 CP949 재인코딩. 개행은 `\n`/`\r` 리터럴 인코딩(런치
> 체인이 명령행을 첫 개행에서 절단하는 실측 교훈). probe_jkctl_init 11-13.
jkctl notify "메시지"                             # 데스크탑 알림
```

- **동기 원컷** — 요청→응답을 stdout으로. 스트리밍/세션/상태는 YAGNI
  (그런 것은 jkchat이 할 일)
- **권한**: jkagentd 기존 allow/ask/deny 파이프라인(docs/31) 재사용 —
  `jkctl`은 채널이 다른 새 control client일 뿐
- **첫 실소비자**: lf의 `A` 키 — lfrc의 직접 LLM CLI 호출(임시해법)을
  `jkctl`로 교체. SDK 문서 예제로도 사용

## 5. 에이전트→앱: jkagentd 도구 확장

- 기존 7종 도구(docs/29)에 **`run_console_app`** 1종 추가
- 입력: 앱 name + 선택 인자; 출력: 스폰된 터미널 surfaceId
- permission 게이트(ask 기본) 재사용, rate limiter(docs/38) 스킴 적용
- 모니터링(상태 폴링)은 YAGNI — 스폰만

## 6. C 씨앗 (풀 프레임워크 이전 발담금)

1. **템플릿 폴더** `engine/templates/console-app/` — manifest.json 예시 +
   README. docs/43 §5 P4 연결 "앱 템플릿 스니펫"의 최소형
2. **샘플 앱 1개** — Python 콘솔 스크립트. SDK 검증 겸 살아있는 문서 소재.
   jkctl notify를 호출하는 e2e 예제

## 7. 검증

- 프로브: 매니페스트 스캔 → 런처 등록 → 스폰을 서버 로그로 검증
  (기존 engine/tools/probes 패턴)
- 샘플 앱: Python 스크립트가 jkctl notify를 부르는 e2e 1개
- 최종: 본인이 런처에서 샘플 앱 클릭 확인

## 8. 비목표 (YAGNI)

- DLL 계약의 변경 — 현재 동작 문서화만 (새 코드 없음)
- .jkx 계약 변경 없음
- 에이전트 상태/세션 관리, 앱 모니터링, 패키지 매니저, 템플릿 생성기
- 다중 언어 런타임 묶어주기 (샘플은 Python이지만 계약은 "콘솔 앱"만 요구)

## 9. P4 갭 흡수 (docs/44에서)

이 설계가 docs/44 P4 갭을 처리하는 방식:

- `terminal:` 스킴 신뢰 경계 → §3.4 (매니페스트 cmd 지문, trust 재사용)
- 창별 shell 설정 모델 → 이번에 다루지 않음 (CLI arg로 유지, 필요 시 확장)
- PTY 시작 환경 → 앱 폴더를 cwd로 스폰하는 것으로 최소 규정
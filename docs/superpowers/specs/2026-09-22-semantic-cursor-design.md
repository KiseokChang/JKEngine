# 의미 커서(semantic cursor) 설계 — 앱 선언형 조작 계약

**상태**: 확정(2026-09-22, 사용자와 대화 합의 — roadmap 갱신 9 항목 6-⑭)
**첫 소비자**: 지뢰찾기(자유 커서) / 두 번째 소비자(테트리스, 네이티브 커서)는 추상화 검증용으로만
**핵심 한 문장**: 앱은 "내 공간의 지도(선언)"와 "행위의 의미(act 콜백)"만 제공하고, 커서·이동·승인·하이라이트는 플랫폼이 소유한다.

## 1. 배경

폰 실전 개선 4건(docs/57 §13)으로 `window_move/resize`·`send_input`·승인 하이라이트 창 링이 갖춰졌으나,
지뢰찾기류 게임 앱은 의미 단위 조작이 없다 — send_input(논리 데스크탑 좌표의 점)만으로는
"3행 5열 깃발"이 성립하지 않고, 승인 배너도 창 단위 링만 보여준다.
이 스펙은 **앱이 자기 의미 커서를 선언**하면 폰(LLM)이 행·열 인덱스로 조작하고, 승인 대상이
셀 단위로 하이라이트되는 계약을 만든다.

선례: filedlg 음성 내비게이션(navigate/list/choose — 다이얼로그 안 커서+상대 이동+상태 읽기),
TerminalHangulInput(순수 로직 상태 머신+뷰 배선 분리+2층 프로브), 앱 도구 허브(docs/58 —
앱이 자기 도구를 AgentToolRegister로 선언), files_access(파킹 재실행형 승인 시점 재검증).

## 2. 계약 — 앱 선언서

앱이 `AgentToolRegister`(와이어 type 22, 이미 런타임 재전송 가능)에 `cursor` 블록을 추가한다.
**선언은 정적 설정이 아니라 "현재 공간 지식의 보고"다** — 생산자만 다를 뿐 항상 데이터다
(네이티브 앱=런치 시 1회, 스크립트 앱=레이아웃 변경 시마다 재선언, 서버 해석기는 단일 — 최신 선언만 유지).

```jsonc
{
  "app": "minesweeper",
  "tools": [ /* 앱 자체 도구 (act/snapshot — §4) */ ],
  "cursor": {
    "type": "cell-grid",            // 조작 단위 = 격자 칸
    "coordSpace": "client",         // v1 유일 좌표계 — 앱 그리기 좌표
    "origin": {"x": 12, "y": 44},   // (0,0)칸의 client 좌표
    "cellW": 16, "cellH": 16,
    "rows": 9, "cols": 9,
    "cursorOwner": "platform",      // free-cursor: 커서 상태를 서버가 소유
    "act": { "kinds": ["reveal", "flag", "question", "clear"], "gate": "ask" }
  }
}
```

| 필드 | 뜻 |
|---|---|
| `type: cell-grid` | "나를 조작할 땐 칸 단위로 말해줘" |
| `coordSpace: client` | origin/cell 크기의 좌표계. **client 단일**(v1) — screen=서버 렌더 전유물, window=레이어 rect(서버 소유), 앱이 screen을 아는 것은 원칙 위반. 변환 사슬 client→layer(×Scale)→물리(×outputScale)는 서버가 소유(DrawApprovalHighlights :6183-6189 기존 산식) |
| `cursorOwner: platform` | 커서 상태를 플랫폼이 들고 앱은 좌표만 받는다. `"app"`이면 네이티브 커서(테트리스류) — move도 앱 구현 |
| `act.kinds` | 앱이 수용하는 행위 어휘 — tools/list inputSchema **enum**으로 LLM 자동 노출(워크숍 API 카탈로그 부재 사건의 재발 방지) |
| `act.gate: ask` | 행위는 승인 파킹. 이동·읽기는 무승인 |

앱이 직접 구현하는 것 = **선언 1블록 + act 콜백 1함수**(+선택 snapshot 커스텀). 유효 전이·클램프·에코·
승인·하이라이트는 전부 플랫폼 몫 — 앱별 서버/도구 코드 0줄.

## 3. 도구 3종 (플랫폼이 `<app>.move`/`<app>.read` 구현, `<app>.act`는 앱 릴레이)

이름 규약은 앱 도구 허브 네임스페이스(docs/58, `vplayer.seek` 선례)를 따른다. 폰/브리지는 기존
generic 릴레이로 무수정 노출. LLM 인자는 **행·열 인덱스** — 좌표계가 LLM에 노출되지 않는다.

### `<app>.move` — 게이트 **allow**(무해: 판 상태 불변)
- 인자: `{"to_row": r, "to_col": c}` 또는 `{"dr": n, "dc": n}` 또는 `{"steps": [{dr,dc}...]}`(다중 스텝, 상한 32)
- 규약: 경계 클램프는 `cursorOwner: platform`일 때 서버(격자 밖 불가), `app`이면 앱이 클램프
- **다중 스텝은 첫 경계에서 중단, 실제 도달 위치를 에코**("끝까지"의 의미를 고정)
- 응답: `{"ok":true, "row": r, "col": c}` — 상태 직렬화(에코 규약)

### `<app>.read` — 게이트 **allow**
- **보드 내용 직렬화 필수** — 열린 칸의 숫자·깃발·커서·status(playing/lost/won). 지뢰찾기는 숫자 추론이
  게임이므로 요약만으론 불충분(9×9=9줄 텍스트, 토큰 부담 0)
- 커서는 플랫폼이, 보드는 앱의 `snapshot` app_tool 응답으로 조립
- 패배 후엔 지뢰 전체 공개(지뢰찾기 표준)를 직렬화 — LLM 사후 설명의 진실원

### `<app>.act` — 게이트 **ask** (파킹 → 승인 시점 원 요청 재실행, files_access 선례)
- 인자: `{"kind": <enum>, "row": r, "col": c}` — **의미만, 원시 클릭 아님**
- 원시 클릭(좌/우/드래그/컨텍스트 메뉴)은 send_input 고유 영역 — "click만 보이면 승인자가
  무슨 입력인지 모름"(docs/62 레슨). kind는 승인 배너를 `minesweeper.flag at (3,5)`로 말하게 한다
- **플러드 필(flood fill)은 별도 kind 아님** — 연쇄 개방은 앱 내부 게임 규칙(선택 행위가 아님).
  act 에코에 `opened: N`으로 확산 보고, 상세는 read 재동기. 코드(chording)는 앱이 `kind: "chord"`
  선언하면 수용(선택)
- **파킹 시점에 셀 rect 고정** — 승인 시점에 재선언(스크립트 앱)으로 격자가 바뀌었으면 재검증 후 어긋나면 거부
- 응답 에코: `{"ok":true, "kind":…, "row":…, "col":…, "opened":…, "status":…}` — boom/won도 에코로 즉시 전달

## 4. 상태 전이 모델 + 커서 생명주기

```
상태 S = 커서 {row, col}          (플랫폼 소유 — free-cursor)
전이  = move(steps) → S'           (무조건 가능)
      = act(kind)   → 판 변화       (승인 게이트 + 유효 전이 검사)
에코  = 상태 직렬화;  불법 전이 = bad_state 에러 응답(에코 포함)
```

- 구현은 **순수 로직 상태 머신 클래스 + 뷰 배선 분리 + 로직/뷰 2층 프로브**(TerminalHangulInput 선례)
- **act 후 커서 유지** — 폰 세션과 독립, **창 닫힘에만 소멸**(허브 자동 제거 룰), 게임 리셋=정의 전이(좌상단)
- 표시(하이라이트)만 페이드 가능(vplayer OSD 선례) — 상태는 남는다

## 5. 하이라이트 렌더 — 서버가 매 프레임 계산

- 커서 상태+최신 선언으로 셀 rect를 **매 프레임 산출**해 컴포지트 프레임 위에 직접 그림
  (DrawApprovalHighlights와 동일 경로) — 앱 표면 무접촉: 앱 크래시/무응답에도 표시 생존,
  window_move 추종 자동(실시간 계산), 비상호작용(클릭 통과)
- **2계층**: 커서 셀(지속, 액센트색) / 승인 대상 셀(파킹 시, 호박 링+배너 — 기존 오버레이 격침)
- 가려진 창도 상층에 비침 = 기존 승인 링 정책 일관(불투명 표현 필요 시 가시성 검사 추가는 백로그)

## 6. 정적/동적 — 선언은 늘 데이터

네이티브 앱=런치 시 1회 정적 선언. 스크립트 앱(워크숍)은 런타임 재선언(`agent.declareCursor()`,
기존 채널 재전송) — **말로 만든 앱이 커서 조작을 즉시 획득**(폰 "할일 판"류와 합류). 재선언 홍수
방지: rate limiter 예산 포함+마지막 선언만 유효.

## 7. 일반화 (3계층)

- **L0 플랫폼 계약**: 선언 해석 → move/read 도구 자동 구현+하이라이트 렌더+승인 배너 자동
- **L1 앱**: 격자 선언 1블록+act 콜백 1함수(read 커스텀 선택)
- **L2 커서 소유권**: free-cursor(플랫폼)/native-cursor(앱, 테트리스류)를 선언으로 분기
- **절제선**: 추상화 고정은 소비자 2개(지뢰찾기+테트리스) 통과 후 — 첫 소비자로 API를 얼리면
  두 번째 앱의 변형을 못 봄. P4 SDK(.jkx manifest) 승격은 그 후.

## 8. v1 범위

- 지뢰찾기만 (자유 커서 첫 소비자). 테트리스(native-cursor)는 2차 검증 런그.
- act kinds: reveal/flag/question/clear. reset(new_game)은 상태 머신의 리셋 전이로 포함.
- 승인 배너 문구 확장(종래 "<app>.<tool>" → "<app>.<kind> at (r,c)").
- 앱별 act allow 선택지(승인 피로 완화)는 permissions.json 운용으로 커버(기본 ask 유지).

## 9. 검증

- probe_semantic_cursor 신설: 선언 파싱/move 클램프·스텝·에코/act 파킹→승인→판 변화/read 직렬화/
  bad_state/reset ×2 연속 PASS
- 라이브 반영 후 폰 실전: "지뢰찾기 켜 줘 → 가운데 열어 줘 → 거기 깃발" (사용자 눈확인)
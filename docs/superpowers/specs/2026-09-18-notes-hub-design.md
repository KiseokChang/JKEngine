# 노트 앱 (notes hub) 설계 — 2026-09-18

## 0. 원천과 결정

- docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md §7 후보 앱
  "노트 — 마진 코멘트 허브 + 백로그 보드"(티어 3).
- **앱 선택 (사용자 재량 위임 2026-09-18 "이거끝나면 다음 단계꺼지 재량껏
  진행해 줘요~")**: 설정 허브(docs/54) 다음 남은 §7 후보. 눈확인 대기 항목들은
  사용자 부재로 진행 불가 → 자율 피처로 노트 선택.
- **접근 방식 (설정 허브 검증 패턴 계승)**: agentmgr/docs/53의 도구-소비
  패턴 + 신규 도구 최소(2종), state 파일은 `.bak 1세대` 관례, 코어 펌프 훅
  소비, events_list 카탈로그 즉시 등록 — docs/54 §9 레슨 전부 적용.

## 1. 문제 정의

에이전트 활동(도구 호출, 창 조작, 캡처)과 사용자의 관찰을 남기는 곳이 없다:
"이 창 왜 열렸지?" 같은 맥락 코멘트가 채팅 로그에 묻히고, "다음에 해볼 것"이
사라진다. 플랫폼 스펙의 철학 — **"떠나는 이유가 백로그다"** — 사용자가
에이전트 데스크탑에서 발견한 것을 적어두면 에이전트가 읽고 다시 표시하는
순환이 필요하다. 수신처(허브)가 없으면 이 루프가 성립하지 않는다.

## 2. 아키텍처

### 2.1 앱·표면

- `jkapp_notes` — ImGui 클라 1창 2탭: **코멘트 | 백로그**. jkapp_agentmgr
  패턴(서버 도구 agentctl 소비) 그대로. 760x520.
- 진입: 런처 셀 + 팔레트 `/notes` + jkchat 슬래시(팔레트 패리티).

### 2.2 신규 서버 도구 2종

- **`notes_read {}`** → `{"ok":true,"notes":[{id,text,win,win_title,ts,src}...],
  "backlog":[{id,title,state,ts,src}...]}` — state/notes.json 전체 반환.
  정렬은 ts 오름차순(파일 순서) — GUI가 역순 표시.
- **`notes_write {op, ...}`** → op 화이트리스트:
  | op | args | 적용 |
  |---|---|---|
  | `add_note` | text(1..512), win(0=범용) | notes.push (src=요청자 소스) |
  | `add_item` | title(1..128), state(0..2) | backlog.push |
  | `move_item` | id, state(0..2) | backlog 상태 전이 |
  | `del` | kind("note"/"item"), id | 삭제 |
  그 외 bad_op / bad_text / bad_state. 응답 `{"ok":true,"id":<신규 id>}`.
- **kPermMatrix +2행**: `{"notes_read","none","allow"},{"notes_write","none",
  "allow"}` — 노트는 저위험 사용자 데이터(브로커 캡 첨부가 있다면 그것으로
  충분). 스팸 벡터는 receipts 감사 + rate limiter(docs/38)가 커버.
- **src 필드**: 서버가 발화자 연결을 "agent"|"user"로 분류한다 — 요청자가
  control-only 연결이면 "agent", 창 연결이면 "user"(GUI는 창 연결,
  jkagentd/jkctl은 control-only). GUI에 [에이전트]/[사용자] 뱃지.

### 2.3 state/notes.json

- `{"notes":[{id,text,win,ts,src}],"backlog":[{id,title,state,ts,src}],"next":N}`
  — next = id 채번기(재시작 후에도 증가). `.bak 1세대`(settings/trust 관례).
  256KiB 캡 초과 쓰기 실패(write_failed — docs/53 3d 선례).
- 노트의 win = 타깃 창 id(0=범용 코멘트). 창이 닫히면 win은 남긴다(역참조는
  읽기 시점 win_title로 — 창 소멸 후에도 코멘트가 남는다; "마진"의 의미는
  고정이 아니라 링크).
- GUI 백업/복원 책임은 프로브가.

### 2.4 이벤트

- 에이전트가 노트를 추가하면 서버가 `agent.notify` 이벤트를 방송한다 —
  알림 센터(docs/33)로 에이전트 노트가 자연 유입(신규 토픽 아님, 소비자 0).
  토픽 `agent.notify`, `data.title="[노트]", data.body=<text>`. **카탈로그
  변경 없음**(기존 토픽 소비) — docs/54 레슨(e) 준수. 사용자 GUI 추가는
  이벤트 없음(내 행위라 방송 불필요).

## 3. GUI

- **코멘트 탭**: 목록(최신순, `[에이전트|사용자] @창제목 · mm-dd hh:mm` 헤더 +
  본문, 200개 캡) + 하단 입력(InputText+엔터=추가, win은 현재 선택 0 유지) +
  창 피커(콤보: list_windows → "범용" + 창 목록, 선택 후 추가 시 win 부여) +
  삭제 버튼(행별).
- **백로그 탭**: 3열(대기0/진행1/완료2) 컬럼 레이아웃, 항목 카드 + 좌우 이동
  버튼 + 삭제, 하단 추가 입력.
- 새로고침: OnInit 1회 + 수동 새로고침 버튼(폴링 없음 — 설정 허브와 동일
  요청-응답 모델. 에이전트 노트 수신은 알림 센터가 담당).

## 4. YAGNI (명시적 배제)

- 마크다운/파일 첨부/태그/검색 — 텍스트만.
- kanban 드래그앤드롭 — 버튼 이동만.
- 실시간 동기(다른 노트 창) — 단일 창 가정.
- 브라우저/터미널에서의 노트 인라인(마진의 원의미) — 후속(§7 후보 "기존 앱
  확장"). 본 앱은 허브 MVP: win 필드가 링크의 씨앗.

## 5. 리스크

| 위험 | 완화 |
|---|---|
| 에이전트 노트 스팸 | rate limiter + receipts 감사 + GUI 행별 삭제 |
| notes.json 손상 | .bak 1세대 + 파싱 실패 시 빈 상태 시작(정직 로그) |
| 256KiB 폭증 | 캡 초과 쓰기 거부 + 오래된 항목 GUI 삭제 유도 |
| control-only 분류 오탈 | jkagentd 연결은 control-only — jkchat도 control-only(노트 읽기만 하면 무해) |

## 6. 검증

- probe_notes.ps1 12체크: read 셰이프 / add_note / add_note with win /
  agent 소스 뱃지(control-only 연결) / add_item / move_item / del / bad_op /
  bad_text / 256KiB 캡 / agent.notify 이벤트 캡처(Wait-EventLine) / state
  백업복원 생명주기. 2연속 ALL PASS.
- 회귀: selftest / jkagentd selftest / probe_agent_notify / probe_settings /
  probe_agentmgr / vpt12 / 팔레트.

## 7. opus 최종리뷰 (상임)

전체 diff + 프로브 결과. 특별 점검 항목: (a) notes_write가 스크립트 토픽
스푸핑 경로와 무관한지(publish_event 예약과의 정합), (b) id 채번 경합(단일
서버 스레드라 순차), (c) 256KiB 캡 경계, (d) agent.notify 방송이 notify
센터 스팸을 만들지 않는지(코멘트 추가 빈도).
# 55. 노트 허브 (notes hub) — as-built

- 스펙: `docs/superpowers/specs/2026-09-18-notes-hub-design.md`
- 플랜: `docs/superpowers/plans/2026-09-18-notes-hub.md`
- 구현 커밋: (본 문서 작성 시점 HEAD) — 1 커밋(서버+GUI+프로브 동봉)
- 원천: docs/32 §7 후보 앱 "노트 — 마진 코멘트 허브 + 백로그 보드"(티어 3)
- 진행 방식: 설정 허브(docs/54) 동일 사이클 — 사용자 재량 위임(자율 피처,
  사용자 취침 중)

## 1. 구성

| 레이어 | 내용 |
|---|---|
| 서버 도구 | `notes_read {}` / `notes_write {op,...}` — JKWindowServer.cpp 디스패치 체인 (read_receipts 뒤, launch_chat 앞) |
| state | `state/notes.json` `{"notes":[{id,text,win,ts,src}],"backlog":[{id,title,state,ts,src}],"next":N}` — 서버 유일 쓰기자 |
| GUI | `jkapp_notes` (760×520 chromeless, 2탭 코멘트|백로그) + `apps/notes.jkx` + 팔레트 `/notes` |
| jkagentd | tools/list 2종 + IsKnownTool/LoadPermissions + args-rebuild(op/text/win/state/id) + selftest 체크 6 |
| 이벤트 | 에이전트 add_note/add_item 성공 → `agent.notify` 방송(title="[노트] 코멘트/백로그", body=앞 128자) — 기존 토픽, 카탈로그 변경 없음 |

## 2. 도구 계약

- **notes_read**: 봉투 `{ok,notes:[{id,text,win,ts,src}...],backlog:[{id,title,state,ts,src}...]}` — ts는 파일엔 epoch ms, 응답은 초 절단(read_receipts 2레벨 규약). 파일 없음/손상 → 빈 배열 + 정직 로그(`notes.json unreadable — starting empty`).
- **notes_write** op 화이트리스트: `add_note`(text 1..512, win 0=범용), `add_item`(text 1..128, state 0..2 생략 시 0), `move_item`(id>0, state 0..2), `del`(id>0 — notes/backlog 양쪽에서 id 매칭 삭제). 그 외 `bad_op`/`bad_text`/`bad_state`/`bad_id`/`id_not_found`/`notes_unreadable`/`write_failed`.
- **id 채번기**: `next` 필드(서버 유일 채번 — 단일 디스패치 스레드라 순차). 파일에 next가 없으면 max(id)+1로 복구(구형/손상 파일).
- **src 분류**: 요청자 연결이 control-only → `agent`(jkagentd/jkctl/agentctl), 창 연결 → `user`(GUI). 스펙 §2.2 그대로.
- **kPermMatrix**: `{"notes_read","none","allow"},{"notes_write","none","allow"}` — 저위험 사용자 데이터. 스팸 벡터는 rate limiter(docs/38) + receipts 감사.

## 3. 상태 파일 안전장치 (docs/54 레슨 직행)

1. **행 스캔은 배열 경계로 한정** — NotesArrayRows는 `find('{')` 대신 다음 비공백 문자가 `{`일 때만 행을 파싱한다. 초기 구현은 배열 끝 `]`를 건너뛰어 backlog 행을 notes로 흡수했고(빈 text 유령 행) **RMW 재직렬화가 그 오염을 파일에 굳혔다**(1회 쓰기 오염 → 영구 지속). 이것이 본 세션 최대 결함 — RMW형 저장은 "읽기 오염 = 쓰기 오염"이라 행 경계가 반드시 정확해야 한다.
2. **256KiB 캡**: WriteNotesFile이 완성 문자열 > 262144이면 거부(`write_failed`) — docs/53 3d 선례.
3. **.bak 1세대**: 기존 파일을 rename으로 .bak화 후 새로 씀(북마크/trust 관례).
4. **rename 실패 시 절단 없이 중단**: rename(브로커/프로브가 읽기 잠금 시 실패)을 검사하지 않으면 이어지는 `fopen "wb"`가 원본을 절단한다 — receipts opus M5의 동일 벡터를 선제 봉쇄. **첫 쓰기(원본 부재)는 fopen rb 프로브로 통과** — 신규 케이스에서 rename 실패=파일 부재라 쓰기가 영구 실패하는 역함정이 생겼다가 픽스(세션 실측: 첫 쓰기 전부 write_failed).
5. **부분 쓰기 복구**: fwrite 크기 불일치 → 새 파일 remove + .bak 복원.
6. **JsonEsc 중괄호 이스케이프**: `{`→`{`, `}`→`}`(JSON 등가) — 노트 본문에 중괄호가 있어도 행 경계 스캔이 안전. 전역 적용이라 receipts/permissions 등 모든 JSON 출력에 함께 적용(유효 JSON 변화 없음).

## 4. GUI (jkapp_notes)

- ClientAgentMgrApp/ClientSettingsApp 패턴: 요청-응답 폴링(PollAgentReply), 이벤트 구독 없음, SetRoot 다크 배경, 상단 24pt 서버 크롬 우회(y>=30), 16ms 타이머, malgun 폰트, Begin-false여도 End() 무조건(docs/53 잔여).
- **코멘트 탭**: 최신순(파일 순서 역순) 목록 — 헤더 `[에이전트|사용자] @창제목 · mm-dd hh:mm` + 본문 TextWrapped + 행별 삭제. 창 링크는 list_windows 스냅샷으로 역참조하고 소멸 창은 `@닫힌창#id`로 표기(스펙 §2.3: 링크는 남긴다).
- **백로그 탭**: 3열(대기0/진행1/완료2) 카드 + 좌/우 이동 + 삭제, 하단 InputText+Enter 추가(대기로).
- 쓰기 성공 시 즉시 notes_read 재조회(자기 쓰기 반영) + 수동 새로고침 버튼.
- 창 피커 콤보: "범용" + list_windows 목록(선택 시 add_note에 win 부여).

## 5. 검증

- **probe_notes.ps1 15체크 ×2연통 ALL PASS**: read 셰이프 / add_note / add_note+win(읽기 역참조) / src 뱃지(agent) / add_item / move_item 라운드트립 / del+소멸 확인 / id_not_found(del+move) / bad_op / bad_text(600>512) / .bak 생성 / **256KiB 캡**(260K→280K 시드 — **250K 시드는 캡 미만이라 쓰기가 정상 성공하는 프로브 버그 2회 수정**)+파일 무절단 / agent.notify 이벤트 캡처(RedirectStandardOutput 폴링) / GUI spawn(launch_app notes → list_windows "Notes").
- 회귀 전부 GREEN: `jkdesktop test`(0), jkagentd selftest(0), probe_agent_notify, probe_settings(16체크 — flip 게이트 포함), probe_agentmgr, probe_agent_shot, probe_agent_palette, vpt11, vpt12×2.

## 6. 프로브 레슨 (신규)

- **PS5.1 값-공백 인용 함정(신규)**: `& $exe agentctl $escaped`는 값에 **공백이 있으면** PS5.1의 네이티브 인수 조립이 임베디드 `"`를 이스케이프하지 않아 argv 재파싱이 JSON을 자른다(→ bad_request). probe_settings가 평생 무사했던 건 보내는 값(key/value)이 전부 공백 없는 토큰이었기 때문. 픽스: `ProcessStartInfo.Arguments`에 원시 명령행을 직접 조립(`agentctl "{escaped}"`) — CRT의 `\"`→`"` 규약에 정확히 맡긴다. probe_settings 등 기존 프로브는 건드리지 않았다(동작 검증됨).
- agentctl stdout의 `[theme] preset` 로더 행(docs/52 오염)은 Invoke-Agentctl에서 정규식으로 제거.

## 7. 알려진 한계 / 후속

- del은 id만으로 매칭(kind 인자는 스펙에 있었으나 구현에서 id 단독으로 단순화 — tools/list 설명 "del(kind by id)"과의 표기 엇갈림은 무해).
- 실시간 동기 없음(단일 창 가정, 스펙 §4 YAGNI) — 다른 소스가 노트를 추가하면 수동 새로고침/알림 센터로 인지.
- 256KiB 도달 시 GUI는 write_failed 상태줄로 알림 — 오래된 항목 삭제 유지.
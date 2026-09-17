# 56. 파일 허브 (file hub) as-built — 2026-09-18

스펙: `docs/superpowers/specs/2026-09-18-file-hub-design.md` / 플랜:
`docs/superpowers/plans/2026-09-18-file-hub.md`. 노트 허브(docs/55) 검증
패턴 계승 — 서버 도구 3종 + 소스 분리 게이트 + 파킹 + jkapp_files 3패널 GUI.

## 1. 서버 도구 3종 (JKWindowServer.cpp)

- **files_list {path}** → `{ok, entries:[{name,kind,size,mtime}], capped}` —
  FindFirstFileA 단일 디렉터리 열거, dir 우선 + 이름 asc(바이트 순),
  512행 캡(초과 시 `capped:1`), `.`/`..` 제외, 숨김 포함(MVP 단순).
  mtime은 FILETIME→epoch 초 변환(receipts 초 절단 규약 동일).
- **files_read {path, maxBytes?}** → `{ok,text,truncated,binary,size}` —
  64KiB 상한(maxBytes ≤ 65536, 0/음수 = 기본), 첫 4KiB NUL 스니핑 →
  `binary:1`(text 공란; UTF-16도 이진 분류 — 스펙 §6(e) 허수를 한계로 기록).
  원시 UTF-8 바이트(JsonEsc) — CP949 변환은 MVP 밖.
- **files_audit {limit?}** → `{ok,rows:[{ts,tool,ok,path}]}` — receipts.jsonl
  꼬리 256KiB 행 스캔에서 `files_*` 행만 필터(read_receipts 선례 복용:
  memchr 행 분해 + AgentJson 행 파서 + raw ts 스캔 + `"ok":true` raw find,
  ok는 "0"/"1" 문자열, 최신 우선, limit 기본 50/상한 200).
  **receipts는 브로커(jkagentd)만 쓴다 — GUI 직접 호출은 기록되지 않는다**:
  스펙 §4 "GUI가 자기 접근도 감사에 부풀림" 위험의 자연 완화 + 감사의
  의미(에이전트 접근 시각화)와 정확히 일치.

## 2. 게이트 "server(files)" — 소스 분리

- kPermMatrix +3행: files_list/files_read = "server(files)", files_audit =
  "none"/allow. agent_permissions 표시: 파일값 없음 = "ask"(에이전트 쪽
  최악값 표기).
- **FilesPermRaw(tool)** — permissions.json 원문값("missing"/"allow"/
  "ask"/"deny"). AgentToolAllowed는 없음과 명시 allow 구분 불가(기본
  Allow 열화)라 원문 직독. AgentToolAllowed askCapable에도 files 2종 편입
  (값 일관성 — 이 스위치는 파일 도구에서 호출되지 않음).
- 분기: deny → permission_denied(**window 연결도** — 사용자가 영구 거부한
  행위) / allow → 전 소스 무승인 / 없음·ask → window 연결 무승인,
  control-only 파킹. 소스 판정 = `client.IsControlOnly()`(notes src 선례).
- 파킹 구독자 검사는 **control-only만**(docs/54 opus M3 — 코어가 모든
  ImGui 클라를 구독시켜 전체 구독자 검사는 공허).

## 3. 파킹 — kind "files_access"

- PendingApproval +filesTool/filesPath/filesMaxBytes(kind 전용 페이로드
  관례 — name 재용용 금지 계승). approval_request 이벤트에 `kind` 필드
  (permission_set 선례) + title=경로(JsonEsc — 백슬래시 2배 확장이라
  버퍼 1024).
- **승인 = 원 요청 재실행**: 해소 시점에 FilesPermRaw deny 재검사 +
  ValidFilePath 재검증 + op 재실행 → 결과를 원 요청자 queryId로 답신
  (run_console_app의 승인 시점 재조회 선례 — 파킹 대기 중 permissions.json이
  바뀌면 최신 게이트가 강제된다). 거부 → denied_by_user.
- jkchat 스트립: kind "files_access" → `[파일 요청] files_list → <path>`
  (ShowApproval 재사용 — target 없음).

## 4. 경로 검증 (ValidFilePath)

절대 드라이브 형식(`X:\` 또는 `X:/` — **슬래시 수용**: CRT 인수 인용기가
닫는 인용 옆 백슬래시 런을 절반으로 깎는 함정 때문에 프로브/수동 검증은
전부 슬라시 경로 — 서버가 양쪽 구분자 수용하므로 공백 없음), 길이 3..260,
`\\`/`//` 이중 구분자 거부(UNC), `..` 구성요소 거부, 구성요소 내 `:` 거부
(ADS), 와일드카드/리다이렉트 문자(`*?"<>|`)는 op 내 재거부. 8.3 짧은 이름은
수용(FindFirstFileA 실명 확장 — 한계).

## 5. GUI — jkapp_files (3패널, 860×560)

- 좌: 경로 InputText(Enter=이동) + [상위][이동] + 목록(클릭 dir=진입,
  file=미리보기, 더블클릭 무시) + 512행 캡 표기 + 새로고침.
- 우: 미리보기 헤더(이름 · size / 이진 / 잘림) + TextWrapped 본문(이진=
  안내문). binary/truncated/capped는 int 직렬화 — GetInt로 읽는다(계약
  통일).
- 하단: 에이전트 파일 접근(files_audit) 최근 행 — `mm-dd hh:mm list|read
  경로 [거부]` + 수동 새로고침(폴링 없음 — notes/settings 선례).
- 진입: 팔레트 `/files` + launch_app. 사용자 소스 무승인 — permissions.json
  빈 상태에서 부팅 목록이 바로 뜬다(소스 분리의 사용자 얼굴 검증).

## 6. 발견·픽스된 기존 결함 — FindFileDataA 패킹

서버 TU의 수기 WIN32_FIND_DATAA 복제체(FindFileDataA)에 `unsigned long
long` FILETIME 필드 → 구조체 패딩 4바이트 → **cFileName이 실제 API 레이아웃
(오프셋 44)보다 4 뒤에서 읽혀 이름이 4바이트 밀렸다**("jkagentd"→"entd").
files_list가 최초 포착; settings_read의 layout_*.json 열거가 같은 결함을
안고 있었다(프로브는 레이아웃 존재만 검사해 미포착). 픽스 = `#pragma
pack(push,4)`(desktop TU의 JkxFindData는 `unsigned long[2]`로 자연 팩 —
올바른 선례). **레슨: windows.h 없이 Win32 구조체를 손으로 선언할 때
alignof를 의심하라 — FILETIME 멤버는 DWORD 정렬이다.**

## 7. 검증

- probe_files.ps1 12체크 ×2연속 ALL PASS: list 셰이프(dirs-first+capped)/
  서브디렉터리/not_found/경로 거부 3종(상대·UNC·`..`)/read 텍스트/잘림/
  이진/missing/**ask 파킹(agent-events 이벤트 캡처 + 승인 + 생존 요청자가
  재실행 결과 수신)**/deny 전소스/audit 행(필터+최신순+path)/GUI spawn.
- 수동 실측: 승인(allow) 시 재실행 결과가 원 요청자 도달, 거부 시
  denied_by_user, 60초 만료 스캔 approval_timeout — 전부 확인.
- 회귀: jkagentd selftest 0 / jkdesktop test 0 / notify / notes / settings /
  agentmgr / shot / ratelimit / palette / vpt12 전부 PASS.
- 프로브 경로 규약: **슬래시 경로 사용** — CRT 인수 인용기가 닫는 escaped
  quote 직전의 백슬래시 런을 절반으로 깎아 역슬래시 경로 JSON이
  변조된다(레슨 계열 확장: docs/55 (c)는 값-공백, 이번은 백슬래시 런).
  GUI는 파이프 JSON이라 역슬래시 그대로 안전.

## 8. opus 최종리뷰

(판정 후 기록)
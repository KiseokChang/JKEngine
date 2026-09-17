# 파일 매니저 (file hub) 설계 — 2026-09-18

## 0. 원천과 결정

- docs/superpowers/specs/2026-09-09-desktop-agent-platform-design.md §7 후보 앱
  "파일 매니저 — files 권한 등급의 사용자 얼굴 — 에이전트 파일 접근의 시각화"
  (티어 3, 마지막 실체 후보 — 티어 2/3 나머지는 전부 완결).
- **앱 선택 (사용자 확정 2026-09-18)**: 노트 허브(docs/55) 후 사용자가
  "파일 매니져 좋네요"로 직접 승인.
- **핵심 정의(사용자 의도 대화 계승)**: 이 앱은 ①사용자의 파일 브라우저
  (탐색기식 — 사용자 자신의 접근이므로 무승인) + ②**에이전트 파일 접근의
  승인·감사 얼굴** 두 얼굴이다. 노트의 레슨 — 두 얼굴을 하나의 앱에 묶되
  각 탭의 소유자가 누군지 스펙에 명시한다.
- 접근 방식: agentmgr/docs/53 + 노트(docs/55) 검증 패턴 계승 — 서버 도구
  최소 3종, state 파일은 receipts 재사용(신규 state 파일 없음), 코어 펌프
  훅 소비(앱은 구독 없음), jkagentd 4곳 등록.

## 1. 문제 정의

에이전트가 파일을 읽으려면 현재 방법이 **터미널 우회**뿐(terminal_exec) —
도구 계약과 감사 로그 밖의 뒷문이다. files 등급은 플랫폼 §6.5의 마지막
빈 칸이고, 승인 파이프라인(docs/31), receipts(docs/29), flip 게이트(docs/54)
부품은 전부 있다. 파일 매니저는 (a) 파일 접근을 도구로 전면화해 뒷문을 닫고
(b) 사용자에게 "무엇이 언제 누구에게 읽혔나"를 보여주는 수신처가 된다.

## 2. 아키텍처

### 2.1 앱·표면

- `jkapp_files` — ImGui 클라 1창 3패널: 좌측 브라우저 + 우측 미리보기 +
  하단 에이전트 접근 로그. 860×560 chromeless. 진입: 팔레트 `/files` +
  launch_app.

### 2.2 신규 서버 도구 3종 (파일 콘텐츠 도구 최초 도입)

- **`files_list {path}`** → `{ok, entries:[{name,kind:"dir"|"file",size,
  mtime}...]}` — 디렉터리 목록(dir 우선, 이름 asc, 상한 512행). `path`는
  **절대 경로 필수**(X:\... 형식; UNC `\\`, 상대 경로, `..` 거부 —
  bad_path). 존재하지 않으면 not_found.
- **`files_read {path, maxBytes?}`** → `{ok,text,truncated,size,binary}` —
  텍스트 미리보기. 상한 64KiB(기본; maxBytes ≤ 65536), 첫 4KiB에 NUL 바이트
  있으면 `binary:true`(text 공란) — 이진 파일은 바이트 덤프하지 않는다.
  인코딩은 원시 UTF-8 바이트(JsonEsc) — CP949 변환은 MVP 밖.
- **`files_audit {limit?}`** → `{ok,rows:[{ts,tool,ok,path}...]}` —
  receipts.jsonl 꼬리 256KiB에서 `files_*` 도구 호출 행만 필터(읽기 전용
  스캔, PruneReceipts/read_receipts 선례). 에이전트 접근 시각화의 원천 —
  신규 감사 파일 없음(receipts가 단일 감사원이라는 설계 유지).
- **kPermMatrix +3행**: `{"files_list","server(files)","none"}`,
  `{"files_read","server(files)","none"}`, `{"files_audit","none","allow"}`
  — audit은 감사 열람이라 저위험 allow.
- **게이트 "server(files)" 신규 태그**: `AgentToolAllowed`는 파일값을
  반환하고, 도구 분기가 요청자 소스를 본다 —
  - **window 연결(사용자) = allow** (승인 없음 — 자기 파일)
  - **control-only(에이전트) = ask**: 파킹(kind `files_access`) —
    close_window/trust_request 선례 파킹 기계 재사용, jkchat 승인 스트립
    신설(`[파일 요청] tool → path`). 승인 → 원 요청자에 실제 응답, 거부 →
    permission_denied.
  - permissions.json 값이 **명시 "allow"**면 에이전트도 무승인(사용자가
    영구 허용한 행위), **"deny"**면 전 경로 permission_denied, 없음/ask는
    위 기본. askCapable에 files_list/files_read 편입(파일값 "ask"가
    Allow로 열화되지 않게 — docs/54 M2 선례).
  - **주의(단일 진실원)**: 서버 도구 분기는 `FilesPermRaw(tool)`로
    permissions.json 원문값을 직접 읽어 Allow/Ask를 구분한다(없음과 명시
    allow의 구분이 AgentToolAllowed로는 불가 — 기본값이 Allow라).
- **경로 안전(공통 검증기)**: 절대+드라이브 형식(`X:\`), 길이 ≤ 260,
  UNC/상대/빈 경로 거부, `\\`와 `\..\`·`/../` 거부. 결과는 receipts에
  args.path로 자동 감사(파킹 성사분도).

### 2.3 GUI

- **브라우저(좌)**: 현재 경로 경로표시(편집 가능 InputText) + `[상위]`+
  `[새로고침]` + 목록(dirs 먼저, 클릭=진입/파일=미리보기) + 항목 size/mtime.
- **미리보기(우)**: 파일 선택 시 files_read — 텍스트(64KiB, 잘림 표기) +
  헤더(이름/size). 이진 파일은 "이진 파일(미리보기 없음)".
- **에이전트 접근(하)**: files_audit 최근 행 — `mm-dd hh:mm  tool  경로
  [ok|거부]`. 수동 새로고침(폴링 없음 — notes/settings 선례).
- Query: List, Read, Audit — pending 3종.

### 2.4 이벤트

- 없음. 파일 접근은 receipts가 단일 감사원(GUI는 files_audit로 소비) —
  신규 토픽/카탈로그 변경 없음(docs/54 레슨(e) 준수). 알림은 승인 스트립이
  담당.

## 3. YAGNI (명시적 배제)

- **쓰기 도구(files_write/move/delete)** — 에이전트 파일 쓰기는 승인 모델
  설계(§8 장치 계층과 통합)가 선행인 고위험 표면. MVP는 읽기 전용 + 감사.
  사용자 GUI의 자기 파일 조작(이름변경/삭제)도 MVP 밖 — 브라우저+감사가
  본질.
- 텍스트 편집/다중 선택/검색/정렬 토글 — MVP는 이름순 고정.
- 네트워크 경로/드라이브 마운트 열거 — 로컬 드라이브만.
- 휴지통/삭제 — YAGNI.

## 4. 리스크

| 위험 | 완화 |
|---|---|
| 에이전트의 임의 경로 열람 | 기본 Ask 파킹 + 승인 스트립 + receipts 감사 + 64KiB 읽기 캡 |
| 경로 주입/탈출 | 절대 경로 강제 + UNC/`..` 거부 + 260 경계 |
| 대형 디렉터리 | 목록 512행 캡 + 미리보기 64KiB 캡 |
| GUI가 자기 접근도 감사에 부풀림 | files_audit은 control-only 요청만 라벨링은 MVP 밖 — ts/tool/path/ok만(소스 미구분을 한계로 문서화) |
| receipts 파일 잠금 | 읽기 전용 스캔 — PruneReceipts 선례(파산 시 빈 목록) |

## 5. 검증

- probe_files.ps1 12체크: list 셰이프(루트)/list 서브디렉터리/not_found/
  경로 거부(상대·UNC·`..`)/read 텍스트/truncated/read 이진 판정/bad path/
  에이전트 ask 파킹(agentctl → approval_request 이벤트 캡처 + 승인 → 응답
  도착)/deny 파일값 → permission_denied/files_audit 행 형상/GUI spawn.
- 회귀: selftest/jkagentd/notify/notes/settings/agentmgr/shot/palette/ratelimit.
- 2연통 ALL PASS.

## 6. opus 최종리뷰 (상임)

전체 diff + 프로브 결과. 특별 점검: (a) 소스 분류 오류 가능성(GUI=
window 연결 맞는지 — jkchat은 control-only라 에이전트로 분류되는지 주의,
notes src 선례), (b) 경로 검증 우회(\, /, 대소문자, 8.3 짧은 이름,
심링크 — Windows 특성), (c) 파킹 경합(파일 도구 동시 요청, 승인 큐 만료),
(d) files_audit가 receipts 파싱에서 args 유실하지 않는지, (e) binary 판정
NUL 스니핑의 허수(false negative on UTF-16), (f) permission_set으로
files_*를 allow로 바꾸는 경로와의 정합.
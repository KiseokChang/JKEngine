# 엔진 네이티브 파일 열기 대화상자 (filedlg) 설계

> 2026-09-13. 사용자 요청: "비디오 플레이어에서 파일 선택창도 좋은거로 달아줘.
> 이번 기회에 파일다이알로그도 개선하는걸루" + 진행 순위 결정 (파일 다이얼로그
> → 북마크 → vplayer 안정성+jog 휠). 사실 기반은 단계별 인벤토리 스캔
> (2026-09-13, 본 세션): 모듈 ABI 인자 슬롯 부재, ImGui 앱 간 모달 기재 전무,
> 레거시 JKFileDialog의 열거/필터 로직 존재, snap 요청자 페어링 + jkchat
> parked-query 승인 파이프라인 선례.

---

## 0. 인벤토리 (스캔 사실 요약)

- **vplayer 열기 경로** — `ClientVPlayerApp::OpenPath`의 유일 호출처는 앱
  자신의 수동 경로 입력(`InputTextWithHint` + 열기 버튼). 파일 브라우저,
  런처/팔레트 진입점 전무.
- **모듈 ABI** — `JKAppModule.h`는 `jk_app_meta()`/`jk_app_run_client(pipe)`
  2 export뿐, 실행 인자 슬롯 없음. 유일한 인자 전달 선례는
  `"terminal:<cmdline>"` appName 접두 관례.
- **ImGui 앱 간 모달 기재 전무** — 새 클라이언트는 기동 시 무조건 키보드
  포커스를 얻는다(서버 `FocusClient`). 서버 특수 기하는 타이틀 키(snap
  오버레이)뿐. 레거시 프레임워크에만 진짜 모달(SetModalWindow)과
  JKFileDialog(400x300, WA_TITLEMOVEABLE 전용)가 있다.
- ** sanctioned 채널** — 각 클라이언트 자기 파이프 위 Desktop Agent API
  (AgentQuery/Reply/EventSubscribe/Event). 선례: palette SendTool 1-in-flight,
  snap pending id + deadline, jkchat 승인은 **쿼리 파킹 → 두 번째 UI가
  approve로 해소**. 요청자-도우미 상관관계 선례는
  `pendingSnapSpawnerConnId_`/`overlaySpawner_`(conn id 기반, snap 전용 하드코딩).
- **권한 게이트** — `AgentToolAllowed` 기본 allow. 신규 도구는 무게이트로
  시작(close_window/trust_request만 예외).
- **파일시스템** — jkcore 공용 유틸 없음. 레거시 JKFileDialog의
  `RefreshList`(dirs-first 정렬)/`MatchFilter`(`*.ext`/정확명/`;` 목록)가
  트리 유일의 열거+와일드카드 로직 — 이식 재료.
- **테마** — 단계 3 봉합(JKThemeImGui.h)으로 Selectable 헤더/선택/필드/
  스크롤이 이미 토큰 추종. 신규 리터럴 금지 정책 유지.

## 1. 설계

### 1a. 새 ImGui 앱 모듈 `ClientFileDialogApp` (jkapp_filedlg.dll)

- `jk_app_meta()`: `{ "filedlg", "파일 열기", 560, 400 }`. 루트 =
  `WA_TITLEMOVEABLE` 전용(리사이즈 불가 — 레거시 JKFileDialog 선례), 일반
  chrome 창(서버 닫기 버튼 보유). 서버 특수코드 없음 — 신규 스폰이
  무조건 포커스를 얻는 기존 규칙이 모달성의 전부다.
- **UI** (상→하):
  1. 경로줄: 위로 버튼 + 현재 디렉터리 InputText(Enter=이동) + 새로고침
  2. 항목 리스트(Child+Selectable, dirs-first, 폴더/파일 접미 태그):
     더블클릭 폴더=하강, 파일 클릭=선택, 폴더 더블클릭 대신
     "선택 후 열기"도 OK 경로
  3. 필터 콤보(예: `동영상 (*.mp4;*.mkv;...)`) + 파일명 InputText(Enter=OK)
  4. [열기] [취소] 버튼
  - 키보드 계약(레거시 RespondMessage 계승): Esc=취소, Enter=열기(파일명
    필드) / 폴더 진입(리스트). `SetKeyboardFocusHere` 재장전은 palette 패턴.
- **테마**: 기존 봉합 1회 호출 + 토큰만(선택 행은 Header/selectionBg 무료
  추종). 에러 색 등 의미색만 리터럴.

### 1b. 요청-회수 채널 (신규 에이전트 도구 3종)

1. **`file_open`** (요청자 → 서버): args `{filter?, start?, title?}`.
   서버는 (a) 쿼리를 **파킹**하고, (b) `filedlg:<json>` appName 접두로
   파라미터를 실어 SpawnClient. 파킹 선례 = jkchat 승인 파이프라인.
   `filedlg:` 접두는 `"terminal:<cmdline>"`과 같은 계열의 두 번째 접두
   관례 — 모듈 ABI 무변경(스폰 인자로만 소비).
2. **`file_dialog_params`** (다이얼로그 → 서버, 기동 직후 1회): 서버는
   파일된 파킹 항목(요청자 conn id + params)을 1슬롯 저장소에서 꺼내 응답
   `{filter, start, title, requesterConnId}`. 요청자-다이얼로그 상관관계를
   conn id로 일반화(snap의 하드코딩 타이틀 페어링을 도구화한 것).
3. **`file_open_result`** (다이얼로그 → 서버, 종료 시): args
   `{ok: bool, path?: string}`. 서버는 요청자의 파킹 쿼리를 완료한다.
   ok=false(취소)와 요청자 소멸(파킹 만료) 모두 자연 처리.
- 권한: 3도구 전부 기본 allow.

### 1c. vplayer 통합

- "열기..." 버튼 추가(수동 경로 입력은 보존): `file_open` 쿼리 1-in-flight
  (palette SendTool 패턴), 응답 프레임 폼프 → `OpenPath(path)`. 마지막
  디렉터리 기억(다음 기동 start 후보).

## 2. 검증

- 빌드 exit 0 + mtime 게이트 + `jkdesktop test` 0 failures + 프로브 15종.
- e2e 실측: vplayer 열기 → filedlg 스폰 → 포커스 → 폴더 하강 → 파일 선택 →
  vplayer 재생 시작. 취소/빈 선택/존재하지 않는 디렉터리/권한 없는
  디렉터리 회귀.
- 테마: 클래식 전환 시 다이얼로그 팔레트 추종 스크린샷(단계 3 게이트 헬퍼
  재사용).

## 3. 리스크

1. **파킹 쿼리 만료와의 상호작용** — 요청자가 다이얼로그 중 닫히면 만료
   처리(jkchat approval_timeout 선례). 다이얼로그는 결과 발행 시 파킹이
   이미 없어도 무해(no-op).
2. **params 1슬롯 경합** — 동시 file_open 요청 2건이면 후발이 대기해야
   하나, v1은 1슬롯(선착순) + 스폰 스로틀이 이미 500ms라 실측 충돌
   가능성 극낮. 이월 기록.
3. **filedlg: 접두 파싱의 인자 이스케이프** — json 1문자열 전달로
   CreateProcessA 인용 처리 재사용(terminal: 선례 준용).

## 4. 결정/직감 장부

| # | 결정/직감 | 근거 |
|---|---|---|
| D1 | **다이얼로그 = 자체 앱 모듈**(프로세스 분리) | ImGui 앱은 주소공간 공유 불가 — 유일한 도달 경로는 스폰 |
| D2 | **결과는 파킹 쿼리 회수** (publish_event 브로드캐스트 아님) | 1:1 상관관계 + 만료 시맨틱이 jkchat 승인 파이프라인과 동형 |
| D3 | **params는 기동 직후 쿼리로 회수** — 모듈 ABI/환경변수 해킹 거부 | file_dialog_params 1쿼리가 ABI 무변경 + 서버 스폰 경로 재사용 |
| D4 | **레거시 JKFileDialog의 열거/필터 로직을 다이얼로그 로컬로 이식** | jkcore 공용 승격은 소비처 1개 시점엔 YAGNI — 후속 기록 |
| D5 | **테마는 기존 봉합 경유, 신규 리터럴 금지** | 단계 3 규칙 그대로 |
| D6 | **서버 타이틀 특수코드 없음** | 신규 스폰 무조건 포커스 규칙이면 모달성 충분 — snap 오버레이류 하드코딩 확산 금지 |
| 직감 | "모달"의 실체가 이 엔진에선 스폰 포커스다 — 레거시 SetModalWindow의 대형 기계를 배우지 않고도 대화상자를 만들 수 있는 지점 | §0 |
| 직감 | filedlg: 접두 관례는 terminal:이 열어둔 길 — 인자 슬롯 부재는 구멍이 아니라 관례의 부재였다 | §0/1b |
| 실행 | **Windows 라벨 `(패턴)` 필터는 표시 형식이 아니라 파싱 계약** — 계획 원문 `동영상 (*.mp4;...)`을 `;`로 자르면 첫 패턴 `동영상 (*.mp4`(exact-name 경로로만 소비)과 끝 패턴 `*.mov)`이 되어 `*.mp4`가 어느 패턴에도 등장하지 않는다. 호출처 문자열이 아니라 파서(ExtractPatterns, 바깥 괄호 추출)를 견고하게 했다 | 사용자 보고 (2026-09-14), 픽스 71c806b |
| 실행 | **ListClipper 도입이 OOB를 자가 발견했다** — 클리퍼는 DisplayEnd 캐시 범위 안에서 콜백하므로 그 범위 내에서 entries_를 축소하면(더블클릭→NavigateTo/OnOk→RefreshList) OOB. 액션을 루프 후로 지연 + 범위검사 + by-value 사본이 필수 | 클리퍼 픽스 자가 발견 회귀, 71c806b |
| 실행 | **KSSM 이중 변환 트랩** — `Utf8ToKssm`은 무효 UTF-8 입력에 `{}`를 반환하므로 이미-KSSM 문자열을 재변환하면 빈 타이틀이 된다. 변환 성공(유효 UTF-8)시에만 KSSM, 실패 시 원문 — 폴백 게이트가 본체였다 | 타이틀 픽스 cbaa53c |

## 5. 후속

- 동시 file_open 멀티슬롯 (필요 실측 후)
- jkcore 경로/필터 유틸 승격 (소비처 2+ 시점)
- 북마크(docs/44 흡수 패턴), vplayer 안정성+jog 휠 — 대기열 순서 확정 (2026-09-13)

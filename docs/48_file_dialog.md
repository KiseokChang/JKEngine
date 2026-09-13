# 48. 엔진 네이티브 파일 대화상자 (filedlg) as-built

- 날짜: 2026-09-13 → 09-14 (구현+사용자 실사용 결함 픽스 라운드 포함)
- 상태: 구현 완료 (코드 커밋 d915cc7 → 670ee9d, 게이트 **GATE GREEN** — 프로브
  18/20, 실패 2건은 도구/기존 귀속으로 성립, §4). 사용자 보고 결함 3건 전부
  픽스 완료(71c806b + cbaa53c) + 회귀 프로브(670ee9d).
- 선행: docs/superpowers/specs/2026-09-13-file-dialog-design.md (스펙),
  docs/superpowers/plans/2026-09-13-file-dialog.md (플랜)
- 비고: 게이트 보고서 `.superpowers/sdd/2026-09-13-file-dialog/task-4-report.md`,
  진행/룰링/결함 레저는 같은 디렉터리 progress.md.

## 1. 개요

vplayer에 "파일 선택창도 좋은거로"라는 사용자 요청에서 출발해, 이번 기회에
엔진 전역의 파일 열기 대화상자를 네이티브로 만든 일이다. 설계의 골격은 스펙
D1~D6에 있지만 요약하면 3가지다. (1) **다이얼로그 = 자체 앱 모듈**
(jkapp_filedlg.dll, 560x400) — ImGui 앱은 주소공간 공유 불가라 스폰이 유일한
도달 경로이며, "모달성"은 서버의 신규 스폰 무조건 포커스 규칙으로 충분했다
(레거시 SetModalWindow 대형 기계 미도입 — 스펙 D6). (2) **요청-회수는 파킹
쿼리** — jkchat 승인 파이프라인과 동형: `file_open`이 요청자 쿼리를 파킹하고
`filedlg:<json>` 접두로 스폰, 다이얼로그가 기동 직후 `file_dialog_params`로
파라미터를 회수, 종료 시 `file_open_result`로 파킹을 해소한다. 모듈 ABI는
1바이트도 바뀌지 않았다(스펙 D3). (3) **vplayer "열기..." 버튼** — 수동 경로
입력은 보존하되 1-in-flight 쿼리로 filedlg를 부르고, 마지막 디렉터리를
기억해 다음 start로 시딩한다.

구현 후 사용자 실사용에서 결함 3건(대형 리스트 잘림, mp4 필터 누락, 타이틀
깨짐)이 보고됐고 §5에서 전부 닫았다. 이 중 타이틀 픽스는 filedlg 한정이
아니라 **한국어 타이틀을 가진 모든 창의 크롬 렌더링 결함**이었다.

## 2. 커밋 (2 feat + 3 fix + 1 test + 1 side-fix + 2 docs — 실측 `git log --oneline 2d7d914..HEAD`)

| 커밋 | 분류 | 내용 |
|---|---|---|
| e1d7ba3 | docs | 스펙 — 파킹 쿼리 + filedlg: 접두 관례 설계 |
| ab65bd4 | docs | 플랜 (5 태스크) |
| d915cc7 | feat (T1) | 서버 file_open 파킹 + filedlg: 스폰 접두 + params/result 도구 |
| fd05664 | fix (T1fix) | 파킹 만료 시 pendingFileDialog_ 슬롯 회수 + escaped 1000자 캡 |
| 7be93d2 | feat (T2) | filedlg ImGui 앱 모듈 — 테마 추종 파일 열기 대화상자 |
| 696b75b | fix (T2fix) | 필터 콤보 라벨/값 분리 + 실시간 증분 오류 표면화 |
| 413f578 | fix (side) | TestWindow 250x250→420x420 (사이드 요청, 본 계획 무관) |
| 5f4f1f7 | feat (T3) | vplayer filedlg 픽커 — 에이전트 채널 열기 버튼 |
| 71c806b | fix (결함1+2) | 대형 디렉터리 ListClipper + Windows 라벨 (패턴) 필터 파싱 |
| cbaa53c | fix (결함3) | 크롬 타이틀 Utf8ToKssm 변환 + 무효 UTF-8 원문 폴백 |
| 670ee9d | test | mp4 필터 + 타이틀 픽스 회귀 프로브 (CP949 argv 회피 — 코드포인트 파생) |

Task 4(최종 게이트)는 검증 전용이라 커밋 없음. 2d7d914..HEAD 실측 diff:
17파일 +1864/−40 (본 계획 코드는 그중 filedlg 7파일 + 서버 2파일 + vplayer
2파일; 나머지는 스펙/플랜 문서와 후속 대기열 문서 143f72d·7e34258).

## 3. 대응표

### (a) 에이전트 도구 3종 (서버 구현 실측 — 모두 기본 allow, 파킹 기계는 jkchat close_window/trust_request 선례 재사용)

| 도구 | 방향/시점 | args | 동작 |
|---|---|---|---|
| `file_open` | 요청자 → 서버 (vplayer) | `{filter?, start?, title?}` | 쿼리 **파킹** + `filedlg:<json>` appName으로 SpawnClient(`--filedlg <json>` 자식 인자). 스폰 실패 시 파킹 즉시 오류 해소. 600s 만료 스캔이 슬롯 회수(fd05664 — 미회수 시 dialog_busy 영구 블로커 픽스) |
| `file_dialog_params` | 다이얼로그 → 서버 (기동 직후 1회) | 없음 | 1슬롯 pending에서 params+requesterConnId 응답 후 소진 — 요청자-다이얼로그 상관을 conn id로 일반화(snap의 하드코딩 타이틀 페어링을 도구화) |
| `file_open_result` | 다이얼로그 → 서버 (종료 시 1회) | `{ok, path?}` | 요청자의 파킹 쿼리 해소. 부재/만료면 no-op `parked:false` — 요청자 사망 경로에서 무해 |

- **filedlg: 접두** — `"terminal:<cmdline>"`에 이은 두 번째 스폰 접두 관례.
  SpawnProcess가 `--filedlg <json>` argv를 받아 quoted 인용으로 전달
  (terminal: 선례의 인용 처리 재사용).
- vplayer 쪽 게이트: `fileOpenQueryId_` 1-in-flight(이미 진행 중이면 무시),
  send 실패 시 플래그 무훼손, 폼프가 미일치 reply 전량 폐기(단일 쿼리라
  현재 안전 — palette 동형).

### (b) filedlg UI 구조 (ClientFileDialogApp, 560x400 + WA_TITLEMOVEABLE 전용)

| 영역 | 실측 |
|---|---|
| 경로줄 | 위로 버튼 + 디렉터리 InputText(Enter=이동) + 새로고침. 존재하지 않는/접근 거부 디렉터리는 빨간 오버레이(의미색 리터럴) + 목록 보존 |
| 항목 리스트 | Child+Selectable, dirs-first, **ImGuiListClipper**(§5 결함1), 폴더 더블클릭=하강, 파일 더블클릭=선택+OK. 더블클릭 액션은 루프 후 지연 실행(§5) |
| 필터 콤보 | 라벨/값 분리(FilterChoice, 패턴 dedup) — 라벨이 MatchFilter에 도달하는 경로 0(T2 리뷰 검증). **ExtractPatterns**로 Windows 라벨 `(패턴)` 파싱(§5 결함2) |
| 파일명 | InputText(Enter=OK, 콤보 팝업 중 Enter 게이트) |
| 버튼/키 | [열기][취소]. Esc=취소, Enter 계약(레거시 RespondMessage 계승) |
| 테마 | `CreateContext()` 직후 `ApplyImGuiTheme()` 1회 + 토큰만 — 클래식 프리셋 추종 실측(§4) |

### (c) vplayer 통합

| 항목 | 실측 |
|---|---|
| 열기 버튼 | 수동 경로 입력 행 옆. `file_open` 쿼리 1-in-flight |
| 응답 폼프 | `{path}` → lastDir_ 갱신 후 `OpenPath`, `{ok:false}`/오류 전부 무시+플래그 해제 |
| lastDir_ | 재생 시작 성공 시 파일 부모 디렉터리 저장(프로세스 수명 — Open 전 갱신은 브리프 문언 대위지만 UX상 우월로 수용, 레저) |

## 4. 검증 실측 (task-4-report.md GATE GREEN)

- **빌드**: ninja dry-run 전 그래프 "nothing to rebuild" — 전 타깃 최신
  (670ee9d는 빌드 입력 없는 프로브 1파일만 추가).
- **`jkdesktop test` → 0 failures.**
- **프로브 18/20 PASS — 실패 2건은 도구/기존 귀속으로 정직하게 남긴다:**
  - `probe_filedlg_fix.ps1`: **설계상 실패**(제품 결함 아님). 서버 진입점이
    ANSI `main(argc, argv)`(engine/src/main.cpp:2561)이고 agentctl이 argv를
    그대로 넘겨, 한국어 argv가 CP949 바이트로 도착 → 무효 UTF-8 →
    `JS_ParseJSON` 실패 → bad_request. ASCII 페이로드의 동일 요청은 파킹~
    스폰까지 성공 — **file_open 경로 자체는 정상, 도달 불가능한 것은
    프로브의 비ASCII argv뿐**. CLI 진입의 기존 결함으로 기록(§7).
  - `probe_terminal_reflow.ps1`: read-back 하네스 실패. 스크린샷
    (shot_reflow_wide.png)으로 리플로우는 육안 동작 — 귀속은 테마 커밋 의심
    이나 미입증. A/B용 base 빌드(2d7d914) 워크트리 준비됨.
- **e2e 5단계 전부 PASS**: (a) vplayer 열기 → filedlg 스폰+포커스+파일명
  박스 자동 포커스 (b) I:\@keep\200GANA-3420에서 .mp4 선택 → **vplayer
  재생 시작 실측**(스크린샷 PLAYING) (c) 취소 → 다이얼로그 프로세스 종료 +
  취소 결과 도달 + 재요청 시 신규 스폰 반복 (d) **요청자를 다이얼로그 중
  킬** → 서버 무크래시, 고아 다이얼로그는 Esc로 깨끗이 해소(disconnect
  가드 성립), 직후 file_open이 새 다이얼로그 스폰 — **1슬롯 회수 라이브
  실측**. 600s 만료는 코드 검증만(게이트에서 실측 비현실) (e) 미존재/
  접근 거부 디렉터리 → 빨간 오버레이 + 무크래시.
- **테마 스크린샷**: classic preset에서 filedlg 팔레트 추종 실측
  (라이트 크롬+네이비 타이틀바, 한국어 파일명/필터 무결) → 다크 기본 복원
  까지 확인. 저장: `.superpowers/sdd/2026-09-13-file-dialog/gate-classic-filedlg.png`.
- **도구 유의(기록)**: PS 5.1에서 공백 포함 JSON 인자는 native-argv 인용이
  분해해 bad_request — 제품 버그 아닌 agentctl 도구 한계. 게이트 헬퍼는
  단일 토큰 JSON 또는 Esc 폴백 사용.

## 5. 사용자 보고 3결함과 픽스 (2026-09-14 실사용)

1. **대형 디렉터리 리스트 잘림** (I:\@keep, 파일 수천 개) — 항목 루프가
   전 엔트리 Selectable을 매 프레임 제출해 프레임 시간 붕괴 → 스크롤 도달
   불가(실질 "다 안 나옴"). **픽스: ImGuiListClipper 도입.** 자가 발견
   회귀: 클리퍼의 DisplayEnd 캐시 범위 내에서 더블클릭 핸들러가
   entries_를 축소하면 OOB — 액션을 루프 후로 지연 + 범위검사 + by-value
   사본. 실측: I:\@keep에서 .mp4 표시 복원, System32 ~4300엔트리 스트레스.
2. **mp4 파일이 필터에 걸려 안 보임** (200GANA-3420) — 근원은 **Windows
   표시 형식 `라벨 (패턴들)` 파싱 부재**: `동영상 (*.mp4;*.mkv;...)`을
   `;`로 자르면 첫 패턴 `동영상 (*.mp4`(exact-name 경로로만 소비)과 끝
   패턴 `*.mov)`이 되어 `*.mp4`가 어느 패턴에도 등장하지 않는다.
   **픽스: 다이얼로그 로컬 ExtractPatterns** — 바깥쪽 괄호 내용만 추출
   (괄호 없으면 전체) — MatchFilter+BuildFilterChoices 양쪽 적용.
   **룰링**: 계획서의 표시형 필터 문자열은 수정하지 않고 파서 쪽을
   견고하게(Windows 관례 준수, 호출처 실수에 관용).
3. **창 제목 깨짐 ("파일 열기")** — JKWindow가 크롬 타이틀을 원문 UTF-8으로
   TextOutX 직행. 레거시 비트맵 폰트는 KSSM(Johab)만 렌더링하며 taskbar는
   그리기 직전 Utf8ToKssm 변환을 하는데 **JKWindow가 유일한 미변환
   소비처**였다. **픽스: 그리기 직전 변환 + 무효 UTF-8 원문 폴백
   (LegacyFontTitle)** — Utf8ToKssm은 무효 UTF-8에 `{}`를 반환하므로
   이미-KSSM 타이틀 이중 변환 시 빈 타이틀이 되는 트랩을 폴백 게이트로
   닫음. 영향 범위: 한국어 타이틀 보유 전 창. 실측: 타이틀 무결
   스크린샷(fix-title-list.png).

## 6. 실행 직감 (스펙 §4 장부에 일부 기록)

1. **표시 형식은 파싱 계약이다.** "동영상 (*.mp4;...)"은 사람에게 표시하는
   문자열이 아니라 Windows 관례의 파싱 대상 — 필터를 소비하는 쪽이 라벨을
   잘라내는 책임을 진다. 호출처를 바로잡는 대신 파서를 견고하게 한 이유는
   관용(다음 호출처가 같은 실수를 해도 동작)이다.
2. **클리퍼/이터레이터 계열 전환은 자가 발견 회귀를 낳는다.** ListClipper는
   부분 범위 콜백이라 "루프 안에서 컬렉션을 축소"하던 옛 관용이 즉시
   OOB로 변한다. 지연 실행+사본은 클리퍼 도입의 필수 동반물이지 보너스가
   아니다.
3. **인코딩 변환 헬퍼에는 폴백이 계약이다.** Utf8ToKssm의 실패 시그널이
   빈 컨테이너라는 사실(문서화 안 된 동작)을 모르면 "변환 추가"만으로
   끝냈을 것이고, 이미-KSSM 문자열을 모조리 빈 타이틀로 만들었을 것이다.
   변환 성공 여부 자체를 게이트로 썼다.
4. **프로브 실패를 제품 회귀로 읽지 않는다.** CP949 argv 실패는 ASCII로
   대체 실측한 순간 "file_open 경로 정상 + CLI 진입 결함"으로 분리됐다.
   실패 원인의 귀속이 게이트 판정의 본체였다.
5. **"모달"의 실체가 이 엔진에선 스폰 포커스다** — 스펙 직감의 실측 확증.
   다이얼로그 사망/요청자 사망/만료의 3방향 정리는 전부 파킹 기계
   (jkchat 선례)가 담당했고 다이얼로그 쪽 코드는 발송 시도만 하면 된다.

## 7. 후속 과제

- **ledger MINOR 2건** (타이틀/필터 픽스 라운드에서 이월): ①
  ExtractPatterns가 괄호 포함 exact-name 필터(파일명 자체를 필터로)를
  망가뜨림 — 괄호 내용에 `*`/`;` 있을 때만 strip하는 방향 ② 변형 라벨
  `"동영상 ()"` 전체 숨김 vs `"동영상 ( )"` 전체 표시 불일치.
- **Task 3 MINOR 이월**: disconnect-mid-dialog 시 vplayer `fileOpenQueryId_`
  영구 잔존 → 재요청 불가 (팔레트/snap 동형 잠재 결함 — disconnect-mid-dialog
  한정).
- **taskbar LegacyFontTitle 재사용** — taskbar의 Utf8ToKssm 변환은 폴백
  게이트가 없는 동형 이중 변환 위험. cbaa53c의 LegacyFontTitle 헬퍼로
  교체(현재 무해 실측이나 구조 정리).
- **CP949 argv** — engine/src/main.cpp ANSI 진입점이 한국어 `--filedlg`/
  agentctl 인자를 망가뜨림(게이트 Concerns 1). 진입점 UTF-8 인식 또는
  프로브의 JSON 파일/stdin 전송 전환 중 선택. probe_terminal_reflow 귀속
  A/B(basecheck 워크트리 2d7d914)와 함께 정리.
- **원자적 쓰기(tmp+rename)** — 엔진 공용 유틸 승격 후보. 브라우저 북마크
  스펙(2026-09-14) D5가 fopen "wb" 관례 유지로 이월한 공유 후속.
- **기존 후속 유지** — 동시 file_open 멀티슬롯(필요 실측 후), jkcore
  경로/필터 유틸 승격(소비처 2+ 시점), lastDir 영속화.

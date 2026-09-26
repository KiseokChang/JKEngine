# docs/65 — JKEdit 결함 통합 대장 (2026-09-25)

JKEdit에 기록된 결함의 단일 통합 대장. 출발점은 docs/46 §7의 "JKEdit 결함 대장
후보"(멀티라인 선택 미렌더 + 사용자 보고 "전반적으로 버그 투성이")와 docs/61의
픽스 시리즈(2026-09-20, §1~§23)다. 원장이 흩어져 있으면 "이 결함 이미 수리했나?"
가 매번 전문 리딩으로 답해야 하므로, 전부를 한 표로 압축하고 현재 상태를 오늘
프로브 2회 실측으로 봉합한다(§2). 새 JKEdit 보고는 이 원장 §1에 누적한다(§5 승계
규칙).

## 1. 통합 대장

뿌리 결함 분류 — **폭 무시**(KSSM 글리프 폭 무시·바이트 좌표) / **경계**(문자셋·
표현 경계 무변환·무정규화, 폰트 커버리지 포함) / **휴리스틱**(값 범위 휴리스틱으로
경계 판정) / **배선**(이벤트 라우팅·모디파이어·호출부·관측점) / **자동사 상태**
(조합 상태 머신 불변식) / **설정**(코드 밖 상태).

| # | 증상 | 뿌리 분류 | 픽스 커밋 (docs/61 §) | 상태 | 증거 |
|---|------|----------|----------------------|------|------|
| 1 | 멀티라인 선택 미렌더 (대장 기원, docs/46:219) | 배선 (멀티라인 페인트 분기에 선택 렌더 부재) | 0669ab5 (§1-1) | 게이트 T2 (48체크 ×2 ALL PASS) | jkedit_case2_multiline_sel.png |
| 2 | 한글 캐럿·선택·클릭 밀림 (바이트×8px) | 폭 무시 (ASCII 8/KSSM 16 혼용 좌표) | 0669ab5 (§1-2, DisplayCells/PosFromCells) | T1 | jkedit_case1_korean_caret.png |
| 3 | 한글 복사→클립보드 오염 | 경계 (버퍼 KSSM 원바이트 직출) | 0669ab5 (§1-3) | T3 | jkedit_probe T3 |
| 4 | 한글 붙여넣기 오염 | 경계 (UTF-8 원바이트 직입) | 0669ab5 (§1-4) | T3 | jkedit_probe T3 |
| 5 | 한글 타이핑이 선택을 살려둠 | 배선 (DeleteSelection 누락) | 0669ab5 (§1-5) | T4/T5 | jkedit_probe T4·T5 |
| 6 | 한 줄 편집 가로 스크롤 부재 — 캐럿 소실 | 배선 (기능 부재) | 0669ab5 (§1-6) | T2 | jkedit_case3_clip.png |
| 7 | DeleteForward/SetSelection 미 스크롤 | 배선 | 0669ab5 (§1-7) | 회귀 | jkedit_probe |
| 8 | 멀티라인 휠 스크롤 부재 + 긴 줄 경계 밖 오버플로 | 배선 (페인트 클립 부재) | b6546bc (§4.1) | 시각 | jkedit_case3_clip.png |
| 9 | 한글 행 통째 소실 + 완성 음절 소실 (NUL-in-buffer, `{0x00,자모}`) | 경계 (엔진 내부 표현 간 — 8비트 슬롯↔KSSM 쌍) | 16dbab2 (§7-9, ToStandaloneKssm) | T7/T8 | DiagDump 라이브 로그 (§7.2) |
| 10 | 조합 연쇄 단절 ("한국어"→한+ㄱ+…) | 자동사 상태 (종료 상태 씨앗 소실) | 1e51e4c (§10-11) | T9-T12 | jkedit_probe T9-T12 |
| 11 | 겹받침 탈락 ("랄가"→"라가") | 자동사 상태 (덮인 curHanState 판정) | bc2f0fb (§12, prevState) | T13/T14 | jkedit_probe T13 |
| 12 | 쌍자음 처음부터 불가 | 배선 (대문자 키코드 봉쇄 + ConvertKey XOR) | 549f30d (§13) | T15/T16 | jkedit_probe T15·T16 |
| 13 | 클라 모드 Shift 무시 (SDL_GetModState 전역 미갱신) | 배선 (클라/서버 분할 — 전역 상태) | 578d847 (§14) | 콘솔 재현 불가 — 라이브 (§3) | docs/61:287 |
| 14 | 한/영 전환키 미지원 (F2 전용) | 배선 (기능 부재) | 16e3ec9 (§15) | T17 | jkedit_probe T17 |
| 15 | 한/영 키가 OS IME에 삼켜짐 → 폴링 ImeChanged 브로드캐스트 | 배선 (관측점 신설) | f313918 (§16) | T18 | jkedit_probe T18 |
| 16 | ImeChanged/TextEditing 포커스 디스패치 누락 + IMM 폴링 부패 → LL 훅 전환 | 배선 + 관측점 부패 | 7587310 (§17) | T19 | server `[ime] toggle` 로그 |
| 17 | ImeHangul 핸드오버 사망 경로 (OS IME 조합 불능 = DetachIme 설계, 9cade2d) | 배선 (설계 전제 붕괴 — 토글은 자기 소유 체계를 뒤집어야) | 37915d1 (§18) | T19a/c/c2/d | jkedit_probe T19 |
| 18 | 조합 중 백스페이스가 조합 통삭제 | 자동사 상태 (자소 pop 부재) | 5cd3422 (§19-1, BackspaceJamo) | T20 | jkedit_probe T20 |
| 19 | LL 훅 wParam==VK_HANGUL로 무발화 | 배선 (훅 파라미터 — vkCode는 KBDLLHOOKSTRUCT) | 5cd3422 (§19-2) | 라이브 실측 | server `[ime] toggle` 로그 |
| 20 | 캡스락 상태 쌍자음 입력 | 배선 (shift XOR caps의 잘못된 일반화) | 77f44b4 (§21) | T21 | jkedit_probe T21 |
| 21 | 화살표 2칸 점프/백스페이스 2글자/쌍 쪼갬/Up-Down 쌍 중간 착지 | 휴리스틱 (둘째 바이트 0x80+ = KSSM 첫 바이트 오판 → 유효성 역인덱스 폐기) | 2449a2b (§23, KssmCharLenAt/Prev/Snap) | T23a-f | jkedit_probe T23 |
| 22 | 터미널 한글 입력 죽어 있음 (DetachIme 설계의 피해자, docs/61:461) | 배선 (설계 전제 붕괴 — OS IME 의존 경로가 유일) | 3d3cef2 (§22, TerminalHangulInput 이식) | terminal_hangul_probe 33/33 + view 18/18 | docs/61:532-537 |
| 23 | 조합 과정 자모가 작은 세로바 (U+3130 폴백 범위 누락) | 경계 (폰트 커버리지 — 쓰는 코드 포인트와 슬롯 범위 불일치) | 9413cb4 (§22.1) | terminal_jamo_atlas_probe 9/9 | docs/61:571-573 |
| 24 | 자모가 하나씩 찍힘 (문자 키 무조건 확정 배선) | 배선 (호출부 — 순수 로직은 옳았다) | 6309ea3 (§22.2) | terminal_hangul_view_probe 18/18 | docs/61:593-599 |
| 25 | "조합 과정이 안보이는" — terminal_.json 개명으로 테마 상실, 저대비 | 설정 (코드 무관) | 무커밋 — 설정 파일 복구 (§22.3) | vpt13/vpt14 PASS | docs/61:628-633 |
| 26 | 워크숍 위젯 한글 깨짐 (UTF-8 원문 직투입) | 경계 (위젯 경계 무변환 — JKEdit류 위젯 공통) | 4637d95 (docs/60 §7, ToWidgetText) | probe_workshop 17체크 ×2 | docs/60:190-192 |

표 외 참조: docs/60 §10(캔버스 API v5)은 "텍스트 조합은 JKEdit가 이미 잘 한다"
(docs/60:293)로 JKEdit를 전제 삼았고, docs/63 §9.7은 JKEdit charWidth_/lineHeight_
를 GetCellMetrics로 치환해 셀 확대를 배선했다(docs/63:331).

## 2. 현재 검증 (2026-09-25 실측)

- `engine/build/jkedit_probe.exe` ×2 — run 1·run 2 전부 **57체크 PASS +
  `RESULT: ALL PASS`** (docs/61 §23의 "49"/이전 세션의 "48"에 이어 O3 재부착
  게이트 T20h-T20o 9체크 신설로 48→57).
  - 로그: `I:/progwork/JKENGINE/tmp/o3_jkedit_r1.log`, `..._r2.log`
- `terminal_hangul_probe` ×2 — **38/38** (O3 터미널 경로 T11 5체크 신설,
  이전 33체크 기록 대비), `terminal_hangul_view_probe` ×2 — 18/18 회귀 무변.
  로그: `tmp/o3_term_r{1,2}.log`, `tmp/o3_termview_r{1,2}.log`.
- `jktext_probe` ×2 — **73체크 PASS + `RESULT: ALL PASS`**(2026-09-26, O4/O5
  게이트: T11(c) fractional 불변식 7체크+T12 기호 왕복 6체크 신설로 60→73).
  JKHangulUtil·JKTextAtlas 변경의 회귀 게이트로 jkedit_probe ×2+terminal ×2
  재실행 — 전부 동일 통과.
- `jkedit_render_probe`는 exe 부재(소스만) — 이전 오토마타 수술(docs/61 §19)
  에도 미빌드로, 본 수술(DeleteBackward 치환 경로)도 미커버. BMP 눈확인은
  §3 틈새에 그대로 유효.
- exe 신선도: build-dir exe 2026-09-21 08:15:30 > JKEdit.cpp 08:12:58 >
  JKEdit.h 08:09:12 > jkedit_probe.cpp 07:35:18 — 소스 대비 스테일 아님.
  `engine/tools/probes/jkedit_probe.exe`(2026-09-20 22:36, ad hoc 빌드 잔재)는
  구형 — 정본은 build-dir exe.
- **기록 편차**: docs/61 §23 기록 "49체크" vs 현재 48체크(docs/61:696) — 1체크
  편차. 결과 판정에는 무영향(ALL PASS), 기록 스테일로 기록해 둔다(§5).
- CMake 타깃 없음 — `engine/tools/probes/CMakeLists.txt` 부재, ad hoc 빌드 레시피
  는 docs/61:701-713(§23).

## 3. 틈새 — 콘솔 유닛 프로브가 검증 못 하는 것

| 미커버 영역 | 대상 결함 | 필요한 검증 형태 |
|---|---|---|
| 라이브 창 서버 e2e — 실화면 멀티라인 선택 강조·마우스 드래그 선택 | #1, #2, #8 | jkedit_render_probe 5케이스 BMP 눈확인(docs/61:69-70) + 라이브 capture_window |
| 클라 경로 (§14류) | #13, #16, #17 | docs/61:287-288 명시 "이 결함은 콘솔 프로브로 재현 불가" — 클라 리그(vpt14 패턴, docs/61:617-625) |
| OS IME 실경로 — IMM 부패·LL 훅·키 삼킴 | #15, #16, #19 | 라이브 세션 실측만 유효(docs/61:351-368, haneng_sdl_diag 실측) |
| 설정 파일 의존 | #25 | "안보인다"면 설정(존재·이름·mtime)을 렌더러보다 먼저 — docs/61:639-642 |
| 폰트 주입 하네스 | #9, #23 | HangulManager 미주입 프로브는 한글을 박스로 그려 오판(docs/61:63-70) — 폰트 매니저는 하네스 기본 장착품 |
| fractional scale 실물 | §4 한계(docs/63:404-407) | scale>1.0 옵트인 눈확인 유예(docs/63:355-356) |

**증거 슈팅 매핑** (`engine/tools/probes/`, 모두 2026-09-20): case1=한글 캐럿·선택
정렬(#2) / case2=멀티라인 선택 렌더(#1) / case3=긴 줄 클리핑(#6, #8) /
case4=IME 조합 오버레이(#16의 렌더 측면 — TextEditing 디스패치 복구 전제) /
case5=사용자 시나리오(멀티라인 한글 타이핑+드래그 — §4 보고 ②④). 참고:
`jkedit_case5_img`(확장자 없는 1.27MB 파일)는 case5 BMP의 오기명 잔재 — 정본은
`jkedit_case5_memo_typing.bmp`.

## 4. 열린 결함 (백로그)

새로 발견된 미수술 결함은 없다 — 열림 전부는 docs에 한계로 기록된 백로그 6건:

| # | 내용 | 근거 |
|---|---|---|
| O1 | 한자 키(LANG2) 미처리 — LANG1만 핸들링 | **수술 완결(2026-09-26)** — 사전=시스템 IME 사전(IMM/TSF 사망 → HanjaDic COM `IHanjaDic` 채택, docs/66 §2-§3.5). 구현: jk::hanja(JKHanjaDict, COM 파이프라인+주입형 테스트 프로바이더) + ImeHanja 이벤트 6단 배관(훅→서버→와이어→뷰, LANG2 스캔코드 **145**) + JKEdit 후보 모드(단일 커밋점=Char 숫자, 멀티라인 세로 목록/한 줄 스트립) + 터미널(조합 중만 오픈, pty 1자 송출). 라이브 e2e ×2: gks→VK_HANJA→팝업 1韓…→'1' 커밋 pty `E9 9F 93`(韓) — 결함 1건 실측 픽스(LL 훅 VK_HANJA down/up 차단: 합성 원시 키다운이 팝업 취소). 게이트: jkedit 87·terminal 44/44·view 27/27·jktext 73 ×2 ALL PASS. **실물 한자키 눈확인 완료(2026-09-26)** — O1 완전 종결 | docs/61:303,367,402 / engine/src/JKEdit.cpp:546 → **docs/66 §6-§9** |
| O2 | 클라→서버 IME 강제 채널 부재 — 클라 모드 SilenceOsIme no-op, F2 진입 시 OS IME 영문 강제 무효(이중 조합 위험) | **실측 봉합(2026-09-25)** — 이중 조합 미발생. 서버 창은 DetachIme(JKWindowServer.cpp:329-335)로 IMM 컨텍스트가 없어 OS IME 조합 채널이 구조적으로 닫힘: 내부 ASCII 모드에서 원문 키코드 도달(type=9=0), 내부 한글 모드에서 프리에디트 오버레이 단일("한"), TextEditing 0건. 양성 대조군(컨텍스트 있는 프로브 창 강제 한국 모드 → GCS_COMPSTR "하") ×2로 조합 채널 생존 별도 입증 — 방어는 구조(컨텍스트 부재)가 아니라 그 부재 자체. o2_double_compose.ps1 ×2 ALL PASS | docs/61:334-337, 402-403 → tmp/o2_r{1,2}_*.png·log |
| O3 | 받침 넘김(학+ㅗ→하+고) 뒤 백스페이스 — 받침 재부착 불가(End2 플러시 이력 미보유, MS IME와 다름) | **수술 완결(2026-09-25, 929114a)** — HangulAutomata::Handover: End2가 플러시 음절의 inpStack 이력을 보관하고 씨앗 pop 시 이력 복원+받침 재부착. 호출부 계약 3값 확장(BackspaceResult: Jamo=쌍 재기록/Reattach=플러시 쌍+조합 쌍 4바이트→2바이트 치환/Empty=쌍 삭제) — JKEdit 버퍼 치환, 터미널 pty DEL+오버레이. End1/Init 무효화(즉시 재부착만). jkedit_probe 57체크 ×2+terminal 38/38 ×2+view 18/18 ×2 ALL PASS | docs/61:417-420 → tmp/o3_*.log |
| O4 | fractional scale에서 JKEdit 쌍=2셀×engW 매핑 근사 — 최대 1px/쌍 드리프트(정수 scale은 정확) | **수술 완결(2026-09-26)** — 뿌리는 ComputeCellMetrics의 독자 반올림(engW=round(8s), hanW=round(16s))이 소수 scale에서 hanW≠2×engW(1.2: 19 vs 20)로 어긋난 것. 셀 모델 진실("KSSM 쌍 = eng 셀 2개")로 hanW를 engW 유도(2×)로 단일화 — JKDC 전진(m.hanW)과 JKEdit 쌍 매핑이 전 scale에서 정합. 구현 함정: CellMetrics 초기자 순서 {engW, hanW, cellH} — 높이를 hanW 슬롯에 넣어 1회 미스. jktext_probe T11(c) fractional 불변식 7 scale ×2 ALL PASS | docs/63:404-407 |
| O5 | 위젯 텍스트 기호(■□●◆)·이모지 → ? 렌더 (Utf8ToKssm 도메인 한계) | **수술 완결(2026-09-26)** — KS X 1001 A1-A2 기호 행은 조합형(KSSM)과 완성형이 바이트 동일: Utf8ToKssm에 등가 매핑+EUC-KR 역인덱스에 항등 루프 신설(KssmCharLenAt 쌍 유효성도 이 표 경유). KssmCodepointToUnicode는 왕복 가드가 자동 승계 — 아틀라스 벡터 경로로 렌더. 이모지는 CP949 불가 한계 유지: UTF-16 서러게이트 2유닛이 각각 '?' 치환 → `??` 2바이트(kApiCatalog charset 문구 갱신). jktext_probe T12 6체크 ×2 ALL PASS | docs/60:213-215 |
| O6 | 터미널 조합 중 스크롤백/선택 — 오버레이는 지워지지만 오토마타 유지(표준 IME 유사, 수용) | docs/61:555-556 |

의도 유지(결함 아님, 문서화): 단독 모음+받침 불가 자음 → 채움 초성 음절(원본
AUTOMATA semantics, docs/61:160-165 — 단, 받침 가능 자음은 §10.1-3에서 modern
IME 동작으로 전환). docs/46 §7의 나머지 후속(JKMenu 팝업 스모크, probe_agent_e2e
백오프 등)은 JKEdit 무관.

## 5. 후속 (레저)

- **승계 규칙**: 새 JKEdit 증상 보고 → 이 원장 §1에 행 누적(증상/뿌리 분류/커밋/
  검증 게이트/증거), 원장 §2에 프로브 실측 갱신. 세부 수술 설계는 docs/61 계열에
  보고 대장 행에서 링크한다 — 원장은 상태의 진실원, 수술 원문이 아니다.
- **프로브 게이트 후보**: (1) 현재 편차 봉합 — 48체크를 공식 체크수로 명기
  (docs/61 §23 "49" 정정) (2) 멀티라인 선택 렌더(#1)·클리핑(#8)은 BMP 눈확인만
  있고 단정 체크가 없다 — render probe의 픽셀 단정화(terminal_jamo_atlas_probe
  패턴)가 값싼 다음 단계 (3) 클라 경로 리그(vpt14 패턴)의 JKEdit 상시화 — §14류
  결함은 유닛 프로브에 원리적으로 닿지 않는다.
- ~~**O2가 가장 큰 레저**~~ — 실측으로 해소(§4 O2 행). 이중 조합은 DetachIme
  구조 방어로 발발하지 않으며, 채널 공사는 하지 않기로 봉합(2026-09-25). 남은
  열림 결함 중 실제 이중 입력 위험은 없다 — O1/O4/O5는 렌더·경계류, O6는 수용.

## 6. 레슨

1. **"버그 투성이"는 뿌리 2-3개로 수렴한다**(docs/61:44-46) — 26건의 표면 증상이
   폭 무시/경계 무변환/배선 3분류로 수렴했다. 결함 수리 후 "뿌리 결함
   교차검토"를 정기 의제로(docs/61 §20-24, 레슨 24) — 그 검토가 §20의 DetachIme
   → 터미널 한글 사망(#22)을 터뜨렸다.
2. **하니스 표현별 케이스 필요**(docs/61:268-272, 레슨 17) — 하니스(소문자
   키코드+shift 플래그)가 통과해도 라이브 SDL(shift 반영 키코드)은 다른 원을
   본다(#12, #13, #20 전부 "두 표현 어긋남"이었다). 표현별 케이스를 나란히 넣고
   정규화 함수 한 곳으로 흡수한다. 확장(레슨 23): 하니스가 거치지 않는 층(LL 훅)
   은 하니스로 검증 불가 — "픽스했는데 변화 없음" 2회 반복 시 관측점 파이프라인
   전체를 처음부터 재검증(#19).
3. **설정≠코드 결함**(docs/61:639-642, 레슨 31) — "안보인다"의 첫 용의자는
   렌더러가 아니라 설정이다(#25). 프로브의 state 백업류 이름변경 잔재가 2주
   잠복 소실을 낳았다 — 백업은 사본을 쓰거나 복원을 보장한다.
4. **방어는 실측 전까지 레저다, 실측 후엔 근거다**(O2) — "채널 부재 = 위험"이라는
   문서 한계는 설계상 근거가 아니라 가설이었다. 실측 설계의 핵심은 양성 대조군:
   같은 조합 채널을 컨텍스트 있는 창에서 강제로 발화시켜(→ GCS_COMPSTR) 측정
   장비 자체가 살아 있음을 먼저 증명하면, 음성 결과가 "도구 불능"이 아니라
   "실제 미발발"로 읽힌다. 구조 방어(DetachIme)는 코드 인용만으로는 닫히지 않는다.
   (프로브 설비 교훈: x64 SendInput INPUT 공용체는 MOUSEINPUT 크기 40바이트여야
   키 입력이 통한다 — KEYBDINPUT 크기로 쓰면 무응답.)
5. **한글 기대값은 완성형(KS X 1001) 표 내 글자로 검증한다**(O3) — 걸+ㄺ(걺)은
   wCodeTable 밖이라 KssmToUtf8 왕복이 `?`를 냈다. 오토마타 코드 자체는 옳았다.
   두벌식 키 실수도 이중 함정: t는 ㅅ(ㄷ는 e), ㅗ+ㅣ는 ㅢ 쌍모음이라 End1이 안
   일어난다 — 기대값이 아니라 코드가 틀렸다는 가정부터 뒤집어 보라(이번엔
   코드가 맞고 기대값이 틀렸다).
6. **프로브 재링크는 멤버 구성까지 복기한다(O4)** — 스테일 메트릭의 첫 용의자는
   링크 스테일이었으나 실제는 CellMetrics 초기자 필드 순서 미스였고, 그 검증
   과정에서 terminal 프로브를 o3_JKHangulAutomata.o 없이 재링크해 34/38로
   소동: libjkcore.a의 아카이브 멤버는 O3 이전 구형이어서 o3_*.o 명시 링크가
   필수. ad hoc 링크 레시피의 객체 목록은 "어떤 멤버를 libjkcore보다 앞세웠는지"
   까지 진실원이다. 인프라 레슨(별도): 세션 환경 블록이 커지면 cc1plus 스폰이
   무출력 exit 1로 죽는다(bash.exe.stackdump 동반) — `engine/build/mgxx.sh`
   최소 env 래퍼로 회피.
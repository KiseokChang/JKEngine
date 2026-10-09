# docs/86 — posix 텍스트 스케일 수리 as-built (T1-T5, 2026-10-09)

텍스트 스케일 라인(2026-10-09-phone-text-scale) T1-T5 원장. 계획 문서=
`docs/superpowers/plans/2026-10-09-phone-text-scale.md`, 스펙=
`docs/superpowers/specs/2026-10-09-phone-text-scale-design.md`
(사용자 승인 2026-10-09 — 크롬 타이틀 벡터화 비 포함·타이틀 밴드 높이/글자
크기 개선 포함·posix 기본 font_scale=1.5 결정), SDD 원장=
`.superpowers/sdd/2026-10-09-phone-text-scale/`(progress·task-1..5
brief/report/review — 워크숍 원장). 코드 계보는 git log가 진실원. 본 문서는
실측 영수증·함정·deferred·사용자 결제 게이트를 봉합한다.

## §0. 문서 체계

docs/85(더티프레젠트 as-built)의 §0-§6 구조를 계승한다 — §1 배선 원장(rev-parse
전체 SHA)·§2 캐논 표+실측 영수증·§3 함정 원장(정직 기록 축 포함)·§4
deferred(D-계열)·§5 사용자 결제 게이트·§6 커밋 원장(IP grep 게이트 병기).
창작·추정 수치 소각 — 모든 SHA·캐논·측정치는 SDD 원장 리포트/review 원문에서
인용했고, 본 문서 작성 시점(T5)에 새로 실측한 것만 "T5 실측"으로 부기한다.

발단(스펙 착수 실측): posix(WSL/폰)에서 settings `text.font_scale`을 올리면
**셀 기하·전진폭만 커지고 글리프는 8x8/8x16 비트맵에 고정** — 타이틀이 밴드를
넘어가고 한글 라벨이 클립됐다. T1이 근원을 규명했고(§3 #1 후보 ⑤), T2 결선,
T3 밴드 산식 승격, T4 폰 실측, T5 본 문서로 봉인.

## §1. 배선 원장 (라인 전체 — rev-parse 전체 SHA 실측, T5 재측정)

| 태스크 | 커밋(SHA — T5 실측 `git rev-parse`) | 내용 |
| --- | --- | --- |
| 직전 라인 종착(#81) | `54d5363e27a65aa130949d10c88803e0c99849c9` | docs(server): 폰 육안 결제 기록 + 계보 접두사 정정 (더티프레젠트 #81) |
| 스펙 | `c738f2735920120328bbede34c60be36a66a6774` | docs(font): posix 텍스트 스케일 수리 스펙 — 기하만 커지는 클라 글리프 결선 |
| 플랜 | `33eda05fb6aeeefc3a3bbc5d00e3daf97db30268` | docs(font): T1 근원규명→T2 결선→T3 타이틀 밴드→T4 폰 실측→T5 as-built |
| #83 소각 1 | `697db8b875bbee44b7c3d8836b5cd338f4dd88f6` | docs: 잔존 IP 레닥션+JKLmEngine 오타 전수 정화 — phone probes는 PHONE_HOST 필수 가드로 |
| 갤러리 초고(라인 외 끼어듦) | `40792d7930fcf611b605556173168c944279ed50` | docs(gallery): #82 갤러리 앱 스펙 초고 — A/B안+사용자 결정 3건 대기 |
| T1 | `24d22a13f642d6e4694a06c9836d5a0fe54fc526` | probe(server): 텍스트 스케일 진단 스크립트 (T1) — 진단 probe 2건만, engine/src 무변경 |
| T2 | `58861c3e39fb6d8faf11850d199736c7aae00468` | fix(client): posix client glyph vector resolution+bitmap fallback cell stretch+default 1.5 (T2) |
| T3 | `856326309b921b42e0d7459935d7245874ea9e87` | feat(server): 크롬 타이틀 밴드 셀 메트릭 연동 (T3) |
| T3 fix r1 | `b651534d4de79001de8d92b662a612893d61411e` | fix(server): 앱 밴드 높이 고정 상수 → 타이틀 밴드 산식 공용 진실원 (T3 fix r1) |
| T4 | `bdbc6877f9bfd4d1a5b2db749de48ff3774adc8a` | test(server): 폰 텍스트 스케일 실측 probe (T4) |
| T4 fix r1 | `0cde36dc3632f0a479ce66f2ca0056116fb4debd` | fix(server): 폰 probe PHONE_HOST fail-closed 가드 회귀 수리 (T4 fix r1) |
| T4 fix r2 | `997693da90640cd985e3ac0a0a307eed583312f8` | fix(server): 폰 probe 가드를 exec 앞으로 이동 — unset 재실행 로그 보존 (T4 fix r2) |
| T5 | 본 문서를 포함하는 커밋 | docs(font): posix 텍스트 스케일 수리 as-built (T5) — SHA는 git log가 진실원 |

파일 축: `engine/include/JKTextAtlas.h`(ComputeCellMetrics 헤더 inline 이동+
`DefaultFontScale()`+`StretchNearestIndex()`+밴드 산식
`ComputeChromeTitleBarHeight`/`ComputeAppContentTopOffset` 공용 진실원)·
`engine/src/JKTextAtlas.cpp`(미설정 분기=DefaultFontScale)·
`engine/src/JKDC.cpp`(폴백 nearest 셀 확대+경고 1회)·
`engine/src/client/JKClientApplication.cpp`+`engine/src/JKApplication.cpp`
(양축 결선 — ComposeScene 지역 dc/SetTextAtlas)·`engine/src/JKWindow.cpp`+
`engine/src/server/JKWindowServer.cpp`(밴드 산식 소비점 승격·배너 캐시키 크기
봉합)·`engine/src/apps/MineSweeperApp.cpp`+ImGui 클라 앱 9곳
(`AppContentTopOffset()` 교체, fix r1)·`engine/src/main.cpp`+
`engine/tools/posix_selftest/main.cpp`(2t 6건+3b 5건 쌍둥이)·
`engine/tools/posix_selftest/build.sh`·진단 probe 2건(T1)·실측 probe
`engine/tools/probes/phone_text_scale.sh`(T4+fix r1/r2). 와이어 프로토콜 접촉
0(T3 리뷰: `CommitSurface` 문자열 0건 grep 실측).

## §2. 캐논 표 + 실측 영수증

### §2.1 selftest 캐논 계보

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#81 종착, 본 라인 진입) | 518 | 495 | 226 | (495) | — |
| T1 `24d22a1` | 518 | 495 | 226 | (495) | 0 (진단 probe만 — 소스 무변경) |
| T2 `58861c3` | **524** | **501** | **232** | (506 준비) | +6 (2t-a..f) |
| T3 `8563263` | **527** | **504** | **235** | (503 전단계) | +3 (3b-a..c) |
| T3 fix r1 `b651534` | **529** | **506** | **237** | **506** | +2 (3b-d/e) |

- **폰 506 = 495 + 2t 6 + 3b 5(3+2) — 정확 등호 산치**
  (`CANON-INCLUSION=TEXTSCALE-FULL-T2-T3-FIXR1`, T4 원문
  `PHONE-SELFTEST rc=0 PASS=506 FAIL=0 1p=23 2t=6 3b=5` — 1p 23 =
  docs/85 원장 수치와 등호). 전 4축 정합.
- 참고: 폰 캐논은 T4 배포 리빌드에서 동시 실측된 것이며 T3 fix r1 시점의 폰
  본식(503 전단계)은 폰에서 개별 실측하지 않았다 — 폰 단정은 커밋 배선 상태
  506 1점이다(단일 커밋 1파일 probe, T4/T4-fix r1 커밋은 캐논 무기여,
  MARKER-AFTER 게이트로 소스 동일성만 단정).

### §2.2 폰 3레그 영수증 (`phone_text_scale.sh` — 2026-10-09 실측, T4)

폰 Termux X11, 캡처 1920x1005(x11grab — 1080 아님 §3 #12).

| leg | settings | 밴드 px | 타이틀 잉크 | AA색 | 전진(px/글자) | 경고 |
| --- | --- | --- | --- | --- | --- | --- |
| def15 | font_path 유지·font_scale **키 삭제**(=posix 기본 1.5 발동) | **32** | **13행**(rows 11..23) | **7**(벡터 AA) | **12 균일**(=engW 8×1.5 등호) | 0/0/0/3(HangulManager 잡음원만) |
| s19b | font_scale "1.9" 재시험 | **38** | **16행**(rows 13..28) | **8** | **15 균일**(=floor(8×1.9)=15 등호) | 0/0/0/3 |
| wake | 1.5 기본 복원(키 삭제)·BOOT-OK | 32 | 13행(rows 11..23) | 7 | 12 균일 | 0/0/0/3 |
| ref10 참조 (`desk_s10`) | scale 1.0(구판 정상) | 24 | 8행(rows 4..11) | 1(비트맵) | 8 균일 | — |
| ref19o 참조 (`desk_s19`, 구판 1.9 결함) | scale 1.9 | **24(고정 상수)** | **4행(rows 1..4 — 플러시탑 클립)** | 1 | 15곳 조잡 런(3,2,11,14,4,… — 스트레치 비트맵 서명) | — |

- def15/wake 캡처 **md5 등호**(8e0d4fd4386535129383237575e9bd0a, 894261B) —
  같은 상태는 픽셀까지 재현(결정론). s19b md5 698ee0f86099909c2e86aad156d8f76c
  (895661B)도 run 간 등호 — settings→아틀라스→그리기 전 경로 결정적.
- **제목 잉크 확대 산술**: 비트맵 고정 8행 → 1.5 13행 → 1.9 16행. 비례 검산 =
  1.5 실측 13행 × 셀 30/24 = **16.25** vs 1.9 실측 16행 — 부합(%.1f 16.2는
  같은 값). AA 색 1 → 7/8 — 단색 비트맵이 아닌 **벡터 경로 렌더**의 서명.
- **밴드 산식 등호**: `ComputeChromeTitleBarHeight(cellH)=max(24, cellH+8)` —
  셀 24(1.5, ComputeCellMetrics(1.5)={12,24,24})→**32** 등호 / 셀 30(1.9,
  floor(16×1.9)=30)→**38** 등호. 구판 1.9는 24 고정 상수 잔존(결함 축).
  `AppContentTopOffset()` = 밴드+6 → 38(1.5) — T3 fix r1의 9곳 앱 소비점이
  실제로 따라감(T4 BUTTON 박스 rows_in_box 4..29 상대 기하 불변으로 검출).
- **1.9 해소 단정**: 구판 rows 1..4(플러시탑 클립·BAND 24) → 신규 rows 13..28
  (밴드 38 내 중앙, 하단 마진 9행) — 클립·밀려남 소멸(S19B-RESOLVED).
- GEOM-CHECK: 서버 기하(x=520 y=190 dw=320 dh=380) 대비 검출(x=840 y=332) —
  미러 레터박스 오프셋 불안정(§3 #11) — 판정은 전역 구조 검출로 기하 무의존.

### §2.3 폰 캡처·로그 영수증 (engine/tmp, md5 — T5 재계산)

| 대상 | md5 | bytes | 의미 |
| --- | --- | --- | --- |
| `desk_s10.png` | `821aa86b70cdc1173a7f17c55bd10e0f` | 424178 | 구판 1.0 정상 참조(T1 legR·T4 ref10 계열 — 비트맵 8행/밴드 24) |
| `desk_s19.png` | `dc2c1a4635164e5e36c60d3a1f0d31c4` | 424102 | 구판 1.9 결함 참조(T1 legP19·T4 ref19o 계열 — 셀만 1.9·글리프 클립) |
| `desk_def15.png` | `8e0d4fd4386535129383237575e9bd0a` | 894261 | T4 1.5 기본 출하 룩 |
| `desk_s19b.png` | `698ee0f86099909c2e86aad156d8f76c` | 895661 | T4 1.9 재시험(해소 상태) |
| `desk_wake.png` | `8e0d4fd4386535129383237575e9bd0a` | 894261 | T4 복원 BOOT-OK — **def15와 md5 등호(크로스-run 결정론)** |
| `ptx_driver.log` | `1050b6d6a55587e735699ab99fc89770` | 14112 | 2차 run 드라이버 원문 197행(T4 fix r2 가드 이동 후에도 불변 — §3 #10) |
| `ptx_phone_run.log` | `ca6b3afa9bffa4dc9bff4f82c58edd02` | — | 원격 run 원문(T5 실측) |

- crop 산출물: `engine/tmp/ptx_{def15,s19b,wake,ref10,ref19o,leg10,leg19o}_
  {mine,band3x,content2x}.png` 21건(T4 리뷰 ls 전수 일치).
- WSL 축 영수증(T1/T2/T3): T1 legA..legZ 6레그(결함 동형 재현+legC 배선
  실험+legZ 원복 등호), T2 legB1=**T1 legC와 픽셀 byte 동일**(mean_abs 0.00 —
  결선 재판정 성립)·legD2(1.5 실화면)·leg0(폴백 1.5 확대+경고 1회 계약), T3
  밴드 before 24 → after 32(잉크 12→17행 비클립, 리뷰어 PNG 직접 디코드
  재실측).

## §3. 함정 원장 (T1-T4 리포트+리뷰 종합 — docs/85 §3 문체 계승)

1. **근원 단정 = 후보 ⑤ "클라가 설치 안 된 다른 dc_로 그린다"(5후보 판정표의 정단)** —

   | 후보 | 판정 | 핵심 근거(T1 원문) |
   | --- | --- | --- |
   | ① 클라 설정 미도달 | 배제 | `/proc/self/exe`(JKFs_posix.cpp:23) 절대경로 — 클라 아틀라스는 클라 프로세스가 settings 직독; legX19에서 클라 3프로세스(server+taskbar+minesweeper, stderr 상속)가 전부 `no vector font configured` 경고 = 클라가 같은 settings.json을 실제로 읽음 |
   | ② font_path 빈 | 배제 | 폰 settings 원문 `{"text":{"font_path":"…/malgun.ttf"}}` — path 존재·readable; legP19 경고 0건 |
   | ③ Init 실패 | 배제(결함 원인 아님) | legP19/legA-D 경고 0 = Init 성공; WSL 출하 상태 leg0의 `vector font init failed(NotoSansCJK-Regular.ttc)`는 **CFF 아웃라인 문 열기 게이트 — 설계 동작**(docs/70 §8.4)이며 legA/B에서도 결함 재현하므로 결함 원인 아닌 별개 사유 |
   | ④ DrawGlyph 내부 실패 | 도달 불가 | `engine/src/JKDC.cpp:198` `if (!textAtlas_ || !textCache_ || !backend_) return false;` — 그리기 지역 dc에는 textAtlas_가 없어(:⑤) 조용히 false → 비트맵 폴백; legC에서 배선만 하면 체인 전체가 벡터로 작동 = 내부 기구 정상 |
   | ⑤ 죽은 dc_ 배선 | **CONFIRMED — 근원** | `JKClientApplication.cpp` ComposeScene(:745-779)이 **지역** `JKDC dc(cmdList)`를 만들고 `SetHangulManager`만 설치(:751-752) — `SetTextAtlas`는 멤버 `dc_`(:149·:200)에만 있는데 `dc_`는 grep 전수 결과 실제 그리기에 쓰이지 않는 죽은 멤버. `textAtlas_/textCache_`는 인스턴스별 멤버(JKDC.h)라 지역 dc가 못 물려받는다 → DrawGlyph 진입 불가. 한편 전진폭은 싱글턴 `GetCellMetrics`(JKTextAtlas.cpp:142-163)를 그대로 쓰므로 **셀만 1.9배** — 관측 결함과 정확히 일치 |
   - 봉합 실험: (a) legC 동형 3행 임시 배선에서 벡터 전환(AA 402px vs 비트맵
     상수 145px), (b) legP19↔legX19 전체 프레임 byte 동일(md5 등호) = settings
     font_path 유무와 무관한 **출력 불변** — 코드 읽기 없이 ⑤ 확정. 싱글
     프로세스 같은 수형 죽은 배선(JKApplication.cpp:471)을 T2에서 함께 수리.
2. **posix 기본 1.5 배선** — `DefaultFontScale()`(JKTextAtlas.h, 컴파일타임
   플랫폼 상수: Win 1.0/posix 1.5, 사용자 스펙 확정)를 GetCellMetrics 미설정
   분기가 소비. posix를 1.5로 결정한 근거 = posix 클라 글리프가 비트맵 고정
   결함(T2 수리) 전의 1.0 출하 룩보다 폰 원거리 가독 우선 — 스펙 사용자 결정.
   폰 settings는 `font_scale` 키 **삭제**가 1.5 기본 발동(키 존재=명시값).
3. **비트맵 폴백 nearest 셀 확대** — PutEngGlyph8x8/PutEngGlyph8x16/
   PutHanGlyph16x16이 `StretchNearestIndex`로 셀 스패만큼 확대(8x8은 cellH/2
   밴드). s=1.0에서 매핑 항등 = Windows 픽셀동일(2t-d/e/f 항등 샘플 단정).
   KSSM 쌍 폴백은 **hanW=2×engW 유도로 4/9px 홀수 폭**이 생길 수 있어
   nearest 매핑 오차 수용 — fail-safe 계약(코드 주석+2t 주석, 단정 없음;
   정상 경로는 벡터 아틀라스).
4. **폴백 진입 경고 프로세스당 1회** — `WarnVectorAtlasInactiveOnce()`
   (static once)가 EngPutCh/HanPutCh 폴백 진입에 1회 인쇄 — 원인 단서 병기
   (`font_scale=%.2f, atlas not attached`). 실측 leg0 2행 = 클라 프로세스
   2개(taskbar+minesweeper)각 1회 — 로그 수준 중복 아님. 멀티 클라이언트
   세션의 서버 로그에는 클라 수만큼 남는다(stderr 상속).
5. **★T3 구조 발견: 크롬 타이틀 밴드 채색·타이틀 글리프는 클라가 그린다** —
   brief는 서버 JKWindowServer.cpp(~8100)를 밴드 소유로 전제했으나 실측상
   밴드 채색+타이틀은 **클라 JKWindow.cpp PaintWindow**(단일 프로세스
   Windows 모드·posix 클라 모드 공유 코드)가 하고, 서버는 ①히트테스트 존
   ②ClampTitlePassthrough ③승인 배너 밴드 두께만 지오메트리를 미러한다.
   서버가 타이틀 문자열을 직접 그리는 경로는 없음(Utf8ToKssm 유일 소비=
   배너 :8209). 스펙 앵커 주석의 "밴드" 소유측 정정 — brief 정정을
   구현자가 실측으로 수행한 정직 사례. 때문에 **타이틀 글자 확대는 T2 클라
   결선이 자연 승계**하고 ADJ_YCENTER 세로중앙정렬(:303-319)이 밴드를 따라
   글리프를 확대 — 별도 크기 코드 불요.
6. **`ComputeChromeTitleBarHeight` 산식 승격** — 구 고정 상수 24
   (`JKCompositor.h` "MUST stay in sync" 계약 상수)를
   `max(24, cellH+8)` 순수 산식으로 승격해 진실원 단일화. Windows 등호 = 산치:
   `ComputeChromeTitleBarHeight(16) = max(24, 24) = 24` — 24 = 셀 16 + 상여백
   4 + 하여백 4(ADJ_YCENTER 중앙정렬 분해)로 구 상수와 정확 등호 → Windows
   무변을 코드 없이 증명(기존 524건이 리터럴 24 기대 유지 PASS = 실증).
   배너 텍스처 캐시키는 `BannerCacheKey(utf8+\x1f+engW+x+cellH)`로 크기
   봉합 — metrics 동적화 대비 무료 보험(툴팁/글리프 LRU는 재시작 적용 계약으로
   스테일 불가, 원장 판정 T3 리뷰).
7. **`AppContentTopOffset()` 9곳 전수 교체(T3 fix r1)** — I-2: ImGui 클라 앱
   9곳의 고정 `topY=30`(밴드 24 가정, posix 밴드 32에서 본문 상단 2px가 서버
   타이틀 이동 그랩 존과 겹침/똑같이 마우스다운이 창 이동으로 빨림)과 I-1:
   MineSweeperApp.cpp:961 `clientH+24+2`(posix에서 8px 부족 리사이즈)를 공용
   진실원 `ComputeAppContentTopOffset = ComputeChromeTitleBarHeight(cellH) +
   kContentTopMargin(6)`으로 교체 — 여백 6은 별도 상수로 보존(s=1.0 등호
   24/30). 리뷰어 grep 전수 잔존 리터럴 0건 재실측. 9곳: AgentMgr/Browser/
   Chat/Files/Library/Notes/Settings/Shot/VPlayer + MineSweeper.
8. **T4 S19B 임계 정정 원장(정직 기록 축)** — T4 1차 run에서 S19B-VERDICT가
   HOLD로 나온 임계 "잉크 ≥18행"은 WSL 17행 브래킷에서 온 **산치 오차**였다.
   교정 = 비례(1.5 잉크 13행 × 셀 30/24 = 16.25 vs 실측 16)+비클립+비플러시탑
   → 2차 run에서 S19B-RESOLVED. 교정 원장은 probe 헤더에 **커밋 원문으로
   부기**. 단, 1차 run 보존물(로그·캡처)은 소각되어 1차 md5 등호/S19B-HOLD
   원문 행은 재검증 불가 — **정직 부기(M2)**. 교정 근거는 임계 오차 뿐이고
   라벨 방향(클립·플러시탑 소멸)을 바꾼 측정 변경 없음 — verdict shopping
   아님(T4 리뷰 승인).
9. **T4 fix r1 — PHONE_HOST fail-closed 가드 계약 회귀 수리(I-1)** — T4
   본식 커밋이 PHONE_HOST unset→LAN 8022 스윕 폴백(48 병렬 wait -n)을
   신설 = 커밋 697db8b("phone probes는 PHONE_HOST 필수 가드로") 계약 회귀 +
   형제 probe 전부 필수 가드 전례와 배치. 제거: 스캔 블록 ~39행 삭제 + 대역
   리터럴 제거 → `unset 즉시 TS-PHONE-FAIL rc=1`(실측 원문 ptx_failclosed.log —
   pre-flight/ssh 0건). 4축 입증: HEAD grep 구동코드 0·unset 독립 실행 rc=1·
   measuring 로직 무접촉·단일 커밋.
10. **T4 fix r2 — 가드 위치 원장(M5 폐곡, 본 라인 T5)** — fix r1의 가드가
    `exec > >(tee "$RLOG")` **뒤**에 있어 PHONE_HOST unset 재실행이 tee
    truncate로 마지막 실측 영수증 로그(ptx_driver.log)를 통째로
    덮어썼다(re-review 중 실제 발생 — 리뷰어가 사전 백업으로 복원). 수리 =
    가드 검사를 exec 행 앞으로 이동. **T5 실측**: `env -u PHONE_HOST` 실행
    rc=1·TS-PHONE-FAIL 1행·`ptx_driver.log` md5 1050b6d6a55587e735699ab99fc89770
    **변동 없음(재실행 후 동일)** — unset 재실행이 로그를 보존함이 동작으로
    단정됨. measuring 로직 접촉 0(diff=가드+주석 이동만).
11. **GEOM-CHECK 오프셋 불안정** — list_window 좌표계(데스크톱면)와 x11grab
    캡처(미러면) 사이 레터박스 오프셋이 run마다 흔들림(1차 시대 +320 vs 2차
    320/142) — 판정은 밴드/잉크 전역 구조 검출로 기하 무의존화. 서버 기하는
    GEOM-CHECK 원문으로만 기록.
12. **폰 화면 = 1920x1005(1080 아님)** — 1920x1080 x11grab 요청은
    `Capture area ... outside the screen size` 실패 → 파서+재시도로 흡수.
13. **advances 측정 원문 출처 불명(T4 M1)** — 전진 런(`advances=[12,…]`)이
    커밋 probe에는 없는 임시 측정 스크립트 산출물로 보존물 상실 — 재생산
    불가. 판정(밴드/잉크/AA색/rows)에는 투입되지 않은 보조 근거였고 전진
    균일성은 T4 리포트 수치로만 남아 있다. **재실행 시 probe에 편입 권고**
    (원장 고정 기록).
14. **LED-DIGITS 카운트=306 동일 관측(결함 아님)** — 지뢰찾기 미니 캡처의
    LED 잉크 카운트 306가 4원천 전부 동일(타이머 미시작·같은 자리수 추정) —
    판정은 LED를 쓰지 않았다(라벨·밴드 중심).
15. **probe 트랩(승계 원장)** — 원격 스크립트는 scp 후 파일 기동(argv 금지)+
    `rm -f -- "$0"` 자기 소각(REMNANT-LS-RC=2 실측 2차 run 포함)+브래킷
    pkill `[j]kdesktop`+타이밍 2s×5 루프 등 — docs/85 §3 #12 원장 계보 +
    본 라인에서 검증 재실측(리뷰: 전 지점 probe 원문 판독).
16. **drvfs 링커/WSL git CRLF 팬텀/posix 빌드 `$?` 파이프 함정** — docs/85 §3
    #14+기억창고 레슨 원장 승계, 본 라인 T1(WSL 임시 배선 재빌드+원복 cmp)·
    T3 fix r1(부수 원장)에서 재실측.

## §4. Deferred (소등 후보 — 유예 라인, docs/85 §4 문체 계승)

| ID | 사항 | 근거 |
| --- | --- | --- |
| D1 | 크롬 타이틀 글리프 벡터화(밴드 내 벡터 룩 고급화) | 스펙 설계상 **비 포함**(교차 없음) — 스펙 구현 기법은 기존 비트맵 경로 연동만, T2가 DrawGlyph를 연 것이므로 실현로는 이동 가능 |
| D2 | 닫기/최대화 버튼 20px 밴드 연동(kChromeCloseSize/MaximizeSize 원문 상수 유지) | T3 concerns 1 — 1.5 밴드 32에서 버튼 중앙/여백 미정렬(미관), 기능 결함 아님 |
| D3 | X11 DPI ↔ 폰 텍스트 스케일 상관 관측 | 본 라인 스코프 밖 — docs/78 DPI 라인 승계 |
| D4 | DeX fit-scale × 텍스트 스케일 상호작용 관측 | 본 라인 미실측 — DeX 진입 기회에 별도 관측(docs/85 §4 D8 가문) |
| D5 | 폰 관찰 항목: 프레임 1 no-op — 첫 compose 프레임 글리프 ≤16ms 블랭크 후 프레임 2부터 벡터(pending flush 설계) | T2 룰링 수용·원장 부기 — 폰 육안에서 관찰 대상(이상 지각 시 사전 래스터라이제이션 검토), T2 리뷰 Minor |

### §4.1 원장 park 목록 (리뷰 Minor 종합 — 전부 비임계)

| 출처 | 사항 | 처분 근거 |
| --- | --- | --- |
| T2 | 비원자 `static bool warned`(atomic bool 1줄) | 단일 compose 스레드 도달 불가 — 리뷰어 실측 확인, 후속 클린업 라인 원장 |
| T2 | 죽은 배선 잔존 — 멤버 `dc_`+`SetTextAtlas`(:149/:200·:138) 미청소 | 동향 파악 필요(도달성 검증) — **별도 백로그**(클린업 후보, T2 리포트) |
| T2 | `JKApplication.cpp:134` 구주석("기본 1.0") | 주석 stale — 다음 접촉 시 갱신 |
| T2 | floor 매핑 좌측 쏠림 | fail-safe 계약 내(정상 경로=벡터) |
| T2 | `jktext_probe.cpp` T11(b) posix 스테일("미설정 기본 {8,16,16}") | T2 접촉 파일 밖 — 프로브 캐논 소유 축 후속 라인에서 플랫폼 분기 갱신 권고 |
| T2 | LRU 재진입 블랭크 | 리뷰 Minor — 재현 조건 좁음 |
| T3 | 폐기 주석 클러스터 10여 곳("first 24 px"/"상단 24pt") | I-2 정리 접촉 시 일괄 갱신 권고 |
| T3 fix r1 | ClientFileDialogApp.cpp:213 "kTitle=24" stale 주석 | 코드는 `GetClientRect()` 유동 — 안전 |
| T3 fix r1 | jkbridge/jkagentd 기동 카운트를 T4 진입 복원 게이트에 원문 첨부 권고 | T4는 전부 원자적 복원 수행 — 향후 probe 강화 제안 |
| T4 | advances 측정 probe 편입 | §3 #13 |
| T4 | 1차 run 원문 상실(재검증 불가) | §3 #8 정직 부기 |
| T4 | HEAD blob 스테이징 `mkdir -p "$(dirname …)"` 부재 | WIPPED>0 분기(선례 phone_dirty_present.sh 동형 결함 승계) — fail-closed 방향이라 잠재, 전화 probe 2건 1행 수리 후보 |
| T4 | coerce 분기 계약(진입 settings에 font_scale 있으면 키 제거 상태로 종료 — 진입 원문은 로그 행으로만 보존) | disclosure 원문 인쇄 이미 존재 — 계약 부기 |
| T1 | WSL probe legD settings 잔존(M1) 등 probe 일회성 minors 5건 | T1 probe는 진단용 — 원장 그대로 |

후속 라인 착지(2026-10-09): 갤러리(#82) as-built = `docs/87_gallery_asbuilt.md`
— 폰 캐논 506→546(CANON-INCLUSION=GALLERY-FULL-T2G-40, 2g 40건) 등호 승계
원장 포함.

## §5. 사용자 결제 게이트 (EYES-PENDING 봉인)

**EYES-PENDING — 라인 최종 결제는 사용자 육안 선언만으로 성립한다. 본 문서와
probe는 결제를 기록하지 않는다.** T4 리뷰가 결제 기록 문장 0건(grep 8건 전부
부정형)을 입증했고, 본 라인의 결제 상태는 **미결제(대기)**다.

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | 폰 육안 | 1.5 기본 출하 룩(`desk_wake.png`, `ptx_wake_mine/band3x.png`): 제목·버튼 라벨·숫자 읽기 크기, 클립·밀려남 소멸 / 1.9 재시험(`desk_s19b.png`)도 구판 `desk_s19.png` 대비 셀+글리프 동시 확대 해소 | **대기(육안)** |
| ② | Windows 싱글 모드 글리프 벡터 AA 전환 | T2의 양축 결선 룰링으로 Windows 싱글 프로세스(사용자 표준 시험 모드) 글리프가 **비트맵 → malgun 벡터 AA**로 전환(기하·전진 = 픽셀동일, 잉크만 상이). **거부 시 부분원복 시나리오(리뷰어 확인 수형): 싱글 배선 3행만 원복** — `JKApplication.cpp`의 지역 dc `SetTextAtlas` 결선 3행(`if (textAtlas_ && textAtlas_->IsLoaded() && resourceCache_) dc.SetTextAtlas(...)`)만 되돌린다. **GetCellMetrics 미설정 기본 분기(DefaultFontScale)·posix 클라 결선·밴드 산식은 원복 대상이 아니다**(posix 기본 1.5 스펙 결정과 무관하게 소취) | **대기(육안)** |
| ③ | #80 브라우저 자연어 승격 결제 | 본 라인과 별개(#80 승계 — 별도 진행) | **대기(별도)** |

## §6. 커밋 원장 + IP grep 게이트

- 커밋 체인은 §1 표(전부 T5 실측 rev-parse 전체 SHA) — 54d5363 → c738f27 →
  33eda05 → 697db8b → 40792d7 → 24d22a1 → 58861c3 → 8563263 → b651534 →
  bdbc687 → 0cde36d → 997693d(fix r2) → 본 T5 커밋(git log가 진실원). 폰 캡처
  PNG는 engine/tmp 미커밋 — 커밋 원문 IP 리터럴의 유일 출처 후보는 probe이며
  dotted-quad·대역 표기(`192.168.11|219`)는 fix r1에서 소각됐다.
- grep 게이트(T5 실측): 스테이지 전체 `git diff --cached` +행 한정
  dotted-quad `([0-9]{1,3}\.){3}[0-9]{1,3}` = **0건**(RC=1)·
  `192.168` = **0건**(fix r2 커밋). docs/86+docs/85 수정 커밋 원문:
  `git show <T5 커밋>` 전체 행에서 `192\.168\.(11|219)\.` = **0건**(§6 게이트
  증명 — 대역 리터럴 문맥 표기 없음; #83 레닥션 계약 697db8b 유지).
- PHONE_HOST 접속 정보는 환경변수만(기록 금지 계약 — docs/84 표기 정정 원장
  승계). 커밋 대상 untracked/잔존 대상 소각 없음(git add 명시 경로만 —
  #83 사건 레슨).
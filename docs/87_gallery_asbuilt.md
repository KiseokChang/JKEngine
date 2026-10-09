# docs/87 — 갤러리 앱 as-built (T1-T6, 2026-10-09)

갤러리 라인(#82) T1-T6 원장. 계획 문서=`docs/superpowers/plans/2026-10-09-gallery.md`,
스펙=`docs/superpowers/specs/2026-10-09-gallery-design.md`(초고 40792d7 → 스펙 확정
6c6382c), SDD 원장=`.superpowers/sdd/2026-10-09-gallery/`(progress·task-1..6
brief/report/review — 워크숍 원장). 코드 계보는 git log가 진실원. 본 문서는
실측 영수증·함정·deferred·사용자 결제 게이트를 봉합한다.

## §0. 문서 체계

docs/85(더티프레젠트 as-built)·docs/86(텍스트 스케일 as-built)의 §0-§6 구조를
계승한다 — §1 배선 원장(rev-parse 전체 SHA)·§2 캐논 표+캡처 영수증·§3 함정
원장(정직 기록 축 포함)·§4 deferred+park(§4.1)·§5 사용자 결제 게이트
(EYES-PENDING)·§6 커밋 원장(IP grep 게이트 병기). 창작·추정 수치 소각 — 모든
SHA·캐논·측정치는 SDD 원장 리포트/review 원문에서 인용했고, 본 문서 작성
시점(T6)에 새로 실측한 것만 "T6 실측"으로 부기한다.

발단: docs/78 갱신 시 예약("사진 갤러리 — 별도 문")→#82 승계. 사용자 의도
"사진 모아보기 허브". 스펙 결정 3건(①A안 신규 gallery 모듈 ②소스
디렉터리=state/screenshots 공유(읽기 전용)+settings `gallery.dirs` ③v1
스코프=격자+전체 보기+이전/다음+메타 행)은 컨트롤러 재량 확정(사용자 "재량껏
쭉쭉 진행하라" 지시, 2026-10-09 — 기각 시 수리 커밋 계약). worktree 미사용·main
직행·최종 브랜치 리뷰 생략(#80/#81/#84 선례 — 태스크 리뷰 전부 Pass 전제).

## §1. 배선 원장 (라인 전체 — rev-parse 전체 SHA 실측, T6 재측정)

| 태스크 | 커밋(SHA — T6 실측 `git rev-parse`) | 내용 |
| --- | --- | --- |
| 직전 라인 종착(#84 텍스트 스케일 T5) | `1a73fac1a1db1b0c7f9c0efe92145aa72efd1e01` | docs(font): posix 텍스트 스케일 수리 as-built (T5) |
| #83 park 소각 배치 1(라인 밖) | `eb13f3ba9895fa69a0ec821694896f89bb2ba6b7` | chore(server): park 원장 소각 배치 1 (캔런 불변) |
| #83 park 유예 소각(라인 밖) | `bf2b3b6c984a457452f2e9bc15e58bd35e187efe` | chore(server): park-batch 유예 소각 — probe 주석 정정+WSL 레시피 갱신 (M-r1/r2) |
| 갤러리 스펙 초고(라인 밖 끼어듦) | `40792d7930fcf611b605556173168c944279ed50` | docs(gallery): #82 갤러리 앱 스펙 초고 — A/B안+사용자 결정 3건 대기 |
| 스펙 확정+플랜(라인 BASE) | `6c6382c81d3adb4ba7ade29864786ccab5c24d5d` | docs(apps): 갤러리 스펙 확정(재량 결정 3건)+구현 플랜 (#82) |
| T1 | `56f26d9476199d7de98885b2b4ce267e4d49315c` | feat(apps): gallery 모듈 골격 — 격자 뷰+settings gallery.dirs (T1) |
| T2 | `298d08f68f26ddaabab722bcb5deec88d5d49461` | feat(apps): 전체 보기 모드 — 핏+이전/다음 (T2) |
| T3 | `c611c8029a7523fead3168aa4bebce7b023583d9` | feat(apps): 썸네일 캐시+nearest 축소+텍스처 풀 (T3) |
| T3 fix r1 | `2a57b4d506dc06097ef43cf910c7eb16afd4e81b` | fix(apps): LRU 퇴출이 동일 프레임 기록 텍스처 파괴 — 프레임 세대 제외 (T3 fix r1) |
| T3 fix r2 | `18274d07e1c175aafa9cacca6740cc0638fab388` | fix(apps): LRU 요청을 가시 셀로 컬 — 영구 기아 봉합 (T3 fix r2) |
| T4 | `1c3aafd937bfee1a8fb08708d9090e31fadee09a` | test(apps): WSL 갤러리 실측 probe (T4) |
| 백로그 명시 | `390c741e8cd314854ea1e7e6307bbcf8a9b5df86` | docs(apps): 갤러리 백로그 명시 — 증분 재스캔+music 라이브러리 후보 (사용자 피드백 승계) |
| T5 rider | `f88b3caaa2985f52ef9337440da9ee7aef2ce12f` | fix(server): 진단 probe XImage 마스크 오프셋 교정 (T4 리뷰 소각) |
| T5 본문 | `b245cf67d3077b8cdb5e0c079c46164205ec60c8` | test(apps): 폰 갤러리 실측 probe (T5) |
| T6 | 본 문서를 포함하는 커밋 | docs(apps): 갤러리 as-built (T6) — SHA는 git log가 진실원 |

T3 폐곡 계보: c611c80 **REJECT**(C1 동프레임 파괴) → 2a57b4d **REJECT**(C2 영구
기아) → 18274d0 **APPROVE**(Critical 0) — fix rounds 2건 소각. T1/T2/T4/T5
리뷰는 전부 APPROVE(Critical 0·Important 0).

파일 축(T6 실측 — 현행 wc/grep):

- `engine/include/apps/GalleryModel.h`(338행) — **순수 부품 헤더 공개**
  `jk::gallery`: GalleryDirList(기본 폴더 앞+유저 dirs 뒤·빈 성분 제거·폭주
  절단·settings 부재/파손=기본 1건 fail-safe)·NormalizeDirs·FitThumb(s=1.0
  상한)·FitFull(상한 부재 — T2)·WrapStep(modulo wrap)·MakeThumb(nearest 박스
  축소)·ThumbStamp/ThumbKey(경로+size+mtime — 2g-d Fnv1a 소비)·
  GalleryThumbPath(thumbs/<key>.png)·PickLruVictim(fix r1)·ThumbRowVisible
  (fix r2)·kThumbPoolMax=96. imgui/SDL/client 타입 무접촉 — selftest 쌍둥이가
  같은 헤더를 직단정한다.
- `engine/include/apps/ClientGalleryApp.h`+`engine/src/apps/ClientGalleryApp.cpp`
  (644행) — 모듈 앱: 디렉터리 탭(settings 원문 직독 — `state/settings.json`
  fopen→GalleryDirList **텍스트** 전달, LoadDesktopSettingsJson 선례)+
  160x120 고정 셀 격자(플레이스홀더+파일명 클립)+뷰 모드 2상태 스왑(같은
  창)+FitFull 뷰포트 핏+WrapStep 이전/다음(`<`/`>` 버튼+←/→ 키·WantTextInput
  방어선)+Esc 복귀+메타 행(파일명+원본 픽셀 크기)+썸네일 LRU 풀 96(캐시
  히트=LoadImageFile, 미스=전체 디코드+MakeThumb+jk::SaveImageFile best-effort
  기록)+실패 슬롯 스캔 세대 재시도 억제. shot 수형 계승(어두운 루트창·16ms
  타이머·OnThemeChanged·한글 데스크톱 폰트·AppContentTopOffset 밴드 산식
  소비 — docs/86 §3 #7).
- `engine/src/apps/JKAppModule_gallery.cpp`(18행) — **C ABI 계약**
  (`jk_app_meta` = gallery/"Gallery" 560x520, `jk_app_run_client` — 모듈 DLL,
  호스트는 JKAppModule.h C ABI만 본다). posix도 동일(모듈 DLL 계약 — WIN32
  게이트 없음).
- `engine/CMakeLists.txt` — jkapp_gallery SHARED(win/posix 양축 — T6 실측
  :672-678)+`JKX_ICON_APPS` 1행에 `gallery`(:979-983)+gallery.jkx pack custom
  command(:1141-1147)+jkx_packages(:1187). 아이콘 2종
  `assets/icons/launcher_gallery@{1x,2x}.png` — **프로그램 생성**
  (`tools/probes/gen_gallery_icon.ps1`, 3x3 격자 모티프·앰버 1칸 — 기존
  아이콘 복제 아님).
- selftest 쌍둥이: `engine/src/main.cpp`+`engine/tools/posix_selftest/main.cpp`
  (2g-a..j §2.1). posix는 헤더 직링크라 build.sh 무변경.
- probes: `engine/tools/probes/wsl_gallery.sh`(465행 — T4)·
  `engine/tools/probes/phone_gallery.sh`(896행 — T5)·rider
  `wsl_text_scale_diag.sh` 마스크 교정(f88b3ca).
- 와이어 프로토콜 접촉 0 — 모듈 DLL C ABI 내 완결(스펙 A안 계약). posix 런처
  grid는 ScanJkxApps _WIN32로 미개방 → posix/폰 launch는 에이전트 launch 계약
  (pre-flight 판정 정합).

## §2. 캐논 표 + 캡처 영수증

### §2.1 selftest 캐논 계보 (2g 계열)

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#84 텍스트 스케일 종착 = docs/86 최신) | 529 | 506 | 237 | (506) | — |
| T1 `56f26d9` | **548** | **525** | **256** | (506) | +19 (2g-a..e) |
| T2 `298d08f` | **557** | **534** | **265** | (506) | +9 (2g-f) |
| T3 `c611c80` | **566** | **543** | **274** | (506) | +9 (2g-g 4건+2g-h 5건) |
| T3 fix r1 `2a57b4d` | **567** | **544** | **275** | (506) | +1 (2g-i) |
| T3 fix r2 `18274d0` | **569** | **546** | **277** | (506) | +2 (2g-j) |
| T5 `b245cf6` (폰 축) | 569 | 546 | 277 | **546** | 폰 = 506+2g 40 |

- **2g 40건 = 라인 전체 신설** — 원문은 41건이지만 2g-b는 `_WIN32` ifdef 쌍
  (T6 실측 main.cpp:4983-4993: backslash 정규화는 Win 축 소유, posix는 '\'
  를 성분 문자로 소유)이라 **플랫폼당 실행 40계** = 산치 정확(T1 19+T2 9+T3
  9+fix r1 1+fix r2 2 등호). T5 리뷰 "2g=40"과 등호.
- **T6 전 축 즉석 재실측(2026-10-09)**:
  - Win `engine/build/jkdesktop.exe test` — **PASS=569 FAIL=0, RC=0**,
    `AppSelfTest: 0 failure(s)`(`engine/tmp/gal_t6_win_selftest.log`).
  - WSL `buildwsl/jkdesktop test` — **PASS=546 FAIL=0, RC=0**
    (`gal_t6_wsl_selftest.log`).
  - posix `engine/build/posix_selftest` — **PASS=277 FAIL=0, RC=0**
    (`gal_t6_posix_selftest.log`).
  - 각 로그 `[PASS] 2g-` 행 = **40건 등호** (grep 실측).
  - 재실측 유효성: 18274d0 이후 `engine/src|engine/include|engine/CMakeLists.txt`
    변경 0건(git log 실측 — T4/T5/T6 커밋은 probe/docs만) → 빌드 3본(10월 9
    12:12/12:19/12:33 — T3 fix r2 배포 창)의 원천이 HEAD src와 동일.
  - 폰: T6에서 기기 재실행하지 않음(기기 계약) — **원장 run 4 원문 재독
    실측**: `pgal_phone_run.log`(10월 9 14:06) `PHONE-SELFTEST rc=0 PASS=546
    FAIL=0 2g=40` + `CANON-INCLUSION=GALLERY-FULL-T2G-40` 등호 확인.

### §2.2 캡처 영수증 (engine/tmp — **전부 untracked, 커밋 금지 충족**)

md5는 전부 **T6 재계산**(md5sum 실측).

**WSL 축(T4 — 1런 GAL-OK, 원장 리포트):** selftest 546 등호(2g 40건 PASS)·
`THUMB-CACHE-CNT=9/9`(placeholder 0)·send_input 3건 sent:true. 복원 완료(서버
DOWN·시드/캐시/permissions 소각·REMNANT=로그).

| 캡처 | bytes | md5(T6 재계산) | 내용 |
| --- | --- | --- | --- |
| `gal_wsl_grid.png` | 904637 | cf7e0c7ad603bb9c4c1bd919ee96f3ad | 격자 |
| `gal_wsl_full.png` | 870514 | 974db0cc9832f0df93352d6849af7c34 | 전체 보기 |
| `gal_wsl_prev.png` | 870659 | 4fd0fed6d604a1a1d6c0f4727ca43be4 | ← 이전 |
| `gal_wsl_back.png` | 904837 | 6a2ab38889819ec3b651702b946ab79f | Esc 복귀 |
| `gal_wsl_surface_ref.png` | 52956 | 2a73e51ffec64808f12f05696a4b8fb3 | surface_ref 1본 |

**폰 축(T5 — run 4 GAL-PHONE-OK, 4 runs 원장 §3 #9):** leg grid
`THUMB-CACHE-CNT=4`(시드 4셀 decode — THUMB-CHECK OK)·click `CLICK-CELL-0`+3건
sent:true(좌클릭/←wrap/ESC)·leg dirs `SETTINGS-GATE-OK: gallery.dirs 주입 성공
(font_path 유지)`+`THUMB-CACHE-CNT-DIRS=4`+`SDCARD-STATE: dir-present`·leg boot
복원 부팅 BOOT-OK(시드 소각 후 빈 목록="사진이 없습니다" — 정상)·종료
`SETTINGS-RESTORED-BYTES: 121 등호`+`PERM-RESTORED-BYTES: 33 등호`+
`REMNANT-LS-RC=2`.

| 캡처 | bytes | md5(T5 run 4 / T6 재계산 등호) | 내용 |
| --- | --- | --- | --- |
| `gal_phone_grid.png` | 687840 | 59ce02f6ba730a07df72cc61ff5c86dc | 격자+시드 4장+한글 라벨+극단 종횡비 2장 |
| `gal_phone_full.png` | 667373 | fd976fd59deeb87172cd54a280f9c750 | 셀 0 클릭 → 전체 보기 핏 |
| `gal_phone_prev.png` | 666944 | 588cd755ef99f0ab7e7497921ddbc376 | ← 꺾임 = index 0 wrap → 100x800 시드 |
| `gal_phone_back.png` | 687641 | c8f1e5e0df00d986d06a8f93dfaea510 | Esc → 격자 복귀 |
| `gal_phone_dirs.png` | 688604 | 137fae9231de89da09a306e41f85052e | "2개 폴더" 탭 screenshots \| Pictures |
| `gal_phone_boot.png` | 644746 | 0c82acf3fec7118f07100e87ac598a27 | 복원 후 BOOT-OK 빈 격자 |

- crop 보조 6종: `pgal_{grid,full,prev,back,dirs,boot}_crop.png`(624x584 —
  GAL-GEO 기하+마진). T6 md5: grid 059ce568cee40ab60b5282b394881f45·full
  a2bf699f6d9b05c6799a69c5d63bfc9a·prev c56a307e9c4855f579f1de33568896dc·back
  bd6e2dedd8ad7c1b4804548ca58cc4cd·dirs d120864344efedc0f2a08b3641d1c36e·boot
  daeda6e8f7c3cdaa59640302c2f6ef67.
- 구동 로그: `pgal_driver.log`·`pgal_main4.log`·`pgal_phone_run.log`(run 4
  원문 — 위 캐논/영수증 인용의 유일 출처) + T6 셀프테스트 로그 4건(gal_t6_*).
  **전부 engine/tmp untracked** — 육안 결제 후 원료 소각 권장(T5 concern 4
  승계).

## §3. 함정 원장 (T1-T5 리포트+리뷰 종합 — docs/86 §3 문체 계승)

1. **stbi_write TU 위치(T3)** — 썸네일 디스크 캐시 기록(stb_save)의 봉합
   위치. 서버 TU 래퍼 (a)는 libjkserver.a 링크 구조상 불가 원문 판정 → (b)
   `jk::SaveImageFile`을 `JKImageLoader.cpp`(jkcore)에 **STB_IMAGE_WRITE_STATIC**
   곁들임 구현. 리뷰 re-review 실측: 2정의 전부 STATIC — jkwinserver 함께 링크
   무충돌·빌드 RC=0.
2. **★LRU 동프레임 퇴출 파괴(T3 C1 — REJECT, fix r1 폐쇄)** — 풀 96에서 폴더
   210장이면 97번째 요청의 victim이 **이 프레임에 AddImage된 직전 셀** =
   RenderDrawData가 파괴된 SDL 텍스처 렌더. 1차 구현의 "same-tick 1건 제외"
   방어선은 동일 tick 1건만 제외해 무의미(리뷰 시뮬 유령 12.0건/프레임).
   수형 = 프레임 세대 `thumbFrame_++`+`useFrame` 스탬프+순수
   `PickLruVictim`(동일 프레임 접촉분 전부 제외, 후보 0=-1 → placeholder) —
   2g-i.
3. **★가시 셀 컬 기아(T3 fix r1 재리뷰 C2 — REJECT, fix r2 폐쇄)** — C1 방어선
   만으로는 요청 루프가 무컬이라 **매 프레임 처음 96개가 슬롯 전부 접촉 →
   후보 0명 → 97번째부터 영구 placeholder**(210장 중 114장 영구 기아 —
   퇴출성 결함이 불가성 결함으로 퇴보). 수형(권고 1안) = 썸네일 요청을 child
   클립rect 교차 셀로 컬(순수 `ThumbRowVisible`)·레이아웃 전수 불변(클릭 히트
   유지)·C1 방어선 유지 — 2g-j. 재판정 입증: 210장 25프레임 슬라이드 시뮬
   유령 0·기아 0(슬롯 접촉 ≤ 가시 < 96 → LRU 회복). 가시>96 극단은 후보 0 →
   placeholder 계약화(§4.1 N2).
4. **XImage 캡처 마스크 오프셋 3틀림+폭=popcount(T4 — 선례 재현, T5 rider
   소각)** — tsd/ts 선례 재현: 오프셋 3값은 **56/64/72**(선례 틀림본 48/56/64
   아님)+마스크 폭 = **popcount**(bit length 아님 — 2런 교정으로 색 충실).
   T4가 wsl_gallery.sh에서 교정(236-241·332-337행)했고 **선례
   wsl_text_scale_diag.sh:69에 틀림본 잔존**을 T4 리뷰 Minor ①이 발견 → T5
   rider f88b3ca가 별도 커밋으로 교정(재실행 스킵 선례 — 진단 probe의 캡처
   디코더 수형이며 T1/T4 결론은 확정돼 있어 소스 대조 동형 봉합). **다음 WSL
   계열 probe는 이 교정본을 원문으로 승계할 것** (원장 계보: docs/85 §3).
5. **drvfs mtime 1초 절단(T4)** — 시드 파일 last_write_time이 drvfs에서 1초
   버려져 최신순(mtime desc) 정렬이 런마다 무안정 → **클릭 대상을 "셀 0"
   계약**으로 바꾸고(이름 아님), 셀 0 결정화는 미래 mtime 스탬프로 봉합(#6).
   touch -t 스탬프 백로그(§4 G5).
6. **셀 0 결정화 — 합성 시드 4장+미래 mtime 스탬프(T4/T5)** — 엔진 소유 합성
   PNG 4장(640x400/1200x120/320x200/100x800 — 극단 종횡비 포함) + os.utime
   2026-11 내림 스탬프 → 최신순 열거에서 셀 0 단값. **i:\@keep 불접촉 계약**
   (vplayer 테스트 미디어 원장 승계).
7. **★폰 permissions.json — 병합+바이트 등호 원복 재량(T5 concern 1 → 리뷰
   Minor ⑤-① "T6 기재 요구", 판정 APPROVE)** — 폰에는 선존 33B
   permissions.json(`close_window: allow`)이 있다. WSL T4 선례는 "ENTRY
   존재=FAIL"이나 그 전제는 **파일이 선존하지 않는 환경** — 폰에서 그대로
   적용하면 dispatch가 명시 요구한 클릭/키 계측(send_input)이 전부 불능 →
   태스크 산출 자체가 성립 못 한다. 택한 원문: ORIG 백업 → **선존 키 보존
   병합**(이미 allow면 무편집) → END **바이트 등호 원복**(`cmp -s`). 리뷰
   판정: 프로브 소유 계약("파일을 소유·잔상 남기지 않는다")의 취지를 더 강하게
   지킨 쪽 — 새로 기록+소각(WSL)보다 원보전 원칙에 가깝다. 리뷰어가
   "정당, 계약 위반 아님"으로 봉합했고 근거가 스크립트 헤더 수의(18-26행)+원문
   행(`PERM-ENTRY-PRE-EXISTING`/`PERM-MERGE`/`PERM-RESTORED-BYTES: 33 등호`)
   로 검증 가능. **선례 판정이 사고 유형으로 분열하는 장(WSL=FAIL 대치
   폰=병합)** 은 전제 차이(선존 유무)로 정연 — 다음 폰 probe는 이 수의 원문을
   승계할 것.
8. **★run 1-3 settings 잔산 → 진품 백업 자가 수복(T5 run 4 — 리뷰 Minor
   ⑤-② 원장 봉합)** — run 3가 dirs 렬 중 도중 사망해 **190B gallery.dirs
   주입본**이 잔상으로 남음. run 4 수복 2종(재실행 가능성 계약으로 명문화):
   ① `PRIOR-ORIG-RECOVERED` — `$TMPDIR`에 남은 **진품 121B 원본 백업**(run 3
   실패 유산)을 최우선 재사용(재구성보다 진품 우선) ② `PERM-SELF-HEAL` — run
   3가 merge 뒤 사망해 남은 60B 병합본을 **진품 33B 백업으로 먼저 원복**(진품이
   위조 백업으로 덮이는 트랩 방지). 부수: 워터마크(gallery 키) 잔상 감지 시
   **캐논 121B 형태** 재구성(2-space·brace 각 행·끝 개행 = 실측 등호 —
   4-space 129B는 오류 후보로 배제). 폐곡: 종료 영수증
   `SETTINGS-RESTORED-BYTES: 121 (ORIG=121 등호)` + user 측 잔상 0건.
9. **★폰 캐논 506→546 상승(T5 — 리뷰 Minor ⑤-③ 원장 봉합)** —
   `CANON-INCLUSION=GALLERY-FULL-T2G-40` — 텍스트 스케일 라인 최신 506
   (docs/86) + 2g 40건 = 546 등호 산치. run 1-4 전부 등호(546/FAIL 0), 배포
   원천 단정은 `MARKER-AFTER gallery_model(2) cmake(5) main_2g(54)
   posix_2g(56) module(2)`+`MODULE-SO-PRESENT buildterm/jkapp_gallery.so
   (16255568 bytes)`+`NINJA-RC=0`(4runs 등호). 캐논 계보 전 축은 §2.1.
10. **폰 probe run 1-3 함정 원장(정직 기록 축)** — 4 runs 전부 probe 버그의
    **실측 발견+수리**로 소각: run 1 = `wait_gallery`가 전역 `GALWIN` 미세팅 →
    `set -u` unbound → click `bad_request`(수리: 전역 세팅+빈 id 가드);
    run 2 = THUMB_A 파서가 sed 후 남는 주석(`4 (grid-input-files=...)`)을
    수치로 흡수 → 미달 오판(수리: 수치 캡처 sed — 캡처/영수증 원문은 전부 OK);
    run 3 = 폰 스폰 플레이크(Gallery 창 60s 부재 — 수리: 대기 3s×30=90s).
    최종 run 4만 일관 판정(GAL-PHONE-OK)까지 도달. probe는 **honest-fail
    모델** — 수치 미달=라벨 FAIL rc=0(원장 목적)·infra 하드 FAIL만 exit 1
    (플레이크를 하드로 오판하지 않는 구조 — T5 리뷰 품질 근거).
11. **tar 스테이징 HEAD blob 오염 게이트(T5)** — `git diff --quiet HEAD --
    $f` → working tree 훼손분을 싣지 않고 `git show "HEAD:$f"` 스테이징+3회
    재시도(실측 tracked-dirty-count=0 → CONTAMINATION-GATE=CLEAN). docs/86
    §4.1 park의 HEAD blob 스테이징 부재(형제 함정)를 probe 쪽에서 봉합한
    사례 — WIPPED>0 분기 mkdir 부재 park는 여전히 원장에 남는다.
12. **PHONE_HOST fail-closed 가드 exec 앞(T5)** — docs/86 §3 #10 M5 렛슨
    승계: unset 재실행이 tee truncate로 영수증 로그를 덮쓰는 함정 — 가드를
    `exec > >(tee)` 앞에(리뷰 실측 119-122행 → 125행). `< /dev/null` 금지
    계약도 승계(사용부 0건).
13. **WSL 기본 폰트 빈값 = 글리프 박스 leg0 계약 열화(T4)** — 결함 아님
    (원장 부기 — docs/86 §3 #3 문맥 승계).
14. **posix는 기본 폴더 1건이 원문** — settings 부재/파손 = 기본
    `state/screenshots` 1건 fail-safe(2g-a) + 폰 /sdcard 권한 부재 = 빈
    목록이 **정상 결과**로 가시화(`SDCARD-STATE` — 스펙 결정 2 계약, 결함
    아님).
15. **폰 화면 = 1920x1005(1080 아님)** — docs/86 §3 #12 원장 승계(캡처
    파서+재시도 흡수). 창 가로 560x520(JKAppModule meta) — crop 624x584.

## §4. Deferred (소등 후보 — 유예 라인, docs/86 §4 문체 계승)

> **형제 라인 포인터** — 갤러리 라인 밖에서 발생한 **클라 idle 스핀 수리(#89)의
> as-built = `docs/88_client_idle_asbuilt.md`** (발단=docs/85 D1 리뷰 I-2 폰
> 갤러리 클라 스핀 — 갤러리 앱이 진단 스파이크의 본판정 표본이 된 라인).

| ID | 사항 | 근거 |
| --- | --- | --- |
| G1 | 회전/삭제/확대·팬(v1 YAGNI) | 스펙 결정 3 — FitFull s=1.0 상한 부재(T2 M1)는 shot 원문 동형·사용자 조작 계약으로 분리(2g-f "확대 허용"이 무상한 계약 명시 단정 아님 — T2 리뷰 원문) |
| G2 | 증분 재스캔(폴더워칭/주기 재스캔) | 스펙 비-목표 → **390c741 백로그 명시** — v1 목록은 열기/새로고침 시점 스냅샷 |
| G3 | music 라이브러리 동형 후보(증분 목록 패턴 승계) | 사용자 2026-10-09 피드백 — **390c741 백로그 명시**·vplayer(재생기)와 스코프 분리 별도 라인 |
| G4 | 네트워크/원격 사진 | 스펙 비-목표(로컬 파일만 — jk::fs 계약) |
| G5 | drvfs touch -t 스탬프 | §3 #5 — drvfs 1초 절단 계약("셀 0 클릭")을 스탬프 계약으로 대치하는 스탐 |
| G6 | 폰 /sdcard 저장소 권한 | 스펙 결정 2 — 권한 부재=빈 목록 정상(§3 #14)·진짜 앨범 필요 시 settings 경로만 켠다 |

### §4.1 원장 park 목록 (리뷰 Minor 종합 — 전부 비임계)

| 출처 | 사항 | 처분 근거 |
| --- | --- | --- |
| T1 | C1 dir탭 경로 라벨 오버플로 | **T2에서 수리 완료**(탭 라벨=폴더 말단 이름, 전문 경로=툴힌트) — 원장 소각 |
| T1 | directory_iterator range-for 암묵 ++ throwing (shot 선례 동형 수형) | 소각 — 신규 결함 아님 |
| T2 | M1 FitFull s 상한 부재 | §4 G1 — 사용자 조작 계약 분리 |
| T2 | M2 텍스처 생성 실패 문구 오인(희귀 경로) | **T3에서 수리 동행**(문구 성립 조건 분리) |
| T3 | N1 가시>96 극단 초과분 placeholder(2g-j 계약화)/N2 수평 클립 낭비 몇 셀 무해 | 2g-j가 이미 계약화 — 재리뷰 park |
| T4 리포트 | REMNANT-COUNT=7 중 galcmp_* 2건 본 probe 산출 아님(서술 부정확) | T4 리뷰 Minor ② — 원장 기록만 |
| T4 리포트 | GRID-ORDER 관측 전용 표기 라인 부재 | T4 리뷰 Minor ③ — 관측 전용 |
| T4 Minor ① | wsl_text_scale_diag.sh 결함 오프셋 잔존 | **T5 rider f88b3ca 소각 완료** |
| T5(Minor 1) | `phone_gallery.sh:295` CANON_PHONE_EXPECT 죽은 변수(awk는 prev+40 리터럴) | **park — 다음 probe 접촉 시 1행 소각** (T6 접촉 금지 지시) |
| T5(Minor 2) | thumb 캐시 소각 실패 WARN(615행) — seed 소각 FAIL과 비대칭 | park — probe 소유 잔상 원칙상 FAIL 정합 후보 |
| T5(Minor 3) | 캡처 폴백 기본값 `SCR=1920x1005` 하드 리터럴(415행) | park — 파싱 실패 폰에서 오캡처 가능(실측 run은 파싱 통과) |
| T5(Minor 4) | 커밋 blob 말미 개행 없음(896행 주석과 반대) | park — cosmetic |
| T5(Minor 6) | 리포트 "FAAIL" 오타 | 리포트는 원료 — 소각 불요 |

## §5. 사용자 결제 게이트 (EYES-PENDING 봉인)

**EYES-PENDING — 라인 최종 결제는 사용자 육안 선언만으로 성립한다. probe와
본 문서는 결제를 기록하지 않는다**(T5 원문 `GAL-PHONE-VERDICT … 육안 스탭
대기`). 본 라인의 결제 상태는 **미결제(대기)**.

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | 폰 육안 4종 | `gal_phone_{grid,full,prev,back,dirs,boot}.png`(+crop 6종) — ①격자 시드 썸네일+한글 라벨+극단 종횡비 2장 ②전체 보기 핏+←꺾임 wrap+Esc 복귀 ③gallery.dirs 레그(/sdcard/Pictures — 권한 부재=빈 목록 정상) ④BOOT-OK 빈 격자 | **대기(육안)** |
| ② | Windows 모양(격자/전체보기) | Win 축은 자동 probe 미실측 — T1 리뷰 셀프테스트(548)/T6 재실측(569) 실측까지만. 사용자 기동 육안 승계(격자·dir탭·전체 보기·이전/다음·메타 행) | **대기(육안)** |
| ③ | 스펙 결정 3건 재량 | A안 모듈/소스 디렉터리/v1 스코프 — 컨트롤러 재량 확정(§0). **기각 시 수리 커밋** | **대기(유효)** |
| ④ | #80 브라우저 자연어 결제 | 본 라인과 별개(docs/86 §5 ③ 승계) | **대기(별도)** |

- 폰 기기 상태: probe 복원 계약이 폐곡 — settings **121B 바이트 등호** 원복+
  permissions **33B 등호** 원복+시드/캐시 소각(§3 #7·#8·run 4 종료 영수증) —
  사용자 측 잔상 없음. 결제 시점에 폰이 이 원문 상태라는 보증으로 쓴다.

## §6. 커밋 원장 + IP grep 게이트

- 커밋 체인은 §1 표(전부 T6 실측 rev-parse 전체 SHA) — 1a73fac → eb13f3b →
  bf2b3b6 → 40792d7 → 6c6382c → 56f26d9 → 298d08f → c611c80 → 2a57b4d →
  18274d0 → 1c3aafd → 390c741 → f88b3ca → b245cf6 → 본 T6 커밋(git log가
  진실원). push 전부 origin main 직행·amend 없음.
- **IP 리터럴 게이트(T6 실측)** — 갤러리 라인 커밋 원문 전체
  (`git show` 56f26d9/298d08f/c611c80/2a57b4d/18274d0/1c3aafd/390c741/f88b3ca/
  b245cf6)에서 dotted-quad·내부 대역 리터럴 grep = **0건**(T5 리뷰의 커밋별
  0건 실측 재실측 등호). PHONE_HOST 접속 정보는 환경변수만(기록 금지 계약 —
  스크립트 표기는 `<폰 IP>` 자리표시·빈 기본값 fail-closed뿐 — T5 리뷰 실측).
  본 T6
  커밋은 docs 2파일(git add 명시 경로만 — #83 사건 레슨 유지)로 제한되며
  IP 게이트 0건을 커밋 전 스테이지로 재확인한다(§6 표기 원문=본 커밋의
  `git show` 판정).
- 캡처/로그 PNG·로그 16종(WSL 4+ref 1·폰 6·crop 6 — §2.2)은 engine/tmp
  untracked — **커밋 금지 충족**(혼입 검증=T5 리뷰 stat grep·T6도 add 경로
  명시로 구조 봉합). 육안 결제 후 소각 권장(T5 concern 4).
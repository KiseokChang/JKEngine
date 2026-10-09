# docs/89 — 뮤직 라이브러리 as-built (T1-T6, 2026-10-10)

뮤직 라이브러리 라인(#90) T1-T5 원장(T3 자기 fix r1/r2/r3 포함). 계획 문서=
`docs/superpowers/plans/2026-10-09-music-library.md`(55a98fd), 스펙=
`docs/superpowers/specs/2026-10-09-music-library-design.md`(5189316 — 설계
승인 "물론이죠"+결정 3건), SDD 원장=`.superpowers/sdd/2026-10-09-music-library/`
(progress·task-1..5 brief/report/review·review-*.diff — 워크숍 원장). 결정 원장=
**D1 재생=vplayer 위임·D2 스캔=파일명 스캔·D3 서브디렉터리 재귀** — 전부 사용자
확정(기각 시 즉시 수리 계약). 코드 계보는 git log가 진실원. 본 문서는 실측
영수증·함정·deferred·사용자 결제 게이트를 봉합한다.

- **부기(T6 docs 전용)** — 본 커밋은 docs 2파일뿐(`engine/src|engine/include`
  변경 0건)이라 **selftest 재실행 불요** — 캐논 표기는 T5 종착 수치(c4715c8)의
  건네받은 값 그대로(Win 614/WSL 591/posix 296/폰 591). probe 런 로그는
  engine/tmp untracked(커밋 금지)라 본 문서는 SDD 원장 리포트/review 원문 인용을
  진실원으로 쓴다(창작·추정 수치 0 — 원문에 없는 수치는 원장행 표기로만 남긴다).
- **룰링 승계** — 표준 빌드 명령=`ninja -C engine/build -j3`(Win)·`ninja -C
  buildwsl -j3`(WSL)(T1 실측 — `mingw32-make` 표기 실표준 정정 룰링)·총계는
  실측 집계로 적는다(`[PASS] N / FAIL 0` 리터럴 서식 표기 금지 — selftest 출력에
  그 라인 서식이 없다, T1 리뷰 M-2 룰링)·T4 리뷰는 "병렬 — spd r1" 룰링으로 T3
  fix와 병행 dispatch(docs/88 §0 각주의 룰링 표기 통합 각주 승계).

## §0. 문서 체계

docs/85(더티프레젠트)·docs/86(텍스트 스케일)·docs/87(갤러리)·docs/88(클라 idle)
의 §0-§6 구조를 계승한다 — §1 배선 원장(rev-parse 전체 SHA)·§2 캐논 표+위임
실측 표·§3 함정 원장(봉쇄 레슨 축)·§4 deferred+park(§4.1)·§5 사용자 결제 게이트
(EYES-PENDING)·§6 커밋 원장(IP grep 게이트 병기).

발단: docs/87 §4 G3 — 갤러리 완료 뒤 사용자 2026-10-09 피드백(같은 형태의
음악 모아보기 앱)이 라인을 열었다(390c741 백로그 명시). 갤러리 쌍둥이 원문
(GalleryModel.h·ClientGalleryApp·JKAppModule/CMake·probe 3종) 승계가 구조
전제이고, 재생은 **위임**(새 오디오 디코더 0건 — 재생 코스토 중복 제작 절감)이
라인의 유일한 새 축이다. 스펙 결정 3건=컨트롤러 재량 확정 아님·사용자 확정
(§0 발단 인용) — 기각 시 즉시 수리. worktree 미사용·main 직행·서브에이전트
스폰 0건(T6).

## §1. 배선 원장 (라인 전체 — rev-parse 전체 SHA 실측, T6 재측정)

| 태스크 | 커밋(SHA — T6 실측 `git rev-parse`) | 내용 |
| --- | --- | --- |
| 직전 라인 종착(#89 클라 idle 스핀 — park 배치 3) | `a5ee2cce9550f236ac45b5198bb4c7d908f1fdc7` | test(probes): park 소각 배치 3 — 클라 idle 라인 park+갤러리 probe 잔여 |
| 스펙 | `5189316383e8845aa49f6768b2cde1b652fabc2f` | docs(specs): 뮤직 라이브러리 스펙 — vplayer 위임+파일명 스캔+재귀 (#90) |
| 플랜(라인 BASE) | `55a98fd1eef1f5d02a84aad16bca33ad35b1decc` | docs(superpowers): 뮤직 라이브러리 구현 플랜 T1-T6 (#90) |
| T1 | `bb999df1a4f3a6c1ca4e158f4fc7f985feab65a5` | feat(apps): music 순수 부품+2m selftest (T1) — MusicModel.h 222행 신설+main.cpp 2m-a..d 15건 (2파일 +326) |
| T1 fix r1 | `08e56b26b0afee7b83324f45641200bb20a54947` | fix(apps): music 재귀 순환 가드 — 심링크 방문 집합 (T1 fix r1) — Win reparse 0x400 스킵+posix (st_dev,st_ino) 방문집합+2m-e 5건 (2파일 +151/−4) |
| T2 | `3378e10fc6db6d55fddfdcc750c5e6d4a2cc19cd` | feat(apps): music 모듈+클라 앱 — 비동기 스캔+리스트+필터 (T2) — ClientMusicApp(.h/.cpp)+JKAppModule_music+CMake(jkapp_music SHARED 양축+music.jkx pack)+2m-f 1건 (5파일 +559) |
| T3 | `3019d5d50630a5a347271f4b51a93040fe944775` | feat(apps): music 더블클릭 → vplayer 위임 배선 (T3) — OpenRequestJson(Path)+PlaybackDelegate+재청구 폴백 20×250ms+2m-g 4건 (4파일 279+/10−) |
| T3 fix r1 | `cc7c24ec2f819a605440e03c88c77eb5298675ec` | fix(apps): music 위임 폴백 소진 표기+답신 분기 정리 (T3 fix r1) — 리뷰 지시 5항 수형 (4파일 59+/18−) — §3 #1 소각 사건 |
| T3 fix r2 | `08ff3961001d62aed6d91b999f79a9a2655e1c54` | fix(apps): music 위임 폴백 소진 전이 수취 시점 이동 (T3 fix r2) — dead code 소각 (2파일 38+/26−) |
| T4 | `283502bf0b4b2c31ea57017a7f02cd1672e1e23e` | test(probes): music WSL probe — 위임 재생 실측 (T4) — wsl_music.sh 583행(probe 1파일·src 0건) — §3 #2 원장 성립 런 |
| T3 fix r3 | `b2341c829291d6e0be8d1f40d4030a1106ea52de` | fix(apps): music 위임 폴백 — 봉투 ok 무신 본문 재판정 (T3 fix r3) — DelegationReplyVerdict+2m-h 1건+MtimeLabel ==0 가드 (4파일 106+/38−) |
| T5 | `c4715c851fefaf85bb3c6d232adfbc069440eb88` | test(probes): music 폰 probe — 실기기 실측 (T5) — phone_music.sh +1,101(병합+바이트 등호 원복 수형·RETRY 계약) |
| T6 | 본 문서를 포함하는 커밋 | docs(apps): 뮤직 라이브러리 as-built (T6) — SHA는 git log가 진실원 |

폐곡 계보: T1 리뷰 **APPROVE**(C0/I1/M3 — I-1 심링크 재귀 무한루프) → T1 fix r1
re-review **APPROVE**(C0/I0/M2 — 방문집합 구조 실측+junction 2m-e 5건) → T2 리뷰
**APPROVE**(C0/I0/M0 클린 — idle 계약·워커 수명·posix 296 등호 사유 전부 PASS) →
T3 리뷰 **APPROVE**(C0/I2/M3 — I1 소진 표기 부재·I2 reply.ok 원문 분류 → fix r1)
→ T3 fix r1 re-review **REJECT**(I-1 dead code — §3 #1) → T3 fix r2 re-review
**APPROVE**(C0/I0/M0 — 소진 전이 도달 가능 시퀀스 역산) → T4 리뷰 **APPROVE**
(C0/I1/M5 — **T3 결함 성립 재확정**: 서버 즉답 봉투 ok=1 고정×봉투-only 소비
:393 — §3 #2 진원 정밀 원문) → T3 fix r3 re-review **APPROVE**(C0/I0/M1 —
본문 진실원 전환+콜드 시퀀스 역산 첫 거부 소멸 소각) → T5 리뷰 **APPROVE**
(C0/I0/M5 — 캐논 591 CANON-INCLUSION+C1/C2=스탭 1건 후 진원 판별·원장행 유계).

## §2. 캐논 표 + 위임 실측

### §2.1 selftest 캐논 계보 (2m 계열 — Win/WSL/posix/폰)

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#89 종착 = docs/88 최신) | 588 | 565 | 296 | 565 | — |
| T1 `bb999df` | **603** | **580** | 296(동결 — T2) | (565) | +15 (2m-a..d) |
| T1 fix r1 `08e56b2` | **608** | **585** | **296** | (565) | +5 (2m-e) |
| T2 `3378e10` | **609** | **586** | 296 | (565) | +1 (2m-f) |
| T3 `3019d5d` | **613** | **590** | 296 | (565) | +4 (2m-g) |
| T3 fix r1 `cc7c24e` | 613 | 590 | 296 | (565) | 0 (유지) |
| T3 fix r2 `08ff396` | 613 | 590 | 296 | (565) | 0 (유지) |
| T4 `283502b` | 613 | 590 | 296 | (565) | 0 (probe 전용 커밋) |
| T3 fix r3 `b2341c8` | **614** | **591** | 296 | (565) | +1 (2m-h) |
| T5 `c4715c8` | 614 | 591 | 296 | **591** | 폰 2m 26건 흡수 (565+26=591) |

- **폰 591 = WSL 591 완전 등호** — 처음으로 Win 614·WSL 591·폰 591이 정합한
  시점(T5 원장 `CANON-INCLUSION=MUSIC-FULL-2M-26 … WSL 591 등호` — posix 296은
  별도 하네스 축이라 별도). 폰은 2m-a..h 26건 전량을 T5 배포에서 한 번에 흡수(2m-e 심링크
  군 5건도 폰에서 출력 — T5 report C3·**dispatch 초산 "+2m 21"은 오류로 폐기,
  CANON 상수 단일 지점(:272) 정정** — T5 리뷰 CANON-INCLUSION 실측).
- 폰 565 이전 기원 = 텍스트 스케일 506+갤러리 2g 40+2i 19(docs/88 §2.1 원장) —
  뮤직 라인은 폰 축에 2m 26만 더한다.
- **posix 296 등호의 구조적 사유**(§3 #8) — T2 이래 매 커밋이 "하네스 TU
  미링크"로 재실측 불요를 증명했고, T3 리뷰에서 posix 재빌드 재실측 296 등호,
  T1 fix r1에서도 포함, T4는 WSL 축 재실측(590)으로 위임. 원장 라인: 277
  표기는 갱신 48 시점 수치 오류였고 진정 원장은 296(T1 리뷰 M-1 — fix r1
  리포트 §7 정정, posix 296 재실측 원문 보존).
- WSL 2m=25(T4 원장 — 소스 27건 중 2건은 윈 junction 조건부 확인 라인이라
  posix 축 미출력·계보 이탈 아님) → 2m-h 1건 추가로 26=폰 등호.
- **T6에서 캐논 재실행 불요**(docs 전용 — b2341c8·c4715c8 이후 코드 변경 0건
  예정·실측) — 건네받은 수치 그대로 봉합.

### §2.2 위임 실측 표 (D1 — 쿼리 쌍 ①launch_app ②app_tool open)

| leg | 더블클릭 launch | 위임 open(콜드 — 등록 전 경기) | 위임 open(직행 — 등록 후) | get_status | probe verdict |
|---|---|---|---|---|---|
| **WSL** (T4 — fix r2 코드) | 합성 2탭(gap 44/37ms — T4 리뷰 M-1 재표기) **pair 2**에서 Video Player 창 출현(id 37·pid 3543·960x640) | **MISS — T3 폴백 자기 소멸**(§3 #2·#6): 16폴링 virgin·UI 캡처 virgin·stderr 0인데 music status "vplayer 재생 요청됨" — 크레딧 20회 즉시 소멸 | `MUSIC-APP-OPEN-REPLY: {"ok":true,"windowId":37,"result":{"accepted":true}}` | 2회 get_status **pos 3.019→6.084·dur 8.000**(합성 8초 WAV) | `MUSIC-OK`(rc=0) — 결함 원장 승계 → fix r3 수형 |
| **폰** (T5 — fix r3 코드) | 합성 2탭 **pair 2**(gap 95ms)→ Video Player 창(id 38)·pair 1(gap 139ms) MISS 5런 일관(§3 #7) | 콜드 전조 `MUSIC-GLUE-COLD-REPLY` 본문(`{"ok":false,"error":"unknown_app_tool"}`— WSL 원문 동형) 수취 후 등록 완료 — **폰 스폰이 15-18s 원장보다 빨라 콜드 MISS 원장은 폰에서 미성립** (fix r3 재청구 흡수 → 귀속 폴링 첫 수취 `opened:true pos 6.130`) | `accepted:true` | 직행 **pos 4.226→8.000**(ended — 캡처 00:02→00:08·재생 진행) | `MUS-PHONE-OK`(rc=0) — `MUSICP-DELEGATE-OPEN: OK — 위임 open이 vplayer에 도달해 열었다` |

- **fix r3 수형의 폰 실측** — 위임 경로가 fix r3 봉투-본문 본문판정(DelegationReplyVerdict)을
  거친 실제 런(T5 BASE=b2341c8)이라 콜드 unknown_app_tool 본문이 거부로 판정 → 재청구 →
  귀속 OK로 흡수 — fix r3 결함 원장 봉합의 폰 축 영수증. WSL축 fix r3 후 재런(콜드
  MISS 소각 확인)은 미실측 — §4.1 원장행.
- 캡처 영수증(engine/tmp — **전부 untracked, 커밋 금지 충족**): WSL 5종
  `mus_wsl_{list,filter,vp_delegate,delegate_status,after}.png`(T4 리뷰 M-2에서
  4종 표기→5종 정정)·폰 6종 `mus_phone_{list,list2,vp_delegate,delegate_status,
  after,sdcard}.png`(md5 등호 회수+crop — T5 원장). 폰 list/list2 동 md5 =
  정적 화면 스크린샷 안정성 영수증.
- 스펙 §7 판정 축 대조: ①dirs 트랙 목록(재귀) = WSL 4행+폰 4행 실측(mtime desc·
  sub 재귀 1행·689.1KB) ②더블클릭 → vplayer 재생 = 양 leg 실측(§2.2 표 — 육안
  최종 결제는 §5) ③idle 계약(#89) = T2 신설 앱의 유일 더티=스캔 도착+답신 수취만
  (T2/T3 리뷰 전수 grep PASS·타이머/자기유지 더티 0건) — 캐논 전 축 신설 케이스
  흡수 후 등가 성립.

## §3. 함정 원장 (T1-T5 리포트+리뷰 종합 — 봉쇄 레슨 축)

1. **★T3 fix r1 소진 전이 dead code(REJECT — 08ff396 폐쇄)** — fix r1은 감법을
   답신 수취에서 하고 소진 전이(표기)를 `openRetries_ > 0` 조건의 OnIdle 분기에
   두었다: 시퀀스 역산 — 시도#1(retries=20)→reply19: 2→1·schedule → 시도#19 발사
   → **reply20: 1→0 뒤 OnIdle 조건 영구 실패 → 소진 표기가 영원히 도달 불가**
   (`ClientMusicApp.cpp:401-409` — re-review REJECT의 dead code 판정). 크레딧
   불변식을 깰 위험이 있는 "20번째 감법이 표기 없이 절화" off-by-one.
   **수형(fix r2)**: 감법은 수취에서만 1회, **0 도달 순간 수취 시점 즉시 소진
   전이**(표기+openPath_ clear+frameDirty_ 1회) — OnIdle 의존 소각. 레슨:
   **전이는 상태를 소유한 원(수취)에서 끝내라 — 발사(OnIdle)에 전이를 두면
   크레딧 0과 발사 게이트(`>0`)의 불변식이 서로를 막아 dead code가 된다.**
   시퀀스 역산이 검출의 유일 축이었다 — 부품 단정(2m)·본체 리뷰(APPROVE C0)
   모두 이 결함을 못 잡았다.
2. **★봉투/본문 이층 ok 경계 원장(봉투 ok=고정×클라 봉투-only 소비 — b2341c8
   폐쇄)** — 서버 답신의 봉투 ok 플래그는 **경로별 의미가 다르다**:
   ①서버 **즉답 경로**(`JKWindowServer.cpp:6747-6750` —
   `ipc::WriteAgentJson(..., queryId, 1, reply)`+`:3564 replied=true` 기본)는
   **봉투 ok=1 고정** — 본문 `{"ok":false,"error":"unknown_app_tool"}`와 무관
   (즉답 거부군 unknown_app_tool·denied·args_too_large·ambiguous·tool_gone 전부
   `replied`를 끄지 않는다) ②등록 **릴레이 경로**(`:4180 replied=false` →
   `HandleToolResult :7252-7256`)는 **봉투 ok=앱 결과 ok**. 클라 `reply.ok`
   (JKClientSurface.cpp:378 — "ok mirrors the wire reply flag")는 봉투 플래그 →
   T3~fix r2 코드는 ①에서 sync 거부를 성공으로 읽었다(T4 콜드 MISS 실측 증거 —
   §2.2). **수형**: 순수 부품 `music::DelegationReplyVerdict(envelopeOk,
   replyJson)` — **본문이 "ok"를 기술하면 본문이 진실원**(봉투 무신), 미기술·
   파손만 봉투 fallback — PollReplies launch/open 양 분기 일괄 전환(launch
   거부도 봉투 원문엔 도달 불가였다 — 동형 결함 일괄 봉합).
   **레슨: 멀티 계층(봉투/본문) 와이어 계약에서 "ok"를 한 계층만 정의하지 마라 —
   클라의 재판정 원은 계층을 명시해야 한다(본문 기술 시 본문 진실원).** 파장
   2건: ①`ClientLibraryApp.cpp:236` 동형 잠재(launch_app도 sync 즉답 — unknown_app
   본문이 봉투 ok=1로 실려 "실행 요청됨" 오판) — **원장만, T3 fix r3 접촉 금지
   준수** — library 라인 접촉 시 정정 권고 ②T3 fix r3 re-review M-1(§4.1 park):
   `HandleToolResult :7260-7264`의 **앱-수준 실패 조립은 본문에 `ok:true`
   리터럴+error 객체**라 본문 판정이 ok=true로 오독 — fix r1의 봉투 판정이
   유일하게 옳았던 간선 — vplayer open의 유일 실패는 bad_args(클라 path 가드로
   불성립)라 **v1 도달 불가**, 서버 조립 계약이 본문 ok=실제 결과로 정정되면
   자연 해소.
3. **MinGW junction 사각지대(순수 std가 링크 루프를 못 자른다)** — libstdc++
   (MinGW)은 junction(IO_REPARSE_TAG_MOUNT_POINT)을 `entry.is_symlink()=0`으로
   놓치고, `fs::canonical()`도 junction을 해석하지 않는다(항등 경로+ec=0 —
   `engine/tmp/reparse_probe` 실측: junction에 `GetFileAttributesW=0x410
   reparse=1`·canonical=항등). **수형**: Windows leg는 reparse point(0x400)
   디렉터리 스킵(`GetFileAttributesW` — kernel32 extern "C" 원문 재선언,
   INVALID_FILE_ATTRIBUTES도 스킵·보수 파) + posix leg는 `(st_dev, st_ino)`
   방문집합(루트 시드·**재귀 진입 전 insert** — ec 떠도 집합 누수 없음).
   레슨: **정체성 키는 os 계층 원문으로 뽑아라 — C++ std::filesystem의 링크
   의미론은 축별 미완비**.
4. **libstdc++ file_clock epoch 2174 — 음수 mtime이 "-"로 오름** — WSL(GCC 13)
   `last_write_time().time_since_epoch().count()`는 **음수**(실측
   -4646113461279417845 — epoch=2174-01-01) → `ClientMusicApp::MtimeLabel`의
   `mtime <= 0 → "-"` 가드가 WSL 표의 수정 열을 **전행 오렸다**(T4 원장 — 정렬은
   원래 음수에서도 올바름). 음수 count는 `FileTimeToSys(file_time_type::duration
   {mtime})` 재구성(clock_cast)에서 정상 시각 변환 실측. **수형(fix r3 동봉)**:
   가드 `<=0` → `==0` 1행 — 음수=정상(연도 표기) 재분류·0(스탬프 부재 stat
   실패 열외)만 "-" 유지. 레슨: **"값 없음" 가드에 부호를 쓰지 마라 — 시계
   epoch는 축별 상수라 부호가 결핍을 대변하지 않는다**(Windows/폰은 양수 epoch).
5. **`fs::remove_all`이 junction 루프를 따라간다(libstdc++ 실측)** — 순환
   junction 트리 정리에서 remove_all이 `sub\loop\sub\loop\…`를 수십 겹 파고들다
   path 소진 throwable(`filesystem error: cannot remove all`·rc=127·잔산 —
   T1 fix r1 미수리 런 실측). **수형**: 2m-e 정리에서 링크 2건을 `fs::remove`
   (링크 자체)로 **선제거** 후 remove_all(+시작 정리에도 동형 선제거). posix의
   remove_all은 심링크를 따르지 않음을 같은 수형으로 실측(WSL rc=0·잔산 0).
   music 앱 본체는 트리를 소각하지 않아 이 함정이 성립하지 않는다(보존 원장 —
   링크 포함 트리를 소각할 앱이 생기면 T2 cleanup 계약 승계).
6. **launch_app 비동기 스폰 vs 도구 등록 콜드 경기 — 재청구 폴백** — launch_app의
   SpawnClient는 **즉귀** `{"ok":true}`(`JKWindowServer.cpp:4583-4598`)인데
   vplayer 도구 등록(`ClientVPlayerApp.cpp:2087` SendAgentToolRegister)은 앱
   연결 뒤 OnInit → 콜드 open 릴레이는 `unknown_app_tool`이 **예정된 상태**.
   **수형**: 클라 재청구 폴백(kOpenRetryMax=20 × 250ms — OnIdle 시간 pacing,
   재발사 무더티·id 계약 유지) — **서버 부품 0건·vplayer 무변경**(폴백 경로
   미발동 원칙 유지). 보조 계약: 크레딧=수취 소모(#1 수형)·launch 미성립 즉시
   취소+`launchAborted_` 사후 답신 겹침 가드(무음 회수)·소진 표기 도달 가능
   (fix r2). 폰 실측: 스폰이 빠르면 경기가 대부분 무해(콜드 전조 본문 거부
   수취 1건 후 등록 완료 → 재청구 흡수).
7. **폰 첫 클릭 포커스 취식+위임 pair 1 MISS(원장행 유계 — C 승격은 스탭 1건
   후)** — ①뮤직 탭 스트립 **첫 클릭이 포커스 취식으로 사라지는 실측**(1-4차
   런 — 캡처 0곡·탭 미전환; 갤러리 폰의 셀 0 클릭은 성립했던 것과 대조) →
   probe에 동일 탭 re-click 방어+list 2캡처(list/list2)로 5차 확정 ②위임
   합성 2탭 **pair 1(gap 139ms) MISS가 5런 일관** — 더 촘촘한 pair 2(95ms)는
   통과라 시간창(300ms) 단일 축으로 설명 불가. sdcard 레그의 "SCAN-FINITE OK"
   (1-4차)도 동형 취식의 **미스캔 무효 원장**(0곡=탭1) — 5차 재구축으로 진원
   실측 복구. 진원(포커스/입력 파이프/프레임 스케줄)은 미판별 — T5 리뷰 판정:
   **결함 라인 즉시 승격은 이르고, 스탭 1건(탭 전후 `focused` 원장+부팅 직후
   첫 클릭 단독 실측)으로 진원 판별 후 승격 — 원장행 유계**(사용자 실사용은
   앱 자체 재청구 폴백+재클릭이 흡수 — probe 합성 2탭 경계로 한정).
8. **posix 296 등호의 구조적 사유(하네스 TU 미링크)** — `tools/posix_selftest`
   의 링크 TU(ChatRouter·JKFrameDirty·JKLlmEngine·JKAgentJson·JKFs/Process/Net/
   ConPty/Pipe/InstanceLock/TextConv posix)는 뮤직 라인 접촉 파일 어디도 링크하지
   않는다 — ClientMusicApp·JKAppModule_music·engine/src/main.cpp·posix main.cpp
   쌍둥이 전부 밖(쌍둥이는 MusicModel.h 무 include). 그래서 **2m 미흡수 상태가
   유지되는 한 296은 구조적 등호** — 매 커밋의 "재실측 불요 사유"를 구조로 증명한
   축(T3 리뷰 등 재빌드 재실측으로 상실하지 않음 확인). 대가: posix main.cpp
   쌍둥이 2m 흡수는 **유보된다(T1 C2 — 하네스를 등록하면 등호 전제가 붕괴하고
   posix 캐논이 오르는 별도 커밋 절차가 된다)**. 원장 부기: 296은 #89 라인이
   이미 도달했던 수치 — 277 표기 오류는 fix r1 리포트 §7에서 진정 원장 정정 완료.

## §4. Deferred (소등 후보 — 유예 라인, docs/88 §4 문체 계승)

| ID | 사항 | 근거 |
| --- | --- | --- |
| D-1 | 폰 입력 경계 진원 판별(첫 클릭 취식+위임 pair 1 MISS) | T5 C1 — 5런/4런 실측·진원 미판별 — **스탭 1건(탭 전후 focused 원장+부팅 직후 첫 클릭 단독) 후 승격 판별**, 현행 원장행 유계(T5 리뷰 §3 판정) |
| D-2 | 폰 캐논 극단 트리 재판정 잔여 | T5 C3 — /sdcard FUSE 실측은 음성 4건·~27s 정착(CPU 2.13→0.75%)·셸 find timeout 120 미완료까지 — **T1 fix r1 M-5(폰 FUSE st_ino 이질)는 폰 실측으로 유한 종료+이질 증상 0이 확정돼 원장 갱신(부분 해소)** — 심링크 혼입/극단 규모는 잔여 유계 |
| D-3 | 표 ListClipper·파괴 join detached 확장 | T2 C1/C2 — 갤러리 선례 확장, 별도 결함 라인 후보(수만 행/대형 폴더 체감 시) |
| D-4 | 재선택 "새 창 없이 open" — windowId 수형 채널 | 스펙 §4 말미 잔여 — 클라 창 목록 수형 채널 실측 후 계약(T3 이래 T4 판정 대기 유지·미성립) |
| D-5 | launcher_music 아이콘 2장 | T2 판정 1 — snap 무아이콘 선례 제외 실측 — gen 원문(gen_gallery_icon.ps1)으로 재작 후 pack DEPENDS — 후속 라인 몫 |
| D-6 | ID3/태그·앨범 커버·플레이리스트·레이트 케이던스·증분 재스캔 | 스펙 D2/§8 비-목표·비-목표 백로그 — 별도 태스크(라인 밖) |
| D-7 | spatial-player 연동(2단계) | 스펙 §6 — **별도 스펙 착수는 사용자 선언** — 이 라인은 위임 지점만 남긴다(위임 대상 교체가 연동이다) |

### §4.1 원장 park 목록 (리뷰 Minor 종합 — 전부 비임계·수리 수형 후보 병기)

| 출처 | 항목 | 등급 | 수리 수형 후보 |
| --- | --- | --- | --- |
| T1 리뷰 fix r1 M-4 | reparse 가드 누락 레이아웃(마운트 포인트·DFS·OneDrive placeholder — 무한루프 대신 트랙 누락의 보수 셈) | M | 사용자 실측에서 트랙 누락 보이면 **이 가드를 원인 1순위로 의심**(리뷰 원문) — 가드 원인 표기 1행 후보 |
| T1 리뷰 fix r1 M-5 | posix 방문집합 2루트 접힘+폰 FUSE st_ino 이질 | M | **T5 실측(/sdcard 유한 종료·증상 0)으로 재판정 — §4 D-2 원장 갱신**, 극단 규모 잔여만 park |
| T2 리뷰 C1/C2/C3 | 파괴 join 스캔 대기·ListClipper 부재·필터 0건 문구 미실장 | M(3건) | **park — T3 확장 금지 준수(T3 리포트 §5 판정)** — C3는 문구 1행 park 배치 후보+아이콘 2장(D-5 합류) |
| T3 fix r3 re-review M-1 | HandleToolResult 앱-수준 실패 조립(본문 `ok:true` 리터럴+error 객체 :7260-7264) — 본문 판정 오독 | M | **park — v1 도달 불가**(§3 #2 원장) — 서버 조립 계약 정정 시 자연 해소 |
| T4 리뷰 M-1 | 리포트 2탭 gap 34-57ms가 승하된 log와 1:1 아님(log 44ms·37ms) | M | **본 문서 §2.2는 승하된 log 1행 기준 재표기로 이행** — 이후 원장 관행 |
| T4 리뷰 M-2 | 커밋 "캡처 4종" vs 실측 5종 | M | 본 문서 §2.2 5종 표기로 이행 |
| T4 리뷰 M-3 | probe 파일 끝 개행 부재(말미 주석과 모순) | M | **T5에서 소각 완료**(+1,101·끝 개행 유지 — 원장 소각) |
| T4 리뷰 M-4 | 소사 변수(VPWIN_ID 인라인 재계산·GS1 선언만) — MISS 시 window_move 무해 실패 지선 | M | 다음 보정 런에서 1행 정리 |
| T4 리뷰 M-5 | 스탠다얼론 g++ 진단·isolation 원문 영수증 미보존 | M | 다음엔 run log에 진단 원문 1행 흘려 영수증화 |
| T5 리뷰 M-1 | run-5↔커밋 원문 미일치 2곳(판정행 캡처 5종 vs 커밋 6종·NOTE-SDCARD-SHELL 미출력) | M | 다음 접촉 시 런 원문 재수취로 정합화(수치 영수증은 전부 1:1) |
| T5 리뷰 M-2 | RETRY 자가 수복 원장 영수증 소실(tee truncate — 중간 런 소실) | M | 다음 RETRY 실측 시 RUNLOG에 원장행 명시 수취(계약 코드는 커밋 원문으로 검증 완료) |
| T5 리뷰 M-3 | 보존 로그 부재 수치 2건(underrun 2-3·gap 147ms) | M | 승하된 log 기준 재표기(T4 M-1 동종) — 본 문서 §2.2는 미수취 수치를 원장행으로만 표기 |
| T5 리뷰 M-4 | 커밋 원문 C3 정정 잔재 코멘트 2행(캐논 +21 원료) | M | 다음 probe 접촉 시 1커밋 정리(게이트·판정 로직은 591/26으로 정확) |
| T5 리뷰 M-5 | 리포트 행수 부기 오차(+1,094 vs 실측 +1,101) | M | 원장 기록만(본 §1 표는 +1,101 실측) |
| 표기 정정 원장 | T3 리뷰 지시 "pos이 296"→"posix 296" 오타 소각·스펙 §7 "EYES 최终"→"최종" 판독 | M | **본 문서에 정정 반영 완료** — 사용자-제공 원장 오타는 오자 근절 계약상 재사용 금지·실측 용어로만 |

## §5. 사용자 결제 게이트 (EYES-PENDING 봉인)

**EYES-PENDING — 라인 최종 결제는 사용자 육안 선언만으로 성립한다. probe와 본
문서는 결제를 기록하지 않는다**(docs/88 §5 동형 — MUS-OK·MUS-PHONE-OK는 수치
영수증 라인이지 육안 선언 아님). 본 라인의 결제 상태는 **미결제(대기)**.

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **music 앱 폰 모양+재생 정착감** | 폰 list/list2(시드 4곡 — mtime desc·sub 재귀 1행·689.1KB)+위임 재생(Video Player 창·00:02→00:08 진행·sdcard 탭 실음성 4행 정착) — 캡처 `engine/tmp/mus_phone_*` 6종(md5 등호 회수)+WSL 동형 `mus_wsl_*` 5종 보존. 스펙 §7 성공 판정의 폰 실측 몫 | **대기(육안)** |
| ② | Windows music 모양 | Win 축 자동 probe 미실측 — 사용자 기동 육안(리스트·필터·더블클릭 위임 동작·mtime 열 표기) | **대기(육안)** |
| ③ | 위임 콜드 런 | fix r3 수형의 콜드 위임 사용자 실사용(재청구 흡수 → vplayer open 도착). 폰 pair 1 MISS=D-1 원장행(진원 판별 스탭 유계·사용자 경험은 재클릭+폴백이 흡수). T4 WSL 콜드 MISS 원장은 코드상 fix r3로 폐쇄 — WSL 재런 재실측 미수행(§2.2 부기) | **대기(육안)** |
| ④ | 스펙 §7 성공 판정의 사용자 육안 | "더블클릭 → vplayer 창이 열리고 클립이 재생된다(폰/WSL 실측 — EYES 최종)" — 스펙 §7 원문 그대로의 최종 결제 항 + idle 계약 준수 육안(스캔 도중만 그린다·idle 체감 정지) | **대기(육안)** |

- 폰 기기 상태: probe 복원 계약 폐곡 — settings.json 121B·permissions.json 33B
  **바이트 등호 원복**+font_path 유지+EXIT Taskbar+Terminal(idle 라인 종료
  상태 원복)·jkweb 21177 before/after 무변·REMNANT-LS-RC=2 게이트 — 사용자 측
  잔상 없음. 결제 시점에 폰이 5차 런 원문 상태라는 보증으로 쓴다(리뷰어 재관측은
  폰 부재로 미실시 — 비파괴 원칙·재런 시 진입 마커가 신선 보증 대행, T5 리뷰 §4).

## §6. 커밋 원장 + IP grep 게이트

- 라인 체인 = **55a98fd..HEAD**(T6 실측): 55a98fd(플랜) → bb999df(T1) →
  08e56b2(T1 fix r1) → 3378e10(T2) → 3019d5d(T3) → cc7c24e(fix r1) →
  08ff396(fix r2) → 283502b(T4) → b2341c8(fix r3) → c4715c8(T5) → 본 T6 커밋
  (§1 표 = rev-parse 전체 SHA — git log가 진실원). push 전부 origin main 직행·
  amend 없음.
- **IP 리터럴 게이트(T6 실측)** — 본 커밋 원문에서
  `\b([0-9]{1,3}\.){3}[0-9]{1,3}\b` grep = **0건**(커밋 전 `git show HEAD`
  게이트 실행). 라인 이전 커밋도 리포트/review 전원 IP grep 0건 실측(T1-T5
  각 리포트+리뷰 재실측). 폰 접속 정보는 `PHONE_HOST` env 자리표시뿐
  (fail-closed 가드 — 기록 금지 계약·known_hosts 2건 BatchMode 판명은 로그
  원장행 — T5 리뷰 IP 재실측).
- 본 T6 커밋은 docs 2파일(`docs/89_music_library_asbuilt.md` 신설+
  `docs/87_gallery_asbuilt.md` §4 형제 라인 포인터 1행 — git add 명시 경로만,
  #83 사건 레슨 유지)로 제한한다. probe·런 로그·캡처 11종(mus_wsl_ 5+mus_phone_
  6)·phone_music.log는 engine/tmp untracked — **커밋 금지 충족**(add 경로 명시로
  구조 봉합 — docs/88 §6 동형).
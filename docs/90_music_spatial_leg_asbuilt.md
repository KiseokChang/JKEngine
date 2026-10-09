# docs/90 — 뮤직 spatial leg as-built (T1-T5, 2026-10-10)

뮤직 spatial leg 라인(#91 — 뮤직 라이브러리 #90의 2단계) T1-T5 원장. 계획 문서=
`docs/superpowers/plans/2026-10-10-music-spatial-leg.md`(47467e8), 스펙=
`docs/superpowers/specs/2026-10-10-music-spatial-leg-design.md`(aab4f11 — 배선
옵션 1 사용자 확정 "추천대로 갈게요"), SDD 원장=`.superpowers/sdd/2026-10-10-
music-spatial-leg/`(progress·task-1..4 brief/report/review — 본 T5는 docs 전용이라
코드 재실측 대신 원류 원문 1:1 수취가 절차). 결정 원장=**D1 재생 leg=내장·D2
배선=fail-closed·D3 더블클릭=vplayer 위임 유지(2채널 공존)·D4 포맷 표기=정직
문화·D5 폰=spatial leg 배제** — 전부 사용자 확정·기각 시 즉시 수리 계약. 코드
계보는 git log가 진실원. 본 문서는 실측 영수증·함정·deferred·사용자 결제 게이트를
봉합한다.

- **부기(T5 docs 전용)** — 본 커밋은 docs 2파일뿐(engine 소스·include·probe 변경
  0건)이라 **selftest 재실행 불요** — 캐논 표기는 T4 종착 수치(9889519)의
  건네받은 값 그대로(Win 631/WSL 608/posix 296/폰 608). probe 런 로그·캡처는
  engine/tmp untracked(커밋 금지)라 본 문서는 SDD 원장 리포트/review/런 로그
  원문 인용을 진실원으로 쓴다(창작·추정 수치 0 — 원문에 없는 수치는 원장행
  표기로만 남긴다).
- **룰링 승계** — 표준 빌드=`ninja -C engine/build -j3`(Win)·`ninja -C buildwsl
  -j3`(WSL)(docs/89 §0 룰링)·selftest 계수는 `-F` 고정문자열 grep — 본 라인
  T1에서 재실측(§3 #6)·폰 접속 정보=PHONE_HOST env 자리표시만(docs/89 §6
  계약 — 기록 금지)·env-set/미설정 변형은 **build 디렉터리를 서로 다르게**
  (구성 캐시는 env에 고착 — T1 실측).

## §0. 문서 체계 + 체인 라인

docs/85(더티프레젠트)·docs/86(텍스트 스케일)·docs/87(갤러리)·docs/88(클라
idle)·docs/89(뮤직 라이브러리)의 §0-§6 구조를 계승한다 — §1 배선 원장
(rev-parse 전체 SHA)·§2 캐논 표+leg 실측 표·§3 함정 원장(봉쇄 레슨 축)·§4
deferred+park(§4.1)·§5 사용자 결제 게이트(EYES-PENDING)·§6 커밋 원장(IP grep
게이트 병기).

발단: docs/89 §4 D-7 — 뮤직 라이브러리(#90) 착지 뒤 사용자 지시가 이 라인을
열었다. 스펙의 중심은 **교체가 아니라 공존**(D3)이다 — 더블클릭=vplayer 위임은
뮤직 라이브러리(#90)의 D1 계약 그대로 무변조이고, 새 축은 행의 **[spatial] 버튼**
발사 경로(내장 leg — 접근성 낮은 실험 배선·v1 종착지가 아님). spatial-player
쪽은 **무변경 계약**(소비자 입장 — add_subdirectory 소비만·경로는 env로만).
worktree 미사용·main 직행·서브에이전트 스폰 0건(T1-T4 각 리포트 선언·T5 실측).

체인 라인 = **47467e8..HEAD**(T5 실측 rev-parse — 플랜 47467e8(라인 BASE)부터
T4 9889519까지 5커밋+본 T5 커밋·스펙 aab4f11 병기 — §1 표 = rev-parse 전체 SHA).

## §1. 배선 원장 (라인 전체 — rev-parse 전체 SHA 실측, T5 재측정)

| 태스크 | 커밋(SHA — T5 실측 `git rev-parse`) | 내용 |
| --- | --- | --- |
| 스펙 | `aab4f1125dc1792cd31ef63a3455280c9dc0ef6f` | docs(specs): 뮤직 spatial leg 스펙 — audio_core 내장 leg+fail-closed 배선 (2단계) |
| 플랜(라인 BASE) | `47467e82001bd0054fee6d444cf7ca09a5562458` | docs(superpowers): music spatial leg 구현 플랜 T1-T5 (#91) |
| T1 | `6a5ffda4368cb57dfd9db020b521fe5af2de6b37` | feat(apps): music spatial leg 골격+2n+CMake fail-closed 배선 (T1) — MusicSpatialLeg.h 196행 신설+main.cpp 2n 17건+CMake env 분기 32행 (3파일 +343/−0) — LegSupport/Apply(순수 전이)/ToolJson |
| T2 | `f9f0244c723a9d971e4a15e0ba668471aa40027a` | feat(apps): music spatial 재생 leg+UI+도구 3종 (T2) — ClientMusicApp(+h) 2파일 +479/−9 — SpatialStart 단일 발사 경로+[spatial] 버튼 열+spatial 패널+SpatialPump(OnIdle) |
| T2 fix r1 | `c10ddabd4f823a0ed6465f8740d4f37fe20d45f8` | fix(apps): status 절단 정직화+정리 순서 규약 정합 (T2 fix r1) — 리뷰 4건 수형 (1파일 +54/−10) — §3 #9·#3 |
| T3 | `eec4b1dae977e48131fd3a71f3bacc2baaad2d16` | test(probes): music WSL probe — spatial leg 실측 확장 (T3) — wsl_music.sh +226/−11 (probe 1파일) — **LEG-OK(WSLg ALC 성립)+LEG-UNSET-OK+M-1 truncated 원문** |
| T4 | `98895197cef4d03763693f8de30eecc0832c8e9f` | test(probes): music 폰 probe — spatial leg 배제+캐논 흡수 (T4) — phone_music.sh +138/−25 (probe 1파일) — 폰 캐논 591→608(2n 17)+D5 배제 원문 |
| T5 | 본 문서를 포함하는 커밋 | docs(apps): music spatial leg as-built (T5) — SHA는 git log가 진실원 |

**env 계약 원문(CMake — T1 리뷰 §2 작업트리 등호 확증, 브리프 17행 원문)**:

```cmake
if(DEFINED ENV{SPATIAL_PLAYER_ROOT})
    add_subdirectory("$ENV{SPATIAL_PLAYER_ROOT}" "${CMAKE_BINARY_DIR}/spatial-player-build" EXCLUDE_FROM_ALL)
    target_link_libraries(jkapp_music PRIVATE audio_core)
endif()
```

+ 설정 시에만 `target_compile_definitions(jkapp_music JK_MUSIC_SPATIAL_LEG)`
(leg 소스의 매크로 가드 — T2 SpatialPump/발사 경로는 이 가드 안 통째·env
미설정 빌드는 빈 함수). 클라 소비면은 단일 발사 경로로 수렴한다: UI [spatial]
버튼과 `spatial_play` 도구가 **같은 SpatialStart 종착**에 합류(T4가 양경로
동종착 원문 실측 — 도구 발사+UI 탭), env 미설정 분기(cpp `#ifndef`)와 설정 축의
ALC 실패 분기가 **전부 `Apply(LegEvent::DeviceFailed)` → status에
`kDelegationHint` 원문 라벨 소비**로 완전 동형(T2 리뷰 축 3 — fail-closed의
"양변 같은 종착" 구조가 배선 원장의 본질).

폐곡 계보: T1 리뷰 **APPROVE**(C0/I1/M2 — 순수성·산수·posix 사유 전부 재실측) →
T2 리뷰 **APPROVE**(C0/I3/M1 — idle·수명·fail-closed 동형·도구 3종 PASS·M-1
status 절단 악형) → T2 fix r1 re-review **APPROVE 재확정**(C0/I0/M0 — 4건 전부
소각·idle 무변조 전수 frameDirty 21==21) → T3 리뷰 **APPROVE**(C0/I2/M0 — 창작
수치 0·원문 1:1) → T4 리뷰 **APPROVE**(C0/M0/I3 — D5 배제 원문 1:1+리뷰어 직접
육안 라벨 렌더 확증·캐논 3로그 교차확증).

## §2. 캐논 표 + leg 실측 표

### §2.1 selftest 캐논 계보 (2n 계열 — Win/WSL/posix/폰)

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#90 뮤직 라이브러리 종착 = docs/89 최신) | 614 | 591 | 296 | 591 | — |
| T1 `6a5ffda` | **631** | **608** | **296** | (591) | +17 (2n-a..d) |
| T2 `f9f0244` | 631 | 608 | 296 | (591) | 0 (신설 어설션 0 계약 — 등호 런) |
| T2 fix r1 `c10ddab` | 631 | 608 | 296 | (591) | 0 (3축 재실측) |
| T3 `eec4b1d` | 631 | 608 | 296 | (591) | 0 (probe 전용 커밋 — 양변 런) |
| T4 `9889519` | 631 | 608 | 296 | **608** | 폰 2n 17건 흡수 (591+17=608) |
| T5 본 커밋 | 631 | 608 | 296 | 608 | — (docs 전용) |

- **2n 17의 분해** = 2n-a 6(OfExt — 대소문자·숨김 ·mp3·빈·무확장자·표 밖)·
  2n-b 5(Apply 전이 4+순수성)·2n-c 4(ToolJson 3종 원문)·2n-d 2(포맷 표 개수
  등호) — T1 리뷰 grep `2n-`=**17/17**·T4 런 원문 `2n=17`·T4 리뷰가 WSL 독립
  로그 3개(t1/t2/t2r1 selftest)로 교차확증.
- **산수 독립 원천** — `git show:main.cpp`의 `check(` 계수 base 47467e8=**634**
  → head 6a5ffda=**651** (+17 등호 — T1 리뷰 §1 #4).
- **posix 296 등호 = 구조적 등호(하네스 TU 미링크)** — docs/89 §3 #8 원리의
  그대로 승계: `tools/posix_selftest` 링크 TU는 posix 어댑터 세트뿐이고
  engine/src/main.cpp·MusicModel.h·**MusicSpatialLeg.h** 어디도
  링크/include하지 않는다(T1 리포트 §2+리뷰 독립 검증·T2-T5 전 커밋 무접촉 —
  재실측 불요 사유가 구조로 성립).
- **폰 (591) 표기 = 흡수 전**(docs/89 §2.1 관행) — T4 배포에서 2n 17을 한 번에
  흡수해 `CANON-INCLUSION=MUSIC-FULL-2N-17` 재판정(`PASS=608 FAIL=0 2m=26(2m-h=1)
  2i=19 2g=40 2n=17` — **WSL 608 등호**·신규 어설션 0·계보 등호 승계만).
- **T5에서 캐논 재실행 불요**(docs 전용 — 9889519 이후 코드 변경 0건) —
  건네받은 수치 그대로 봉합.

### §2.2 leg 실측 표 (probe 원문 3 leg — wsl_music.sh 2변형+phone_music.sh)

| leg | 배포/영수증 | spatial_play 종착 | status 원문 | stop 원문 | 판정 |
| --- | --- | --- | --- | --- | --- |
| **WSL — env 설정** (T3 — set 런) | scratch(/tmp/mus_scratch) env-set 빌드를 buildwsl에 배포 — `MUS-LEG-SYM-SET: alcOpenDevice refs=2` | `{"ok":true,"windowId":4,"result":{"accepted":true}}` | `active:true·positional:true·deviceOk:true·pos:1.600` → `pos:3.600`(delta=2.000 진행) → post-stop **Eos 스냅 `pos:8.000=dur:8.000`** | 재생 구간 stop `{"active":false,"idle":true}`(사전 Eos 경기 — stop이 도착해 Eos가 먼저였다) | `MUS-LEG-VERDICT: LEG-OK` |
| **WSL — env 미설정** (T3 — unset 런) | leg-less .so — `MUS-LEG-UNSET-SYM: alcOpenDevice refs=0`(deploy 전) | `{"ok":true,"windowId":4,"error":{"error":"start_failed","detail":"spatial leg 불가 — vplayer 위임 이용"}}` | STATUS-1/2 `active:false·deviceOk:false·pos 0.000→0.000·error=""→"spatial leg 불가 — vplayer 위임 이용"` | idle leg 정지도 성공 — `{"active":false,"idle":true}`(idempotent 원문) | `MUS-LEG-UNSET-VERDICT: LEG-UNSET-OK` |
| **폰 — 미정의 계약** (T4 — D5) | `MUSP-LEG-ENV: unset`(설정 런은 계약 위배 hard FAIL 가드)·`MUSP-LEG-SYM: alcOpenDevice strings=0`(nm 대신 바이너리 문자열 grep — Termux binutils 의존 0) | `{"ok":true,"windowId":8,"error":{"error":"start_failed","detail":"spatial leg 불가 — vplayer 위임 이용"}}` → `MUSP-LEG-BARRED: OK` | 기저+post-tap `active:false·deviceOk:false·err=kDelegationHint 보존`(성공 시작이 없으면 소멸하지 않는다)·**`MUSP-SPATIAL-POS: MISS(미활성)` 1행 남김**(T3 리뷰 I-2 승계 — 자기교정 원장행) | —(성립 재생 없음) | 폰 leg 배제 원문 성립 (D5 정직 계약) |

- **M-1 승계 원문 실측(T2 fix r1 소각 — T3가 완성)** — 초장 경로 트랙(955B —
  7단×132자 세그먼트·컴포넌트 255 상한 안전) spatial_play → `spatial_status`가
  `{"active":true,"positional":true,"deviceOk":true,"pos":2.100,"dur":8.000,
  "truncated":true}` — **path 생략+truncated:true 정직 축소 전문**(절단 분기
  `need>=768` 도달 재확증) → LONG stop `{"active":false}` 원문.
- **vplayer 위임 회귀(D3 공존 계약 — 재유입 결함 0건 실측)** — WSL set 런:
  pair 1(gap 447ms — 시간창 0.30s 초과라 자동 재탭 규약)→pair 2→Video Player
  창(id 46) → 귀속 폴링 `opened:true` → 직행 open `{"accepted":true}` →
  get_status **pos 3.437→6.780/dur 8.000**. unset 런: 귀속 `pos 3.715`·직행
  `pos 3.111→6.362`. 폰: pair 2 탭 gap **129ms < 0.30s** → 창 → 귀속
  `opened:true` → open accepted → **pos 4.226→7.954/dur 8.000**.
- **판정 라인 원문** — WSL 양 런 `MUSIC-VERDICT: MUSIC-OK(WSL 빌드 rc=0·selftest
  FAIL=0 계보 PASS=608(캐논 608) 2m=26·delegate=창 출현(deleg-open=1)·glue=
  accepted·get_status opened=true — 육안 스탭 대기)`·폰 `MUSIC-PHONE-VERDICT:
  MUS-PHONE-OK(...pos 4.226→7.954·폰 leg 배제 원문 성립...— 육안 스탭 대기)` —
  **수치 영수증 라인이지 육안 선언 아님**(§5).
- test 미디어는 합성만(엔진 소유 — 사용자 보관 풀 불접촉 계약·플랜 원문): 8초
  사인 WAV
  705,644B 시드 4종(root 3+sub 1)+초장 경로 1종. mp3 합성 = WSL ffmpeg 부재 →
  wav 단축(플랜 원문 수형)·폰 ffmpeg **존재** 재실측 — 원장행(§4 SP-6).
- LEG-FALLBACK-OK 분기(ALC 실패 환경 시뮬레이션)는 양 런 모두 **미출현** —
  set 런은 디바이스가 성립했고 unset/폰 런은 처음부터 leg가 없다(수형 분기 코드
  probe 대기 — §4.1 park·폰 재판정은 §5 ③과 묶음).
- 캡처 영수증(engine/tmp — **전부 untracked, 커밋 금지 충족**): WSL 6종
  `mus_wsl_{spatial,list,filter,vp_delegate,delegate_status,after}.png` —
  `mus_wsl_spatial.png`(903,267B) 신설=이전 라인에 없던 [spatial] 패널·폰 7종+crops — `mus_phone_spatial.png`(706,659B)+`pmus_spatial_crop.png`
  = "spatial leg 불가 — vplayer 위임 이용" **라벨 렌더**(T4 리뷰어 직접 육안
  확증 — 영수증이지 사용자 결제 아님 §5).
- 스펙 §4 판정 축 대조: ①WSL leg 재생+상태 표기+정지 = §2.2 set 런 원문 ②위임
  회귀 없음 = 위 bullet(더블클릭 그대로) ③폰 캐논 등호+미활성 표기 원문 = T4
  원문 — 전부 probe 실측으로 성립·최종 결제만 미결(§5).

## §3. 함정 원장 (T1-T4 리포트+리뷰 종합 — 봉쇄 레슨 축)

1. **뮤직 라인 원장 승계 (요약)** — docs/89 §3의 8건(봉투/본문 이층 ok 경계·
   소진 전이 dead code·MinGW junction 사각지대·file_clock 음수 epoch·
   remove_all 루프·launch 콜드 경기·폰 첫 클릭 취식·posix TU 미링크)은 그대로
   유효하며 **이 라인에서 재유입 0건** — 위임 경로가 원문 유지된 실측(T3 리뷰
   축 4: 위임 세그먼트가 전부 context 행·훅 교체 동봉 삭제 유형 무발견)이
   승계를 증명한다. 이 라인의 판정 구조는 순수 부품(2n)+양변 빌드 영수증+
   probe 3 leg 원문 — **결함 0건 폐곡**으로 닫았다(REJECT 0·fix 1회).
2. **★CMake compat 2행(openal-soft 상류 1.23.1 × 새 도구 세대 — T1 §6)** —
   ①msys64 CMake(4세대)는 openal의 `cmake_minimum_required(3.0.2)`를 거절 →
   `set(CMAKE_POLICY_VERSION_MINIMUM 3.5)`(add_subdirectory 전·env 분기 안)
   ②GNU 16.2가 `<bitset>`의 전이 `<cstdint>`를 잘라 alu.h:18
   `enum CompatFlags : uint8_t`를 거절 →
   `set_target_properties(OpenAL COMPILE_OPTIONS "-include;cstdint")` — 단
   CMake `COMPILE_OPTIONS`는 세미콜론 리스트라 `"-include cstdint"`(공백
   1-arg) 원문은 GCC에 "선행 공백 파일명"으로 죽는다(**2-arg 수형** ·
   target_compile_options 분해도 같은 함정). ③**C++20 상향 = 역함정 원장** —
   CXX_STANDARD 상향 시 alspan.h:219 unqualified `to_address`와 std::to_address의
   ADL 충돌(1.24에서 수리된 상류 결함 — 실측 7 errors·almalloc계 6파일) →
   C++14 고정 유지가 옳다 · **GIT_TAG를 1.24로 재지정하는 것은 spatial-player
   무변경 계약 위배 — 금지**(재판정은 라인 밖 — §4 SP-11).
3. **★env-set/미설정 양변 심볼 영수증 — fail-closed의 구조 원리** — env
   **미설정**: 빌드 로그에 spatial|audio_core|openal 문자열 0건(컴파일 단계부터
   생략)·jkapp_music 바이너리의 AL 심볼 **0건**(T3 `alcOpenDevice refs=0`·T4
   `strings=0`) — "leg가 없다"가 대상 바이너리 문자열로 증명된다. env
   **설정**: scratch 빌드 배포 뒤 `alcOpenDevice refs=2`(T3 set)·Win dll의
   SpatialEngine 심볼 11(T2 fix r1) — 양변 같은 grep 축의 정반대 극값. 변형
   구별은 build 디렉터리를 서로 다르게(구성 캐시 고착). 부기(하네스 결함
   원장): 헤더 inline-default ctor × 불완전형(unique_ptr 멤버 SpatialEngine/
   StreamPlayer) 소멸자 인스턴스화 = **env-set 빌드에서만 `sizeof` incomplete
   type 사망** → ctor 원밖 수형(cpp `= default` — env-unset 무접촉, T2).
   fail-closed가 "옵션 빌드"를 사기 없이 지탱하는 4축: 컴파일 생략·링크 생략
   ·심볼 0·UI 동형 종착.
4. **scratch+buildwsl 동시 빌드 I/O 경합 — 시차 배치(원장행→구조 봉인)** —
   T2 fix r1 env-set scratch 런에서 decoder.cpp.obj가 컴파일러 출력 0건으로
   fail 1회(WSL buildwsl 빌드와 동시각 — 공유 트리 I/O 경합 추정·재시도 1회
   소각·재현 0) → **동시 `-j` 금지·시차 배치**. T3 probe가 이 원장을 구조에
   봉했다 — 3b(scratch) 세그먼트가 buildwsl 빌드+셀프테스트 뒤에만 위치하는
   직렬 구성(T3 리뷰: 동시 -j 흔적 0건·이후 전과 0).
5. **컴파일 경고 행에 경로 문자열 유입 — "grep 0" 검증은 세척 뒤** —
   audio_core 경고 행의 `-I` 플래그가 spatial-player 경로를 비춤 → tmp 로그에
   경로 문자열 4건 유입(T2 fix r1) — 경로 포함 행 sed 제거 후 재그렙 0
   검증. 레슨: **로그가 커밋 대상이 아니어도 경로 비출 금지 계약은 같다(T1
   위생 계약 승계) — 위생 그렙은 세척 뒤에 실행하는 것이 절차**(T2 fix r1
   re-review 승계 그대로).
6. **grep 인용 함정 — `[PASS]` 문자클래스 — `-F` 필수** — shell이 `\`를 벗기면
   `[PASS]`가 문자클래스로 편성되어 무관 행까지 집계된다(T1 실측 636 오표기 →
   `-F` 재측정 631 정정) — 캐논 계수·리뷰 재측정 전부 `-F`/앵커 행 카운트로
   고착(docs/89 룰링의 이 라인 재실측).
7. **pos 표기 = `current_frame` 기반 ~0.2s 선행(정직 표기 원장)** —
   SpatialPump의 pos는 `StreamPlayer::current_frame()`(디코더 커서 — prefill
   ~200ms 선행 포함)을 sample_rate로 나눈 것: 원문 CLI와 동일 수령계약이지만
   실재 청각 착지 위치보다 최대 ~0.2s 앞서 보인다(T2 concern→T3 WSL 축 동형
   승계 — EYES 청안(§5 ①)에서 같이 판정하는 원장 부기).
8. **폰 [spatial] 탭 좌표 상수 추정(원장행 유계)** — SPAT_X=495/SPAT_Y=132 —
   env MUSP_SPAT_X/Y 재보정 열기·창 위치 변동 재런에서 탭이 빗나가도 verdict는
   OK(도구 경로만으로 성립) — 다만 **도구 발사가 같은 SpatialStart 종착을 먼저
   증명하고 UI 탭은 게이트에서 뺀 설계라 verdict 오염 없음**(T4 리뷰 판정)·
   성립 판정은 캡처 라벨 렌더 육안이 대행.
9. **stop 답신 축약 인용 1행 정정(T3 리뷰 I-1)** — T3 리포트가 LEG-OK stop을
   `{"active":false}`로 인용했으나 set 로그 원문은 `{"active":false,"idle":true}`
   (`,` 생략) — 이 축약이 정확했던 곳은 LONG stop(`{"active":false}`)뿐이고
   본 문서 §2.2는 원문 병기로 정정했다. 레슨: **"원문 인용" 표기에 축약을
   섞지 마라 — 수치 창작은 아니어도 원문 표기의 1:1 계약은 표기 위생이다**
   (§4 SP-2 소각).

## §4. Deferred (소등 후보 — 유예 라인, docs/89 §4 문체 계승)

| ID | 사항 | 근거 |
| --- | --- | --- |
| SP-1 | 도구(status) 절단 분기의 **런타임 원장행 — 소각** | T2 fix r1 concern(700B+ path 트랙이면 실측 가능) → **T3에서 955B 초장 트랙로 `truncated:true` 원문 실측 완료**(§2.2) — 소각 표기 |
| SP-2 | T3 리뷰 I-1 stop 답신 축약 인용 | **소각** — 본 문서 §2.2 원문 병기(§3 #9) |
| SP-3 | T3 리뷰 I-2 fallback 런 MISS 원장행 누락 | **소각** — T4가 `MUSP-SPATIAL-POS: MISS(미활성…)`을 무조건 1행 인쇄로 수형·본 런에서 실측 출력 확인(재런 자기교정 원장) |
| SP-4 | T4 리뷰 I-1 [spatial] 탭 좌표 상수 추정 | 원장행 유계(§3 #8) — verdict 비오염 판정 승계·재런에 env 재보정 몫 |
| SP-5 | T4 리뷰 I-2 cosmetic 3건 묶음 | §4.1 park |
| SP-6 | T4 리뷰 I-3 폰 ffmpeg 존재 편차 | 원장행 등록만(`MUSP-AUDIO-MP3: ffmpeg 존재 — 그래도 wav 단축 계속`) — mp3 합성 실측은 WSL T3 소유·부재 기대와 반대였으나 파탄 0 |
| SP-7 | T2 concern — pos ~0.2s 선행 표기 원장 | §3 #7 — 청안 게이트와 동시 판정 |
| SP-8 | DLL 크기 편차 **정정 원장** — T1 표기 27MB → 실측 61MB | T2 concern·T2 리뷰 승계 — env-set 정적 openal 오버레이 **Win dll 61MB(T2/T2 fix r1 실측)**이 진실값·27MB는 T1 시점의 모듈 성장/링크 시점 차 표기로 폐기·미설정 WSL .so 5.86MB(무팽창 — fail-closed 소극 영수증 부합) |
| SP-9 | spatial 패널 하단 200px 고정 분할 | T2 리뷰 EYES 병기 — 560×520 기본 창에서 슬라이더 3조+진행 bar+버튼+표기행 밀집 — 자식 내 스크롤 흡수 후보(GUI 모양 EYES 몫) |
| SP-10 | FetchContent 재다운로드 비용 | T1 D-D·T3 concern — binary-dir당 로컬 캐시라 scratch 재건마다 재다운로드(WSL scratch 풀빌드 ≈17분 — drvfs 지연·Windows ≈5분) — 장기 캐시는 비-목표 |
| SP-11 | openal 1.24 상류 수리(alspan to_address) 흡수 재판정 | T1 D-E — **GIT_TAG 재지정 금지(무변경 계약)** — 라인 밖·사용자 결제 몫 |
| SP-12 | T1 리뷰 concerns 3건 승계 | **전부 T2 흡수 완료** — I-1 `.vorbis` 표기 문구("ogg(vorbis 코덱)" 1행)·M-2 durSec=0 진행 가드("길이 미상")·M-3 SpatialStart 선두 공문자 가드(`bad_args`) — T2 리뷰 §6 원문 이행 확인 |
| SP-13 | T1 D-A/D-B/D-C (WSL leg·폰 배제·audio_core 소비) | **소각** — T3/T4/T2 각 승계 실측(§2.2·§2.1) |
| SP-14 | docs/89 §4 D-7 (spatial-player 연동 2단계 — 별도 스펙 사용자 선언 대기) | **소각** — 본 라인: 스펙 aab4f11(옵션 1 사용자 확정)+T1-T4 배선/실측으로 착수·이행 — docs/89 쪽 포인터는 §6에 병기 |

### §4.1 원장 park 목록 (리뷰 finding 종합 — 전부 비임계)

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T4 리뷰 I-2-① | SPATIAL_INACTIVE 대입 후 미사용 변수 | M | 다음 probe 접촉 시 1행 소각 |
| T4 리뷰 I-2-② | LEG-ENV 위배 시 echo concat 표기가 "UNEXPECTED-SETunset"으로 인쇄 | M | FAIL 바로 뒤따라 판정 무영향 — 다음 접촉 정리 |
| T4 리뷰 I-2-③ | VERDICT OK 행이 CANON 어노테이션 전문 중첩 삽입(괄호 장행) | M | 정보 손실 0 — 다음 접촉 1행 정리 |
| T3 concern 4 | LEG-FALLBACK-OK(수형 ALC 실패 분기) probe 원문 미출현 | M | §2.2 원장 — **폰 재판정(§5 ③)과 묶음** — 다음 probe 접촉(ACTUAL 실패 환경) 실측 몫 |
| T3 concern 3 | LEG_MODE=1 재런마다 scratch 풀빌드+FetchContent 재다운로드 | M | SP-10 원장 — 장기 캐시 비-목표 |

## §5. 사용자 결제 게이트 (EYES-PENDING 봉인)

**EYES-PENDING — 라인 최종 결제는 사용자 육안/청안 선언만으로 성립한다. probe와
본 문서는 결제를 기록하지 않는다**(docs/89 §5 동형 — LEG-OK·MUSIC-OK·
MUS-PHONE-OK는 수치 영수증 라인이지 육안/청안 선언 아님). 본 라인의 결제 상태는
**미결제(대기)**.

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **청안(육안의 청각판) — HRTF 위치감** | WSL(WSLg pulse 백엔드 실측)또는 Windows에서 합성 wav(상황 여건이면 mp3/flac)의 spatial 재생 — POSITIONAL 토글+위치 슬라이더 3조(방위각 -180..180°/거리 0.5..20m/고도 -45..45°) 조작으로 위치 소감이 실제로 변하는지. env-set 변형 빌드 필요(§1 절차 — T1 §4 재연 절차 원문). pos 표기 ~0.2s 선행(SP-7)도 동시 판정 | **대기(청안)** |
| ② | music 앱+spatial 패널 모양 (Win+폰) | Win/폰: [spatial] 버튼 열·spatial 패널(진행 bar·POSITIONAL/STEREO 토글·슬라이더 3조·지원 표기 행)·행 [spatial] 열(미지원 회색+툴팁)·폰 미활성 라벨 — 캡처 WSL 6종+폰 7종 보존(§2.2)·SP-9 밀집도가 1차 관측점 | **대기(육안)** |
| ③ | 폰 leg 정착 | 스펙 D5 — OpenAL/디바이스 백엔드 폰 재판정 — **청안(①) 후 후속 판정·별도 합의**(폰에서 leg 성립 런은 계약 위배 hard FAIL 현재 계약) | **대기(후속 합의)** |
| ④ | 스펙 §4 성공 판정의 사용자 육안 | "WSL: music 앱 [spatial] 재생 → 상태 표기(pos 진행)+정지 — probe rc=0 원문"(§2.2)+vplayer 위임 회귀 체감(더블클릭 그대로)+idle 계약 체감(재생 중 진행 표기 갱신만 그린다·정지/고장 leg 무더티) | **대기(육안)** |
| ⑤ | 스펙 결정 재량(내장·fail-closed) | D1 내장·D2 fail-closed — 스펙 확정(aab4f11 옵션 1)+본 문서 원장 성립 — **기각 시 즉시 수리** | **대기(유효)** |

## §6. 커밋 원장 + IP grep 게이트

- 라인 체인 = **47467e8(플랜)..HEAD**(T5 실측 rev-parse — git log가 진실원):
  aab4f11(스펙) → 47467e8(플랜) → 6a5ffda(T1) → f9f0244(T2) → c10ddab(T2 fix
  r1) → eec4b1d(T3) → 9889519(T4) → 본 T5 커밋(§1 표 = rev-parse 전체 SHA).
  push 전부 origin main 직행·amend 없음.
- **IP 리터럴 게이트(T5 실측)** — 본 커밋 원문에서
  `\b([0-9]{1,3}\.){3}[0-9]{1,3}\b` grep = **0건**(커밋 전 staged/`git show
  HEAD` 게이트 실행). 라인 이전 커밋도 리포트/review 전원 IP grep 0건 실측
  (T1·T2·T2 fix r1·T3·T4 각 리포트+리뷰 재실측 — §1 계보).
- **경로 리터럴 게이트(T5 동일 축)** — spatial-player 저장소 위치·호스트 드라이브
  경로는 commit 파일에 기입 금지(`$ENV{SPATIAL_PLAYER_ROOT}` env 이름 자리표시만
  — T1-T4 전 리뷰가 호스트 경로 표기 그렙(리포트 원문 축 — 사용자 루트 폴더명·
  드라이브 글자·/mnt 계열 표기) 0건 실측)·**본 문서도 동일 게이트로 커밋** —
  probe 런 로그의 경로 포함 행(shot/capture 절차 행) 인용 배제·판정/JSON 행만
  인용.
- 본 T5 커밋은 docs 2파일(`docs/90_music_spatial_leg_asbuilt.md` 신설+
  `docs/89_music_library_asbuilt.md` §6 형제 라인 포인터 1행 — git add 명시
  경로만, #83 사건 레슨 유지)로 제한한다. run 로그 4종(wsl_music_run_set/
  unset·phone_music.log·phone_music_run.log)·캡처 WSL 6종+폰 7종+crops는
  engine/tmp untracked — **커밋 금지 충족**(add 경로 명시로 구조 봉합 —
  docs/89 §6 동형).
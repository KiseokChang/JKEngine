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

- **부기(#92 폴더 관리 라인 — 하단 부기·변형 아님)** — 본 문서에 §7 신설
  (뮤직 폴더 관리 UI 라인 T1-T3+T2 fix r2 원장): 후속 체인 =
  **7f66b81(플랜)..52f672c**+본 T5 커밋 — §7.0 표 = rev-parse 전체 SHA.

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

신규 park 행(#92 폴더 관리 라인 — §7.4 승계 · **아래는 추가만, 기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T1 리뷰 M | .bak 사다리 3연속 실패 창(대피 성공+재치환 실패+복원 실패 — 원문 `.bak` 좌초) | M | err 표면화+원문 바이트 `.bak` 잔존의 수형 — 실사용 극저빈도·**수리 금지 판정** |
| T2 리뷰 M-2 | ComposeKeyed 중복 관리 키 last-wins 편차(리더가 구치 읽기) | M | 외부 수기 원문 한정(자체 작성 원문 도달 불가) — 원장 |
| T2 리뷰 M-5 | posix 296 등호 = 하네스 TU 미링크 **간접 근거만** | M | T1 계보 정합 수용 — 다음 posix 직접 실측 시 확인 |
| T3 리뷰 M-1 | probe CANON 상수 608 진부(캐논 624와 미일 — 침묵 회귀창 16건) | M | **소각 — T4(cf690c8)에서 624 실측 승격 완료**(WSL 등호 — §7.2) |
| T4 원장 ① | dirs 등호 앵커 = 작가 정규형(저장소 작가 ComposeKeyed 출력형 — 웜업 왕복 1회) | N | WSL T3 "seed 출하형 교환" 수형의 폰 런타임 동형 원장 — probe 원문 동봉 |
| T3 리뷰 M-2 | T3 리포트 수치 오기("probe 상수 604" — 실제 608) | M | 캐논 등호 본안 유효 — 원장 부기 |
| T3 리뷰 M-3 | 브리프-구현 계약 2원화(부적합 주입 "probe에서 안 함" ↔ 16c probe 주입) | M | 상위 원문(T2 fix r1 재검 M-1r) 전입 정합 — 브리프 갱신 렛슨 |
| T3 리뷰 N-1 | I1-OK 판정 1행에 P1 봉인(cure가 크래시+재스폰 분기) 미표기 | N | 판정 행 `p1=crash-observed` 동봉 — 다음 probe 접촉 |
| T3 리뷰 N-2 | 캡처 7종 md5 리포트 미동봉 | N | T3 리뷰 라이브 수취(dirs_panel 471b6cdd 등 7종) 승계 — 재보정 런 시 재실측 |
| T2 fix r2 원장 | 합성 입력 간헐 수신(MousePos 무갱신 세션 — 하니스 결함 후보 — 재검 N-1r 원문 수용·capture 결함 라인 교차 검토 가치) | M | 실기기(사람 입력)와 결정 차이 — 별도 라인 권고·[제거] E2E는 §5 ⑧ |
| T2 fix r1 재검 원장 | AddDir의 invalid 문서 영속화 경로(쓰기 leg 잔여) | N | 패널 제거 치유 가능 — 전각 치유는 별도 라인 |
| T2 fix r1 재검 원장 | 도구 인자 "C:" 표기 엣지(fs::path filename과 다른 반환) | N | 절대 경로 계약상 도달 불가 — 표기 전용 필드 |
| T2 fix r2 재검 N-2r | break 도입의 신계약 "제거 1회/프레임"(연속 제거 시 행당 1프레임) | N | UX 원존과 동형(이전도 프레임당 1클릭)·관측상 무영향 |
| T4 **P3 후보** | 폰 [폴더 관리] 토글 Button 클릭 **무응답**(정산 좌표+hover tooltip 원문 조건에서 3런 2회+라이브 2회 — 같은 클라에서 필터 InputText 포커스·표행 더블클릭은 성립 — **스트립 행 Button 클릭 경로 전용** 관측) | P3 | 코드 무변경 판정 — 폰 스트립 행 클릭 전용 캘리브레이션 세션(WSL 16c 차분 게이트 수형) 후속·**패널 개방 = EYES**·**T4 리뷰 승격 판정 병기: "앱(폰 스트립 행 Button) 쪽 유력·하니스 release-half 불배제"** — 확정은 후속 세션 몫 |
| T4 리뷰 I-1 | P3/C2의 라이브 실험 텍스트 원문 미보존(`approval_timeout`·sent:true — 생존 로그 0행) | M | 결제 게이트 무계약(실체는 픽셀 원문 승인·커밋 주석 계약 봉합) — **이번 한정 면책·후속 세션부터 원문 1행 기록 권고** |
| T4 리뷰 M-1 | probe 헤더 내부 모순("dirs=열림 프레임" 서술 ↔ 실측 등록 상태 스트립·개방 0) | M | `mus_phone_dirs.png`를 열림 영수증으로 오독 방지 — 다음 probe 접촉 시 헤더 교정 1행 |
| T4 리뷰 M-2 | 치우기(window_move) 직접 수취 원문 부생존(after 판별 불가 — 결과 캡처로만 정산)·1차 OTHER-N 런 로그 tee truncate 부생존 | M | verdict 무계약 — 계보 원장 부기 몫 |
| T4 원장 ② | 토글 좌표 편차(WSL 토글 (130,70)은 2자 라벨 기준 — 폰 14자 라벨 스트립 시프트 → 캡처 픽셀 1:1+offset 실측 정산)·leg list 후반 vplayer 전면 덮음(topmost 취식 — window_move 치우기 전배선 수형) | N | env 재보정 열기 원장 — 좌표 재런 몫·치우기 이후 런 등호 성립 |
| T4 원장 ③ | 폰 라이브 실험 계약: send_input 미병합 클릭=승인 창 `approval_timeout` 원문(무승인 운용 확인)·실험 후 permissions.json 바이트 원복(33바이트 등호)·폰 클라 pkill은 **`[m]usic` 브래킷 필수**(무브래킷 = 원격 셸 자기 매치로 사망 — 런 중 실측) | N(계약) | probe 재사용 몫 — probe 소유 잔산 소각(원장)·종료 상태 = 서버 UP+taskbar/terminal 유지 |

### §4.1-2 신규 park 행(#93 스캔 안전화+#94 경로 인코딩 라인 — §7.7 승계 · **아래는 추가만, 기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T3 리뷰 M-1 | `mus_wsl_scan_busy.png` EYES 영수증 스테일 — cp 우선순위(폴백 경계 `mus_land_c0.png \|\| mus_land_c0b.png`)가 무착탄 c0 프레임을 우선 복사(런 4/5 둘 다 — 탭 강조 없는 0곡 프레임) | M | 착탄 프레임(c0b) 우선 cp 순서 수형 — 다음 런 몫·판정축(CANCEL-OK)은 land 플래그+로그 원문 소관이라 무영향 |
| T3 리뷰 M-2 | big-tree REMNANT rc 계약 비대칭 — REMNANT는 MUSIC-FAIL(rc=0 honest)뿐, 시드는 hard FAIL(exit 1) — "probe 소유 잔상 0"의 동일 소관에 rc가 다름 | M | 완화: ENTRY 가드 big_a/big_b 잔존 hard FAIL이 무음 소멸 봉쇄 — 다음 접촉 rc 정합 수형 몫 |
| T3 리뷰 M-3 | wsl_music.sh 16d-5 주석 "탭 스캔 6곡 → 표 '6곡'" — 실측 mus_seed 5곡(캡처 5행/5곡) — 주석 오자(T2 M-1 선형) | M | **미소각 — HEAD 97b2d8e 실측 잔존(wsl_music.sh :1488·:1493)** — 다음 접촉 커밋 병행 소각 |
| T2 리뷰 M-1 | ClientMusicApp.cpp:185 소멸자 주석 오자 "(스캔CancelFn)" → "ScanCancelFn"(주석 전용 — 계약 무접촉) | M | **미소각 — HEAD 97b2d8e 실측 잔존(:185)** — 다음 접촉 커밋 병행 소각 |
| T2 리뷰 I-1 | SettingsText 한글 exe 디렉터 **런타임 케이스(selftest) 부재** — fs화 수형은 착지(97b2d8e)하나 2q-계열 읽기 케이스 원장행 없음 | M | settings probe(한글 exe 설치 디렉터 settings.json 판독 케이스) 원장행 — 후속 probe 접촉 몫 |
| T2 리뷰 I-2 | [제거] 취소 1발 = 진행 스캔 루트 대조 없는 **무조건 1발**(스캔 루트≠제거 대상인 제거도 절단+재청구 — 재비용 1회) | M | T3/T4 실측 후 재판정 대기 — T3/T4 3실측 전부 동일 루트 경계라 판정 불성립·park 유지(현 실규모 스캔 ms급 — 비용 원장성 낮음) |
| T3 리뷰 I-2 | probe 사망 코드 — `tapq` 헬퍼 정의 전용(landtap이 대체 — 미호출) | M | 다음 접촉 소각 몫 |
| T3 리뷰 I-3 | 런1 writeback 경합(도구 응답 15s×3 초과)의 **수치 원문 미보존** — 수리 수단(sync)의 효과는 run16d_2-5 drain 원문으로 실측 성립 | M | 후속 동형 라인 원장행 승계 시 원문 보존부터(T4 폰 원장행 · §7.7.2 부기) |
| T3 리뷰 I-1 | 브리프 문언 "하위 40개×파일 30개" → 실측 40×11,520=460,800 — 캘리브레이션 커밋 주석 원장화+`MUSIC-SIZE-EVIDENCE` 정직 부기 | M | 승계 편차 수용 원장 — 재정산 노프(수형 원장 §7.7.2) |
| #94 리뷰 C-2 | 수리 리포트 §4 grep 인용 흠집 — 인용 행(`770:` 주석)이 인용 명령 패턴과 불일치 | M | 결론 옳음(주석=코드 아님) — **재발시 인용-명령 일치 원칙** |
| #94 리뷰 I-3 | 실측이 "커밋 트리 그대로 빌드"가 아님(작업트리 2p→2q 기반 — 블록 상호독립 등가) | M | 원칙상 커밋 트리 기준 빌드 차이 기록 — 병렬 수술 원장(§7.7.3 #7) |
| T4 원장 ① | landtap 폰 미이식 — tab2 병합 구성(tree_a를 dirs_[1]에 앉힘·TAB2_X 상수 재용)으로 대체 | N | 원장행(§7.7.3 #5) — 신규 좌표 원장 0 원리 |
| T4 원장 ② | 폰 MISS 시 2차 사이클(2차 = 캡처 없는 고속 탭→remove) 미실측 — 1차 사이클 직행 성립 | M | 폰 스캔이 캡처+ctl 왕복보다 빠른 기기에서만 유성 — MISS 수취 시 rc=0 honest(SCALE 재정산 재런 몫) |
| T4 원장 ③ | close→소멸 임계 5.0s(WSL 3.5s 수형의 폰 정산 — 1s 폴링+ctl 왕복) — 실측 1.25-1.26s는 임계 0.75 미만 여유 | N | 다른 폰 기기 재정산 몫·MUSP_BIG_SUBS/MUSP_BIG_FILES env 재정산 몫 동봉 |

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
| ⑥ | **[폴더 관리] 패널 모양 (§7 — Win+폰)** | 토글→패널(UserDirs 행별 [제거]+경로 Edit+[추가]) — 캡처 `mus_wsl_dirs_panel.png`(패널+CP949 행 "????" 표기) 보존·폰 캡처는 **T4(cf690c8)에서 9종 회수**(dirs `56f49927`·dirs2 `638204e8` — 등록 상태 스트립+상태행·**패널 개방 프레임은 P3 원장행** — §4.1) | **대기(육안)** |
| ⑦ | **settings 실물 쓰기 확인 (§7)** | 폴더 추가 수동 동작 → settings.json에 `music.dirs` 실물 기록+audio/retention 보존 — 도구 경로는 C1-FILE-OK 실측(§7.2)·수동 동작 확인은 사용자 선언만 | **대기(수동 동작)** |
| ⑧ | **[제거] 실수동 E2E — 사람 클릭 (§7)** | 행 [제거] 1회 클릭→쓰기 성립→목록 갱신 — 합성 입력 플레이크로 fix r2에서 최종 확정 미달(런 A/B 사망 0+기계 프루브+도구 차별 3점 보험 — §7.3 #1) | **대기(사람 클릭)** |
| ⑨ | **CP949 항목 표기 수용 (§7 — 가능하면)** | 보존-가시 표기는 바이트 그대로 — 폰트 깨짐("????")은 표기-한계 원장(§7.3 #2) — 사용자 수용 판정 | **대기(육안·가능하면)** |
| ⑩ | **스캔 안전화 캡처+체감 — WSL (§7.7)** | 스캔 진행 중 표기 캡처(`mus_wsl_scan_busy.png` — 리뷰 M-1: 무착탄 c0 프레임 폴백 복사 스테일 — 재런 c0b 우선 몫(§4.1-2))+재스폰 정착 캡처(`mus_wsl_rescan_settled.png` — 탭 강조·표 5행·5곡 카운터 — T3 리뷰 직접 열람)+실기기 체감(스캔 중 종료/제거의 스무스함 — **라인 최종 게이트 — 플랜 사용자 게이트 요약 1**) | **대기(육안·체감)** |
| ⑪ | **스캔 안전화 캡처+체감 — 폰 (§7.7)** | `mus_phone_scan_busy.png`+`mus_phone_rescan_settled.png`(crop 2종 동봉 — 캡처 11종 보존 — 200,000/트리 실측)+close→소멸 1.25s·폐기 0.62s 경계의 체감 — 사용자 선언만 결제 | **대기(육안·체감)** |
| ⑫ | **한글 트랙 재생 재시도 — #94 결제 (§7.7)** | 더블클릭(위임) 재생 **사용자 재시도** — 사용자 보고 문제("파일을 찾을 수 없습니다")의 실기기 결제 — vplayer fs 게이트+SpatialStart `Utf8ToAnsi` 네이티브 수취 수형의 최종 결제 | **대기(실기기)** |
| ⑬ | **폰 Music 런처 아이콘 (§7.7 병행 — 배포)** | 사용자 "폰 화면에 뮤직 안 보임" — Music 런처 아이콘 설치 배치가 본 T5와 **병렬 dispatch**(배포 절차 계약 전부 — 본 라인 밖 소관·별도 원장) | **대기(별도 배치)** |

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

## §7. 폴더 관리 UI 원장 (#92 라인 — T1-T5, 2026-10-10)

뮤직 폴더 관리 UI 라인(#92) 원장 — 사용자 요청 "음악 폴더를 추가하는 UI"가
연 라인(플랜 원문 "음락 폴더 추가하는 UI가 있어야 하지 않아요?" — "음악"
오자 정정 인용)·목표=settings.json **수기 편집 소거**.
플랜=`docs/superpowers/plans/2026-10-10-music-dirs-ui.md`(7f66b81), 스펙=
`docs/superpowers/specs/2026-10-10-music-spatial-leg-design.md` §2 확장
(spatial 라인과 스펙 문서 공유 — D4 정직 문화 승계), SDD 원장=
`.superpowers/sdd/2026-10-10-music-dirs-ui/`(progress·task-1..3 리포트 6종
(fix 리포트 포함)·review 4종(재검 3종 동봉)·review diff 7종). 이 라인의 지질학적 의미: settings.json을 쓰는
**클라 앱 측 최초 작성자**(기존 작성자=서버 WriteSettingsKv 단일) — 교차
작성자 C1이 라인의 중심축이었고, **CP949 바이트 투명 원문 슬라이스 스캐너**가
라인의 서명 수형. T5는 본 §7 — docs 1파일 전용(engine/probe 무접촉)이라
selftest 재실행 불요(§0 부기 원칙 승계 — 647/624/296 계보 수취 · 캐논
계수는 `-F` 고정문자열 grep로 T1에서 재실측 완료된 룰링).

### §7.0 체인 (rev-parse 전체 SHA 실측 — T5 측정 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 |
| --- | --- | --- |
| 플랜(라인 BASE) | `7f66b81c0ba8c9ab0a4e4208d67b3f1be560982b` | docs(superpowers): music 폴더 관리 UI 플랜 T1-T5 (#92) |
| T1 | `77cabf7af989e01b611bbdc8e3f5e55c3178036d` | feat(apps): music 폴더 저장소 — settings.json 원자적 쓰기 (T1) — MusicDirStore.h 신설 492행+main.cpp 2o 10건 (2파일 +599/−0) |
| T1 fix r1 | `f4bfcf3ba4ef2b164b2b875445806b31e51746e0` | fix(apps): music dirs CP949 보존+RemoveDir/가드 보강 (T1 fix r1) — ReadDirs 승격+2o-f 4건 (3파일 — MusicDirStore.h·MusicModel.h·main.cpp) |
| T2 | `6de919b2229dde0b78b1924453cf29d822e3c19b` | feat(apps): music 폴더 관리 UI+도구 3종+서버 쓰기 보존 합성 (T2) — 5파일 (ClientMusicApp .h/.cpp·JKWindowServer.cpp C1·MusicDirStore.h·main.cpp 2o-g 2건) |
| T2 fix r1 | `f1d9c494e5178f4e078ba9154aac2444c78e2fb8` | fix(apps): settings 부적합 원문 정직 거부+tmp pid 접미 (T2 fix r1) — 3파일 (JKWindowServer.cpp·MusicDirStore.h·ClientMusicApp.cpp) |
| T2 fix r2 | `52f672c6802a99d08b30c7793c65a62d3daacdd5` | fix(apps): music 폴더 제거 후 클라 크래시 — UI 행 루프 UAF 수리 (T2 fix r2) — 1파일 ClientMusicApp.cpp (+20/−5) — 제목 정정 사유 원문(원인 실측이 "UI 반복자"보다 좁음 — UAF 규명) |
| T3 | `85bf59e1e29610f8ceb36f9a55cca881a188a6ba` | test(probes): music dirs 도구 세그먼트 — settings 쓰기+CP949 거부 실측 (T3) — wsl_music.sh probe 1파일 (429/−3) |
| T4 | `cf690c86f23811c21cd607fd79f9f13db259954b` | test(probes): music 폰 probe — dirs 도구 동형 세그먼트 (T4) — phone_music.sh probe 1파일 (+279/−31·코드 무변경) — **폰 캐논 624 흡수+DIRS-OK·MUS-PHONE-OK** |
| T5 | 본 문서를 포함하는 커밋 | docs(apps): music 폴더 관리 as-built 갱신 (T5) — SHA는 git log가 진실원 |

폐곡 계보: T1 리뷰 **APPROVE**(C1/I2/M3 — C1=WriteSettingsKv 재조립 소각
**실측 확정**·원자적 쓰기 2검 성립·posix 사유 실증) → T1 fix r1 재검
**APPROVE**(C0/I0/M2 — CP949 보존 봉합 WSL 실측·0x5C 규약 4곳 역원·T2 필수
인계 2건 C1+read leg) → T2 리뷰 **APPROVE**(C0/I1/M5/N2 — C1 경계 사례 원문
추적·도구 6종 병합 유일 수형·WSL 독립 624 실측·**I-1 보존의 구조적 부작용
부팅 무음 리셋 체인**) → T2 fix r1 재검 **APPROVE**(C0/I0/M1/N2 — 거부 경로
3조건 동형 대칭(:2907)·pid 접미·terminate 4검 완성) → T3 리뷰 **APPROVE**
(C0/I0/M3/N2 — I1-OK 8체인 1:1·P1 정직성 PASS·봉인 0) → T2 fix r2 재검
**APPROVE**(C0/I0/M1/N2 — 리뷰어 프루브 재실행 SIGSEGV 1/1 재현·수리
완벽성 PASS·토글 Refresh 복원 PASS·WSL 624 실측·**P1 전면 소각 확정**
·**M-1r 서사 교정** = 결정론 진원을 빈 리스트 null-data OOB로 좁힘 —
§7.3 #1·N-1r 라이브 E2E는 §5 ⑧·N-2r park — §4.1). → T5 본판(ddd0f45 —
docs 전용) → T4 리뷰 **APPROVE**(C0/I1/M2 — 618 부정 정산 1:1(2o-g 2건
실측·코드 좌표 main.cpp:5857 링크 근거)·세그먼트 등호·P3 정직성 픽셀
원문 승인 — I-1/M-1/M-2는 §4.1).

### §7.1 배선 원장 (원류 원문 1:1 수취 — 리포트/review 근거 · 코드 재실측 0)

- **MusicDirStore(순수 부품 — `engine/include/apps/MusicDirStore.h`)** —
  settings.json 읽기-수정-쓰기 **원자적**: 같은 폴더 tmp+`fs::rename` 1세대
  선시도 → UCRT 존재 대상 교체 실패 시만 `.bak` 대피 사다리(대피/재치환/복원
  실패 전부 err 표면화 — 조용한 눌먹음 없음; 유일 파손 창=3연속 실패 —
  §4.1 park). tmp는 `settings.json.<pid>.tmp`(T2 fix r1 — 서버↔클라 공용
  tmp 충돌 접점 소각 · 크래시 pid 잔산 무해 정직 부기). **CP949 바이트
  투명 스캐너 원장**: ReadDirs(ScanObjectFields+ScanStringArray — dirs 한정
  자기 스캐너)·NormalizeDirsPure(fs::path 폐기 쌍둥이)·Slashize(MusicModel.h
  본체 승격 — 0x5C 규약 4곳: ScanJsonString/DecodeJsonKey/EscapeJsonStr/
  Slashize)·LastPathSegment(terminate 4검). AgentJson(quickjs) 원독 재용
  기각 근거: read-only+상위 키 열거 부재+CP949 바이트 변질 — 원문 슬라이스
  재조립은 알 수 없는 상위 키와 비UTF-8 바이트를 바이트 그대로 보존.
- **C1 서버 보존 합성(T2)** — WriteSettingsKv(JKWindowServer.cpp:2890)가
  쓰기 직전 원문을 ComposeKeyed(ScanTopLevelFields)로 스캔: 미관리 상위 키
  (music.dirs·gallery·장래 키) 원문 슬라이스 재부착·관리 3키(audio/
  retention/text) 같은 자리 재기입·부재 키 말미 신설 + 파일 교체를
  **WriteSettingsAtomic 재용**(비원자 `fopen(wb)` 절단 창 소각) + 구
  .bak 선대피 dance 폐지(선대피 뒤 사다리 remove(bak)가 유일 잔존을 소각하는
  신규 창 — 사후 실측 근거) + .bak 1세대=직전 원문 사본 유지.
- **부적합 원문 정직 거부(T2 fix r1)** — 쓰기 직전 원문이 존재+비어있지
  않은데 quickjs 파싱 실패이면 재합성·쓰기 **하지 않는다**: settings_set
  5키 전원 답신 `write_failed`+detail "settings.json 파싱 실패 — 수기 치유
  필요"(JsonEsc)+부팅 stderr 경고 1행 — 부팅 리더 LoadSettingsKv(:2907)와
  **같은 파일·같은 리더·같은 판정** 대칭. 빈 파일은 관리 3키 신설 통과
  (씻어낼 데이터 없음). 거부=치유 안내 수형(패널 [제거]→문서 순수 회귀 —
  §7.2 I1-OK).
- **read leg 통일(T2 — T1 인계 M-2)** — ResolveDirs/RefreshUserDirs/
  music_dir_list → UserDirs 스캐너 통일(quickjs 원독 폐기): CP949 항목이
  **보존-가시**(표기는 바이트 그대로 — 폰트 깨짐은 표기-한계 원장 — §5 ⑨).
  terminate 3검: LastPathSegment 바이트 스캔(라벨)·RequestScan 진입 가드
  (Utf8ToUtf16 fail-closed — 비유효 UTF-8 루트는 "표기·관리만 가능" status
  1행)·ScanWorker throw 보호선(0건 표현 — 스캔 계약의 독립실패 형태).
- **UI+도구 6종 1콜 병합(T2)** — dir strip 옆 **[폴더 관리] 토글**→패널
  (UserDirs 행별 [제거]+경로 Edit+[추가]) + 도구 `music_dir_add{path}·
  music_dir_remove{path}·music_dir_list` — UI 버튼과 도구가 단일 경로
  (DirAddManaged/DirRemoveManaged)로 수렴. 등록은 spatial 3종과 같은
  SendAgentToolRegister **1콜 6종 병합** — 서버 등록이 연결 단위 upsert
  (`appToolManifests_[connId]=m`)라 제2호 호출이 매니페스트 전체를 치환해
  spatial 3종을 소각하기 때문(§7.3 #3 — 소각 사회 결함 봉쇄). Store
  성공=재스캔 3연(RefreshUserDirs+ResolveDirs+RequestScan)·실패=status
  1행 정직(무음 금지 — D4). 공문자·기본 폴더 가드는 저장소+UI 이중 방어.
  idle 계약(docs/88) 무변조 — frameDirty_ 신설 진원=입력 이벤트·도구
  수취·사유 표기 틱뿐(T2 리뷰 전수 확인).

### §7.2 캐논 계보 (2o 계열) + I1-OK 라이브 영수증

| 커밋 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#91 spatial 종착 = §2.1 T4 행) | 631 | 608 | 296 | 608 | — |
| T1 `77cabf7` | **641** | **618** | 296 | (608) | 2o 10 |
| T1 fix r1 `f4bfcf3` | **645** | **622** | 296 | (608) | 2o-f 4 |
| T2 `6de919b` | **647** | **624** | 296 | (608) | 2o-g 2 |
| T2 fix r1 `f1d9c49` | 647 | 624 | 296 | (608) | 0 (3축 등호) |
| T3 `85bf59e` | (probe 전용 커밋) | 624 (런 등호) | 296 | (608) | 0 |
| T2 fix r2 `52f672c` | 647 | 624 | 296 | (608) | 0 (3축 등호) |
| T4 `cf690c8` | — | — | — | **624** | 폰 2o 16건 흡수 (608+16 — **2o-f 4+2o-g 2 동봉 — WSL 624 완전 등호**·`CANON-INCLUSION=MUSIC-FULL-2O-16`) |
| T5 본 커밋(T5 fix r1 갱신) | 647 | 624 | 296 | 624 | — (docs 전용 — 재실행 불요) |

- posix 296 등호 = **하네스 TU 미링크 구조 등호**(§2.1 원리 승계) —
  2o-a..g 전원이 engine/src/main.cpp 소속이고 tools/posix_selftest 링크 TU는
  미포함(T1 리뷰·T1 fix r1 재검 실증). 다만 직접 실측 부재는 T2 리뷰 M-5로
  원장(§4.1) — 계보 정합(2o 추가 때도 296 유지)으로 수용.
- 폰 — **T4 실측 종착(cf690c8): 624** — 2o 16건(2o-f 4+2o-g 2 동봉) 흡수
  PASS=624 FAIL=0 (2m=26·2i=19·2g=40·2n=17·2o=16) — **WSL 624 완전 등호** ·
  `CANON-INCLUSION=MUSIC-FULL-2O-16` (§2.1 관행의 CANON-INCLUSION 재판정
  동형). **부기(브리프 기대 618 부정 — T4 정산)**: 폰 캐논 표기의 원
  기대(608→618 = 2o 10)는 2o-g 서버 측 2건이 폰 구조에 미반영이라는 가설
  이었다 — **실측은 폰 selftest main에 서버 보존 합성 2건도 동봉되어 전
  16건 실측(624 = WSL 등호)이 확정 — 가설 부정·상수는 실측 후 갱신 계약(
  T3 리뷰 M-1 승계)대로 624 갱신**(§4.1 소각 표기).
- **T4 dirs 도구 원문(폰 축)** — DIRS-OK: add=ok·list 등록/제거 원문·
  SET-OK(python3 독립 파서 — music.dirs 실기록+text/font_path 보존)·
  **원복 바이트 등호**·SWEEP-POST-DIFF=0·SETTINGS-RESTORED 121/
  PERM-RESTORED 33 바이트 등호·REMNANT gone·honest-fail 0건. **CP949
  거부/치유 세그먼트는 폰 스킵 원장행 1행**(브리프 전제 — WSL T3 16c가
  소유·전부 실측됨 — probe에 원장행 명시). **N-1 승계**: DIRS 판정 행
  p1=crash-fixed 표기(T2 fix r2 수리 후 도구 경로 재실측)·dirs 도구
  원문을 MUS-PHONE-OK 게이트에 편입. **N-2 승계**: 캡처 9종 md5 대차 OK
  (dirs `56f49927`·dirs2 `638204e8` 등).
- **I1-OK 8체인 원문 표 (T3 런 로그 — engine/tmp/wsl_music_run.log 10:05
  런·T3 리뷰 1:1 대조 확증)** — CP949 시딩('\xB0\xA1\xBF\xE4' = '가요' —
  2o-f corpus 동형·log:140 `MUSIC-I1-SEEDED`)을 전제로:

| # | 체인 | 런 로그 원문 (좌표) | 내용 |
| --- | --- | --- | --- |
| 1 | boot=1 | log:143 | 부팅 stderr 경고 1행 — "파일 무접촉·기본값으로 기동 — 수기 치유 필요"(코드 :2870-72 정확 일치) |
| 2 | ss=1 | log:152 | settings_set 거부 detail 원문 `{"ok":false,"error":"write_failed","detail":"settings.json 파싱 실패 — 수기 치유 필요"}`(코드 :2948 그대로) + stderr 거부 1행(log:154) |
| 3 | keep=1 | log:156-159 | remove 도구의 스캐너 수용 ok:true+`MUSIC-I1-LIST-2` CP949 항목 **보존-가시** `["\uFFFD\uFFFD\uFFFD\uFFFD"]` |
| 4 | rsc=1 | log:160 | 도구 인자 CP949 = bad_request(UTF-8 검사 원문 근거 — 패널 원촉 유일 논증) |
| 5 | cure=1 | log:196-197 | 패널 [폴더 관리] 토글(차분 게이트)+[제거] 클릭 → **PURE-OK** audio=True retention=True dirs=[](독립 파서) — 문서 순수 회귀 |
| 6 | retry=1 | log:202 | add 재시도 성공 — 단 **스폰 새 클라(windowId 33) 경유**(P1 크래시+재스폰 분기 — §7.3 #1 · 판정 행 p1 부기 권고 §4.1) |
| 7 | ss2=1 | log:205 | settings_set 재시도 성공+**C1-FILE-OK**(music 키 생존 — 파일 판정·app 사망 시 tool_gone 오독 함정 회피 설계) |
| 8 | restore=1 | log:208 | 백업 원복 바이트 등호(`MUSIC-I1-RESTORE-BYTE` — probe `cmp -s`) |

- **P1 계보(이 라인의 결함 종착 — 착지 후 소각)** — T3가 신규 결함을 정직 보고([제거]
  쓰기 성립 직후 music 클라 세그폴트·2전 2 재현 — 증거
  `/tmp/mus/mus_srv_i1.log:49` `Segmentation fault (core dumped)`·tool_gone
  답신·**서버 생존**) → T2 fix r2(52f672c)가 인덱스 무효화 UAF/OOB 부류로
  원인 규명+수리(§7.3 #1 — 재검에서 진원 서사 교정) — 수리 후 2런 사망 0
  ·재검 **전면 소각 확정**. 봉인 0·전달 계약 성립(T3 리뷰 정직성 PASS).
- DIRS-OK 세그런(16b) 병기 — music_dir_add(/tmp 합성 폴더)→list 원문
  `{"ok":true,"dirs":["/tmp/mus_seed","/tmp/mus_dirs_x"]}`→settings.json
  실기록(audio/retention 보존 SET-OK)→remove→list 제거 원문→왕복 후 seed
  원문 바이트 등호 cmp OK(④ T5 수형 — ComposeKeyed 출하형 승계). 캐논
  등호 라인: `MUSIC-OK … PASS=624(캐논 624) 2m=26` — 수치 영수증 라인이지
  육안 선언 아님(§5).

### §7.3 함정 원장 (#92 라인 신규 — 봉쇄 레슨 축)

1. **★move-대입 교체 뒤 인덱스 무효화 UAF/OOB 부류([제거] 클릭 경로
   한정 — SIGSEGV)** — DirRemoveManaged의 성공 경로가 RefreshUserDirs()로
   `userDirs_`를 **move-대입 교체**(libstdc++ `operator=(vector&&)` = RHS
   버퍼 수취+구버퍼 해제)하는데, 같은 프레임의 **사후 문**(클릭 행 k의
   `SetTooltip(userDirs_[k].c_str())` — 구현자 리포트 원문 좌표 :1052 부근)
   이 `userDirs_[k]`를 다시 색인한다 — k는 move-대입 뒤 **범위 내면
   신버퍼, 축소로 범위 밖이면 OOB**. **진원 부류 정밀 교정(T2 fix r2 재검
   M-1r — 원장 갱신 정직 반영)**: 구현자 서사("소멸한 std::string의
   _M_p가 tcache next-ptr로 독살 → strlen SIGSEGV")는 OOB-침범 케이스
   (행 복수 잔존+말단 제거)에는 방향이 맞으나, **프루브가 실측한 결정론
   진원은 빈 벡터(0행)의 operator[] = nullptr c_str()(null-data OOB)**
   (리뷰어가 프루브 재실행 — SIGSEGV core dumped 1/1 재현 확증) — 서사
   교정이 수리의 정확성·완전성에는 무영향(사본+break가 부류 전체를
   소각). **범위 실측 3점 교집합(원인 확정 수형)**: ①오리지널 probe 로그
   2/2 사망(`mus_srv_i1.log:49`) ②도구 경로 차별 — `app_tool
   music_dir_remove`(동일 저장소 체인·행 루프 없음) 3/3 생존 → 크래시는
   행 루프의 **클릭 경로 한정** 확정(도구 경로는 안전 — DirAddManaged는
   멤버 char 버퍼 소비) ③동일 수형 최소 컴파일 프루브 결정론 1/1
   SIGSEGV(backtrace `libstdc++ c_str/strlen`). gdb/core 캡처 불가(WSL
   gdb 미설치·`core_pattern = |/wsl-capture-cRASH`)는 정직 부기 — 대용
   실측 3점으로 확정. **수리(최소 변형 3점)**: 행 사본 선취(for 머리
   `const std::string row = userDirs_[k]` — BulletText·인자·SetTooltip
   전부 사본 소비)+제거 후 `break`(제거 1회/프레임 — 시프트 성분 재독
   소각·N-2r 신계약 원장 §4.1 — UX 원존과 동형)+토글 열림 프레임
   RefreshUserDirs() 복원(T2 원문 주석의 구현 누락 동봉 수리 — 열림 직후
   빈 목록으로 [제거] 불가의 사실상 결함). 수리 후 2런 사망 0·동형 전수
   감사(dirs_ strip·tracks_) 잔존 0건(리뷰어 독립 재확인).
2. **★quickjs CP949 문서 전체 거부 vs 스캐너 수용(=보존-불-가시) → read
   leg 통일** — AgentJson(quickjs)은 CP949 바이트 1건으로 **문서 전체
   거부**(WSL 프루브 실측 `AgentJson(text).ok()==0`·`UserDirs()` n=0) →
   T1 시점엔 "CP949 dirs 기존 항목이 있는 문서에서 AddDir 제2호"가 기존
   항목을 **무음 소각**하고 ok=true(부품 자기 열의 소각 — fix r1 ReadDirs
   스캐너로 봉합). 그런데 C1 보존(T2)이 그 CP949 혼입 문서를 **영구 잔존**
   문서로 바꾸는 순간, 거부가 **부팅 무음 리셋 체인**으로 재발했다(T2 리뷰
   I-1): 부팅 리더 3곳(LoadSettingsKv :2907·GalleryDirList :140·
   LoadDesktopSettingsJson :39)이 전부 quickjs 원독 — CP949 혼입 문서에서
   audio/retention/text·갤러리 dirs·폰트 설정이 기본값으로 떨어지고,
   구-T2 치유 경로였던 "다음 settings_set이 dirs를 소각하는 ASCII 회귀"를
   C1 보존이 소각했다 — **보존하는 순간 리더의 부정합이 영구화**. T2 fix
   r1의 "보이게 하기"(부팅 stderr 1행+settings_set 거부 detail)와
   체인 역산(패널 [제거]→문서 순수 회귀)으로 봉합 — 단 부팅 리더 자체는
   여전 quickjs(기본값 폴백)로 **표기-한계 원장**·전각 치유(CP949→UTF-8
   변환)는 별도 라인(§4.1). 레슨: **보존하면 리더 전원·쓰기 전원과 같은
   눈이 되어야 한다 — 같은 파일은 같은 눈으로**(부팅 리더·settings_set
   쓰기·저장소 스캐너 3 leg 대칭 — fix r1이 성립시킨 계약). 부기(지시 vs
   코드 소유자 교정): 거부 detail "수기 치유 필요"의 소유자는
   **settings_set 서버 정문**이고 music_dir_add는 자기 스캐너로 CP949
   문서를 수용·보존한다 — T3가 양 경로 전부 실측으로 소각(원장 부기).
3. **도구 등록 upsert = 제2호 등록이 매니페스트 전체 치환(소각 사회
   결함)** — 서버 원문 `appToolManifests_[client.Id()] = m;`(연결 단위
   **전체 치환 upsert**) — 제2호 SendAgentToolRegister 호출이 기존 등록을
   통째로 바꿔 **spatial 3종을 소각**한다. 6종 1콜 병합(T2)은 필수가
   아니라 **유일한 올바른 수형** — vplayer 등록 수형과 동일형.
4. **★UTF-8 strict 툴체인(narrow fs::path) — CP949 throw 원장** — 이
   MinGW 툴체인의 fs::path narrow 계층은 **UTF-8 strict 기수** 실측: CP949
   바이트는 ctor에서 `filesystem_error: Illegal byte sequence`(T1 fix r1
   2o-f 최초 수행 terminate 실측)·UTF-8 바이트는 한글 폴더 실제 생성·
   열거·generic_string 왕복 성공(전 축 native형=UTF-8 — utf8↔native 변환
   불요). 수형: fs::path 폐기 쌍둥이(NormalizeDirsPure·LastPathSegment)+
   진입 가드+try/catch의 **terminate 4검** — 저장·관리·표기는 CP949에서
   가능, Win 축 **스캔 leg만 표기-한계로 진입 가드 거부**(기능 복원
   ListAudioFiles wide-path 확장은 별도 라인 — 2m 계보 파탈 주의).
5. **0x5C 후행 바이트(뷁류) 규약 — 4검 도입 원장** — CP949 확장 영역
   (선단 0x81-A0) 한글의 **후행 0x5C** 파일명 문자를 이스케이프/스페이퍼로
   접으면 재쓰기에서 바이트 변질(데이터 손실) — "고바이트 뒤 0x5C=리터럴"
   규약을 4곳(ScanJsonString·DecodeJsonKey·EscapeJsonStr·Slashize)에 동형
   주입(인코드/디코드 역원·왕복 안정·UTF-8 투명성 성립). 한계 2원
   (정직): ①Slashize는 UTF-8 멀티바이트 바로 뒤의 **진짜** '\'를 접히지
   않는다(데이터 무손상·정규화 철자만 혼합 — T2가 TrimTrailSeparators로
   상쇄) ②서버 JsonEsc(전역 백슬래시 이중화)로 쓴 상위키(text.font_path류)
   원문은 스캐너가 고바이트 뒤 `\`를 리터럴로 오인 → 문서 전체 부적합
   판정 → **정직 거부**(원본 보존·기능 마비 — T1 재검 원장 · 발화 창
   원장 부기).
6. **settings_set 거부(부적합 원문 정직 거부)와 치유 경로** — §7.1 3번째
   bullet의 대칭 계약 본문(부팅 리더와 같은 판정) — **거부는 치유 안내
   수형**(원본 보존·부패 아님) — 빈 파일 신설 통과, 세부는 §7.3 #2와
   묶음(중복 기피 — 여기는 원장 인덱스행).
7. **첫 합성 클릭 무음 소멸+합성 캡처 좌표계 트랩(합성 입력 런타임
   원장)** — ①레지스터 재현(5세션): 앱 **첫** 합성 클릭이 0-diff로 소멸 —
   웜업 클릭+차분 게이트(≤3 재시도)로 probe 소거 — 합성 클릭 설계 공용
   렛슨 후보. ②좌표계 트랩: 데스크톱 합성 캡처는 클라 좌표계와 (417,173)
   오프셋(XWayland 창 단위 XGetImage=BadMatch hard-fail 원장) — 절대
   좌표 직용 불가. ③fix r2의 **합성 입력 간헐 수신**(send_input ok 응답과
   무관하게 클라 MousePos 무갱신 세션 — 실기기 사람 입력과 결정 차이)은
   하니스 결함 후보 별도 라인(§4.1) — [제거] 최종 E2E는 §5 ⑧(사람 클릭).

### §7.4 Deferred/park 승계 (#92 라인 — 소등 후보)

- **소각 원장(라인 안에서 끝난 것들)**: T1 진행 중 소각(ScanJsonValue
  in/out 위치 — 휴면 0건이 실측 캐논이 잡은 종류·77cabf7에 교정 상태)·T1
  리뷰 I-1(CP949 무음
  소각)/I-2(RemoveDir 성공 경로 미실측)·M-1/M-2/M-3 → **fix r1(f4bfcf3)
  전부 소각**·T2 리뷰 I-1(부팅 무음 리셋 체인 — 보이게 하기)·M-1(pid 접미)
  ·M-3/M-4(주석·오자)/N-1(LastPathSegment) → **fix r1(f1d9c49) 전부
  소각**·T2 fix r1 재검 M-1r(거부 경로 라이브 실측) → **T3 probe I1-OK로
  소각**·T3 P1(세그폴트) → **fix r2(52f672c) 수리+재검 APPROVE 전면
  소각 확정**.
- **park 신규행**(§4.1에 추가만 — T5 본판 12행+T5 fix r1 7행 = 19행):
  .bak 사다리 3연속 창(수리 금지 판정)·ComposeKeyed last-wins·posix 간접
  근거·**probe CANON 624 승격(T4 cf690c8에서 실측 소각)**·T3 리포트 수치
  오기·브리프-구현 2원화 렛슨·I1 판정행 p1 부기·캡처 md5 부기·합성 입력
  간헐 수신(별도 라인)·AddDir invalid 문서 영속화(전각 치유 별도 라인)·
  "C:" 표기 엣지·break 1회/프레임 신계약(N-2r — 무영향 원장)+T4에서
  신설 **7행**(dirs 등호 앵커 원장·**P3 폰 [폴더 관리] 토글 무응답**·토글
  좌표 편차+vplayer 치우기 원장·폰 라이브 실험 계약 — `approval_timeout`
  원문·[m]usic 브래킷 pkill 계약+T4 리뷰 3행 — I-1 라이브 원문 미보존
  면책·M-1 probe 헤더 교정 대기·M-2 치우기/1차 런 원문 부생존).
- **라인 잔여(별도 라인 후보 원장)**: 교차 작성자 레이스 잔여(프로세스 간
  파일 락 시설 부재 — 쓰기 직전 재독으로 ms 단위 축소·근본 봉쇄는 파일 락
  신설)·CP949 Win 스캔 leg(ListAudioFiles wide-path 확장)·전각 치유
  (CP949→UTF-8)·부팅 리더 스캐너 폴백(표기-한계 원장의 구조 수형 후보).
- docs/90 §4.1 기존 불변(SP-6/SP-11 등 — **변형 금지·추가만 원칙 이행**).

### §7.5 결정 원장 (재량 — 기각 시 즉시 수리)

1. **원자적 쓰기 수형 = 1세대 rename 선시도+.bak 대피 사다리(교체 시도
   우선)** — posix 원자적 교체를 사다리 비가동 유지로 살리고 Windows UCRT
   의미론 불정합은 실패 시 사다리로 흡수 — 플랫폼 의미론 분기를 실측 없이
   봉합하는 수형(T1 재량·리뷰 성립).
2. **AgentJson 기각 → 원문 슬라이스 자기 스캐너(재조립)** — CP949 바이트
   투명이 라인 전체의 전제(§7.3 #2·#4) — 이 결정이 지우자 read leg·서버
   쓰기·거부 경로 전부 같은 스캐너 원문으로 수렴.
3. **ComposeKeyed 서버 보존 합성(별도 라이브러리 중복 없이 MusicDirStore의
   스캐너를 공용 include로 승격)** — T1 리뷰 수형 후보 ① 채택(수술 반경
   최소) — ②KV 쓰기 저장소 리드모디파이 통일은 미채택.
4. **거부=치유 안내(수기 치유 필요) + 부팅 경고(전각 치유 미실장의 정직
   표기)** — 조용한 치유(빈 문서 재해석)는 데이터 소각이라 금지.
5. **도구 6종 1콜 병합 등록** — upsert 전체 치환 원문에 대한 유일 수형(§7.3
   #3).

### §7.6 커밋 원장 + IP grep 게이트 (#92 라인)

- 라인 체인 = **7f66b81(플랜)..cf690c8**+본 T5 커밋(§7.0 표 = rev-parse
  전체 SHA) — push 전부 origin main 직행·amend 없음. **스냅샷 갱신(통합
  리뷰 M-1 소각 — 본 문서 최신 T5)**: 구 표기 `..52f672c`는 후속 T3
  (85bf59e)·T4(**cf690c8**)를 미수록 — 체인 말미를 cf690c8로 정산
  (§7.0 표와의 합일).
- **IP·내부경로 리터럴 게이트(T5 실측)** — 본 커밋 원문에서
  `\b([0-9]{1,3}\.){3}[0-9]{1,3}\b` grep = **0건**·호스트 경로 표기
  (사용자 루트 폴더명·드라이브 글자·/mnt 계열 표기) grep = 적중 0건
  (커밋 전 staged/`git show` 게이트 실행 — §6 동형 문구). 라인 이전
  커밋도 리포트/review 전원 IP 0건 실측(T1·T1 fix r1·T2·T2 fix r1·T2
  fix r2·T3 원문 — §7.0 계보).
- **본 T5 커밋은 docs 1파일(`docs/90_music_spatial_leg_asbuilt.md` — §7
  신설+§4.1/§5 추가만+§0 부기)로 한정** — git add 명시 경로만(#83 사건
  레슨)·docs/89 무접촉. probe 런 로그·캡처 9종(T4 cf690c8 회수)은
  engine/tmp untracked — 커밋 금지 충족. **스냅샷 갱신(통합 리뷰 M-1
  소각)**: 구 표기 "캡처 7종"은 T4(cf690c8)의 9종 회수 이전 산출 — 현행
  9종으로 정산(§5 ⑥ 원문과의 합일).

## §7.7 스캔 안전화+경로 인코딩 원장 (#93+#94 라인 — T1-T4, 2026-10-10)

뮤직 스캔 안전화 라인(#93 — 사용자 요청 "스캔중 폴더 제거나 앱 종료시에
문제 없게 해주세요" — #92 T2 park C1(파괴 join 대기)의 정식 소각 라인)과
병렬 경로 인코딩 수리(#94 — 사용자 라이브 보고 "재생하려니 파일을 찾을 수
없습니다" — 한글 트랙)를 한 원장에 봉한다 — 같은 창에 병렬 진행(같은
main.cpp를 두 커밋이 나눠 쓴 인덱스 수술 원장 — §7.7.3 #7)이라 분리 표기가
역사를 왜곡하기 때문. 플랜=`docs/superpowers/plans/2026-10-10-music-scan-cancel.md`
(4473e8b — 결정 원장 5건=재량(§7.7.4)·실기기 체감=EYES), SDD 원장=
`.superpowers/sdd/2026-10-10-music-scan-cancel/`(progress·task-1..4
리포트 4종+review 4종+path-encoding brief/report/review·review diff 4종·
런 로그 wsl_music_run16d_{1..5}.log·phone_music{,_run}.log — engine/tmp
untracked **인용만이 진실원**). 이 라인의 원장 원리 이중: **와이어/저장/
표기=UTF-8·파일 터치=네이티브(#94 규약)**·**경계 콜백 취소(#93 — 디렉터
경계 1회+256 보조)**·소멸자 cancel∥quit OR·도착 멤버십 필터 세대 앞단
·SettingsText fs화. T5는 docs 전용(engine/probe 무접촉) — selftest 재실행
불요(§0 부기 원칙 — 건네받은 수치 Win 661/WSL 638/posix 296/폰 638 그대로
봉합·`-F` 계수 룰링 승계). 서브에이전트 스폰 0건(T1-T4 각 리포트 선언
·T5 실측)·worktree 미사용·main 직행.

### §7.7.0 체인 (rev-parse 전체 SHA 실측 — T5 측정 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 |
| --- | --- | --- |
| 플랜(라인 BASE) | `4473e8bbe084759f5a59a1a93bb7b1dec31a0ef3` | docs(superpowers): music 스캔 안전화 플랜 T1-T5 (#93) |
| T1 | `1dfb8cb36cdc7f97b2e70a0d8d45edde8365302c` | feat(apps): music 스캔 취소 기구 — 디렉터 경계 콜백 (T1) — MusicModel.h `ScanCancelFn`+경계 콜백+2p 7건 (2파일 +148/−4) |
| #94(병렬 수리) | `adbd0f1edacbe64236ee8cbe1b398d6e08297a90` | fix(apps): 경로 인코딩 사슬 — 파일 터치 네이티브 변환+존재 검사 fs화 (#94) — 6파일 +157/−5 — jk::text Utf8ToAnsi/AnsiToUtf8 3축 동봉+vplayer fs 게이트+SpatialStart 네이티브 수취 (2q 7건) |
| T2 | `f4098565259ce0773e110077b882ac8a18b83fac` | feat(apps): music 스캔 취소 배선 — 종료·제거 경계 안전화 (T2) — 4파일 +163/−90 — 소멸자 취소 join+[제거] 취소 1발+도착 멤버십 필터+SettingsText fs화(#94 동승) |
| T3 | `bf38ea99c08a4f1d4008bb427fc4850b9e0e6a40` | test(probes): music WSL probe — 스캔 취소 경계 실측 (T3) — wsl_music.sh +436/−5 — **CANCEL-OK ×2**(0.60s 소멸·0.50s 폐기)+landtap 착탄 판정기 봉합 |
| T4 | `97b2d8e3ee6d243501597c8bf120bf0e03ef18e7` | test(probes): music 폰 probe — 스캔 취소 동형 세그먼트 (T4) — phone_music.sh +499/−63 — **폰 캐논 624→638 흡수+MUS-PHONE-OK ×2**(1.25s 소멸·0.62/0.66s 폐기) |
| T5 | 본 문서를 포함하는 커밋 | docs(apps): music 스캔 안전화+경로 인코딩 as-built 갱신 (T5) — SHA는 git log가 진실원 |

폐곡 계보: T1 리뷰 **APPROVE**(C0/M0/M2 — WSL 독립 재실측 631 등호 확증·
경계 1회+256 보조·원존 시맨틱 무변조 — M-1 주석 라벨/M-2 인용 수치는 T2
병행 소각) → #94 리뷰 **APPROVE**(C2/I3/M0 — Win 661/WSL 638 독립 재실측
(수정 TU 강재 재컴파일)·**2q-a가 Win에서 진짜 ACP 검증**(변환 바이트로 실제
std::fopen 개방 단정 — WSL 항등과 분리된 진원 검증)·C-1 로캘 전제 원장행
(§7.7.3 #3)) → T2 리뷰 **APPROVE**(C0/I2/M1 — WSL 전체 재빌드 638 PASS/0
FAIL·2p↔2q 블록 이동 multiset 72/72 상쇄(신설 어설션 0건)·수명 3역산(취소
∥quit OR·리셋 경합·래치 오표기) 안전 판정·**멤버십 필터 세대 앞단 배치
PASS**) → T3 리뷰 **APPROVE**(C0/M3/I3 — CANCEL-OK ×2 런 로그 1:1·settled
캡처 직접 열람 — M-1 스테일 캡처/M-2 REMNANT rc 비대칭/M-3 주석 "6곡"은
§4.1-2 park) → T4 단독 리뷰는 **폐곡 통합 리뷰에 흡수**(ruling — 독립 리뷰
1회 절감+T5 병렬 dispatch — T4 리포트 원장) → **통합 리뷰 M-1(§7.6 2행 —
cf690c8 미수록·캡처 7종 현행 9종) = 본 T5에서 소각 완료**(§7.6 갱신).

### §7.7.1 배선 원리 (원류 원문 1:1 수취 — 리포트/review 근거 · 코드 재실측 0)

- **취소 기구(T1 — 순수 부품)** — `using ScanCancelFn = std::function<bool()>;`
  (참=취소)+ScanAudioTree 폭탄 전달: 재귀 진입마다 `if (cancel()) return;`
  (진입 체크가 "자식 재귀 직전" 경계를 겸는다 — **경계당 콜백 정확 1회**)
  +**엔트리 256개마다 보조 체크**(단일 디렉터 폭주 leg의 취소 응답 상한 256
  엔트리). `ListAudioFiles(root, cancel)`: 래치(`fired` — 콜백 1회 참이면
  재청구 0)로 절단·취소 시 **부분 수집 폐기 후 빈 목록 반환**(정렬 생략 —
  중간 결과 유출 0). 무인자 오버로드는 항상-false 콜백 위임 — 원존 시맨틱
  0 변조(워커 호출부 무접촉). ec 중립형·순환 가드·독립 실패·mtime desc+
  rel asc 정렬 원존 그대로.
- **소멸자 취소 join(T2 결정 ②)** — join 전 `scanCancel_.store(true)` 1발 —
  진행 중 스캔이 경계서 절단되어 join이 ms급으로 돌아온다(#92 T2 park C1
  무한 대기 소각). **콜백 = `scanCancel_ ∥ scanQuit_`의 OR** — 워커의 취소
  리셋(요청 수취 시점)이 소멸자의 cancel을 지우는 역산 경기에서도 quit는
  누가 리셋하지 않아(일단적=true 상주) 다음 경계 1회 내 절단. T1 취소판
  래치 폐기 계약(2p-①/②)이 원문이라 "완주 대기 원칙 완화"는 시맨틱 신규
  아님(T2 리뷰 확인).
- **[제거] 취소 1발(결정 ③)** — DirRemoveManaged 성공 경로에 취소 1발+
  기존 재스캔 3연(RefreshUserDirs+ResolveDirs+RequestScan) 원문 그대로.
  워커는 요청 수취 때 `scanCancel_=false` 리셋 — **재청구가 취소를 먹지
  않는다**.
- **도착 멤버십 필터(결정 ④)** — 도착 스냅샷 루트(신설 `scanOutRoot_` —
  결과 박스 발행 때 동봉·스캔 시작 시점 dirs_ 성분과 바이트 동일 — 동일
  lock)가 현재 dirs_에 없으면 폐기+stderr 진단 1행(`[music] 도착 폐기 —
  제거된 폴더의 스캔 결과(root=…)`·fflush 포함). **세대 게이트 앞단 배치**
  — [제거]의 재청구가 세대를 이미 소진한 뒤 도착하는 지연 결과는 세대
  불일치로 무음 지나가는 길이 있어, 정직 진단은 루트 멤버십이 세대에 앞서
  소유한다(세대 뒤 배치였으면 stderr 진단이 사망 코드 — T3 probe 관측 경로
  그 자체. T2 리뷰: 앞 단 배치가 정직 계약의 배치 조건 — PASS). 정상 운행
  도착(root ∈ dirs_ 항상)엔 영향 0.
- **SettingsText fs화(#94 동승)** — `.string()`(UTF-8 바이트) narrow fopen
  (ACP 재해석 — 한글 exe 설치 디렉터 settings 판독 실패 잠복 결함, #94
  리뷰 전수 grep `:365` 확증) → `fs::path`를 `std::ifstream`에 직접(#94
  규약 "파일 터치점 = 네이티브"의 소비 — 기존 스캔 leg와 같은 narrow→
  fs::path 변환 수형). 부재/개방 실패 = 빈 텍스트 fail-safe 원존·`read/
  gcount` 청크 루프 = 원 fread 반복 계약 동형.
- **#94 수형 원문** — vplayer OpenStage: narrow fopen → `fs::status`+
  `is_regular_file` ec-중립형(부재·디렉터·비정규 entry 공통 실패의 원
  fopen 관측 보존 — 종착 문구 "파일을 찾을 수 없습니다" 무변조)·avformat
  채널 무접촉(UTF-8 원래 기대). `jk::text::Utf8ToAnsi`(win32
  MbToWide(CP_UTF8, MB_ERR_INVALID_CHARS)→WideToMb(CP_ACP, 0) — 부적합
  UTF-8 = 빈 문자열 fail-closed·'?' 치환 관측은 Utf8ToCp949 원문 계약;
  posix 항등 — Linux 경로 인코딩은 파일시스템 소유)+`AnsiToUtf8` 역변환
  3축 동봉. SpatialStart 경계: `Decoder::open`에 `Utf8ToAnsi(t.full)`
  수취(third_party dr_wav/dr_mp3/dr_flac/stb_vorbis 내부 narrow fopen과
  정합 — spatial-player 무변경 계약)·표기/전이(`legState_.path = t.full`)
  는 UTF-8 원문 유지·빈 변환 fail-closed는 `err="경로 인코딩 변환 실패"`
  정직 종착.

### §7.7.2 캐논 계보 (2p/2q 계열) + 경계 영수증

| 커밋 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#92 종착 = §7.2 T4/T5 행) | 647 | 624 | 296 | 624 | — |
| T1 `1dfb8cb` | **654** | **631** | 296 | (624) | 2p 7 — 하네스 TU 미링크 구조 등호(2p 전원 engine/src/main.cpp 소속 — T1 리뷰 build.sh TU 목록 실측) |
| #94 `adbd0f1` | **661** | **638** | 296 | (624) | 2q 7 |
| T2 `f409856` | 661 | 638 | 296 | (624) | 0 (3축 등호 — 신설 어설션 0 계약·posix는 정적 판독 재실측) |
| T3 `bf38ea9` | (승계 등호) | 638 (런 등호 2런) | 296 | (624) | 0 (probe 전용 — `MUS-CANCEL-CANON-EQ: OK`) |
| T4 `97b2d8e` | (승계 등호) | (승계 등호) | 296 | **638** | 폰 2p2q 14건 흡수 (624+14 — **WSL 등호**·`CANON-INCLUSION=MUSIC-FULL-2P2Q-14` — 2런 등호) |
| T5 본 커밋 | 661 | 638 | 296 | 638 | — (docs 전용 — 재실행 불요) |

- **2p 7의 분해** = 2p-a 트리 구성(setup 판정선 — 빈 목록 계약의 무공적
  흡수 방지)+①사전 취소(즉시 빈 반환+콜백 1회 — 트리 열거 미접촉)+②경계
  3회째 취소(302 엔트리 트리 — 열거 순서 무관 결정론)+②호출 상한(경계 3곳
  이내 — 무경계 폭주 0)+③루트 선삭제(ec 중립·crash 0)+④무인자 계보 불변
  (2m-c 원문 수치 재용)+⑤REMNANT 0.
- **2q 7의 분해** = 2q-a(한글 더미 구성·fs status 수형 통과·Utf8ToAnsi/
  AnsiToUtf8 왕복 보존·변환 바이트 narrow fopen 실개방)+2q-b(한글 폴더
  루트 ListAudioFiles 수취+Track.full/rel UTF-8 유효 단정 — posix는 no-op
  변환이라 왕복 단정으로 통과).
- **posix 296 등호 = 하네스 TU 미링크 구조 등호**(§2.1 원리·§7.2 원리
  승계 — T2 리뷰 정적 판독: tools/posix_selftest/main.cpp에 ClientMusicApp
  ·music::·2p/2q 0건 grep 실측·build.sh RC=0+0 failure).

**경계 영수증 표 (WSL run16d_4/5 — 2런 동치 · 폰 — 2런):**

| 경계 | WSLg — 460,800/트리 (40×11,520·2트리) | 폰 — 200,000/트리 (80×2500·2트리) |
| --- | --- | --- |
| ② 스캔 중 music_dir_remove | `[music] 도착 폐기 — 제거된 폴더의 스캔 결과(root=/tmp/mus_big_a)` stderr 1행 — 제거→도착 **0.50s**(취소 없으면 460k 완주 후 ~5s+) — 클라 생존·제거/원복 원문 | 동형 원문(root=…/mus_big_a) — 제거→도착 **0.62/0.66s**(절단이 완주 잔량 앞섬 — `MUS-CANCEL-EFFECTIVE: OK`) — 클라 생존 |
| ① 스캔 중 창 close | close→소멸 **0.60s**·crash 마커 0·재스폰+music_dir_list 응답 정상 — 임계 3.5s(`MUS-CLOSE-JOIN-LEDGER`: 소멸 시차 ≤3.5s면 취소 join ms급 성립) | close→소멸 **1.26/1.25s**·crash 0·재스폰 도구 응답 OK — 임계 5.0s(WSL 수형의 폰 정산 — 1s 폴링+ctl 왕복) |
| ③ 재스폰 정착 | 재스폰 클라 스캔→채택 전이 — landtap 차분 30행·탭 강조+표 5행+"5곡" 카운터 캡처 | tab2 재스캔 정착 캡처(landtap 폰 미이식 — 탭 병합 구성 — §7.7.3 #5) |

- 판정 라인: **`MUSIC-CANCEL-VERDICT: CANCEL-OK` ×2(WSL)**·**`MUS-PHONE-OK`
  ×2(폰)** — 수치 영수증 라인이지 육안/체감 선언 아님(§5 — 결제 대기).
- 부기(폰 제1런 수치 원문 미보존 — T3 리뷰 I-3 동형 원장행): 보존 런 로그
  2종(phone_music{,_run}.log)은 0.66/1.25 런이고 0.62/1.26은 제1런(r1 —
  마커 hard FAIL 런)의 리포트 원장 — 후속 동형 라인은 원문 보존부터.
- 스칼라 규모 원장: WSL 시더 캘리브레이션(브리프 문언 "하위 40개×파일
  30개"→실측 40×11,520 — 링크 실측 ~10µs/트랙 외삽 — 커밋 주석 원장화+
  `MUSIC-SIZE-EVIDENCE` 정직 부기 — T3 리뷰 I-1 수용)·폰 시더 python3
  16.6/18.1s(2×200k)+셸 find 200,000=0.57-0.58s(열거 고정비 원문 — 클라
  스캔이 셸보다 수 배 느린 stat+Track 비용 = 폰 제거 창 성립의 근거)·
  **VHD writeback sync 선행 배수**(§7.7.3 #2)·폰 1차 사이클 직행 성립
  (2차 사이클 발동 0 — §4.1-2).
- 병행 배치 병기(#94 — 스캔 leg 자체는 원래 UTF-8 일관): 한글 트랙 재생의
  병목은 vplayer 존재 게이트(CP_ACP 재해석 fopen)와 spatial 디코더 경계뿐
  — 실기기 결제 = §5 ⑫.
- 병행 dispatch 병기: 폰 Music 런처 아이콘 설치 배치(사용자 "폰 화면에
  뮤직 안 보임") — 본 T5와 병렬·배포 절차 계약 — §5 ⑬.

### §7.7.3 함정 원장 (#93/#94 신규 — 봉쇄 레슨 축)

1. **★tap 전송 ok ≠ 착탄(판정기 원장)** — 탭의 ok/sent 답신=전송 성립만.
   UI 전이(dirIndex 전환+스캔 중 행)는 수 초 뒤 화면 도달(WSL 런 1-3 —
   현행 클라 5시도 무착탄 → 재스폰 클라 3시도째 착탄). 착탄 판정기
   **landtap**(기저 캡처 px 차분 임계 3행+y 스윕 5시도+고착 클라 close
   회수→재스폰 경로)이 probe에 봉합된 뒤 판정 경계가 결정론화 — **높히
   확신하는 정직 결함 원장행 후보 아님**(엔진 결함 라인 비신청 — 원인
   추정은 프레젠트(화면 관할)와 이벤트 처리의 probe 내부 경계 — 판정기가
   흡수). 레슨: **합성 입력의 "ok"는 도착 영수증이 아니다 — 픽셀 차분으로만
   착탄을 말하라**.
2. **★VHD writeback sync 선행** — 2×460k 파일 시드 직후 music_dir_add의
   원자적 쓰기가 도구 응답 15s×3 초과(WSL 런 1 — 인프라 원장). `sync` 1회
   선행 배수(drain 0.4-0.7s)로 런 2-5 소각 — 대형 트리 세그먼트를 도입하는
   probe에 같은 배수 승계(폰 T4도 $TMPDIR가 VHD가 아니어도 동형 배량 수형
   유지 — 원장행 승계).
3. **★CP_ACP(949) 기기 전제(#94 리뷰 C-1)** — 2q-a 왕복/narrow fopen
   실개방 단정군은 KS X 1001(한글 ACP) 기기에서만 통과 — EN-US ACP(1252)
   기기면 '?' 치환으로 round-trip·개방 단정 FAIL. 사용자 실기기(CP949)
   캐논은 유효하나 **타 ACP 기기의 캐논 재현성 = 로캘 전제 원장** — 수형
   방향(로캘 게이트/CP_ACP 대조 선취) park(§4.1-2).
4. **이(UTF-8) 모지/strict throw 흡수(#94 리뷰 I-1)** — openPath_에 부적합
   UTF-8 진입 시 `fs::path(openPath_)` strict ctor가 throw 가능 —
   WorkerLoop의 `catch(...)` 장벽이 흡수해 종착 문구가 "재생 중 내부
   오류가 발생했습니다"로 갈림(원존 "파일을 찾을 수 없음") — 장벽 자체가
   원장 계약(spec 1a-3)이라 **결함 아님·분류 미세편차 원장**.
5. **landtap 폰 미이식 — tab2 병합 구성 대체 원장** — 폰 probe는 WSL의 px
   차분 착탄 판정기+tab3 좌표 캘리브레이션을 이식하지 않았다(원장행). 대신
   **tree_a를 dirs_[1](tab2)에 앉히는 병합 구성**(ResolveDirs 원문:
   dirs_=[기본(AudioDirFallback), …music.dirs]) — 실측 상수 TAB2_X=90
   재용으로 "활성 탭 = big tree" 경계를 **신규 좌표 원장 0**으로 재현.
6. **★스캔 취소 마커 상수 오착(T4 r1 — hard FAIL 1회)** — probe 마커를
   "ScanCancelFn"(주석 토큰)으로 쓰면 미검출("wiring marker missing" hard
   FAIL) — **실배선 토큰은 `scanCancel_`**(식별자). 레슨: **마커 상수는
   커밋 전 로컬 grep 실측으로 정산해야 한다** — r2/r3 정산 후 2런 재현
   (배포는 이미 HEAD 동일 — 증분 무영향).
7. **★병렬 커밋 수술 — 같은 main.cpp를 두 커밋이 나눠 쓰는 창** — T1(#93)
   과 #94가 병렬 진행하며 동일 파일에 2p/2q 블록(+include 행)을 uncommitted
   공유 — T1은 **인덱스 수술**(`git hash-object`+`update-index
   --cacheinfo` — HEAD 원문+자기 2p만 스테이지·타 커밋 hunks는 작업트리
   현행 보존)으로 분리 커밋하고, 후속 커밋이 나머지를 수취해 **2p→2q 블록
   수렴**(T2 리뷰: diff removed/added **multiset 72/72 전부 상쇄** — 유일
   잔차는 주석 2행 — 내용 등가·순서만 확정). 레슨: 병렬 태스크 동시 진입은
   **인덱스 수술+작업트리 보존+multiset 등가 검증** 트리오로만 안전하다(#94
   리뷰 I-3 = 빌드 표기의 정직 부기 — 커밋 트리 그대로 빌드 아님).
8. **실행 순서 변경의 정직성(원장행)** — T3 세그먼트 실측 순서=②→①→③
   (브리프 표기 ①②③ — **순서 등가 원장**): ②의 제거가 자체 재청구 창
   (pub2 재스캔 ~5s)을 세우므로 그 창을 ①의 in-flight close 경계로
   재사용·③은 재스폰 클라 대리(①의 close가 pub2를 소멸 — 파괴 취소 계약
   이라 pub2의 채택 전이는 동 세그먼트 창에서 관측 불가 — 시차 원장; pub2의
   채택은 런 1-2 원장의 간접 관측). probe 머리 주석+원장행으로 봉인 —
   **순서 바꿈은 원장행으로 봉인하면 정직하다**(T3 리뷰 PASS).

### §7.7.4 결정 원장 (재량 — 기각 시 즉시 수리 — 플랜 Global Constraints 원문)

1. **취소 체크 단위 = 디렉터 경계**+엔트리 256 보조(결정 ① — 무경계 폭주
   leg의 응답 상한 256 엔트리).
2. **소멸자 = cancel 1발→join(무한 대기 소각 — 대체로 ms급)**·OR 봉합
   (결정 ②).
3. **[제거] 성공 = 취소+재스캔 1발**(결정 ③).
4. **도착 폐기 = 멤버십 필터(세대 앞단)+stderr 진단 1행 — UI 무변화**(결정
   ④ — 정직·무음 금지(D4) 계승).
5. **외부 삭제 = ec 중립 유지**(결정 ⑤ — 이미 계약+2p-③ 원문 단정).
6. **#94 규약 = 와이어/저장/표기=UTF-8·파일 터치=네이티브** (+Utf8ToAnsi
   fail-closed 어댑터가 "빈 변환 =경로 인코딩 변환 실패" 정직 종착을
   소유 — spatial-player 무변경 계약 위에서 JKEngine 측 경계만 수리).
7. **멤버십 필터의 세대 게이트 앞단 배치** + [제거] 취소 1발의 무조건성
   (루트 대조 없음) = 브리프 문언 직행 — 절약 변형은 park(§4.1-2 T2 I-2)
   재판정 대기.

### §7.7.5 커밋 원장 + IP/호스트 경로 게이트 (#93/#94 라인)

- 라인 체인 = **4473e8b(플랜)..97b2d8e**+본 T5 커밋(§7.7.0 표 = rev-parse
  전체 SHA) — push 전부 origin main 직행·amend 없음.
- **IP·호스트 경로 리터럴 게이트(T5 실측)** — 본 커밋 원문에서
  `\b([0-9]{1,3}\.){3}[0-9]{1,3}\b` grep = **0건**·호스트 경로 표기(사용자
  루트 폴더명·드라이브 글자·/mnt 계열) grep = **0건**(커밋 전 staged/
  `git show HEAD` 게이트 — §6 문구 동형). 라인 이전 커밋도 리포트/review
  전원 IP grep 0건 실측(T1·#94·T2·T3·T4 각 리포트 — §7.7.0 계보).
- **본 T5 커밋은 docs 1파일(`docs/90_music_spatial_leg_asbuilt.md` — §7.7
  신설+§4.1-2/§5 추가만+§7.6 스냅샷 갱신)로 한정** — git add 명시 경로만
  (#83 사건 레슨). probe 런 로그(run16d_{1..5}·phone_music{,_run}.log)·
  캡처(mus_wsl_scan_busy·mus_wsl_rescan_settled·mus_phone_scan_busy·
  mus_phone_rescan_settled+crops)는 engine/tmp untracked — 커밋 금지 충족.

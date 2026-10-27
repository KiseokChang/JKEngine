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
- **부기(#93 스캔 안전화+#94 경로 인코딩 — 하단 부기·변형 아님)** — 본 문서에
  §7.7 신설(스캔 안전화+한글 트랙 재생 라인 T1-T4 원장): 후속 체인 =
  **4473e8b(플랜)..97b2d8e**+본 T5 커밋 — §7.7.0 표 = rev-parse 전체 SHA.
  (이 #93 부기 bullet 자체는 **1b13b82(T5)에서 신설** — #92 bullet 뒤의 갭을
  본 T5 커밋이 메꾼 것 — T5 리뷰 M-3 정정 표기.)
- **부기(#97 폴더 브라우저 라인 — 하단 부기·변형 아님)** — 본 문서에 §7.8 신설
  (뮤직 폴더 브라우저 라인 T1-T4+fix 3종 원장·2026-10-11): 후속 체인 =
  **6ad3834(플랜)..2999f3d**+본 T5 커밋 — §7.8.0 표 = rev-parse 전체 SHA.
- **부기(#99 music 태그 라인 — 하단 부기·변형 아님)** — 본 문서에 §7.9 신설
  (뮤직 태그 읽기 라인 T1-T4 원장·T4 리뷰 없음(BLOCKED — 원장)·2026-10-11):
  후속 체인 = **8315b15(스펙)·7e39a28(플랜)..cf978f8**+본 T5 커밋 —
  §7.9.0 표 = rev-parse 전체 SHA.
- **부기(#100 vplayer 조그 재설계 라인 — 하단 부기·변형 아님, 2026-10-16)** —
  본 문서 신설 절 0건(라인 as-built = docs/50 말미 부기 bullet — 휠=연속
  워크+관성 감쇠·WSL probe `engine/tools/probes/wsl_jog_flow.sh` T2 —
  2런 ALL PASS·무음 축 원문): 체인 = **fb38d79(플랜)..8d83027(T1)**+T2 커밋
  (주석 정정 8548dac·probe·docs — rev-parse 전체 SHA), 캐논 등호 승계
  Win 691/WSL 668/posix 304.
- **부기(#101 백워드 조그 프리필 라인 — 하단 부기·변형 아님, 2026-10-17)** —
  본 문서 신설 절 0건(라인 as-built = docs/50 말미 부기 bullet — 역방향
  선디코드 프리필(세션 디코더 재용·접두 경계 삽입)+WSL probe
  `engine/tools/probes/wsl_jog_flow.sh` BWD legs 2건 신설(BWD SLOW=
  프리필 히트·BWD DEEP=크리프 클램프 — probe만 775f589 1커밋, 런 3회
  ALL PASS)): 체인 = **f78a767(플랜)..18429c3(T1)**+본 T2 커밋(probe·docs
  — rev-parse 전체 SHA), 캐논 등호 **Win 691/WSL 668/posix 304** — T1
  리포트의 "WSL 671" 표기는 오표기 원장행(T1 캡처 2파일+본 T2 런 3회 전부
  668 등호·PASS 세트 diff=0 — probe PASSSET-DIFF 자가 실측). probe 수취
  단열 dir = engine/tmp/t2jog101(구 t2wheel 동명 겹침 사고 분리 봉합).

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

### §4.1-3 신규 park 행(#97 폴더 브라우저 라인 — §7.8 승계 · **아래는 추가만,
기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T4 concern 1 | **⑤ [이 폴더 추가] 폰 등록 강도(FULL) 미성립** — 3런(2/3/4) 종료 전이 착탄(DELIVERED)이나 music_dir_list에 sub_[ab] 미출현 = cwd=루트 no-op 원문 | M | 라이브 캘리브레이션 세션 수형(WSL 16c 선례 — 1탭-1캡처-1판정) 또는 tapsettle에 "클라 포인터 유도" 수형(탭 후 같은 창 무해 지점 park — io.MousePos 희생 지점) — 후속 런 몫·**T2 fix r1 재검 I1(행 격자 ±22px 재보정 — T4 소관 원장행)의 승계 종착** |
| T4 리포트 본안 | ③JoinDir 진입·④Parent·⑤ 추가의 **자동 탭 체인 미성립** — 폰 렌더 지연(~10s)+커서 잔상+표 스크롤의 3중 드리프트로 착탄 판정 흔들림 | M | 상행과 동일 라이브 세션 몫 — ②체인(탭 전환·개봉·cwd 표기+sub 목록)은 XTEST 수취 프레임 원문으로 성립(§7.8.2)·⑤ 좌표 정산 원문 동봉(런 2/3 — sub_a 클라 y≈362·".."≈311·버튼행 322-342 — WSL 원산(348/294/321)과의 폰 폰트 행높이 쏠림) |
| T3 concern 2+T3 리뷰 concerns 2·I-4 | **캡처 = 클릭-전(개봉-전) 프레임 수수** — ② open/subs 캡처가 개봉-전 [폴더 관리] 패널 프레임(false-positive 30-diffs — T3 리뷰 뷰잉으로도 재확정 — sub2 캡처만 개봉 상태 원문) | M | MUS_BROW_RECON=1 정찰 런(set-timed 재수취) 또는 차기 런 ② 캡처 재수취 몫 — **tapsettle 8s 무음 판정이 이 지연 하에서 더 안전**(false-FAIL은 false-SUCCESS보다 안전 — 토글 역전 함정 회피)·프레젠트 지연 2-8s·간헐 수십 s 원장(§7.8.3 #5) |
| T2 fix r1 concern 3 | 압착 상태에서 spatial 패널 4px 띠(콘텐츠 클립) | M | 압착 가드 2검의 의도 봉합(음수 잔여 child 소각 — §7.8.1) — EYES 관측 몫·런 3 폐쇄 상태 화면 = 원존과 동형 실측 |
| T2 concern 3+T2 리뷰 concern 3 | [폴더 관리]+[찾아보기] 동시 개봉 시 **표 영역 압착 잔여 관측** | M | fix r1 가드로 -200 고정폭 소각(reserve = clamp(남는 높이−56, 4, 200)) — 잔여 체감은 사용자 선언(EYES) 후 재판정·probe는 cwd 표기·목록 캡처만 소관이라 영향 0 |
| T2 fix r2 재검 I1 | index 범위 밖(valid=false→deny 공문자)+stale cwd 잔존 조합의 무음 개봉 이론 경로 | N | dirIndex_는 ResolveDirs 클램프+탭 스트립 유효 인덱스만 취급 — 실도달 불가·**방어선 등급 원장행(수리 불요)** |
| T1 fix r1 재검 I1 | 절대성 residual 2폼 — name 자체가 절대면 결합에 이중 슬래시(JD(/a,"/sub")=/a//sub·JD("C:","/sub")=C://sub) | N | 수리 전 구형(3슬래시 "C:///sub")보다 나은 폼·현 UI 계약(폴더 행 = bare 이름만 전달) 미도달·**선존(JoinDir 이전부터 동거) — 원장행(수리 의무 없음)** |
| T1 fix r1 재검 I2 | UNC "//" 루트 → 단일 "/" 루트 흡수 | N | Windows 브라우즈 cwd는 드라이브 폼·posix는 선행 이중 슬래시를 implementation-defined로 취급 — **양축 안전·원장행** |
| T1 리뷰 I2 | ListSubdirs 열거 도중 실패 경로는 directory_iterator의 던지는 operator++ 원문 수형 승계(ScanAudioTree 동형) | M | 결함 아님 — 계약 표기 정밀도("ec 중립형 = 개시+성분 판정은 ec, 열거 계속은 원문 수형 승계") — 다음 MusicModel.h 접촉 시 주석 1행 몫 |
| T3 리뷰 M-2/M-3/M-4/I-1..I-3 묶음 | probe 내원 원장행 — ⑤ 후보 계수 정정(110/5런 = C2/C3/D 20 후보+E/F 25·런 C는 BADD=0·런 C2 계보 누락)·landtap 이동의 sleep 1.8→5 견적을 "본문 무변경"으로 표기(PX 본문 전용이 무변경)·settings 보호 "engine/tmp 사본 2벌" 기록의 비교 원문 부존재(복귀 이행 시 소각 — T3 리뷰 복귀 실측으로 결과 닫힘)·selftest 648 "5회"→7회 과소 계수·MUS_BROW_TH_ADD 미소비 상수+주석 y 산식(458-482 vs 442-457) 동커밋 불일치·tapsettle "무음=재시도 안전" 서사 과잉(토글 오복원 이론 잔여 — 상태 사슬이 지켜줌)+BADD 루프 후보 간 캡처 부재 | M | 판정 본안(2런 등호·WSL 648 7회·⑤ 0 등록) 유효 — 다음 probe 접촉 시 원문 정정·소각 몫 |
| T2 fix r1 재검 I3 | T2 fix r1 리포트 파일별 계수 오기(cpp +153/−28·sh +97/−42 — numstat 실측 +114/−39·+68/−33) | M | **소각 — 본 문서 §7.8.0/§7.8.6 표기는 numstat 실측 기준으로 정정 기재**(총계 182/72 등호) |
| T3 리뷰 M-1 | ⑤ 원인 서사 "negative-height 압착 경계 — 실결함 성립" 원장 초과 기술 | M | **소각** — T2 fix r1에서 원장 정정(기하 기각·단절 서사)+⑤ 재검 성립(BROWSE-OK 8/8)+diag2 분절(§7.8.1) — **T2 fix r1 re-review "⑤ 무전달 소각" 원문** |
| T2 리뷰 I1+I2 | 행 클릭 동프레임 툴힌트 드리프트(진입 후 cwd로 재조립 1프레임)·리포트 링크 표면 서사(jkdesktop+jkapp_music 2건 표기 — 실측 jkapp_music 단독) | M | **소각** — fix r1 dest 사본 선취(클릭 판정 전 JoinDir 1회 구성)+원장은 링크 표면 실측 기준 기재(§7.8.6) |
| T2 리뷰 I3 | 광 드라이브 표기 cwd("C:") — 드라이브 현재 디렉터 건드림 함정 도달(ListSubdirs("C:") 부정확 목록) | M | **소각** — fix r1 개봉 가드 2검(bare "C:" 거부+"X:/ 형식으로 재등록" 정직 1행 — JoinDir 드라이브-상대 함정 cwd 폼 차단) |
| T2 fix r2 재검 I2 | **diag2 trace 원본 = WSL /tmp 휘발**(클라 무죄(IPC 응답지연) 귀결의 유일 실물 — srv.log 83KB) 영구화 미수행 | M | **소각 — 본 T5에서 사본 1벌 복원 수행**: `engine/tmp/t2f_diag2_srv.log`(83,916B — 원본 WSL /tmp/t2f_diag2/srv.log와 md5 `4f29566412cc3f17bf363c1ae2085e3d` byte 등호 실측 — untracked 커밋 금지 충족)·전반부(영구화) 소각 — 후반부(구 ⑤ 지대 y 442-457의 직접 재판정)는 fix r1 행 이동으로 지대 자체 존재 소멸 — 재판정 의무 소멸(diag2가 토글 지대만 재현한 "제어 클릭 축 실측" 표기 원장 유지) |
| T4 원장 | SWEEP-DIFF=8/319(기대 집합 11 — ddd0f45 앵커 계보) — **probe 기대치 갱신 필요** | M | **리뷰 판정 확정 = 앵커 stale·실물 누락 아님(근거 4건 — §7.8.3 #7)** — 차기 런 전 `PHONE_DEPLOY_BASE`를 실배포 HEAD로 정산+앵커 미갱신 기간의 사이즈-등호 3파일 의존을 원장행에 명기 몫 — **소각 — park 배치 4(2026-10-11): 폰 blob md5 실측으로 정산 — 기대 집합 11파일+ipc 1파일(=12) 전수 대차(`git show HEAD:$f` vs 폰 `tr -d '\r' \| md5sum` — sweep 개행 정규화 동형 축): 11파일 전부 md5 등호(사이즈-등호 3파일이 콘텐츠까지 최신임을 원문 실측 — 사이즈 블라인드 함정은 이번엔 미발화 — 원장 유지)·잔여 1파일 = `engine/src/ipc/JKPipeTransport_posix.cpp`(#98 수리 e28fd81+fix r1 c021006 사후 계보 — 폰 미재배포 구조)로 단일 blob 재배포 수행(git show HEAD 스테이징 tar 1파일 — D1 전량 archive 금지 계약 준수·오염 게이트 HEAD blob md5 등호·폰 서버/jkweb 무접촉) — 사후 md5+바이트 17,221B 등호 = 폰 트리 12/12 HEAD(c021006) 등호 → **PHONE_DEPLOY_BASE 정산 = c021006**(probe 기본 상수는 env 승수 계약 — 다음 폰 런에서 환경변수 권장 원장). 재빌드·재기동 미수행(서버 존중 — 폰 실행 바이너리는 d5c6875-era 유지·소스/바이너리 갭은 다음 폰 배포 런에서 해소 — 정직 원장)** |
| T4 리뷰 M-2 | 커밋 본문 결함 원장 계수 4건 vs 실제 수형 5건(런 2 remove 서기 지연 폴링 = N4-10 CLEAN-POLL 미계수) | M | **소각 — 본 문서 §7.8.3 #6에 5건째 원문 봉인**(런 2 remove 서기 지연 폴링 — clean-poll 수형) |
| T4 리뷰 I-1 | probe 판정행 `MUSIC-BROWSE-VERDICT: BROWSE-OK(…③④④2…)` 라벨이 cwd 전환 성립처럼 읽힘 — 실제 = tapsettle 착탄(EYES)+SUB-MISS 원문(리포트 본문은 "미성립"으로 정직 정정 — 라벨만 미세 과다) | M | 다음 런 ⑤ FULL 재판정 시 라벨-원장 정렬(⑤ 라이브 캘리브레이션 몫과 동일 라인 — §4.1-3 상단 행) |
| T4 리뷰 I-2 | phone_music.sh L1727/1729 인접 중복 sdcard 헤더 주석 1행(N4 블록 삽입 시 재삽입) | M | 동작 영향 0 — 다음 probe 접촉 1행 소각 |
| T4 원장 | 폰 probe 병합 수형 교환 — 진입 settings의 music.dirs = 사용자 원문(163B) → probe dirs 선행+사용자 dirs 보존 병합(워터마크 hard FAIL 폐곡)·탭2 = probe 시드·사용자 dirs = 후속 탭 | N | 원장행(T4 리포트 concern 2·리뷰 3 PASS — 구 ORIG 폐기 가드(ORIG-STALE-SUPERSEDED — 구 ORIG 원복 시 사용자 dirs 소각 위험 수형 원장)까지 실측) — 사용자-first 재배치 필요 시 MUSP env 재런 몫·**탭2=probe 시드 표기가 구 브리프 문언("탭2 = /storage…Music")과의 편차 — 복원 뒤 사용자 원본 상태 캡처(mus_phone_final_restore.png)가 대변** |

### §4.1-4 신규 park 행(#99 music 태그 라인 — §7.9 승계 · **아래는 추가만, 기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T1 리뷰 I1 | 갭 서사 ②"narrow CRT/ACP" 과단정 — `drflac_open_file_with_metadata_w`(dr_flac.h:1076)·`drmp3_init_file_with_metadata_w`(dr_mp3.h:474)의 `_w` 수형 존재 | M | **소각 — 본 문서 §7.9.1에 서사 정리 반영**(부담 근거는 ①공급 경로 계약 — 전 수형 공통·결론(재용 불가→own 파서) 불변) |
| T1 리뷰 I2 | ID3v2 extended header(플래그 0x40) 미핸들 — 확장 헤더 뒤 프레임 사슬이 패딩 break로 전멸 = 빈 TagSet(파일명 폴백 graceful)·미테스트 | M | 실세 레어(대부분 태거 미서기) — 다음 파서 접촉 시 2s 케이스 원장행 |
| T1 리뷰 I3 | v2.4 전역 unsynchronisation(휴드 flags 0x80) 미전개(v2.3만) — v2.4 전역 unsync 태그는 텍스트 오염 가능 | M | frame-level unsync(fmt 0x02)는 스킵으로 봉인·실세 소멸 계열 — 후속 원장행 |
| T1 리뷰 I4 | v2→v1 폴백 판정 = TagEmpty 계열(필드별 합병 아님) — 스펙 §1 "v2에서 못 찾으면" 문언의 필드별 해석 여지 | M | 컨트롤러 승인 사항(부분 v2에서 v1 아티스트 소실 케이스) — 원장행 |
| T1 리뷰 I5 | 2s 경계 커버리지 갭 — FLAC 잘린 블록(`off+bsz>n break` leg)·Ogg 잘린 페이지·v1 n<128B·WAV id3 청크 절단 = 구현됐으나 미테스트 | M | 코드 리딩+시뮬레이션 유효 실측(무한루프/파산 0) — "미테스트 표기" 원장행 — 2s 확장 케이스 몫 |
| T1 리뷰 M1 | kBlobV24PadFooter 배열 보정(1바이트 0x00)+주석 정산 — 실측 53B·패딩 11B·푸터 13B(문언 정산은 본 문서 §7.9.4 완료) | M | 코드 보정 = 다음 **main.cpp(2s 블록 :6212-6221)** 접촉 커밋 몫(park) — MusicModel.h에는 kBlob* 0건(지도 정정 — T5 fix r1) |
| T1 리뷰 cosmetic | "사이본 상한 1MB"(사이즈)·"구형 리터"(라이터) 표기 + continuation 주석 행두 공백 1개(` // 규약보다 우선` 계열) — MusicModel.h:782·:810(+행두 공백) HEAD 실측 잔존 | N | 다음 파서 접촉 1행 소각 |
| T2 리뷰 I4 | **MatchFilter(Track) 오버로드 무어설션(REMNANT)** — 2s-⑨는 string 수형(title/artist 문자열)만 단정 — Track 오버로드(3행 OR 래퍼)는 UI·VisibleTrackCount 경유 소비 전용으로 직접 어설션 없음(신설 어설션 계약 밖 — 결함 아님) | M | REMNANT 표기 원장(위험 하) — OR 누락류 실수는 2s 미검출 — 2s 확장 케이스 또는 probe 경유 소비 원장 몫 |
| T2 리뷰 I1 | 취소 경계 사이 갭의 시간 — 간격 64 수형(fix r1) 이후의 **폰 저속 저장소 실측 부재**(간격×파일당 머리 1MB read) | M | 코드 소각 완료(256→64)·**폰 도착 지연 실측 = 다음 폰 런 몫(2p-② 상한 계약 무변조 — 래치 소유)** ·필요 시 간격 절하 재판정 |
| T2 리뷰 cosmetic | 필터 힌트 "곡명·아티스트 검색"에 파일명 매치 문언 누락(원존 유지 — 사용자 오해 여지) | N | 다음 UI 표기 접촉 1행 몫 |
| T2 fix r1 재검 cosmetic ① | 간격 산식 주석 "실사용 태그는 수십 KB"(MusicModel.h:274) — read 크기는 min(total,1MB)·"수십 ms급"도 NVMe 핫캐시 근사(콜드 HDD 수백 ms급) | N | nuance 원장행 — 다음 접촉 표기 정산 몫 |
| T2 fix r1 재검 cosmetic ② | main.cpp 2p-② 어설션 메시지·블록 헤더 주석 "256 보조" 문자열(main.cpp:5947대) | N | 무변조 계약상 문자열 유지 정당 — 표기 정산(park) |
| T3 리뷰 M-1 | probe 4c/16e-2 최상행 예상 코멘트("mtime desc — alpha 최상단") — 실측 캡처는 t_raw_noid 최상단(시더 write 순서 alpha→beta→raw = raw 최신 mtime) | M | 캡처가 진실원·판정 영향 0 — 재런 시 코멘트 정산 몫 |
| T3 리뷰 M-2 | mus_bandpx 임계 TAG_BAND_PX=60 > 실측 도착 14px(런 5) — 커밋 probe 재런 시 양 라운드 전원 TAG-FAIL 구조 | M | **다음 probe 접촉 임계 하향+밴드 재정찰 원장행**("캐럿 ~30px 배제" 근거 대비 실측 글리프 변화 14px) |
| T3 리뷰 M-3 | SET-RESTORED 무조건 "byte-exact" 문언 — cmp 불일치 시 MUSIC-FAIL행과 모순 원장(런 원문 불일치 0 — 실측 무해) | M | `&& echo` 단락 권고 — 다음 probe 접촉 몫 |
| T3 리뷰 I-1 | /tmp/mus LOGDIR 런 간 공유 누적(REMNANT-COUNT=281 — "서버·빌드·셀프테스트·shot 로그만 잔존" 원장 문언과 어긋남) | M | 런별 서브디렉터리 — 후속 태스크 몫(런 5 영수증 스테일 상실의 구조적 뿌리) |
| T3 리뷰 I-2 | 크롭 파일명 스왑 2종(…_filter_crop.png = 무필터 8곡 상태·…_artist_crop.png = 곡명 매치) | N | 런별 영수증명 분리 관례화(T4 폰 캡처명 상수 원장과 묶음) |
| T3 리뷰 I-3 | 18f 캐논 문언 stale — wsl_music.sh:2537 "런 시점 Win 671/WSL $CANON_WSL_SELFTEST/posix 296"(상수행 :128은 668 갱신 — 에코행만 구계보 문언) | M | 다음 probe 접촉 1행 정산(본 문서 캐논 표 = 691/668/304 — §7.9.2) |
| T4 concern 4 | 배포 드라이버 `engine/tmp/pmus_t4_deploy.sh` = **미커밋 산출물**(tmp 정리 관례와 상충하는 유산) | M | 다음 폰 접촉 런에서 사용 후 probes/ 산입 커밋 원장(명시 경로 계약상 본 라인 밖 소관) |
| T4 concern 5 | 폰 taglist/taglist2 캡처명 상수 재용 — FULL 런 2회의 2차가 1차를 덮음(마지막 런 영수증 = 원문) | N | 런별 분리 관례화 시 병기(T3 리뷰 I-2·concern 5 원장 계승) |

### §4.1-5 신규 park 행(#111 music 태그 쓰기+커버아트 라인 — §7.10 승계 · **아래는 추가만,
기존 행 변형 금지** · 본 라인 원장 = 본 문서 §7.10):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T7 부기 | 태그 쓰기 라인 커밋 체인(1행 — rev-parse 전체 SHA = 본 문서 §7.10.0 — git log가 진실원): **662ed23(스펙) → cd17d65(플랜) → 15c2bb4(T1) → 2973933(T2) → c8f31f2(T3) → 5b95c63(T4) → eaba3a4(T5) → d2c91ce(T6) → aed0add(T6 fix r1) → 2aa03f0(T6 fix r2)**+ 본 T7 커밋 | N | 1행 등록 — §7.10.5 커밋 원장과 등호 · 잔여 유계(탭 좌표 재보정 별소관·spatial_mix 재런 원문 대기)는 §7.10.3 원장이 소관 |

### §4.1-6 신규 park 행(#112 STT 구두 라인 — docs/91_stt_voice_asbuilt.md 승계 · **아래는 추가만,
기존 행 변형 금지** · 본 라인 원장 = docs/91):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T7 부기 | STT 구두 라인 커밋 체인(1행 — rev-parse 전체 SHA = docs/91 §0 — git log가 진실원): **496a5e6(스펙) → 79a27d1(플랜) → 9512ba8(T1) → 049b94e(T2) → a5e65cf(T2 fix r1) → 1b828e2(T3) → 4279d96(T4) → 688514f(T5) → 6128bb4(T5 정산) → e9d13ca(T5 fix r1) → 036b41e(T5 fix r2) → 58d89e6(T6) → 7cd8ae6(T6 fix r1)**+ 본 T7 커밋 | N | 1행 등록 — docs/91 §8 커밋 원장과 등호 · 잔여 원장행(F6 포그라운드 경합·채팅 합성 타이핑 단절·.rec.m4a 정산 미달 분기 등)은 docs/91 §6이 소관 · **#111 정산행(recover_opt 재런 원문)은 docs/91 T5에서 RECOVER-OK 실측 소각 완료** |

### §4.1-7 신규 park 행(#114 JK APK 라인 — docs/92_jkapk_asbuilt.md 승계 · **아래는 추가만,
기존 행 변형 금지** · 본 라인 원장 = docs/92):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T4 부기 | JK APK 라인 커밋 체인(1행 — rev-parse 전체 SHA = docs/92 §0 — git log가 진실원): **6a7d6d7(스펙) → 32a74d5(스펙 전환 — D2 v1 얇은 타이핑) → 432f4d7(스펙 정정+플랜) → eb4de96(T1) → ad3834d(T1 fix r1) → 0f18157(T2) → cce4420(T3 — probe 신설)**+ 본 T4 커밋 | N | 1행 등록 — docs/92 §8 커밋 원장과 등호 · 잔여 원장행(설치·런치·발화 EYES·LLM 스파이크 자리)은 docs/92 §5-§7이 소관 · **엔진 ninja 캔논 무접촉 라인 — 캐논 계보는 #112 계보(Win 837/WSL 814/posix 304/폰 814) 그대로 유지(표기 — 재측정 0)** |

- **§4.1-7 캐논 부기(park-5 정산)**: 위 행의 계보 표기(837/814)는 #114 라인
  종착 시점 원문 — **head 캐논 = 841/818/304/814 #113(사용→리뷰 루프, 2w=4)
  승계·#114 라인 기여 0**(기존 행 변형 없음 — 추가만 원칙 준수; 정산 원장 =
  docs/90 §7.11·#113 최final GO 행).

### §4.1-8 신규 park 행(#113 사용→리뷰 루프 라인 — §7.11 승계 · **아래는 추가만,
기존 행 변형 금지** · 본 라인 원장 = 본 문서 §7.11):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T1 deferred | 저널 회전/사이즈 상한 부재(v1 비-목표 — append 전용·상한 코드 0) + 핫 경로 per-call fopen+fflush 지연(~ms — 저널 볼륨 증가 시 배치 flush 검토) | M | 사용 실측 볼륨이 콜 수 비례 소량 — 운영 정산 후보(수확 라인 후속) |
| T1 원장 | 2w-② "도구 응답 ok 무영향"은 구조적 어설션(실 dispatch 도표 아님 — writer void·무예외·훅 반환 미소비 2계약의 조합)·`err:"parked"`는 스펙 밖 재량 상수 | M | 실기기 콜 원문은 T2 harvest 2런에서 확보 완료(journal 행 원문) — parked 상수는 T2 저널 파싱 계약과 정합 — 기각 시 즉시 수리 |
| T2 deferred | JsonEsc `{}` 유니코드 이스케이프 인쇄가 저널 arg 가독성을 누름(계약 무접촉 — 사람 읽기 형태)·저널 tail 20 vs 스펙 200 정산 후보·pgrep -x 타서버 매치(같은 exe명 서버 다중 환경)·OBSERVE bridge_report 위치 = jkbridge ExeDir 규약 의존 | M | 다음 probe 손때 몫 — 전원 파싱·판정 무영향 |
| T2 원장 | 드라이버의 "no-op guard" 명명 명령이 실제로는 probe 1런 실행이었다(행 10→15 — 명명 부정확 원장행 — 판정 무영향)·WSL pre-clean = 라이브 동빌드 서버 종료 접점(wsl_* 선형 원존)·분류 err 시료가 close_window deny 1종(폰 재배포 후 agentctl 다양 접점에서 err 다양 원문 몫) | N | 원장 기록으로 소화 — 재발 방지(명명=원장 사건) |
| T2 fix r2 | FAIL 출구 잔상(리뷰 Important 1건) — trap EXIT 래더 1원 재용 수형 **소각**(5fce208) — 실험 원문: OK 런 REMNANT 0+failsim(pingxx=not_implemented) FAIL→HARV-TEARDOWN-LEFT: 0+실행 후 pgrep 0 | M | 소각 원장행 — 실험 부기: failsim 런이 저널에 `not_implemented` err 행 1행을 추가(probe 소유 — 분류 요약 err 시료 2종째 — 의도치 않은 다양성 성과) |


### §4.1-9 신규 park 행(#115 opus/m4a 라인 — §7.12 승계 · **아래는 추가만,
기존 행 변형 금지** · 본 라인 원장 = 본 문서 §7.12):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T4 fix r1 | OM_COVER_B64 수기 재타이핑 4자 절단(런1 honest — 원장행) | M | 소각 — a43e4b0(프로그램 복사 수형 — B64 인라인 상수 재용은 수기 금지) |
| T4 fix r2 | 재독파서 cmt_htype off-by-one([4)=version·[5)=htype — 폰 r1 OPUSM4A-FAIL honest) | M | 소각 — 083997c(2파일 동형 수리+18i OM_ALL 본조건 채용) |
| T4 원장 | wsl.exe env 전파(런 라벨 — WSLENV "VAR/u" 접미 필수) | N | 원장 소화 — T2 원장(스크립트 파일 단일 진실원)의 env 확장 수형 |
| T4 원장 | t115_t4_deploy.sh verdict "bad substitution"(CANON A/B 토큰 결합 표기 — remote 영수증은 완전) | N | 드라이버 몫 원장행(pmus/finres 선례 동류) — 본 라인 배포는 재런 불요(원장 소화) |
| T4 원장 | phone_music.sh boot_server WHISPER_ROOT 누락(구 수형 — 사용자 구두 leg 다운그레이드 위험) | M | 소각 — F0b 수리(bfe2011 — 런 후 부팅 env 실측 원문) |
| T1/T2/T3 | probe CANON 상수 갱신 채권(wsl_music.sh 795→833·phone_music.sh 계보 행 — T4에서 소각) | M | 소각 — T4 probe 커밋(bfe2011) |
| 라인 유계 | moov-후행 2차 read 재판정(사용자 실기기 관찰 시 — §7.12.3/§7.12.4 ④) | N | 유계 — 원장행 유지 |

### §4.1-10 신규 park 행(#115b 구두 어휘 확장 라인 — §7.13 승계 · **아래는 추가만, 기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T1 원장(승계) | SttVoice.h 계약 주석 4종 표기 미갱신·LLM 스키마 app_tool 합의(v2 후보 — 오발 봉쇄 원존)·타이핑 표면 app_tool 전송 배선 부재(정직 안내문까지만 — 3 소비자 fix r1 소각은 완료) | M | 유계 — 후속 라인 후보 원장행 |
| T2 원장 | probe 계보 원문 갱신 3건 — ①fc3 기대 원문(T6 rc=3 스폰 leg → 현재 스폰 전 fail-closed "구두 끔 (whisper 모델 부재)" — park-5 쌍둥이 정합 수형 이후 resolve 계약 변경이 probe 재런 0 동안 놔둔 것 — 115b-1ws 런 실측 fc=2)·②dirrun 직접 스폰 `--prompt` 부재 = 인자 비등가 대조(실측: chain=[(뽀송한 소리)] vs direct=[[구독 / 좋아요]] 갈림 — 전사 비결정의 잠재 계통)·③캐논 상수 814→850(2y 17) | M | 소각 — 8ad5501(양쪽 수용+--prompt = kVoicePrompt 문자 리터럴 수형 — **프롬프트 갱신 시 probe 동기 원장행**(표=진실원 동기 계약 승계)) |
| T2 fix r1 | phone vocab 원격 스크립트 FAIL 출구 잔상 — permissions.json 33→62B **중복 close_window 행**+Video Player/Notifications 창 잔존(수기 원복 소각 실측: 33B 등호+close 2건) | M | 소각 — 705ac89(vcleanup() 무해 재입 — FAIL/성공 출구 통일)+병합 생략(close_window 상존 시 — 무변경 0) |
| T2 fix r2 | PHV_BTN env 판별이 기정치 채우기에 매몰(SET=835 만성 → AUTO 재산출 스킵 → tap 2런 (835,602) 미달 반복 — rec=done 전이 0·isRecording false) | M | 소각 — 46c4767(SET 수취를 기정 채우기 전으로)→재런 PHV-BTN-AUTO (1283,720)·rec/done 증가 2/2 |
| T2 원장 | wsl.exe env 전파 1런 라벨 소실(VOICE_RUN_NO=115b-R2 → run=auto — WSLENV 접미 필수 — #115 T4 원장 승계 누락) | N | 원장 소화 — R3 재판정 WSLENV="VOICE_RUN_NO/u" → run=115b-R3 |
| T2 원장 | stt_t2_canon=0 — T2 계보 원문 [구독 / 좋아요] MISS·실측 "(뽀송한 소리)"(440Hz 사인 환각 계열 — §2.4 CANON 밖 비결정) | N | 원장 유지(honest MISS — 전사 본문 = 사용자 재판정 밖 — 자기 원장행) |
| T2 원장 | 브리프 "기대 = 873 승계" 표기 — Win 축 캐논(3축 판정에서 폰 = **WSL 등호 계약** — 668/818/833 계보 원존) | N | 원장 소화(§7.13.2 캐논 표 — 폰 실측 850 = WSL 등호) |
| 라인 유계 | v1.6 후보(도메인 확장 — 파일/갤러리·LLM 스키마 app_tool·TTS·vplayer stop/next 도구 신설(토글 수용 원장행)) | N | 유계 — 스펙 비-목표 원존 |

### §4.1-11 신규 park 행(#117 구두 응답 TTS 라인 — §7.14 승계 · **아래는 추가만, 기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T2 원장 | **폰 TTS 엔진 웨지 사건(117-R5 돌입)** — termux-tts-speak 무한 블록(CLI 구간 stall ~25분 — 빈 CLIOUT receipt)·libexec TextToSpeech helper 잔존(helper 소각 뒤에도 무응답 — 시스템 TTS 엔진 서비스 측)·speak stamp는 계속 도출(spawn 성립)·`[TTS] fail (rc=137)` = **hang 스폰의 마감 kill 증상**(R6/R7에만 출현 — R1-R5 무-fail receipt ~10건 — fix r6 원장 정정)·"15s 마감×장문 겹침" 계통 요소 폐기 | M | 폰 현장 복구(재부팅 권장)=사용자 몫 — **수형 후보 순위: ① 스폰 후 즉시 반환(백그라운드 spawn — hang rc=137 반복 구조 소거 = 1순위) ② 마감 비례 상향 = near-no-op 표기** — 기각 시 즉시 수리 |
| T2 원장 | posix 캐논(+9) 재판정 채권 — ClientChatApp.h/.cpp = posix 빌드 목록 공통 TU이나 2z 순수 부품(C++ 헤더 인라인)이라 304→313 예상(WSL 859 계보와 동형 +9) — posix 머신 접촉 = 폰 접촉 계약 밖 | M | 승계 재판정(다음 posix 접촉 라인 몫) |
| T2 원장 | stamp 행 = 다중행 guide의 1행만(`[TTS] speak:` 이후 첫 개행까지 — D4 clip에 개행 포함 — 행 표기 한계 — 후속 행은 무표기 계행) | N | 원장 소화 — 지연 원문·도착 판정은 별개 진실원(§7.14.3) |
| T2 원장 | BLOCKED 원장행 미발화(전 런 폰 접속 성립 — 정찰 MISS 0) — 미접속 코드 경로는 본존하나 실측 0 | N | 원장 소화(honest — 미발사 코드 경로) |
| T2 원장 | engine/engine/tmp 이중경로 산출물(#115b~117 fix r1 전 런 캡처·RLOG — probe ROOT 2단 결함의 계보 산출물) | N | fix r1(48c5474)로 3단 수형 — 기존 산출물 = 원장 보존(삭제 대상 아님) |

### §4.1-12 신규 park 행(#118 창 번호 오버레이 라인 — §7.15 승계 · **아래는 추가만, 기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T7 부기 | 오버레이 라인 커밋 체인(1행 — rev-parse 전체 SHA = 본 문서 §7.15.0 — git log가 진실원): **8ad6d9e(스펙) → d9ef40b(플랜) → 6708134(T1) → 813ac43(T2) → 2658c97(T2 probe) → 2b4c950(fix r1) → f641cfa(fix r2) → 5ea90c5(fix r3) → 3c706d6(fix r4) → a01e832(fix r5) → 763fedf(fix r6) → 966bc62(fix r7) → 3a953c5(fix r8)**+ 본 T2(docs) 커밋 2건(원장+fix r6 정산) | N | 1행 등록 — §7.15.0 표와 등호 · 캐논 계보 = Win 915/WSL 892/posix 304(승계 채권 +25 누적 — §4.1-11 채권과 합산)/폰 892(T1 리포트 3a 채권 + T2 +12) |
| T1 원장(승계) | T1 커밋 메시지 오염 1행·10분 소멸 live 실측 미수행·최소화 번호 소모 0 재량(v2 재판정 후보)·배지 capture_region 등장 원존·3a 원소 단정·WSL 9p 캐시 사건 — 6건 | M | 유계/원장 유지(§7.15.3 원문 수취) — 10분 소멸 live = 다음 세션 승계 채권 |
| T1 리뷰(승계) | **폐색 창도 배지**(z 최상단 — diag_j118_badge_two_windows.png 증거) — 사용자 기각 시 수리 후보(①폐색 면제 ②z-순위 인지 그리기)·null-캐시 규약 주석 문언 nit | M | 원장 유지 — §7.15.3/§7.15.4 ③ EYES 게이트 대기 |
| T2 원장 | 한글 수사("창 삼") = v1 미구어(2y-⑲(h) stub 원존) — 전사 계보 실관측 EYES 몫·off 어휘 생략(v1 — 10분 소멸 원존·"창 번호 꺼줘"는 close 가족이 잡는다 — 서버 window_not_found 정직) | N | 유계 — v2 어휘 합의 시 재판정 |
| T2 원장 | wsl probe "15b" 헤더 라벨이 물리 순서상 15 앞(fix r3 이동의 잔여 — 산치·판정 무영향) | N | nit — 다음 wsl probe 접촉 1행 정산 원장행 |
| T2 원장 | **"2/3 스왑" 관측 = 리포트·docs 문언 오기(폰 배지 라벨 Video Player↔Chat 스왑)** — 정찰 원문(j118_swing_diag.sh WSL 1런+스윕 세그먼트 2런)으로 **배지 라벨 확정 = Chat=1·Video Player=2·Notifications=3**(PW 표+캡처 원문) — 렌더/도구 부정합 불성립·코드 수형 불요(badgeTargets 단일 수집 원존 성립) | M | 소각 — fix r6(763fedf)+정찰 원문(§7.15.2/§7.15.3) — 폰 P3/P4·P6/P7 전원 등호 |
| T2 원장 | 복합화법("창 3 앞으로") = 표 접미 행 우선 — 숫자 부품 비도달(Focus appless — "3" 의미 소실 무해) | N | 유계 — v2 어휘 합의 시 복합화법 판정 재판정 후보(§7.15.3 원문) |


### §4.1-13 신규 park 행(#119 폰 자연어 릴레이 라인 — §7.16 승계 · **아래는 추가만, 기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T7 부기 | 릴레이 라인 커밋 체인(1행 — rev-parse 전체 SHA = §7.16.6 — git log가 진실원): **f1d57e6(T1)**+본 docs 커밋 | N | 1행 등록 — 캐논 계보 = Win 919/WSL 896/posix 304 등호/폰 896(T2 run5 실측) |
| T2 원장 | **폰→PC L2 격리 BLOCKED**(EHOSTUNREACH·ARP 0·동일 서브넷 소스 주소·PC→폰 양시 성립 — 일측 격리) — 폰 /talk 릴레이 턴·지연 대조(스파이크 1.58s)·즉발 회귀 = 미실측 | M | 사용자 네트워크 조치(공유기 격리 해제/SSID 재접속) 후 t119 드라이버 재런 — EYES ① 전제 |
| T2 원장 | git archive 전수 전송의 폰 sshd 절단(71.9MB — run1)+Windows autocrlf 아카이브 CRLF 스머지(run2/3 md5 오진)+폰 트리 등화 보증 불성립(jkweb 잔유) — 3건 | M | 함정 원장(§7.16.4 ①-③) — 증분 배포+아카이브 멤버 md5 게이트+서브셋 등화 원존 승계 |
| T2 원장 | 수기 win CLI probe stdin 웨지(120s+ 무응답 — 스파이크 §3c win leg 재실측)+PC 주소 세션 콘솔 누출 1회(보존 파일 0건) | N | 원장 소화 — probe 폐기·engine spawn 계약 무영향(파이프 스폰) |
| T1 원장 | portproxy 불요 판정(0.0.0.0 바인딩+ollama.exe 규칙 = 기존 상태 실측 — 신규 노출 아님) — 보안 옵트인 원존 = §7.16.3 | M | 사용자 판정(옵트인/기각 — 원존 1커맨드 병기 완료) |

### §4.1-14 신규 park 행(#121 JK APK v2 라인 — §7.17 승계 · **아래는 추가만, 기존 행 변형 금지**):

| 출처 | 항목 | 등급 | 처분 근거 |
| --- | --- | --- | --- |
| T3 부기 | jkapk v2 라인 커밋 체인(1행 — rev-parse 전체 SHA = §7.17.0 — git log가 진실원): **2314b8b(플랜) → 469de84(T1) → 607e2a5(T2) → a0293b6(fix r1) → f22dbe6(fix r2) → ecf0508(T3 probe)**+본 docs 커밋 | N | 1행 등록 — 엔진 캐논 무접촉(APK 별도 dir·별도 gradle 빌드)·본 라인 Win selftest 922 PASS/0 FAIL 2회 재실측(T1/§7·T2/§6) — 캐논 4축 등호 승계 |
| T2 원장 | **probe prescan rc 전파 결함**(구 MSW_LIST의 Minesweeper 미매치 rc=1이 prescan FAIL로 전파 — 부제 세션을 agentctl 부재와 동일 FAIL) — 2런 연속 재현 후 T3 소각 | M | 함정 원장(§7.17.3 1행) — probe v2 census/필터 분리(ecf0508366ad7… — 2런 FULL 등호로 폐곡) |
| T2 원장 | **stdout 버퍼 후행 순서 함정**(파이프 결합 시 stderr 원문이 stdout 마지막 라인 뒤로 밀림 — fail-closed B/C stderr 원문 묻힘) | M | stderr 파일 분리 수취 확정판 계약(§7.17.2 ③ — probe ⑧ 세그먼트 승계) |
| T2 원장 | 폰 재빌드 경합(런 중 타 세션의 폰 ninja 재빌드 — jkdesktop 사망 관측·loadavg 10.5 실측) — probe 2런 BLOCKED → 서비 복귀 폴링 후 완수 | M | 타 세션 빌드 접촉 존중 원존(동시 빌드 경합 위험 — §4.1-11 원장행 승계)·정직 BLOCKED 원장 2건 봉인(phone_apk_run1.log 이력) |
| T2 원장 | 폰 네트워크 주소 이동(known_hosts 상주 후보 중 1곳만 생존 — 셀프판명 원존 재판명)·scp 덮어쓰기의 +x 회수(chmod 원존 필요·r3-r5 원장) | N | phone_voice.sh 셀프판명 원존 승계·probe ⑧ chmod 래치 |
| T1 원장 | **T1 리포트 §4 링크 표기 정정**(`-static-libstdc++`는 C 드라이버에서 C++ runtime을 링크하지 않음 — 17심볼 undefined 실측; T1 이진 성립의 실제 링크 = `-lm -lc++_static -lc++abi`) | M | T2 리포트 §9.2 정정 원장 — §7.17.4 ② |



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

### §5.1 EYES 결제 행 규약 (#113 D4 — 형식만 규약 · 2026-10-26) · **사용자 선언만 결제 원칙 불변**

위 §5 표(그리고 이 뒤의 §7.x EYES 표 전부)의 결제 상태 행은 #113 D4 형식을
쓴다 — **결제를 성립시키는 것은 여전히 사용자 선언뿐이고, 이 규약은 기록의
형식만 고정한다**(probe·본 문서는 결제를 기록하지 않는다 — §5 원칙 그대로):

> `EYES-<라인/항목>: <PASS|FAIL|PENDING> — <사용자 원문 인용 1줄>`

- `PASS|FAIL|PENDING` = 결제 상태 3치 뿐 — probe 영수증(rc=0·VERDICT 행)은
  이 상태가 될 수 없다(수치 영수증 ≠ 육안/청안 선언). `PENDING` = 선언 미접수.
- 사용자 원문 인용은 **1줄·각주 없이 그대로** — 요약·의역·조합 금지(원문 정직
  문화 — docs/90 §5 ⑫ "파일을 찾을 수 없습니다" 표기 선례).
- 이 폼으로 회수 가능한 최근 사례(예시 원장행 — 출처 원존·상태는 출처 표기 그대로):

| 예시 행 | 출처 원문 |
| --- | --- |
| `EYES-#97 폰 위임 재생: PASS — "재생 잘 되네요"` | 2026-10-11 사용자 선언 — 기억창고 갱신 57/59 원문(폰 폴더 브라우저 결과 결제) |
| `EYES-#94 한글 트랙 재생: FAIL — "파일을 찾을 수 없습니다"` | 사용자 보고 — docs/90 §5 ⑫ 원문(수형 vplayer fs 게이트+Utf8ToAnsi 후 재시도 = ⑫ 게이트 대기) |
| `EYES-#97 폰 Music 런처 아이콘: FAIL — "폰 화면에 뮤직 안 보임"` | 사용자 보고 — docs/90 §5 ⑬ 원문(#95 그리드 개방 T4에서 Music 아이콘 등장 실측 소각 — §7.8.2) |
| `EYES-#114 APK 홈 창 실행: PENDING — "지뢰찾기 창은 실행됨"` | 2026-10-25 사용자 실측 보고 — docs/92 §7 ① 원장행(**원장행 ≠ 결제** — 확정 선언 대기) |

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

## §7.8 폴더 브라우저 원장 (#97 라인 — T1-T4, 2026-10-11)

뮤직 폴더 브라우저 라인(#97) 원장 — 사용자 발단 "파일 다이알로그로 폴더 선택
해야 하지 않아요?"(2026-10-11)의 브라우즈 백로그 정산 = **내장 미니 브라우저**
(타이핑 소거 — 폰 키보드 회피가 동기: 폰에서 타 트리 위치의 폴더를
타이핑 없이 [추가]하는 것이 [이 폴더 추가]의 존재 이유). 플랜=
`docs/superpowers/plans/2026-10-11-music-dir-browser.md`(6ad3834 — 결정 원장
5건=재량(§7.8.4)·사용자 게이트=폰/Win 브라우저 체감(§7.8.5)), SDD 원장=
`.superpowers/sdd/2026-10-11-music-dir-browser/`(progress·task-1..4 리포트
7종(fix 3종 포함)·review 4종+재검 별도 2종(T1 fix r1 재검만 task-1-review에
동봉 — T5 리뷰 I-3 정정 표기)·review diff 7종·런 로그
engine/tmp/wsl_music_t3_run{1,2,C,C2,C3,D,E,F}.log·phone_music_run{1..4}.log —
untracked **인용만이 진실원**). 라인 부품 계승: #92 폴더 저장소
(MusicDirStore·DirAddManaged 가드 2검·재스캔 3연)·#93 취소 기구(스캔은
DirAddManaged 기존 계약 소관)·#94 경로 인코딩 규약(와이어/저장/표기=UTF-8).
T5는 본 §7.8 — docs 전용(engine/probe 무접촉)이라 selftest 재실행 불요(§0 부기
원칙 — 건네받은 수치 Win 671/WSL 648/posix 296/폰 648 그대로 봉합·`-F`
고정문자열 계수 룰링 승계). 서브에이전트 스폰 0건(T1-T4 각 리포트 선언·T5
실측)·worktree 미사용·main 직행. T4 리뷰는 **APPROVE(C0/M2/I2)** verdict로
폐곡(commit 2999f3d) — spd 병렬 룰링 수형(T5 병렬 dispatch 마지막 사이클 —
verdict는 본 T5 커밋 전 흡수).

### §7.8.0 체인 (rev-parse 전체 SHA 실측 — T5 측정 · git log가 진실원)

표 행 순서 = git log 리니어 사슬 1:1(park 배치 4 정정 — 구판은 T3 행이
T2 fix r1/r2 뒤에 놓인 "논리 그룹 순서"였다 — T5 리뷰 M-1).

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 |
| --- | --- | --- |
| 플랜(라인 BASE) | `6ad38344d0b106123a1c035a7b0535fb696d9ea1` | docs(superpowers): music 폴더 브라우저 플랜 T1-T5 (#97) |
| T1 | `04e11b78cd672662713d6eddffe324054b69ff3a` | feat(apps): music 폴더 브라우저 순수 부품+2r (T1) — MusicModel.h `jk::music::browse`(ListSubdirs/JoinDir/Parent)+main.cpp 2r 8건 (2파일 +152/−0) |
| T2 | `8e1d84c55039da6a05af915ec3d9b3d45560aede` | feat(apps): music 폴더 브라우저 패널 — 타이핑 없는 선택 (T2) — ClientMusicApp .h/.cpp 2파일 +127/−0 — [찾아보기] 토글+cwd 표기+.. 행+[이 폴더 추가]=DirAddManaged+성공=브라우즈 종료+[닫기] |
| T1 fix r1 | `c6be5003dc62f49b9573fe5be0dd97f769877a9e` | fix(apps): JoinDir 루트 폼 — 이중 슬래시·posix 흡수 수리 (T1 fix r1) — MusicModel.h JoinDir 본문(main.cpp 2r-b/c 회귀 2건 동봉 — 2파일 +35/−8) |
| T3 | `e68ff4615472cfe98822382a25eabf98cfc95827` | test(probes): music WSL probe — 브라우저 실측 세그먼트 (T3) — wsl_music.sh probe 1파일 (+508/−68) — ②③④④2 2런 등호·tapsettle 신설·⑤ 무전달 실측(T2 fix r1 dispatch 근거) |
| T2 fix r1 | `05bd36888508257ce604e1444adba030827d067f` | fix(apps): 브라우저 압착 가드 — 이 폴더 추가 도달 복원 (T2 fix r1) — 2파일 +182/−72(numstat — ClientMusicApp.cpp +114/−39·wsl_music.sh +68/−33) — 압착 가드 2검+⑤ 행 목록 위 이동+⑤ 재검 세그먼트(tap timeout 12→35) |
| T2 fix r2 | `d5c6875048e24dec6350c96d01e012e8a6bc11d9` | fix(apps): 브라우저 거부 경로 cwd 소각 복원 (T2 fix r2) — 2파일 +9/−4(cpp +5/−1·h +4/−3) — 거부 분기 browseCwd_.clear()+`!deny.empty()` 확장+h주석 부하성 정정 |
| T4 | `2999f3df7e2d160fe8af14a75488f330ed0072ec` | test(probes): music 폰 probe — 브라우저 동형 세그먼트 (T4) — phone_music.sh probe 1파일 (+835/−81·코드 무변경) — **폰 캐논 638→648(2r 10 — 4런 등호)**·XTEST(ztap) 재조립 |
| T5 | 본 문서를 포함하는 커밋 | docs(apps): music 폴더 브라우저 as-built 갱신 (T5) — SHA는 git log가 진실원 |

폐곡 계보: T1 리뷰 **APPROVE(조건부 fix r1 — C0/M2/I2)**(격리 워크트리 재빌드
669 등호·부자기 함정 실측 재확정 pp(C:/)=C:/·M1 "I://"·M2 posix 루트 흡수) →
T1 fix r1 재검 **APPROVE(C0/M0/I2)**(14폼 프로브 실측 — `JD(I://,X)=I:/X`
입력 폼 정규화 강화 보너스·2r 10행 PASS — **Task 1 complete**) → T2 리뷰
**APPROVE(C0/M1/I3)**(idle 계약 M1 — 부하성 문어·상위 탐색 재량 승인·행 사본
수형 원천 0) → T2 fix r1 재검 **APPROVED(조건부 — C0/M2/I3)**(⑤ DELIVERED
판정+diag2 분절 성립+압착 가드 2검 ImMax 원문 검산 — **M1 거부 경로 browseCwd_
미소각 신규 회귀 → fix r2 dispatch**) → T2 fix r2 재검 **APPROVED(C0/M0/I2)**
(거부 계약 4단계 시나리오 실측·격리 재빌드 671 등호·무변조 확정 — **Task 2
complete (commits 8e1d84c·05bd368·d5c6875)**) → T3 리뷰 **APPROVE(C0/M4/I4)**(2런 등호
·WSL 648 **7회** 실측(리포트 5회 과소 — I-1)·⑤ 서사 판정 — 기하 서사 반증·
층위 불확정 지정(T2 fix r1에 판별 실험 동봉 요구 — 이행됨)) → T4 리뷰
**APPROVE(C0/M2/I2)**(폰 캐논 648 receipt 런 4회 원문 전수+
`CANON-INCLUSION=MUSIC-FULL-2P2QR-10` 삼원 일치·WSL 등호 3로그 교차·⑤
DELIVERED but cwd=루트 no-op의 등급 분판 정확·SWEEP-DIFF 8/11 = **앵커
stale 판정(기대치 산식 미갱신 — 실물 누락 아님 — 근거 4건)** — M-1·M-2는
§4.1-3·라인 코드 축 폐곡).

### §7.8.1 배선·원리 원장 (원류 원문 1:1 수취 — 리포트/review 근거 · 코드 재실측 0)

- **DirBrowserModel 순수 부품(T1 — `namespace jk::music::browse`)** —
  ListSubdirs(ec 중립형·directory_iterator ec 실패 = 독립 빈 페이지·
  is_directory(ec) 폴더 전용(파일·".." 배제)·숨김 = 선단 '.' 스킵(LibraryCatalog
  수형)·byte asc `std::sort` — char_traits 비교 = 로캘·ABI 무관 **진짜
  결정론**)·JoinDir(Slashize 재용(0x5C 고바이트 규약 승계)+말단 구분자 접기
  →무조건 1슬래시 결합(드라이브 루트 "C:"도 "C:/name" 성립 — fix r1 근원
  조건행 삭제)+posix 절대 루트 보존 — 접기 **이전** `front()=='/'` 절대성
  표지 수형)·Parent(문자 스캔 1단 제거 — 루트/빈값/단일 성분 = 빈 문자열
  (**부자기 금지** — fs::parent_path 루트 자기 반환 함정 회피))·"/a"="/"
  ·드라이브 루트 표기 유지(I:/music→I:/)). **루트 폼 3함정 원장 — 전부 fix
  r1 소각**:
  ①fs::path::parent_path 루트 자기 반환 실측(pp(C:/)=C:/·pp(/)=/·pp(C:)=C:)
  — 문자 스캔 승계의 근거 ②드라이브-상대 "C:name" 함정(ListSubdirs("C:")가
  드라이브 현재 디렉터를 건드림 — MusicModel.h 자기 문서화) ③말단 접기가 "/"
  를 소각하는 posix 루트 흡수(JoinDir("/","x")="x" — 상대 조립 = 프로세스 cwd
  기준 오판). 폰 상위 사다리(/sdcard/Music→…→/→"") 전체가 이 규약 위에서
  성립(리뷰 6폼 실측).
- **브라우저 패널(T2 — [폴더 관리] 패널의 [찾아보기] 토글)** — 개봉 cwd =
  `dirs_[dirIndex_]`(현재 탭 루트)+`Utf8ToUtf16` 가드(RequestScan 동형
  fail-closed — CP949 레거시 루트·광 드라이브 "C:"(fix r1 I3) 개봉 거부)·
  cwd 표기 1행(TextUnformatted — 툴힌트 전문 재소유)·상위 ".." 행(Parent
  빈값 = 루트 → `(루트 — 상위 없음)` TextDisabled — 부자기 금지의 UI측 소비)
  ·폴더 행(Selectable + **행 사본 선취 P1 수형 재용** — 클릭 핸들+툴힌트
  전부 사본 — ListSubdirs는 프레임 로컬 재구성이라 UAF 원천 0건 리뷰 전수)
  ·**[이 폴더 추가] = DirAddManaged 단일 경로**(UI [추가]·music_dir_add 도구와
  동일 — 신설 중복 경로 0건): 가드 2검→Store.AddDir→성공 =
  RefreshUserDirs+ResolveDirs+RequestScan(재스캔 3연 원존)+**브라우즈 종료**
  (browseOpen_=false+browseCwd_ 소각)·실패 = status 정직 1행 브라우저 잔류.
  **거부 경로 계약(T2 fix r2 복원 — fix r1 가드 개편의 회귀 소각)**: 거부
  판정을 `browseCwd_.empty() || !deny.empty()`로 확장+거부 시
  `browseCwd_.clear()` — 개봉→토글 폐쇄(stale cwd 잔존)→deny 탭 전환→재개봉
  시퀀스의 stale cwd 개봉+거부 무음을 소각(개봉 거부 = 브라우저는 닫힌 채+
  거부 사유 1행 — 합격 분기 무변경).
- **부하성 더티 계약(1곳)** — 브라우저 고유 신설 frameDirty = **개봉 거부
  사유 표기의 부하성 봉인 1곳**(h주석 원문 문구 — T2 fix r2 M2 정정) —
  status_는 BuildUi **선두** 상태행에 그려지므로 거부 클릭 프레임의 선두 행은
  이전 status를 이미 그었다(동선: 거부는 폴더 패널 블록에서 일어난다) —
  frameDirty는 다음 프레임 선두 행 표기를 유발하는 하중(**1프레임 지연 표기**
  — 더티가 없으면 #89 idle 게이트에서 거부 사유는 영영 미표기 — T2 리뷰 M1
  원문). 탐색·개봉·닫기·행 클릭 = 입력 이벤트 자연 귀결·[이 폴더 추가]의
  더티는 DirAddManaged 기존 계약(성공/실패 status 표기) — **브라우저 고유
  신설 더티는 1곳뿐**(idle 계약 docs/88 무변조 — 리뷰 3회 전수).
- **★무전달 지대 소각 원장(T3 발견 → T2 fix r1 수리 + diag2 분절)** — T3:
  [이 폴더 추가] 행(구 y 442-457 — 클라 흐름 최하단 row·tracks child의 -200
  부근)의 합성 클릭 = 런 5회×110 후보 전부 무전달(ok:true,sent:true 답신 =
  **전송 성립만** — 클라 [cpustat] frames=0까지 직결) → **기하 서사
  (negative-height 압착 = 클릭 불가)는 리뷰 뷰잉으로 기각**(sub2 캡처 실측 —
  버튼 rect(11-73, 439-461) = 클릭 그리드 내부·압착은 버튼 아래 tracks/spatial
  에만·드로우 순서상 버튼이 앞선 항목이라 압착과 클릭 불가의 인과는 불성립)
  → **단절 서사 채택 후 diag2로 층위 분절 성립**: 동일 v1 바이너리·
  JK_CPU_TRACE+allow의 제어 클릭(폴더 관리 토글 그리드) = relay 12행+클라 수취+
  input 1-5·frames 1-4 원문 — **수취·처리 양존 = "클라 무죄 — agentctl 응답
  회수 지연(~33s > tap timeout 12)의 IPC 응답지연" 귀결** → **"수취 0 vs
  처리 0" 층위 해소**(T3 리뷰의 미확정 지점). 수리 = ①압착 가드(tracks
  1검+spatial 2차) ②⑤ 행을 목록 위 버튼행으로 이동(도달성 원장 — 하위 행
  성립 축·런 E/F 전달 원문 y≈294-350 — 무전달 지대 회피·계약 존재 순서
  유지·공간 순서만 교체) → 재검 **BROWSE-OK stage 8/8**+⑤ 종료 전이 착탄
  (DELIVERED — diff 43)+DirAddManaged 발화("폴더 추가됨") 2회 원문 —
  **T3 concern 1 무전달 소각**. 레슨: **합성 입력의
  무전달은 좌표 지역 결함을 먼저 의심하지 마라 — relay/수취/처리 3층을
  컨트롤 클릭 1발로 분절하라**(§7.7.3 #1 "tap ok ≠ 착탄"의 클라 측 계승)·
  표기는 "제어 클릭 축 실측"임을 명시(구 ⑤ 지대 자체의 직접 재판정은 fix r1
  행 이동으로 지대 소멸 — §4.1-3).
- **압착 가드 2검(T2 fix r1 — ImGui 원문 근거 리뷰 실측)** — tracks child:
  고정 `(0,-200)` → **reserve = clamp(남는 높이−표 최소 56px, 4, 200)**(여유
  상태 = -200 원존과 완전 동형 — 동일 측정점·폐쇄 부작용 0)·spatial child:
  (0,0) 잔여 의미 → 명시 `max(4, 잔여)`(압착에서만 4px 봉합 — 음수 잔여
  child 소각). ImGui 원문(third_party imgui 1.92.9b 리뷰 검산): `CalcItemSize`
  y<0 → `ImMax(4.0f, avail + size.y)`(imgui.cpp:11815)·BeginChild (0,0) →
  default_h = `GetContentRegionAvail()`(:6574).

### §7.8.2 캐논 계보 (2r 계열) + 브라우저 실측 영수증

| 커밋 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#94 종착 = §7.7.2 T5 행) | 661 | 638 | 296 | 638 | — |
| T1 `04e11b7` | **669** | **646** | 296 | 638(흡수 전) | 2r 8 |
| T2 `8e1d84c` | 669 | 646 | 296 | 638(흡수 전) | 0 (UI 전용 — 등호 런·신설 어설션 0 계약) |
| T1 fix r1 `c6be500` | **671** | **648** | 296 | 638(흡수 전) | 2r-b/c 2 (+2) |
| T2 fix r1 `05bd368` | 671 | 648 | 296 | 638(흡수 전) | 0 (3축 등호 — 런 3 `MUS-BROWSE-CANON-EQ: OK` 원문) |
| T2 fix r2 `d5c6875` | 671 | 648 | 296 | 638(흡수 전) | 0 (3축 등호) |
| T3 `e68ff46` | (승계 등호) | 648 (런 등호 — 7회) | 296 | 638(흡수 전) | 0 (probe 전용 — 코드 무접촉) |
| T4 `2999f3d` | (승계 등호) | 648 | 296 | **648** | 폰 2r 10건 흡수(638→648 — 2r-a..⑦+2r-b/c — **WSL 완전 등호·receipt 런 4회(r1-r4) 등호** — `CANON-INCLUSION=MUSIC-FULL-2P2QR-10`·2p=7 2q=7 2r=10 2m=26 2n=17 2o=16 리뷰 원문) |
| T5 본 커밋 | 671 | 648 | 296 | 648 | — (docs 전용 — 재실행 불요) |

- **posix 296 등호 = 하네스 TU 미링크 구조 등호**(§2.1/§7.2 원리 승계) — 2r
  전원이 engine/src/main.cpp 소속이고 tools/posix_selftest의 build.sh 65행은
  harness 자 own main.cpp만 링크(T1 리뷰 구조 입증 — 전 커밋 무접촉).
- **2r 10의 분해** = 2r-a 임시 트리 구성(setup — %TEMP%\jk_music_browse ·
  REMNANT 0 사후 판정선)+①ListSubdirs 폴더 3건 오름차순(파일·숨김·상위 행 0)
  ②JoinDir 등치+1슬래시 결합 ③Parent 중간 왕복 등호 ④Parent 루트 표기 유지
  (I:/music→I:/+posix /a→/) ⑤Parent 루트의 부모 = 빈 문자열(부자기 3폼)·
  ⑥없는 cwd = 빈 페이지(ec 중립·crash 0) ⑦사후 소각 REMNANT 0 + **2r-b
  `JD(C:/,sub)=C:/sub`(I:// 회귀 봉인)·2r-c `JD(/,sub)=/sub`(루트 상대 흡수
  봉인)** — 캐논 산수 661+8=669+2=671·638+8=646+2=648(리뷰 격리 재실측 원문).
- **WSL 브라우저 실측(T3 — 런 E/F 최종 코드 2회 등호)** — tapsettle 착탄
  원문(도달 시차 4-5s): tab (158,76)→[폴더 관리] (220,70)→[찾아보기] (44,262)
  →하위 진입 (86,324)→상위 ".." (24,294)→재진입 (64,324) — ②③④④2 체인을
  상태 사슬로 재검증(차분 게이트·토글 안전 원장). ⑤ = 무전달 실측(110 후보
  0 등록 — 도구 진실원 poll 25/25 `["mus_seed","mus_browse"]` 변동 0 — 위
  §7.8.1 소각 원장 근거).
- **폰 브라우저 실측(T4 — XTEST(ztap 재조립 · 삭제 금지 계약) 수취 프레임
  원문)** — ② mus_browse 탭 활성 전환·②2 [폴더 관리] 패널 개봉·②3 브라우저
  개봉+cwd 표기(/data/.../mus_browse)+".."+[이 폴더 추가|닫기]+sub_a/sub_b
  하위 목록 렌더 = 수취 프레임 원문 — **T5-era 폰 스트립 행 Button 무응답
  관측(#96 P3 원장행)이 X 수준 주입에서 소각**(X 주입 경로는 성립 — #96 수형의
  정직 영수증). MUS-PHONE-OK(r1·r3·r4)·브라우저 stage 8/8·restore=1·hard=0.
  배치 전 원문: SWEEP-DIFF=8/319 → tar blob 배포 → SWEEP-POST-DIFF=0
  DEPLOY-FRESHNESS-OK → ninja 264/264 aarch64 NINJA-RC=0. ③④⑤ 자동 탭 체인
  = 미성립 원장행(§4.1-3 — 캡처·판정 본안은 원문 성립).
- **probe 원복 원문(T4)** — settings 163B(사용자 music.dirs 보존)·permissions
  33B 바이트 등호(REMNANT 게이트 전부 OK)·브라우즈 트리·시드·정찰 캡처 소각 0
  ·주입기 보존(삭제 금지 계약)·**사용자 화면 = Music 창 focused 복원**
  (캡처 mus_phone_final_restore.png md5 `2cbb595660ce43b02ef20f7d0a76261c`)
  ·jkweb 생존 1. T3 측: ENTRY 가드가 사용자 settings 원문을 발견해 hard FAIL
  중단(올바른 중단 — probe 소유 아님 계약)→브래킷 이동-런-원복(RESTORE-BYTE
  OK·바이트 등호 실측 — T3 리뷰 복귀 확인·사본 기록 부기 §4.1-3).

### §7.8.3 함정 원장 (#97 신규 — 봉쇄 레슨 축)

1. **★Win32가 "I://"를 무사히 접는다 — 셀프테스트는 영영 미검출(T1 리뷰 M1)**
   — JoinDir("I:/","x")="I://x" 실측(무조건 1슬래시가 드라이브 루트 조건행과
   겹침 — 3행 안의 결함)인데 **directory_iterator("I://") ec=0·열거 39건 실측**
   — Win32 경로 정규화가 이중 슬래시를 흡수해 기능은 우발 우회되고 2r-②는
   성분 2+ 경로만 검증해 **검출 구조적으로 불가**(셀프테스트 체계가 폼을
   접어버리는 함정 — "테스트가 통과 = 폼 계약 성립"은 등호가 아니다). UI
   표기 드리프트("I://" 표시)·규약(#94) 위반이 유일 피해 — fix r1에서
   조건행 삭제(근원 소각)+입력 폼 자체 정규화 강화("I://"·"C://" → 1슬래시 폼)
   ·2r-b 봉인. **렛슨: 플랫폼이 관대한 폼은 결함을 은폐한다 — 표기 계약은
   셀프테스트 밖에서(프루브 1-오프 실측으로) 검증돼야 한다.**
2. **★fs::path::parent_path = 루트 자기 반환(T1 부자기 3폼)** — `pp(C:/)=C:/`
   ·`pp(/)=/`·`pp(C:)=C:` 실측(리뷰 미니 프루브) — 루트에서 Parent가 자기
   자신을 돌려주면 ".." 행이 영원히 사라지지 않는 무한 사다리. 문자 스캔
   승계(루트/빈값/단일 성분 = 빈 문자열)가 규약 — UI 측은 빈값을
   `(루트 — 상위 없음)` 행으로 소비(부자기 금지의 이층 봉합).
3. **★WSLg 9p 링크 잠금 = ENOENT 착시 + ld 실패 시 출력 소실 축 비대칭(T1
   리뷰 입증·T1 원장)** — buildwsl 재링크가
   `cannot open output file jkdesktop: No such file or directory`를 4회
   반복 — 진원 = WSLg 데스크톱 3 인스턴스(--server·taskbar·music.jkx)가 구
   바이너리를 물어 9p에서의 링크 잠금이 **ENOENT로 표시된다**. **Windows의
   ld는 실패 시 기존 출력 파일을 유지하고 WSL의 ld는 삭제한다**(축 비대칭 원장
   — 동일 잠금이 Win에선 stale 바이너리 유지로 겉보기 소멸) — 재시도 반복하면
   BASE 바이너리가 실제로 소실. 수리 = `kill -9` 3 프로세스 후 재링크 rc=0.
   **렛슨: 동명 잠금 재발 시 이름 착시(touch jkdesktop = ENOENT·인접 이름은
   생성 성공)를 잠금으로 먼저 판정할 것.**
4. **jkagentd(PID 19164) stale 원장행** — 데몬 생존 중 jkagentd 링크만
   Permission denied(jkdesktop·jkwinserver·jkapp_music 링크 rc=0 — 캐논과
   무관·main.cpp TU 소속) — **disk 바이너리는 2r 미반영 stale 유지(Windows
   ld가 구파일 보존 — #3의 Win 축 행동)**. T1→T2→T1 fix r1 3세대 미수수·
   taskkill 후 재링크는 차기 축 소관(T4 리포트 축 폰 배포는 무관 — probe
   자체 빌드).
5. **★프레젠트 지연 2-8s(간헐 수십 s)+캡처 = 클릭-전 프레임 수수(T3 원장
   수치 첨부)** — 클릭의 UI 전이는 컴포지트 프레젠트로 2-8s 지연·간헐 수십 s
   (런 1-3 라이브 정찰 실측). tapsettle의 **8s 무음 판정 = 이 지연 하에서
   false-FAIL이 false-SUCCESS보다 안전**(토글형 버튼의 재시도가 역전을
   만들 수 있어 — 상태 사슬이 최종 판정을 소유). 폰 축은 렌더 지연 ~10s로
   증폭(§4.1-3 — ③⑤ 자동 체인 미성립의 3중 드리프트). 렛슨: **개봉 캡처를
   판정 근거로 쓸 땐 "그 프레임이 어떤 시점의 것인지"를 먼저 증명하라 —
   캡처 시각이 아니라 캡처된 시점이 진실원이다.**
6. **★XTEST 클라 커서 잔상(T4 — 원장 4건 중 ② · 5건째 리뷰 M-2 보정)** —
   XTEST 커서 스프라이트 = 클라가 그린 io.MousePos 잔상(서버는 창 밖 park
   모션을 swallow해 클라 포인터가 탭 지점에 영주) — 행 임계 8px 폭의
   스프라이트가 차분에 혼입(런 1 — 140px 커서뿐 원문). 나머지: ffmpeg PNG
   Sub 필터 누적(제한 사각 복호는 전위를 풀어야 — prev 계단식 오염)·
   merge_dirs_json 라벨 누설(가짜 dirs 기록 — shift 수형)·1-behind 프레임
   (전 단계 탭의 늦도착 렌더 — settle 뒤 +6s 유출 캡처 수형)·**런 2 remove
   서기 지연 폴링(N4-10 CLEAN-POLL — 5번째 수형 — restore=0 BROWSE-FAIL
   honest-fail 라벨의 원인·폴링 수형으로 런 3/4 복구)** — 전부 T4 커밋 본문
   +리뷰 원문 봉인. **런 1 실측 오염이 영수증에 남고 수형이 커밋 본문에
   봉인되는 재사용성 구조(MUSP_BROW_* env 16종·bc_*.png 유산 보존) 원장.**
7. **★SWEEP-DIFF 기대치 = 배포 계보와 함께 갱신 계약(T4 — 8/319 vs 기대 11 —
   리뷰 판정 = 앵커 stale)** — 배포 전 SWEEP-DIFF=8·tar blob 배포 뒤
   **SWEEP-POST-DIFF=0 4런 전부**(HEAD 319파일 전수 사이즈 등호 배포 완료)·
   ninja 264/264 aarch64. 리뷰 판정(근거 4건): 기대 집합 11의 앵커
   `PHONE_DEPLOY_BASE=ddd0f45`가 폰 실제 트리보다 오래된 것 — 폰은 638 라인
   (2026-10-10 런) 배포로 3파일이 이미 최신·`git diff --name-only ddd0f45
   2999f3d -- engine/src engine/include engine/CMakeLists.txt` = 11파일 전부
   blob 사이즈 변동 재현(bare 명령 = 18파일 — docs 3+probe sh 4+코드 11 —
   #94 리뷰 "인용-명령 일치 원칙" 재발 경로 — 경로자 보강 — T5 리뷰 I-1)·
   배포 마커
   `main_2r=10 · DelegationReplyVerdict=2 · cancel_wiring=6`로 #97 코드
   실배포 실측 — **실물 누락 아님**. **렛슨: 사이즈-등호 3파일은 콘텐츠 원문이
   아닌 사이즈 추정 의존 — 앵커 미갱신 기간의 blob 사이즈 등호는 콘텐츠
   변경을 블라인드한다(clear post-sweep에도 미검출)** — 차기 런 전
   PHONE_DEPLOY_BASE를 실배포 HEAD로 정산(§4.1-3). 또 T4는 probe 병합 수형
   교환(진입 settings의 music.dirs = 사용자 원문 163B — probe dirs 선행
   병합·워터마크 hard FAIL 폐곡)을 원장동봉(§4.1-3).

### §7.8.4 결정 원장 (재량 — 기각 시 즉시 수리)

1. **상위 탐색 = 등록 루트 위로도 허용(T2 재량 — 리뷰 승인)** — 근거: T1이
   Parent의 드라이브 루트 표기 유지를 `".." 행 후 스캔이 다시 성립하는
   표기`로 명시(2r-④ 원문)+브라우저의 존재 동기(폰 타이핑 불요 — 봉쇄 시
   [이 폴더 추가]의 유용성 소멸). 상한 = 부자기 금지(루트에서 ".." 소멸 —
   전역 루트 밖 미출가)·안전은 [이 폴더 추가] 가드 2검이 소유·툴힌트가 정직
   공개 — UX 봉쇄 아님.
2. **브라우저 = 선택기·스캔 드라이버 아님(T2)** — 시작점 = 현재 탭 루트·
   스캔 = RequestScan tab-scoped 기존 계약 그대로·[이 폴더 추가] 성공만
   DirAddManaged 기존 재스캔 3연 — 폰 탭 단위 스캔 계약 유지(별도 마운트·
   파일 다이알로그 없음).
3. **거부 = 치유+정직 표기, 무음 금지(T2 fix r2)** — 거부 시 브라우저는
   닫힌 채(cwd 소각)+거부 사유 1행(부하성 = 다음 프레임 선두 표기) — v1의
   거부 보증을 가드 개편 창에서 유실하지 않도록 복원(§7.8.1).
4. **probe 무전달 3단 판정(T2 fix r1 수형)** — 전달성(FULL = 등록 원문)/
   DELIVERED(전달 성립+등록 미출현 = no-op 원장행)/무전달 = hard rc FAIL —
   등록 강도와 전달 강도를 분리 표기하는 계약(T2/T4 런 원문 준수).

### §7.8.5 EYES 게이트 (사용자 선언만 결제 — probe 영수증은 결제 아님)

**EYES-PENDING — 본 라인 결제는 사용자 육안/체감 선언만 성립(§5 원칙 동형 —
§5 본문 표 무변경·게이트는 본 소관 표로 병기). 본 라인의 결제 상태 = 미결제
(대기).**

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **폰 브라우저 체감 — 라인 최종 게이트**(플랜 사용자 게이트) | 폰: music 탭 스트립 진입→[폴더 관리]→[찾아보기]→cwd 표기+폴더 행+[이 폴더 추가]→**추가 성립 → 재스캔 전 사이클** — 타이핑 0 폰 체감. XTEST 수취 원문(T4 — §7.8.2)은 성립·⑤ 등록 강도 FULL은 §4.1-3(라이브 캘리브레이션 몫) | **대기(육안·체감)** |
| ② | **Win 브라우저 체감** | Win: 동일 체인+[찾아보기 ▸/▾] 토글 모양+cwd 표기 1행+폴더 행 모양 | **대기(육안)** |
| ③ | **압착 모양** | 동시 개봉 시 표 영역 압착 잔여+압착 시 spatial 4px 띠(§4.1-3) — 가드 2검 후 체감 판정 | **대기(육안)** |
| ④ | **posix 루트 사다리 육안** | T1 fix r1이 바꾼 유일 실동작 경로 — ".."로 "/"까지 올라 뒤 폴더 클릭(T1 fix r1 재검 concern 수취 권장 원문) | **대기(육안)** |
| ⑤ | **거부 경로 수동 시연** | 거부(CP949 레거시·광 "C:") 개봉 → 브라우저 닫힌 채 유지+거부 사유 1행(1프레임 지연 표기 — §7.8.1 부하성) 체감 | **대기(육안)** |

### §7.8.6 커밋 원장 + IP/호스트 경로 게이트 (#97 라인)

- 라인 체인 = **6ad3834(플랜)..2999f3d**+본 T5 커밋(§7.8.0 표 = rev-parse
  전체 SHA — git log가 진실원) — push 전부 origin main 직행·amend 없음.
  리포트 계수는 **numstat 기준 표기**(T2 fix r1 재검 I3 룰링 — 리포트 원문의
  "+153/−28/+97/−42"는 부정합·본 문서 표는 실측 +114/−39·+68/−33 등호).
- **IP·호스트 경로 리터럴 게이트(T5 실측)** — 본 커밋 원문에서
  `\b([0-9]{1,3}\.){3}[0-9]{1,3}\b` grep = **0건**·호스트 경로 표기(사용자
  루트 폴더명·/mnt 계열) grep = **0건**(커밋 전 staged/`git show HEAD`
  게이트 실행 — §6/§7.6 동형 문구). 라인 이전 커밋도 리포트/review 전원
  IP grep 0건 실측(T1·T1 fix r1·T2·T2 fix r1·T2 fix r2·T3·T4 리포트 원문 —
  §7.8.0 계보).
- **본 T5 커밋은 docs 1파일(`docs/90_music_spatial_leg_asbuilt.md` — §7.8
  신설+§4.1-3/§0 부기 추가만)로 한정** — git add 명시 경로만(#83 사건 레슨)
  ·engine/probe 소스 무접촉. probe 런 로그(wsl_music_t3_run{1,2,C,C2,C3,D,E,F}
  ·phone_music_run{1..4})·캡처 13종+md5·**diag2 trace 사본
  (engine/tmp/t2f_diag2_srv.log — 83,916B·md5 `4f29566412cc3f17bf363c1ae2085e3d`
  — WSL /tmp 원본과 byte 등호·본 T5 복원 수행)**은 engine/tmp untracked —
  커밋 금지 충족.

## §7.9 music 태그 읽기 원장 (#99 라인 — T1-T4, 2026-10-11)

뮤직 태그 읽기 라인(#99) 원장 — 사용자 "남은거 해줘요"(2026-10-11) =
태그 백로그 결제 승계(스펙 원문 — 2026-10-09-music §8 "ID3/태그 백로그 별도
태스크"의 정산). **music 목록을 파일명이 아니라 곡명·아티스트·앨범으로
보여준다**(#92 D2 파일명 스캔 v1의 2단계 — "파일이 곡명을 갖는다는 게 음악의
정의라는 것 이외의 이유 없음(DRY 최소)" — 스펙 원문). 스펙=
`docs/superpowers/specs/2026-10-11-music-tags-design.md`(8315b15 — 결정 원장
6건 D1-D6 전부 재량 확정), 플랜=`docs/superpowers/plans/2026-10-11-music-tags.md`
(7e39a28), SDD 원장=`.superpowers/sdd/2026-10-11-music-tags/`(progress·
task-1..4 리포트+리뷰 — **T4 리뷰는 없음**: 배포·실측이 BLOCKED라 리포트
원문 검토만으로 BLOCKED 경로 승인(컨트롤러 ruling — progress.md 원장) —
probe 코드 병합은 폐곡이되 런 실측 원문은 다음 폰 접촉 런 몫(§7.9.3 #10)).
라인 부품 계승: **#94 UTF-8 규약**(파서 출력·Track 필드 전부 UTF-8)·
**#93 취소 경계 기구**(ScanCancelFn — 파싱이 같은 패스에 얹혀도 경계 콜백
무접촉)·**#92 도구 6종 원존**(music_dir_list 불변 — 태그를 나열하지 않는다
— 스펙 §2)·**#89 idle 계약**(파싱=워커 몫·도착 표기만 더디)·**#97 브라우저
무접촉**·정렬 v1 고정(mtime desc — D4). T5는 본 §7.9 — docs 전용(engine/probe
무접촉)이라 selftest 재실행 불요(§0 부기 원칙) — 캐논 = 전 태스크 리포트/
리뷰의 건네받은 수치(Win 691/WSL 668/posix 304/폰 638→기대 668 대기) 그대로
봉합·`-F` 고정문자열 계수 룰링 승계. 서브에이전트 스폰 0건(T1-T4 각 리포트
선언·T5 실측)·worktree 미사용·main 직행.

### §7.9.0 체인 (rev-parse 전체 SHA 실측 — T5 측정 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 |
| --- | --- | --- |
| 스펙 | `8315b15a2180bb48e66791528fbc61b218d5eb01` | docs(specs): music 태그 읽기 스펙 — own ID3v2 파서+third_party 메타 API (#99) — 결정 D1-D6(조달 0·스캔 1패스·Track 확장·열 재편·필터 확장·폰/WSL 동형) |
| 플랜(라인 BASE) | `7e39a286d1b47cc5ad9a505848c020a93f756751` | docs(superpowers): music 태그 플랜 T1-T5 (#99) |
| T1 | `77323b12215b2f35350bac1c322377bcda03dab1` | feat(apps): music 태그 순수 파서+2s (T1) — MusicModel.h `jk::music::tags` 신설(2s 8블롭 20 어설션)+2s 블록 (2파일 — numstat 499/0·281/1) — third_party 4헤더 갭 실측(§7.9.1) |
| T2 | `2d20829fe8114eb22b9f6af16bf8177259406879` | feat(apps): music 태그 스캔 통합+행 라벨 (T2) — 3파일(ClientMusicApp.h 15/4·MusicModel.h 89/13·ClientMusicApp.cpp 45/16) — Track 확장+스캔 1패스 접합+5열 재편+MatchFilter(Track) |
| T2 fix r1 | `5f50a0545bd0aa6ee7706b52a8414bb761e995b2` | fix(apps): 스캔 취소 간격 64+툴팁 문언 (T2 fix r1) — 3파일 +31/−14(numstat — 4/2·21/8·6/4) — `kScanAuxEntryCheck=64` 신설+상한 계약 주석+툴팁 문언 4곳(구현 변조 0) |
| T3 | `96d615b5bde616cba1b4eb3dcbd6ae1088d8a84d` | test(probes): music WSL probe — 태그 표기 실측 (T3) — wsl_music.sh +412/−9(probe 1파일) — **태그 3행+폴백 행+곡명 매치+openFailed 정직 영수증** |
| T4 | `cf978f8d81d71e76a7476271d8e0dbeeda68ac1c` | test(probes): music 폰 probe — 태그 동형 세그먼트 (T4, 배포 미측) — phone_music.sh +378/−39(probe 1파일·코드 무변경) — **BLOCKED 원장(배포·실측 = 다음 폰 접촉 런 — §7.9.3 #10)** |
| T5 | 본 문서를 포함하는 커밋 | docs(apps): music 태그 as-built (T5) — SHA는 git log가 진실원 |

폐곡 계보: T1 리뷰 **APPROVE**(C0/M2/I5 — 격리 worktree 재빌드(691 — 그중
2s 20행)·파서 독립 재동작(전 assertion PASS·무한루프 0)·third_party 서사
1:1(dr_mp3:5082 원문 등호)) → T2 리뷰 **APPROVE**(C0/M0/I4 — 격리 재빌드
668(2s 20행)·ScanAudioTree 전체 read 0(머리 1MB+말미 128B)·UI 원존 1:1
(baseline 대조)·MatchFilter 오버로드 무모호·numstat 등호) → T2 fix r1
re-review **APPROVE**(C0/M0/I1 — 간격 64 = 상수 1행+주석·2p 산수 양측 등호
(stride 256/64 모두 calls2==3)·문언 정정 구현 변조 0 — I1 "256" 잔존 문언
2곳은 T5 몫(§7.9.4)) → T3 리뷰 **APPROVE**(C0/M3/I3 — 캡처 육안 1:1(태그
3행+8곡 카운터)+openFailed(런 2-5)+TAG-FAIL(런 1) 정직+캐논 668 5런 회귀 0
+보호 원복 등호) → **T4 리뷰 없음 — BLOCKED**(배포·실측 미수행 — 폰
미네트워크 — 컨트롤러 BLOCKED 경로 승인: 리포트 원문 검토만·런 미실측이므로
probe 병합 코드의 실측 원문(태그 표기·cann 668 산치)은 전부 "기대 병기"
상태로 다음 폰 접촉 런에서 정산 — T4 리포트 원장 그대로).

### §7.9.1 배선 원리 (원류 원문 1:1 수취 — 리포트/review 근거 · 코드 재실측 0)

- **★조달 0 규약(D1) — third_party 메타 API 실측 원문(T1 리포트 §1+리뷰 확증
  — 헤더는 `SPATIAL_PLAYER_ROOT` env-only 공급 계약, repo 경로 기재 금지
  룰링 승계)**:
  - **dr_flac** — `drflac_open_file_with_metadata(...)`(dr_flac.h:1075)은
    메타 체인 전용 오픈 수형으로 실존하나 **재용 불가**: ①공급 경로 계약
    (env-only 소비 — audio_core 조건부 링크, 미설정 빌드 = API 부재)·②콜백
    내부 개방의 인코딩 환경(narrow CRT — Windows ACP 해석 — **I1 정정**:
    `_w` 수형 `drflac_open_file_with_metadata_w`(L1076)·
    `drmp3_init_file_with_metadata_w`(dr_mp3.h:474)이 존재해 "narrow
    개방만"은 과표현 — 부담 근거는 ①(전 수형 공통)이고 결론 불변) → FLAC
    레그 = **own `ParseFlacTags`(24BE 블록 사슬)+공용
    `ParseVorbisComments`**.
  - **dr_mp3** — `drmp3_init`의 `onMeta`(L452/3339)은 존재하나 **갭 원문
    (L5082): "Currently this only reports the raw tag data which means
    applications need to parse the data themselves"** — ID3v1/APE/ID3v2 =
    탐지·스킵+**원문 블롭 전달만**, 텍스트 프레임 파서는 갭 → mp3 레그 =
    **own ID3v2.3/2.4 파서(TPE1=TIT2 동형·v2→v1 태그 계열 폴백)**.
  - **stb_vorbis** — `stb_vorbis_get_comment(stb_vorbis*)`(L153/4317)은
    **열린 스트림을 요구**하고 `stb_vorbis_open_*`은 v2.4 코드북·setup
    헤더 체인 전체를 요구 → **합성 원문 블롭 개봉 불가 = 코멘트 단일 접근
    불가**(§7.9.3 #1) → Ogg 레그 = **own `ParseOggTags`(OggS 페이지 연쇄
    페이로드 → 최초 "\x03vorbis" 지목)**.
  - **dr_wav** — LIST INFO 메타 존재
    (`list_info_title/artist/album` = INAM/IART/IPRD — dr_wav.h:3004-3014)
    나 재용 불가(갭 ① 동일) → **"id3 " 청크 leg(own `ParseWavTags` —
    RIFF 청크 연쇄의 "id3 " 청크에 ID3v2.3 payload 재용
    — MusicModel.h:1003-1009 원문)** — 현실 WAV 태그 주류·LIST INFO =
    백로그(2s ⑩ Wav PASS — 2s 어설션은 "id3 " leg만 단정).
  조달 신규 0건(외부 라이브러리 조달 금지 — 스펙 D1 원문)·third_party
  무변경 계약(4헤더 수정 0건 — T1 리뷰 재실측).
- **파싱 원리(own 파서 — 합성 8블롭 20 어설션 계보)**: ID3v2 = 10B 헤더
  (+v2.4 syncsafe size·v2.3 BE 크기 동형 분기)+10B 프레임(TIT2/TPE1/TALB
  — TDRC/TRCK는 TagSet 3필드 제한으로 소비 안 함 — 원장행)·ID3v1 = 말미
  128B("TAG")·FLAC = fLaC+24BE 블록 사슬(VORBIS_COMMENT last=1)·Ogg =
  페이지 연쇄·WAV = "id3 " 청크. **인코딩 규약(D1·#94 승계)**: 어떤 인코딩
  이든 UTF-8로 정규화 — enc 0 = Latin-1 승격(바이트<0x80 등가·0x80-0xFF는
  2바이트 승격 — 2s-② 원문 `00 E9 FF`→`C3A9 C3BF`·**CP949 아님** —
  원장 규약)·enc 1/2 = UTF-16(`FF FE` BOM 절취+LE·enc 2 무BOM BE)·enc 3 =
  UTF-8 pass. vorbis 코멘트 값 = UTF-8 규약 통과(레거시 CP949 바이트 값은
  모지바케 — 구조상 인코딩 표지 부재 — 원장행).
- **경계 강건성(전 수형 클램프 — 리뷰 #4 원문)**: v2.4 syncsafe(0x7F 마스크
  — 0x7F7F7F7F=268435455 = 2^28-1 최대·2^28 자체는 syncsafe32로 부표현
  불가)+파산 클램프(선언을 믿지 않음 — kBlobV24Huge)·BE32
  `fLen=min(fsz, bodyN-off)`·FLAC 24BE `off+bsz>n break`·WAV csz(size_t
  64비트+guard 256/1024)·vorbis `count>16384 거절·clen>n-off break` —
  순환/재귀 0(FLAC/Ogg/WAV 사슬 off 단조 증가 — 종료 보장)·empty
  파일/부서진 fid/절단 모두 **빈 TagSet(파일명 폴백 — 정상 경로)**.
- **머리 1MB+말미 128B 컷(D2 — 리뷰 원문 대조 확증)**: `ParseTag` = ifstream
  개방 → `seekg(end)+tellg` 크기만 → **head = min(total, kTagHeadCap=1MB)
  1회 read** → v1 폴백은 `TagEmpty && isMp3 && total>=128` 조건부
  `seekg(total-128)`+**128B만** read — **전체 read 존재 0**·FLAC/Ogg/WAV도
  head 버퍼 안에만 파싱(말미 접촉 없음). 실패 = 빈 TagSet(파일명 폴백 정상
  경로·오류 원문 표기 없음)·err은 스캔 계약상 폐기 — 실패가 스캔을 죽이는
  경로 0.
- **스캔 1패스 접합(D2)** — `ScanAudioTree`의 행 조립 지점 1곳
  (`out.push_back` 직전)에서 `tags::ParseTag(tr.full, terr)` — 재파싱 0
  (재능력 재실행 금지 계약)·취소 콜백 수형·호출 수 **무변조**(diff hunk
  삽입만 — 2p-② 호출 상한 계약 유지). **순서 계약**: `tags::TagSet`+
  `ParseTag` 선언만 namespace 머리로 승격 — 파서 본체는 말단 block 원문
  (정의 위치만 이동 — 옮긴 것 없음).
- **★간격 256→64(리뷰 I-1 → fix r1 — 상한 계약)** — T2 이후 취소 응답의
  **지배항 = 파일당 머리 read**(체크 사이 갭의 시간이 파일 1건당
  개방+~1MB read+폐쇄를 갖게 됨) → `kScanAuxEntryCheck=64` 상수화(간격
  4배 축소)+**상한 계약 주석 1행**(취소 응답 지연 ∝ 간격×파일당 머리 read
  — 64×1MB read ≈ 페이지 캐시 히트 수십 ms~0.1s·경계 폭 64행 이내 봉인
  — 저속 저장소(NVMe 여유통제·콜드 HDD)의 수치 nuance는 §7.9.4 원장)·
  **2p-② 호출 수 계약 [2,3]은 래치(ListAudioFiles)가 소유 — 간격 무관**
  (양측 stride 산식 등호 원문: stride 256 → 진입1+aux@256+sub 진입3·
  stride 64 → 진입1+aux@64+aux@128(적중)·양측 calls2==3·assert
  (>=2 && <=3) 성립). 폰 저속 저장소 실측은 T4 런 몫(park — §4.1-4).
- **Track 확장(D3)+모델/표기 분리** — `Track`에 `title/artist/album` 말단
  필드(위치 초기자 계열 소비 — 2m/2p 무변조)·**원문 보존 필드**: 태그
  부재 = 빈값 그대로(모델에 "-"를 심으면 태그 유무 진실원이 죽는다) —
  폴백 표기는 **투명 leg `TrackRowLabel`(태그 있음 = "곡명 (아티스트)"·
  부재 = 파일명 확장자 제거 — fs::path::stem)**·**`TrackAlbumLabel`**
  (부재 = "-" — 단독 열 원존)의 순수 부품 소유(문자열 조립 전용 —
  파일 IO·태그 파싱·설정 접촉 0). 곡명 행 라벨의 아티스트 부재 = 괄호
  생략(합성 열 특례 재량 — "-" 단독 표기는 앨범 열이 소유 — 잡음 회피).
- **열 재편(D4)** — `BeginTable("tracks", 5)`: [곡명] [앨범] [크기] [수정]
  [spatial]. 곡명 = 첫 데이터 셀 — 더블클릭(원존 `IsItemHovered`+
  `IsMouseDoubleClicked` → PlaybackDelegate=vplayer 위임 원존 수형)·
  [spatial] 버튼/툴팁 leg 열 인덱스 이동뿐 본체 무변조·**앨범 =
  WidthStretch(곡명 2.0/앨범 1.0)** — 앨범 WidthFixed 132의 560px 창에서
  곡명 stretch 열 ~25px 압착 원장(§7.9.3 #4)을 수형으로 회피·rel 경로
  데이터는 행 툴팁으로(**툴팁 = Track.full 절대경로 — I-3 정정 문언 —
  정보량 = rel 상위집합·Track.rel은 도구/status leg가 그대로 소비**).
- **MatchFilter(D5)** — `MatchFilter(const Track&, filter)` 오버로드 신설
  (Track↔string 상호 변환 불가 — 오버로드 모호성 0 기존 string 원존 무접촉)
  — 진실원 = **`name || title || artist` OR**(앨범 미접음 — D5 스코프
  밖·파일명 매치 원존 유지)·필터 힌트 "곡명·아티스트 검색" 갱신·
  VisibleTrackCount/BuildTrackTable이 같은 오버로드 소비 — 카운터/행
  일치 유지.
- **idle 계약(#89 무변조)** — 파싱 = 워커(ScanAudioTree) 몫·UI 더티 =
  도착 1곳 무변조·행 라벨 조립은 dirty 프레임에서만 — 파싱 비용은 도착
  표기만 늦춘다(취소 체계와 별개).

### §7.9.2 캐논 계보 (2s 계열) + probe 실측 영수증

| 커밋 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#97 종착 = §7.8.2 T5 행) | 671 | 648 | 296(구 표기) | (648) | — |
| T1 `77323b1` | **691** | **668** | **300**(실측 — M2 부기) | (648) | **2s 20** (합성 8블롭 — 671+20=691 산수 독립 재현) |
| T2 `2d20829` | 691 | 668 | **304** | (648) | 0 (3축 등호 — 신설 어설션 0 계약) |
| T2 fix r1 `5f50a05` | 691 | 668 | 304 | (648) | 0 (간격 상수 1행 — 2p 산수 등호) |
| T3 `96d615b` | (승계 등호) | 668 (런 등호 — 5회) | (승계) | (648) | 0 (probe 전용 — CANON_WSL_SELFTEST 648→668 갱신+`SELFTEST-2S-CASES=20` 신설) |
| T4 `cf978f8` | (승계) | (승계) | (승계) | **638(실행 바이너리) — 배포·실측 BLOCKED** | probe 1파일(코드 무변경 — 2s 흡수 준비 상수 `CANON_PHONE_2S=20`+배포 마커만) |
| T5 본 커밋 | 691 | 668 | 304 | 638→**기대 668** (배포 런 대기) | — (docs 전용 — 재실행 불요) |

- **posix 삼중 계보 정산(T1 리뷰 M2 — 단일 진실원 표기)** — 착수
  기대치(플랜 "posix 304 사유")·T1 측정 300·docs/90 구 표기 296의 삼불일치
  를 본 T5에서 등호 재수립: **296(구)** = #97 라인 종착 표기(§7.7.2·§7.8.2
  — **#98 이전 하네스 계보**)·**300(#98 Case 5b 후 하네스 측정)** = T1
  실측(하네스 TU 미링크 구조 무접촉 확인 후의 첫 측정 — 부기)·**304(5b 후
  재빌드 기준)** = T2/T3 측정 등호 = **현행 단일 진실원**. 계보 원료 =
  #98 Case 5b(`JKPipeTransport` 리슨 수복 — e28fd81(2026-10-11 14:17)가
  posix 하네스에 착지 — +115행/Check 8건 실측 grep·fix r1 c021006는
  코멘트 보강 전용) = 296→304 상승의 원료. 300 vs 304의 +4 차액 = 하네스
  TU/환경 계보 차 원장행(원문 근거: 하네스는 **환경 조건부 [SKIP] leg를
  소유** — `TestPipeEndpointMapping`의 live-server 가드가 상존 서버 감지
  시 early-return+`[SKIP]` 1행으로 케이스를 전부 건너뛰는 수형
  (posix_selftest/main.cpp:1045-1054 원문)·c021006 원문이 2026-10-11
  사용자 세션의 실측 [SKIP] 발화를 기록 — **측정치가 동일 하네스에서도
  환경/TU 계보에서 변동하는 경로가 실재**). 2s = posix 하네스 구조 미링크
  (own main.cpp·MusicModel.h 미포함 — T1/T2 리뷰 독립 확증) — 라인 결함
  없음·등호 유지.
- **2s 20의 분해(T1 리포트 원문)** = ①v2.4 UTF-8 1·②Latin-1 승격
  helper+블롭 2·③UTF-16LE/BOM+BE 2·④v1+말미 지목 2·⑤syncsafe 경계+파산
  클램프 2·⑥패딩+푸터 1·⑦FLAC 1·⑧Ogg 1·⑩ParseTag 파일 4(v1 폴백·FLAC
  매직·미지 매직·부재 경로)+⑨필터 확장 1+WAV 1+REMNANT 0 1 — 블롭 8종
  (`kBlobV24` 98B·`kBlobV23Latin` 23B·`kBlobV24U16le` 29B·`kBlobV24U16be`
  25B·`kBlobV24PadFooter` **53B(§7.9.4 정산)**·`kBlobV24Huge` 24B·
  `kBlobFlac` 132B·`kBlobOgg` 129B·`kBlobWavId3` 50B)·`check(` 20건
  (커밋 2s 블록 — 리뷰 diff 실측).
- **WSL probe 실측 영수증(T3 — 5런 원문)**: 합성 ID3v2.3 최소 mp3 2곡+태그
  없는 raw 1건+wav 5건 — 표 원문 **`Wsl Tag Beta (Probe Artis… / Tag
  Album T… / 149 B`·`Wsl Tag Alpha (Probe Art… / Tag Album O… / 151 B`**
  +폴백 행(`t_raw_noid`·앨범 "-")·8곡 카운터(런 5 캡처 mus_wsl_taglist.png
  883,734B — 리뷰 직접 열람 1:1) — **최상행은 t_raw_noid**(mtime desc에서
  시더 write 순서의 끝 = raw 최신 — 예상 코멘트 "alpha 최상단"은 M-1
  원장행). **TAG-PARSED-PREVIEW 3행(bytes=151·149·256 — title/artist/album
  전원 — 런 5회 전원 동일 원문) = 시더 자기재파싱(앱 파서와 독립)의
  이중 원문**. **필터 곡명 매치 영수증(런 1)** — 박스 `Wsl Tag Alpha`
  타이핑 + 카운터 **1곡** + 표 **1행**(`tag_diag_mus_wsl_tag_filter_
  artist_crop.png` — 런 간 cp 덮어쓰기 전 원문 보존)·**아티스트(TPE1)
  매치 = 5/5 정직 MISS**(런 원장행 — §7.9.3 #7 — 매치 순수 규약은 2s-⑨
  string 수형이 소유·계약 밖 과잉 원장의 미달). **openFailed 정직
  영수증(런 2-5)** — `MUS-TAG-OPENFAIL: OK — open은 수취되고 개방은 실패`
  +get_status 원문 `{"opened":false,"opening":false,"openFailed":true,…,
  "error":"open: Invalid argument"}`(accepted:true 수취 뒤 판정 폴링)
  ·런 1 = 정직 **TAG-FAIL**(open reply `unknown_app_tool` — vplayer 미등록
  계보 원문 — 무플랜 조정 0). **캐논 등호 5런 원문**: `selftest rc=0
  PASS=668 FAIL=0 == CANON_WSL_SELFTEST=668`·`SELFTEST-2S-CASES=20`
  (wsl_music.sh:358 원문)·
  `MUS-CANCEL-CANON-EQ/MUS-BROWSE-CANON-EQ OK`·2m=26·기존 세그먼트 회귀 0
  (MUSIC-OK·DIRS-OK·I1-OK(cure=0 5런)·LEG-UNSET-OK·CANCEL-OK·BROWSE-OK)
  +보호 원복 등호(SET-SNAPSHOT→SET-RESTORED 바이트 등호 — 29→36→29 —
  진단 사고 원장 §7.9.3 #9).
- **폰 BLOCKED 원장행(T4 — 리포트 병기 전문 원문)**: 폰 미네트워크(폰이 현
  Wi-Fi에 없음 — known 주소 전부 접속 타임아웃·로컬 서브넷 /24 8022 정찰
  리스너 0(ARP 정상 — 네트워크 생존 후 부재 확정 전문)·전 세션 env에
  PHONE_HOST 부재(계약상 기본값 없음·레지스트리 부재·orchestrator 세션
  부재)) — **런 0회·배포 미수행**. **폰 캐논 = 638(실행 바이너리 638-era
  빌드·소스 트리 = c021006 — HEAD와의 소스 갭 4파일 = T1/T2 계보)** →
  **기대 668 = 638 + 2r 10 + 2s 20**(배포 런에서 selftest PASS=668 FAIL=0
  실측 후 CANON 정산(T3 리뷰 M-1 승계)·실패 시 진단 원장 = 폰 바이너리
  재빌드가 RERUN_CMAKE 갭으로 모듈 .so에 미출현 패정 — `CANON_PHONE_PREV=
  638` 비교+G 마커 grep이 결손 감지 원문). **배포 = 절반 목적(폰 바이너리
  갭 소각 638-era→HEAD) → 다음 폰 접촉 런 수형(T4 리포트 원문 병기)**:

```bash
# (저장소 루트에서 — 경로 기재 금지 게이트)

# 0. 사용자 실조치(선행): 폰을 현 Wi-Fi(로컬 서브넷 — 주소 기록 금지)에 접속
#    → Termux에서 `sshd` 1행 기동. 폰 주소는 env로만(기록 금지 계약).

# 1. 배포(T4판 드라이버 — 준비 완료·미커밋 산출물 — §4.1-4):
PHONE_HOST=<폰 IP> bash engine/tmp/pmus_t4_deploy.sh
#   전수 git archive(SWEEP-POST-DIFF=0 hard 게이트)·sync 선행·buildterm 재빌드
#   (~10분·aarch64)·music.jkx 재팩(설치 아트 유지 — 554/995B 스테이징 유지)·
#   서버 재기동·selftest 668(A/B) 원문·사용자 화면 Music 창 복원 원문.

# 2. probe 2런(폰 런 2회 = FULL 등호 — 원문 5런 불요 계약 승계):
PHONE_DEPLOY_BASE=<배포 HEAD> PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_music.sh
PHONE_DEPLOY_BASE=<배포 HEAD> PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_music.sh
#   → selftest PASS=668 FAIL=0 실측(CANON 정산 — 상수 갱신 후속 몫)+N5 태그
#     표기 원문(캡처 2종+openFailed 영수증) — 기존 세그먼트 회귀 0 원문.
```

  사용자 실조치 요령(T4 리포트 복붙 블록 원문):

```bash
# ① 폰을 방 사용 Wi-Fi에 접속 (설정 → Wi-Fi).
# ② Termux 열고 sshd 기동:
sshd
# ③ 폰의 현재 IP 확인(Termux):
ifconfig wlan0 | grep 'inet '
#    → 그 주소를 런 명령의 PHONE_HOST=<폰 IP>로 채워서 알려 주시면 됩니다
#      (채팅에 주소 인쇄는 기록 금지 계약상 자제 — 사용자 본인 결정).
```

  폰 라벨 계약: `CANON_PHONE_EXPECT=668`·`CANON_PHONE_2S=20` 상수+
  `CANON-INCLUSION=MUSIC-FULL-2P2QRS-<2r 10+2s 20=30>`(구 라벨
  MUSIC-FULL-2P2Q-*/2P2QR-* = 판정 사다리 폐곡 원장행 유지)·2s 배포
  마커 grep(≥20 hard)+2s 카운터(H 선행)·**폰 타이핑 세그먼트 스킵 원장
  행**(합성 타이핑 신뢰 원장 T3 concern 2·정착기 3세대 — 필터는 도구
  경로 부재 = EYES 원장행·WSL 16e-3/4 소유).

### §7.9.3 함정 원장 (#99 신규 — 봉쇄 레슨 축)

1. **★stb_vorbis 전체 스트림 필요 — 코멘트 단일 접근 불가** —
   `stb_vorbis_get_comment`은 열린 stb_vorbis\*을 요구하고 개봉은 setup
   헤더 체인 전체를 요구 → 합성 원문 블롭(유효 프레임 0)이 개봉 불가 —
   메타 단독 접근 API가 존재해도 **개봉 계약이 코덱 스트림 정합을 강제하는
   구조는 메타 독립 소비의 원장행 갭** → Ogg 레그 = own `ParseOggTags`
   (2s-⑧ 통과). 조달 0 규약이 이 갭을 T1에서 실측으로 확정한 것이 라인의
   첫 성과(own 파서 = 갭의 근거 원문 수형 — 창작 부품 아님).
2. **★dr_mp3 = raw 태그 전달만 (원문 인용)** — dr_mp3.h:5082
   *"Currently this only reports the raw tag data which means applications
   need to parse the data themselves"* — onMeta가 존재해도 ID3v1/APE/ID3v2
   프레임 파서는 갭. **"onMeta 존재 = 이용 가능"의 오독이 재용을 만들고
   파서 부실을 잠재시키는 함정 — 원문 인용으로 갭을 봉인하는 것이 조달
   실측의 진실원 형태**(T1 리뷰가 서사 1:1로 확증).
3. **dr_wav LIST INFO 미지원(재용 불가) — WAV 태그 주류 = "id3 " 청크
   leg** — LIST INFO(INAM/IART/IPRD) 수형이 헤더에 실존하나 공급 계약
   갭(①)으로 재용 불가 → own `ParseWavTags`(RIFF 청크 연쇄 — 대소문자
   무시 "id3 " 매치·payload = ID3v2.3 재용) — LIST INFO = 백로그(빈
   TagSet 폴백 정상 경로).
4. **★fixed+stretch 열 배분 압착 함정 재현(D4 런 중 수리)** — 앨범
   WidthFixed 132 → 560px 창에서 곡명 stretch 열이 **~25px로 압착**(행
   라벨 첫 2글자만 렌더) — 원존 경로 열과 같은 fixed+stretch 배분
   함정의 재현(§7.8.1 계열). 수형 = 앨범 WidthStretch(곡명 2.0/앨범
   1.0) — 2/3-1/3 배분·원존 fixed 3열(크기/수정/spatial) 유지.
   **"~25px" 수치 = 런 중 관측치(수리 뒤 캡처만 남음 — 재현 캡처 부재
   원장행 T2 리뷰 I-2)** — 수치 단정에 캡처가 남지 않으면 원장행으로만
   말한다.
5. **★취소 간격의 2원화 — 상수와 문언이 별개로 남는 계보** — 간격이
   256→64로 바뀌어도 문언(헤더 주석 2곳·어설션 메시지·커밋 본문·산식
   주석)은 각각 다른 시점에 흘러나와 **"256 보조" 문언이 상수(64)와
   공존**한다(§7.9.4 정산 원장 4곳). 레슨: **간격 상수는 처음부터 한
   곳(kScanAuxEntryCheck)에 두고 문언은 상수명으로만 말하라 — 수치
   문언은 커밋 계보마다 복제된다(본 라인에서 4곳이 실측됨)**. 계약
   본안(2p-② 상한 [2,3])은 래치가 소유 — 간격 조정은 계약 무변조를
   산식으로 동봉해야 한다(양측 stride 등호 원문 = fix r1 수형).
6. **kBlobV24PadFooter = "원문 블롭"의 문언-실체 어긋남(T1 리뷰 M1)** —
   주석/리포트 "len 54·12바이트 패딩·10B 푸터" vs 실측 **53B·패딩 11B·
   푸터 13B**(중간 "ID3" 3B 과잉) — 기능 무해(bodyEnd=41 클램프+패딩
   break로 푸터는 항상 태그 밖 — 2s-⑥ 목적 충족)이나 **블롭이
   "원문 수형"인 계보 문서에서 오염 재용 위험** — 본 문서가 실측 원문으로
   정산(§7.9.4)·코드 배열 보정은 다음 접촉 몫(park §4.1-4).
7. **★타이핑 프레젠트 지연·적용 불신뢰(정착기 3세대 원장 — T3)** —
   ①타이핑 프레임의 프레젠트가 클릭 프레임과 달리 수초~수십 초 지연(활동
   게이트 #89 축 원장)·②키 선행 없는 타이핑 = 30s 창 3/3 미적용·
   ③텍스트 지닌 박스의 두번째 타이핑 라운드 0/5(클리어 키 5종 무효) —
   수형 = 상태행 밴드 차분기(mus_bandpx)+영수증 = 30s 창 말미 프레임
   (스테일 회피)+미판정 = 정직 MISS. **mus_bandpx 임계 60 > 실측 도착
   14px — 커밋 probe 재런 시 양 라운드 전원 TAG-FAIL 구조(T3 리뷰 M-2
   원장행 — 다음 접촉 임계 하향+밴드 재정찰)**. 근거 원문 = run5_tftitle
   _last.png(박스 빈 값·8곡·band_px=14)+런 1-5 로그. **아티스트(TPE1)
   매치 5/5 MISS = 이 원장의 수형이 아니라 태그 피처 결함 아님**(매치
   규약은 2s-⑨ 소유 — 원장행 분리 표기).
8. **openFailed = 정직 2단 분기(수취 vs 개방)** — 도구 open의 accepted
   수취와 개방 실패는 별개 축 — `{"ok":true,"accepted":true}` 뒤
   get_status 폴링이 `openFailed:true`를 회수하는 수형(16e-5 원문) —
   "수취 ok = 재생 성립"의 오독을 차단하는 probe 영수증 원장(합성 최소
   mp3는 유효 프레임 0이므로 개방 실패가 정상 종착 — 정직 계약).
9. **tmp 진단 스크립트도 원복 계약(T3 16f 운영 사고 — 원자적 원장)** —
   엔진 tmp 진단이 buildwsl 런타임 settings.json을 원복 없이 2회 덮어씀
   (사용자 29B 원문 → 진단 36B 잔상) — **probe의 스냅샷 가드가 없는 tmp
   진단은 같은 사고를 낳는다** — 원문이 리포트에 남아 바이트 등호 복구
   완료(`{"music":{"dirs":["<WSL 마운트>"]}}` 29B)·렛슨: **tmp 진단
   스크립트도 probe와 같은 원복 계약(스냅샷→등호 원복)을 지킨다**.
10. **★배포 드라이버 = engine/tmp 미커밋 산출물(정산 보류의 원장)** —
    폰 배포 절차(pmus_t4_deploy.sh)가 BLOCKED로 tmp 산출물 대기 — tmp
    정리 관례와 상충하는 유산(§4.1-4 park) — 렛슨: **차단된 절차의
    수형이 tmp에만 있으면 "다음 접촉"이 유산 의존이 된다 — 런 수형
    절차는 커밋 병기가 기본**(본 문서 §7.9.2의 절차 원문이 소관).
11. **LOGDIR 누적+영수증명 상수(T3 리뷰 I-1/원장)** — /tmp/mus에 런 5회+
    진단 3종 누적(REMNANT-COUNT=281)·런 1 곡명 매치 영수증이 런 2-5 cp로
    덮어씀 — **런별 서브디렉터리+런별 영수증명이 재사용성의 계약**
    (T4 폰 캡처명 상수 원장과 묶음 — §4.1-4).

### §7.9.4 문언/오자 정산 원장 (T5 — 커밋 불요 계보 문언의 봉합)

- **"256 보조" 잔존 문언 2곳(T2 fix r1 re-review I1 — HEAD 실측 등호)**:
  `engine/include/apps/ClientMusicApp.h:52`("boundaries(디렉터 진입+엔트리
  256 보조)")·`engine/src/apps/ClientMusicApp.cpp:529`("// 256 보조)마다
  읽히고") — 상수는 이미 `kScanAuxEntryCheck=64`. **정정 수형 병기**:
  h:52 → "boundaries(디렉터 진입+엔트리 `kScanAuxEntryCheck`(64) 보조)"·
  cpp:529 → "경계(디렉터 진입+엔트리 `kScanAuxEntryCheck`(64) 보조)마다
  읽히고" — 기능 영향 0(주석)·다음 코드 접촉 커밋 몫(park §4.1-4).
  (MusicModel.h의 "블록 상한 256" 문언 = Ogg 파서 블록 가드 — 별개
  상수·무관 — 리뷰 무변조 실측.)
- **main.cpp 2p-② 어설션 메시지·블록 헤더의 "256 보조" 문자열**
  (main.cpp 2p 블록 — fix r1 re-review cosmetic ②) — 무변조 계약상
  미접촉·문자열 유지 정당 — 표기 원장행(park).
- **kBlobV24PadFooter 53B 정산(T1 리뷰 M1 — HEAD 실측 등호)** — 커밋 배열
  = **53B**(리포트/주석 표기 "len 54" 부정)·프레임 뒤 패딩 = **0x00
  11B**("12바이트" 부정)·푸터 영역 = **13B** `3D 44 49 | 49 44 33 | 04
  00 10 | 00 00 00 1F`(규약 푸터 10B에 중간 "ID3" 3B 과잉) —
  기능 무해(bodyEnd 클램프+패딩 break로 푸터는 태그 밖 — 2s-⑥ assertion
  원문 성립)·본 문서가 실측 원문으로 표기 정산·배열 보정(1바이트 0x00)
  +주석 정산 = 다음 main.cpp(2s 블록 :6212-6221 — kBlob* 배열 leg) 접촉
  몫(park) — **MusicModel.h에는 kBlob* 0건**(T5 fix r1 지도 정정).
- **커밋 본문 "샤 캐논" 오자 원장행** — **176c822**("feat(apps): music
  재런치 활성 탭=유저 폴더 우선 (park 배치 4)" — 10-11 15:02·본 라인 체인
  밖 커밋) 커밋 본문의 "샤 캐논: Win 671/WSL 648/posix 304/폰 648 등호."
  → "캐논" 오자(git log 본문 실측 원문 1:1 — **T5 본판의 b2fe0f8 지목은
  오기 — 본 fix r1에서 본문 원문 재실측으로 지목 정정**) — amend 금지
  계약상 미수정·본 문서 표기로 정산.
- **이전 라인 오자 재유입 0 확인 원장** — T3 리뷰 cosmetic 원장(号→호·
  컬릭→클릭 — progress/리포트 문언)·T1 리뷰 cosmetic(사이본 상한 1MB
  (사이즈)·구형 리터(라이터) — MusicModel.h:782·:810 HEAD 실측 잔존 —
  park)·§4.1-2의 M-3 주석 "6곡" 원장 — 본 라인 원문(체인 6커밋 본문+
  캐논·결정·서사 표기) 재유입 grep = 0건(T5 본문 직전 실측)·**캐논 수치
  문언의 2원 원장**(#93/#94 "posix 296" 문언이 하네스 계보 뒤 304로
  갱신 필요 — wsl_music.sh:2537·phone_music.sh:202 — §4.1-4 park).
- **간격 산식 주석 nuance(T2 fix r1 re-review cosmetic ①)** —
  MusicModel.h:274 "(kTagHeadCap — 실사용 태그는 수십 KB)" — ParseTag의
  read 크기는 태그 크기가 아니라 **min(total, 1MB)**(태그를 읽기 전엔
  모름)·"수십 ms급" 기기 의존(NVMe 핫캐시 근사 — 콜드 HDD 수백 ms급)
  — read 크기/기기 의존 nuance 원장행(park).

### §7.9.5 EYES 게이트 (사용자 선언만 결제 — §5/§7.8.5 원칙 동형 ·
**EYES-PENDING — 본 라인 결제는 사용자 육안/체감 선언만 성립 — 본 라인의
결제 상태 = 미결제(대기)**)

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **태그 목록 모양+필터 체감 — WSL/Win** | 5열 [곡명(아티스트)][앨범][크기][수정][spatial] 모양·태그 행/폴백 행(파일명+"-") 표기·필터(곡명/아티스트 — 파일명 원존) 체감 — WSL 캡처 원문 보존(mus_wsl_taglist.png+tag_diag 크롭 2종 — I-2 파일명 스왑 원장 주의) — 아티스트 매치 원문은 미실측 원장(§7.9.3 #7) | **대기(육안)** |
| ② | **음악 폴더(개인 라이브러리) 체감** | 사용자 실제 라이브러리(ID3v2.3 — T2 smoke 캡처 t2_music_smoke.png/t2_crop_table.png 원문 — **라이브러리 경로는 커밋 게이트상 미기재 — T2 리포트 원장 참조**)에서 태그 열 실감·앨범 절단(툴팁 미제공 — T2 review cosmetic — 비율 재조정 몫)/560px 압착의 체감 판정 | **대기(육안)** |
| ③ | **폰 — 배포+태그 런(2s 20 흡수+표기)** | BLOCKED 원장(§7.9.2 폰 행) — 실조치(폰 Wi-Fi 접속+Termux `sshd`) 후 배포 드라이버 1회+probe 2런 — selftest 668 실측(CANON 정산)+N5 태그 표기 캡처+openFailed 영수증 — **라인 최종 게이트**·**N5 탭 좌표 = 정찰 산치 근접 추정(TAB2_X=90±그리드 — T4 concern 3) — 재보정 = 다음 런 캡처(bc_tag0/bc_*) 몫·MISS 시 send_input 폴백 정직 이행** | **대기(실기기 — 폰 미네트워크·실조치 후 런)** |
| ④ | **결정 D1-D6 재량** | 스펙 확정(8315b15)+본 문서 원장 성립 — 조달 0·스캔 1패스·Track 확장·열 재편·필터 확장·폰/WSL 동형 — **기각 시 즉시 수리** | **대기(유효)** |

### §7.9.6 커밋 원장 + IP/호스트 경로 게이트 (#99 라인)

- 라인 체인 = **8315b15(스펙) → 7e39a28(플랜) → 77323b1(T1) → 2d20829(T2)
  → 5f50a05(T2 fix r1) → 96d615b(T3) → cf978f8(T4)**+본 T5 커밋(§7.9.0 표
  = rev-parse 전체 SHA — git log가 진실원) — push 전부 origin main 직행·
  amend 없음.
- **IP·호스트 경로 리터럴 게이트(T5 실측 — 이중)** — 본 커밋 원문에서
  `\b([0-9]{1,3}\.){3}[0-9]{1,3}\b` grep = **0건**·호스트 경로 표기
  (드라이브 글자·사용자 루트 폴더명·/mnt 계열·서브넷 옥텟 포함) grep-F =
  **0건**(커밋 전 staged 게이트 실행 — BLOCKED 진단 서사는 마스킹 수형
  — 서브넷 크기(/24)와 판정 원문만 기재). 라인 이전 커밋도 리포트/review
  전원 IP 0건 실측(T1·T2·T2 fix r1·T3·T4 리포트 원문 — §7.9.0 계보).
- **본 T5 커밋은 docs 1파일(`docs/90_music_spatial_leg_asbuilt.md` —
  §7.9 신설+§4.1-4/§0 부기 추가만)로 한정** — git add 명시 경로만
  (#83 사건 렛슨)·engine/probe 소스 무접촉. 런 로그(wsl_music_tag_run
  1-5.log)·캡처(t2_music_smoke.png·t2_crop_table.png·mus_wsl_taglist.png·
  tag_diag*·run5_tftitle_last.png)·배포 드라이버(engine/tmp/pmus_t4_deploy
  .sh — 미커밋 원장 §4.1-4)는 engine/tmp untracked — 커밋 금지 충족.
## §7.10 music 태그 쓰기+커버아트 원장 (#111 라인 — T1-T7, 2026-10-25)

music 태그 쓰기+커버아트 라인(#111) 원장 — 사용자 설계 승인 "진행"(2026-10-25
chat) = 태그 쓰기/커버아트 백로그 결제 승계(갱신 64/65 대기항 원장). **music
목록 곡의 태그를 파일에 직접 쓰고(제목/아티스트/앨범 편집) 파일에 삽입된
커버아트를 보고 새 아트로 교체한다** — 읽기 라인(#99 — §7.9)의 역방향.
"파일이 곡명을 갖고 커버를 갖는다는 것이 음악의 정의 이외의 이유 없음(DRY
최소)" — 스펙 원문. 스펙=`docs/superpowers/specs/2026-10-25-music-tagwrite
-design.md`(662ed23 — **사용자 결정 3건 D1-D3**: 커버아트=표시+삽입 전체·
쓰기 형식=4형식 전부·UI=행 [편집] 버튼+패널·**재량 7건 D4-D10**: 조달 0·
v2.3 정규화·원자적 쓰기·커버아트 지연 읽기+LRU·완전 교체·FLAC 재조립·Ogg
재번호 — 기각 시 즉시 수리), 플랜=`docs/superpowers/plans/2026-10-25-music
-tagwrite.md`(cd17d65), SDD 원장=`.superpowers/sdd/2026-10-25-music-tagwrite/`
(progress+task-1..6 리포트+review diff 6종 — **전 태스크 review clean**).
라인 부품 계승: **#99 own 태그 사슬**(파서·인코딩 규약·헤더/스니프 진실원 —
쓰기는 그 역방향·같은 파일 MusicModel.h)·**#92 MusicDirStore 원자적 쓰기
선례**(D6 rename 사다리·.bak 3종 사다리 승계)·**#97 브라우저 부품**([커버
선택] 재용 — 타이핑 0 계약)·**갤러리 썸네일 계열**(LRU 96·MakeThumb·
FitThumb·pendingThumbs 선례 — §7.10.1)·**#89 idle 계약**(pendingCovers_
조건화 재청구 수형)·**#99 D2 스캔 1패스 원존 무변조**(D7 — ReadCover는
스캔 경로 무접촉·2u 신설 어설션 0은 스캔 캐논의 전제). T7은 본 §7.10 —
docs 전용(engine/probe 소스 무접촉)이라 selftest 재실행 불요(§0 부기 원칙 —
§7.9 T5 동형) — 캐논 = 전 태스크 리포트/progress의 건네받은 수치 그대로
봉합. 서브에이전트 스폰 0건(T1-T6 각 리포트 선언·T7 실측)·worktree 미사용·
main 직행·`git add -A` 미사용(전 태스크 명시 경로 staging).

### §7.10.0 체인 (rev-parse 전체 SHA 실측 — T7 측정 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 |
| --- | --- | --- |
| 스펙 | `662ed232a57ad86d86134779009a0f1c36078960` | docs(apps): music 태그 쓰기+커버아트 설계 스펙 (#111) — 결정 D1-D3(사용자)+재량 D4-D10 |
| 플랜(라인 BASE) | `cd17d654d92c7841cc72dc360bcfba9e494e05ad` | docs(apps): music 태그 쓰기+커버아트 구현 플랜 (#111 T1-T7) |
| T1 | `15c2bb49bad74081a3c28040867668b7386a57bc` | feat(apps): music 태그 쓰기 mp3 leg — ID3v2.3+APIC (T1) — MusicModel.h `jk::music::tags` 쓰기 leg 신설(WriteTag·ReadCover APIC·fsync 사다리·err 어휘 20종)+2u ①-⑦ 47건 (2파일 — +760·+460/−1) |
| T2 | `2973933c3d64ea0891e1f68892b45eb0c17bbd36` | feat(apps): music 태그 쓰기 FLAC/Ogg/WAV leg+ReadCover 전형식 (T2) — MusicModel.h +1351/−159(공용 부품 6그룹 추출 — 조립·사다리·CRC·base64·PICTURE 구조)+main.cpp +908/−7(2u ⑧-⑭ 57건) |
| T3 | `c8f31f22de3e8e80bb6b4c218e3e45e02caceba9` | feat(apps): music 태그 편집 패널+커버아트 표시 (T3) — ClientMusicApp.h +130/−2·cpp +826/−16 — 7열 재편([커버 40px]+[편집 56px])·태그 편집 패널+커버 선택 브라우저·D7 지연 읽기+LRU 96·3축 등호(신설 어설션 0) |
| T4 | `5b95c6303c1b384a672c3d140e1be9df5f9ce315` | feat(apps): music_tag_write 도구 — agent 쓰기 경로 (T4) — MusicModel.h +89(ParseTagWriteArgs·LoadCoverByMagic·TagWriteTool)+ClientMusicApp.cpp +55/−6(도구 7종째 등록)+main.cpp +174/−1(2u ⑮ 23건 — 2u 누계 127) |
| T5 | `eaba3a43889e222618f4f8d552ab2a10eaa17498` | test(probes): music WSL probe — 태그 쓰기 원문 (T5) — wsl_music.sh +397/−1(16f-TW 세그먼트+판정 18h+TW-RUN-VALS — −1 = CANON_WSL_SELFTEST 668→795 상수 갱신 1행) — **TAGWRITE-OK 2런 등호** |
| T6 | `d2c91ce740a7c3cccc4106ce9908f8db81754157` | test(probes): music 폰 probe — 태그 쓰기 동형+캐논 (T6) — phone_music.sh +556/−35(N6 세그먼트+2u 배포 마커 게이트·PHONE_DEPLOY_BASE 정산) — **폰 접촉 런 성립(TW-OK·폰 캐논 795)** |
| T6 fix r1 | `aed0addd3aab497b10943b9162015aa2da2e1047` | test(probes): TW 재독 파서 BOM 리터럴→escape 형식 6ASCII (T6 fix r1) — 런 중 자가발견(T5 함정 원장의 BOM 리터럴 재발)·동작 동일·계보 표기 통일 |
| T6 fix r2 | `2aa03f01faa7feb7cae73a61dc02d8ea17d29e0f` | fix(probes): recover_opt 정의 순서 수형 (T6 fix r2) — 런 1 부검의 **기존 함정** 소각(§7.10.3 #7-①) |
| T7 | 본 문서를 포함하는 커밋 | docs(apps): music 태그 쓰기 as-built (T7) — SHA는 git log가 진실원 |

폐곡 계보: T1 리뷰 **clean**(deferred minors 5건 — 스냅샷 1행 강화·v2.4 dli
TCON 한계 부기 등 — park 성격) → T2 리뷰 **clean**(**구현 결함 6건 — 그중
A급 1건 — 전원 커밋 전 자체 소각 원장**·deferred minors 8건) → T3 리뷰
**clean**(**자기발견 결함 2건 커밋 전 소각** — §7.10.3 #3·deferred 4건·
썸네일 32px 유지 ruling/열 폭 40px = 레이아웃 재량 수용) → T4 리뷰 **clean**
(deferred 4건 — 리포트 표기 오차 포함) → T5 리뷰 **clean**(TAGWRITE-OK 2런
등호·deferred 2건·**concern 1 = step 11-14 MISS — T3 상속 상태 판정 수용
ruling**: 본 diff 무접촉·좌표 재보정 = 탭 좌표 원장행 별소관+T6에 상속 지시)
→ T6 리뷰 **clean**(폰 접촉 런 성립 — TW-OK·폰 캐논 795 등호 3측정·fix 2건
= 런 중 자가발견 소각·**Important 유계 1건 = recover_opt fix 후 첫 재런
원문 대기 — #112 상속 ruling**).

### §7.10.1 배선 원리 (원류 원문 1:1 수취 — 리포트/progress 근거 · 코드 재실측 0)

- **★쓰기 사슬 4형식(D2 — T1+T2 — `jk::music::tags` 확장)**:
  `WriteTag(path, TagSet, const CoverArt* cover, err)` — **스니프가 진실원,
  확장자 보조**(fLaC/OggS/RIFF-WAVE 매직은 확장자와 무관 leg 진입·매직
  없는 .mp3만 v1-raw 신설 leg·0바이트 = "empty" 정직 거부).
  - **mp3 = v2.3 정규화 재작성(D5)**: 태그 헤더 size = **syncsafe**·프레임
    size = **BE32**(양방향 바이트 봉인 — 2u-①②·컨트롤러 ruling 원문: 읽기
    ParseId3v2가 SyncSafe32로 읽는다+2u-① 패딩 케이스 round-trip 실측·v2.3
    규약 원문)·텍스트 인코딩 = **1 (UTF-16LE+0xFF 0xFE BOM) 고정**(v2.3의
    유니코드 인코딩이 1뿐 — 인코딩 3은 v2.4 전용 — `Utf8ToUtf16Le` 느슨
    역변환 수형 = 읽기 Utf16BytesToUtf8의 쌍둥이)·TCON 복제(장르 보존)+그
    외 기존 프레임 **원문 바이트 복사 보존**(drop 금지 — TXXX 201B 등호
    실측)·**cover=null = APIC 프레임 원문 바이트 보존 leg**(재합성보다
    엄격 — 2u-⑥ 재독 등호)·빈 필드 = 프레임 생략(비움과 등치)·v1 존재 =
    v1 유지(말미 128B 등호)·v2 부재 = 신설(패딩 1KB)·기존 태그 = 선언
    본문 크기 유지(오디오 제자리 — 초과 시에만 재배치).
  - **FLAC(D9)**: 구역 버퍼 스캔(≤256 블록·kTagWriteMax) → 블록 단위
    재조립 — STREAMINFO 첫 블록+34B 필수(`flac streaminfo missing` 정직
    거부)·SEEKTABLE 판정 `bsz ≡ 1 (mod 18)` 실패 = `seektable broken`
    (보존 실패 = 원문 무손상 — D9 정직 구현)·VORBIS_COMMENT 첫 것만 교체
    (복수 소각)·PICTURE = cover 지정 시 첫 유효 자리에서 교체+복수 소각
    (D8)·cover=null = 전부 유지·신설 블록 = type≤4 고정 순서 뒤(FLAC
    규약 — 5 이상 자유 순서)·is-last = 재보행 마지막 블록만 0x80 재마킹
    (기존 표지 소각)·오디오 = [chainEnd, EOF) 원문 스트림 복사.
  - **Ogg(D10)**: 코멘트 패킷(0x03"vorbis")을 페이지 보행(lacing 런
    재사용)으로 지목 — **페이로드 선두 개시 계약**(그 외 = `ogg structure`
    정직 거부)·C0 기록(gtype/serial/seq/granule 원문 수취) → Ck 잔여
    세그먼트 = 트레일(원문 바이트+lacing 원문 — 패킷 경계 불변 재사용) →
    신설 코멘트+트레일 255세그/페이지로 재포장(런 초과 = 다음 페이지 htype
    0x01) → Ck 이후 모든 페이지 = **시퀀스 재기입+CRC 재계산**(CRC 필드
    소각 후 계산 — libogg 규약)로 원문 복사(**granule·lacing·payload
    불변 — 2u-⑪ 독립 판정판 등호: seq 연속·CRC 규약 등호·셋업/오디오
    패킷 바이트 등호**).
  - **WAV**: RIFF 청크 연쇄 스캔(1024 상한·홀수 패드 규약) → "id3 " 청크
    (대소문자 무시 — 읽기 ParseWavTags 규약 그대로) 교체/**말미 신설**(
    기존 청크 순서 보존)·RIFF 크기 = 청크 합산 재계산(**산출 leg와 기입
    leg가 같은 걸음**)·패딩 0(임베디드 청크 — mp3 1KB 패딩은 제자리
    재배치 계약이라 다른 사정)·RIFF 한계 초과 = "tag too large" 정직
    거부·기존 id3 태그 = ReadId3Tag→BuildId3V23Body 재용(mp3 leg 동형).
- **★원자적 쓰기(D6 — 4형식 같은 사다리)**: 동일 dir `.pid.tagtmp` 쓰기→
  fsync(**Win = CreateFileW+FlushFileBuffers·posix = ::open+::fsync —
  TagFileWriter 저수준 수형 — 리포트 원문: MusicDirStore는 ofstream이라 fd
  접점 부재**)→close→rename(.bak 사다리 3종 — UCRT 교체 실패 실측 선례
  승계)·**순서 계약 = 원본 ifstream close가 rename에 선행**(Windows 공유
  핸들 rename 거부 — T1 결정 10·T2 함정 A급 재발을 사다리 안에 봉인)·
  **kTagWriteMax = 16MiB** — 쓰기는 편집 지점 1곳 파일만 다루므로 스캔
  1패스 상한(#99 머리 1MB)을 쓰기 leg가 그대로 받지 않는다(선단 12B
  프리읽→선언 태그 끝까지만 조건 read)·MinGW narrow 기수 = UTF-8 strict
  (CP949 → filesystem_error — 2o-f 실측 원문) → 쓰기 경로 전부 fs::path
  원문 개방(#92 승계)·posix = O_WRONLY|O_CREAT|O_TRUNC 0644.
- **ReadCover 전형식(D1)**: mp3 = APIC 프레임(본문 레이아웃 = **[enc]
  [mime NUL][desc NUL][picture] — 그림타입 바이트 불내장**)·FLAC =
  PICTURE 블록(첫 유효 — 파손 후보 스킵 관용)·Ogg = lacing 런 보행으로
  코멘트 **패킷**을 정확 수취(ParseOggTags의 페이로드 concat과 달리 255
  런 페이지 분할 무관) → METADATA_BLOCK_PICTURE → base64 → ParseFlac
  PictureStruct(WAV = "id3 " 청크→ReadId3Tag→ApicFromFrames 재용)·
  **부재 = "no_cover" 관용(부재=부재가 원칙 — 구조 파산만 전파)**.
- **err 어휘 단일 표**: T1 20종(open·size·empty·unsupported_format·
  unsupported_id3_version·extended header unsupported·global unsync
  unsupported·unsupported flags·tag truncated·tag too large·frame id
  broken·frame truncated·tmp open/write failed·audio read failed·fsync
  failed·.bak 사다리 3종·cover empty·cover mime missing/unsupported·
  no_cover)+T2 신설 9종(flac structure/truncated/streaminfo missing·
  seektable broken·vorbis comment broken·ogg structure·riff structure·
  chunk truncated) — 4형식 leg 공용 표기(리포트 원문 표·T2가 FLAC leg
  착수에 맞춰 2u-⑦의 FLAC 기대 어휘를 `flac streaminfo missing`으로
  갱신 — 구 표기 "unsupported_format"은 T1의 임시 거부 원장).
- **UI·표시(T3 — 7열 재편)**: `BeginTable 7열` = [커버 40px] + 원존 5열
  순서(곡명/앨범/크기/수정/spatial)+**[편집 56px]**(spatial 옆·표기 "편집"
  한글 SmallButton) — 곡명 셀 열 0→1 이동뿐 **더블클릭 위임·행 툴팁·
  [spatial] 버튼 배선 = 원존 무변조**·발사 게이트 = `TagWritableExt`
  (mp3/flac/ogg/wav — 정직 표기: m4a/aac/wma 행은 "-"+"태그 쓰기 미지원
  포맷(v1)" 툴팁 — **매직 불일치는 [저장]의 정직 거부가 흡수 — 확장자는
  가능성 게이트**)·프리필(artist/album = 태그 원문·title 부재 = 파일명
  stem — D3 표기 폴백 규약 승계)·패널 = **[폴더 관리] 동형 전환 패턴**
  (같은 창·tab·새 창 금지)·[저장] = WriteTag **1콜로 완결**(원자적
  tmp+rename 내장 — UI 추가 파일 처리 불요)+CloseEditor+RequestScan(
  재스캔 재청구 기존 배선 — #93 취소 계약 무접촉)·실패 = err 원문 1행
  패널 유지+입력 값 보존(정직)·**[저장] 스캔 게이트** = scanBusy_
  BeginDisabled+표기 "스캔 중"(SaveTagEdit에도 동형 방어선 — 도구 경계성
  대비)·**[커버 선택] = #97 브라우저 부품 재용**: cwd 표기 1행+전문
  툴힌트+".." 상위+[닫기]+이미지 child 128(하위 폴더 Selectable = JoinDir
  원문·로컬 ListCoverFiles = .jpg/.jpeg/.png만·IsCoverImageExtension
  Stricmp·도트 스크립+바이트 asc 결정론)·클릭 = 즉시 선택(성공 = 브라우저
  종료)·**타이핑 0**(폰 계약)·개봉 시작점 = 편집 곡의 parent_path.
- **★커버아트 지연 읽기+LRU(D7)**: ReadCover는 `ListAudioFiles`/ScanWorker
  에 무접촉(스캔 1패스 원존 무변조) — **행 렌더의 캐시 미스만 렌더 스레드
  채움+프레임당 2건 예산(kCoverFillPerFrame)** — 초과분 = placeholder+
  pendingCovers_(다음 틱 재요청)·플 = `std::vector<CoverSlot> covers_` —
  **LRU 상한 96**(갤러리 선례 승계)·슬롯 key = **(full, mtime, size)** —
  태그 쓰기(커버 교체) 후 재스캔의 mtime/size 이동이 행 표기 재충 진실원·
  실패 슬롯 = `failed`+`failedGen=coverGen_`(RequestScan마다 증가 세대 —
  다음 재스캔 세대에서 1회 재시도 — 60Hz 재시도 방지)·"no_cover"도 같은
  placeholder(행 라벨·툴팁 무변조)·디코드 = **JKImageLoader 접점 재용**
  (`jk::LoadImageMemory` 내장 바이트 — 파일 터치 0·`jk::LoadImageFile`
  커버 선택 파일) — **앱 TU는 stb_image를 include하지 않는다**(단일 구현
  계약 원문)·축소 = ShrinkToBox(행 32·미리보기 96 — nearest·gallery
  MakeThumb 수형·**미리보기도 96 박스 텍스처 — 폰 SW 렌더러 대형 텍스처
  비용 회피**)·표기 = draw list 직접(테마 launcherCell 토큰 — 회색
  자리표시 원존 토큰·FitImageBox 비율 유지)+Dummy 32px(셀 진행)·**컬 =
  행 클립 rect 교차 시에만 요청**(gallery fix r2 C2 수형 — 컬 없는 전수
  요청 = LRU 촬영 잠금 원저 회피).
- **★#89 idle 계약의 이원화 원장(T3 — 조건화 수형 승계)**: `OnIdle`이
  pendingCovers_에서 `frameDirty_=true` 재청구(게이트 배송 경로) — **이것이
  실제 동작 경로**·RenderOverlay 끝의 `frameDirty_ = pendingCovers_` 대입
  = 갤러리 선례 원문 유지(갤러리의 RenderAndCommit 뒤 소각 사정 — §7.10.3
  #3)·도착 완료 = 마지막 프레임, idle로 원상 복구.
- **동프레임 텍스처 파괴 금지의 예방 파견(갤러리 C1 원장 승계)**: ①행 풀
  퇴출 = 이번 프레임 접촉 스탬프(`useFrame≥coverFrame_`) 후보 전부 제외
  (PickCoverVictim) ②미리보기 교체·패널 닫힘의 파괴 = **프레임말 스윕**
  (`texSweep_` 포인터 적립 → RenderDrawData 뒤 파괴)·미리보기 재구성 =
  draw 전 경계(previewDirty_ → 다음 프레임 패널 머리) 소화.
- **도구(T4 — music_tag_write — music 앱 7종째 등록)**: args =
  {path 필수·title/artist/album(빈값 = 비움 쓰기 — 프레임 생략 등치)·
  cover(이미지 파일 경로|생략/`null`/빈문자열 = 유지 — 3형식 빈값 수렴
  2u-⑮ 3건 봉인)} — **파서 = 도구 허브 원존 AgentJson(quickjs) 재용**
  (이스케이프 복호·UTF-8 계약 — 직수 파서 신설 금지)·`LoadCoverByMagic` =
  **마법수 mime 판정**(0xFF 0xD8→image/jpeg·0x89 0x50 0x4E 0x47→
  image/png·그 외 = "cover magic unsupported" 정직 거부+bytes 소각)·
  `TagWriteTool` = `tags::WriteTag` **1콜**(UI [저장]과 동일 사슬·원자적
  사다리 내장·도구 경계 추가 파일 처리 0 — DRY 최소)·등록 =
  OnInit `SendAgentToolRegister("music", tools)` 1회 발송 7종(연결 단위
  upsert — 제2호 호출이 매니페스트 전체 치환 — 이전 등록 소각 없음)·응답
  = `unknown_tool` 앞에서 `{"error":"scan_busy","retry":true}` 정직 거부
  (스캔 진행 중 — 도구는 대기 없이 중단 표기 — 스펙 §5 규약 수형)·쓰기
  실패 = `{"error":"write_failed","detail":err 원문 EscapeJsonStr}`(dir
  3종 응답 규약 수형)·**기존 도구 8종 원존 무변조**(diff 제거행 = 등록
  개수 문어 6종→7종뿐).
- **2u = 127 어설션 계보**(①-⑮ — T1 47+T2 57+T4 23·1케이스 다중 어설션
  확장 계수 수형·폰 캐논 계보 = 스펙 §6 ⑯ — progress ruling: 스펙 ⑯=T6
  폰 실측행 — 폰 캐논은 selftest 어설션이 아니라 **계보 실측이 진실원**).

### §7.10.2 캐논 계보 (2u 계열) + probe 실측 영수증

| 커밋 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#99 종착 = §7.9.2 T5 행) | 691 | 668 | 304 | 668 | 2s 20 |
| T1 `15c2bb4` | **738** | **715** | (승계) | 668 | **2u 47** (①-⑦ — 기존 계보 0건 변경·연속 2런 738/738 재현·+47/+47 동증분 = 축 무관) |
| T2 `2973933` | **795** | **772** | (승계) | 668 | **2u 57** (⑧-⑭ — +57/+57 동증분·2u ①-⑦ 등호 47건 재현 — 공용 부품 추출 후 출력 계약 동일) |
| T3 `c8f31f2` | 795 | 772 | **304**(실측) | 668 | 0 (3축 등호 — UI 태스크 신설 어설션 0 계약·posix = 하네스 own main.cpp — ClientMusicApp 미링크 재증명) |
| T4 `5b95c63` | **818** | **795** | 304(승계) | 668 | **2u 23** (⑮ — 2u 누계 127 — +23/+23 동증분) |
| T5 `eaba3a4` | (승계) | 795 (2런 등호) | (승계) | 668 | 0 (probe 전용 — CANON_WSL_SELFTEST 668→795 상수 1행 갱신+SELFTEST-2U-CASES=127) |
| T6 `d2c91ce`(+fix 2건) | (무접촉) | (무접촉) | (무접촉) | **795** (3측정 등호 — 668→795 = 2u 127 소각) | 0 (probe 전용 — CANON-INCLUSION=MUSIC-FULL-2P2QRS-127·2u 배포 마커 게이트[≥100] 신설) |
| T7 본 커밋 | 818 | 795 | 304 | 795 | — (docs 전용 — 재실행 불요) |

- **캐논 최종 = Win 818/WSL 795/posix 304/폰 795 — 2u=127(CANON
  MUSIC-FULL-2P2QRS-127)**. 기존 계보 원수치 보존: 2r=10·2s=20·2m=26·
  2n=17·2o=16·2p=7·2q=7 전부 변조 0(T6 probe H 원문 라인)·캐논 증분은
  전부 Win/WSL 양축 동증분(47/57/23 — 플랫폼 조건 분기 0 계약) — posix는
  하네스 TU 미링크(own main.cpp)라 신설 어설션 0(구조 무접촉 원장 — 계열
  원존).
- **캐논 정산(#112 STT 구두 라인 승계 — 2026-10-25 · T7 부기 1행)** — 위 행의
  "Win 818/WSL 795/posix 304/폰 795 — 2u=127"은 **#111 종착 수치로 원장 보존
  (재작성 없음)** — 현행 계보 = 818/795 → **2u 최final fix 5(f33d41a — Win
  823/WSL 800·폰 = 800 승계 대기 원장)** → **2v 14(#112 T1-T3 구두 — Win 837/
  WSL 814)** → 폰 승계(#112 T5 6128bb4 — 800+2v 14 = **폰 814**·WSL 등호 A/B
  2런) → **현 캐논 = Win 837/WSL 814/posix 304/폰 814 — 2u=132·2v=14** —
  정산 이력 원장 = docs/91_stt_voice_asbuilt.md §3 캐논 계보 · **#111 정산
  지시 원문(progress.md)의 recover_opt 재런 원문은 #112 T5에서 RECOVER-OK
  실측 소각 완료** · TW 게이트 채용 정산완료(6128bb4).
- **WSL probe 실측 영수증(T5 — 정식 2런 등호 — rc=0·~652s씩)**: 도구
  relay(atool — app_tool app=music 직행) 쓰기 **3콜 1/1/1**(scan_busy
  재시도 폴링 수형 — 실측 3콜 전부 1차 시도 성공 — 소형 시드 dir 경합 0)·
  재독 표기 **'Wsl Write Alpha'→'Wsl Write Reup' 등호**(TW-PARSED
  3필드 — 재독 파서 mus_twparse.py = 시더·앱 파서와 독립 — 16e
  TAG-PARSED-PREVIEW 수형)·**APIC md5 == cover md5 3등호**
  (90155ae6b2bb6f91691fe7724db9b165 — 941B 합성 cover — 드라이버 PIL
  산출 인라인 — 외부 파일 의존 0)·FLAC 재독 `keys=3 picture=0`(42B→
  127B — D9 재조립 실측)·**목록 행 캡처 4종**(list/list2/flac/filter —
  행 32px 썸네일에 4색 밴드 렌더+회색 자리표시 대조+상태행 "태그 저장
  (도구) — 재스캔" 표기 — **표기·썸네일은 캡처 육안 몫 = EYES**)·
  `TW-REMNANT: 0` 2런·기존 세그먼트 회귀 0(DIRS-OK·I1-OK·LEG-UNSET-OK·
  CANCEL-OK·BROWSE-OK — 10/11 run5 기저 등호)+복원 수형 원존(SEED-BURIED·
  SET-RESTORED byte-exact·PERM-BURIED·END 서버 DOWN 원복+17d 신설 가드)·
  **MUSIC-TAGWRITE-VERDICT: TAGWRITE-OK 2런 등호**+TW-RUN-VALS/TW-RUN-CAP
  요약 스트림(JOG-RUN-VALS 수형 — stdout 수취 부재 재발 방지 봉인 승계)·
  캡처 경로 원장행 — 2런이 겹침 교체(runs 1 원문 = run1 로그 CAPTURE 행으로
  정직 보존 — T4 concern 5 계승).
- **폰 실측 영수증(T6 — 폰 접촉 런 성립)**: 접속 = known_hosts 자기
  판명(KH-SELFRESOLVE-MATCH — 주소 미인쇄 계약)+서브넷 정찰기 동봉(
  로컬 서브넷 /24 포트 8022 전수 — KH 1차 매치라 미실행·향후 폰 서브넷
  이동 시 자동 폴백)·배포 = 전수 git archive HEAD(T1-T4 코드 원문 이관)·
  **SWEEP-POST-DIFF=0**(blob 크기 등호)·리빌드 405s RC=0(aarch64)·
  **폰 spatial leg WIRED 보존**(LEG-STAGE-WIRED·MUSP-LEG-SYM
  alcOpenDevice=3 — #110 원존 재용 — 삭제 금지 계약)·selftest **A/B = rc=0
  PASS=795 FAIL=0 2u=127 등호 2측정**+probe H 섹션 795 등호 = **3측정
  등호**·시드 랜딩 = 도착 md5 등호 hard 게이트(TW-COVER-MD5-PHONE 등호)·
  쓰기 3콜 1/1/1·재독 원문(TW-ID3-VER 3·title 2단 교체 'Phone Write
  Alpha'→'Phone Write Reup'·APIC md5 == cover md5 2콜 등호+cover=null
  APIC 유지 apic_keep=1)·FLAC 42B→129B·`TW-FLAC-BLOCKS keys=3`·
  **행 썸네일 표기 원문 캡처 3종+crops 3종**(4색 밴드 썸네일·상태행
  표기·7열 [편집] 열 — 캡처 육안 선확인)·TW-REMNANT 소각(TW-BURIED·
  REMNANT-TW-SEEDS: gone·P3 TAGDIR 통짜 소각)·
  `MUSIC-TAGWRITE-PHONE-VERDICT: TW-OK`+TW-RUN-VALS/TW-RUN-CAP.
- **MUSIC-PHONE-VERDICT = MUSIC-PHONE-FAIL(honest)** — 요인 2건·**둘 다
  본 세그먼트(도구 쓰기)의 재축 아님**: ①N4 ⑤ [이 폴더 추가] 무전달 재발
  (BROWSE-FAIL — **잔여 원장행 상속** — 갱신 65 "탭 좌표 재보정" 별소관 ·
  T6 수형 = hard 마커(rc=1)·마커 원존·**종결 시점만 probe 말단으로 이동**
  — 구 수형은 즉시 FAIL로 REMNANT·회수·판정(섹션 5-9)을 통째로 생략했다
  (run8 원문: 집합 미회수 유산) — 본 수형이 T6 자기 산치를 그 위에서
  정산) ②spatial_mix 회수 결손(기존 함정 — fix r2 소각 — **첫 재런 원문 =
  유계 — §7.10.3 #8**). 폰 부팅 원문: BOOT-SECONDS=9·SCAN-COUNT=27·
  SCAN-SORTED=OK·ICON-DECODED=26·복원 = settings/perm 바이트 등호·jkweb
  불접촉·Music 창 복원.

### §7.10.3 함정 원장 (#111 신규 — 봉쇄 레슨 축)

1. **★T1 블롭 산술 25vs26(테스트 산술 — 계보 결함 아님)** — first run
   2 FAIL: ⑦ 잘린 ID3 합성 블롭 산술(10B 헤더+15B 본문 = 25B vs 체크
   26B) — fix 후 738/715 전원 PASS. 주석 표기 "매agic" 오기 6곳 = 커밋
   전 sed 정정 실측(기존 블록 무접촉).
2. **★T2 함정 원장 7항(구현 결함 6건 — 그중 A급 1건+테스트 결함 4건 —
   전원 커밋 전 자체 소각)**:
   ① **[A급] TagAtomicWrite가 원본 ifstream을 닫지 않아 rename 사다리가
   일괄 실패**("original backup failed") — T1 결정 10(원본 close 순서)의
   재발: leg별 tail 람다가 스트림을 쓴 뒤 사다리가 close하지 않았다 —
   수리 = 사다리에 `f` 인수+rename 전 `f.close()` 봉인(3 leg — WAV는 이미
   직접 close) → **이후 전 leg가 같은 사다리를 공용 소비 = 재발 불가
   구조**. ② RebuildVorbisEntries 관리 키 **대소문자 파산**(FoldVorbisKey
   소문자와 관리 표 "TITLE" 대문자의 등식 비교 = 교체가 아니라 신설로 가면
   3필드 round-trip 파산) — 소문자 관용 표+신설 캐논 대문자 표 분리.
   ③ Ogg **[0, C0) 머리 페이지(id) 미복사** — 코멘트 교체 후 id 페이지가
   사라지는 재조립 누락 — head1 앞 원문 복사+tail `seekg(ckEnd)` 명시
   (보행 종료 위치 기점). ④ 뒤페이지 CRC 재계산이 **CRC 필드를 0 소각
   없이 계산**(규약 위반 — 독립 판정판 불일치) — pg[22..25]=0 후 계산.
   ⑤ WAV RIFF 크기 산출에서 신설 id3 청크의 **8B 헤더 이중 산출**
   (riffSize > fileSize−8 — 8 초과) — `+= id3ChunkTotal`. ⑥ Base64Decode
   종결 검사 `bits == 0` 파산(RFC4648 오판 — 2/3바이트 결합의 잔여 패딩
   비트까지 거부 = 모든 %3≠0 원문 거부) — 패딩 비트 완화 수용(구조 파산은
   ParseFlacPictureStruct가 걷는다 — 문서화). ⑦ **테스트 결함 4건**(⑧
   STREAMINFO "last=false" 리터럴의 사슬 종결 파산 — 테스트 구성이 D9 계약
   의 반대·⑫ 코멘트 패킷 lacing 255 런 규약 위반 합성(325>255)·⑬ 신설
   id3 자리 산술 잔산·⑭(b) WAV 픽스처 크기 자리+"WAVE" 누락 — walk
   오프셋 파산) — 전부 테스트 산술·구성 결함. (부기: bash heredoc `\\`
   이스케이프 훼손 3회 — Write 파일 도구로 재작성·rc=127 = PATH 미설정
   DLL 사망 — 2줄 PATH 표준 재확인)
3. **★T3 자기발견 결함 2건(커밋 전 소각 — 텍스처 생명 계열)**:
   ① **previewSweep 플래그의 누수** — 미리보기 재구성에서 "파괴 예약
   플래그"만 쓰면 프레임말 스윕이 **새** 텍스처를 파괴하고 옛 텍스처는
   누수 — 스윕을 **포인터 적립 목록(`texSweep_`)**으로 정정(파괴 대상이
   대입 순서에 묶이지 않는다). ② **선택 후 미리보기 정지** — 이미지 행
   클릭 → 재구성 예약이 다음 프레임 몫인데 그 프레임을 만들 더티 원천이
   없으면(브라우저 종료 = 이번 프레임의 입력뿐) 미리보기가 영원히 안
   나온다 — 클릭 경계 `frameDirty_=true` 1행 봉합. + **OnIdle 재청구 =
   유일 동작 경로 원장**(갤러리 선례의 RenderOverlay 끝 대입은
   RenderAndCommit 뒤 OnFrameCommitted가 소각해 단독으로는 다음 프레임을
   못 만든다 — JKClientApplication Run 루프 순서 실측 — 대입은 선례 원문
   유지+원장행 이원화). + **ninja "no work to do" 오판 방지 원장(실측)**:
   jkdesktop.exe는 ClientMusicApp을 링크하지 않는다(앱 = 별도 dll) — 본
   라인 실제 산출물 = jkapp_music.dll(Win)/.so(WSL)+apps/music.jkx 재팩
   — 클럭 스큐 mtime 경계에서 touch 강제 재컴파일로 최종 상태 재증명.
4. **T4 표기 오차(원장행 — 진실 계보)** — 리포트 numstat 표기
   **+55/−6 vs 실측 +50/−5**(ClientMusicApp.cpp)·총계·캐논 유효·문언
   정산 원장행(park — 진실원 = git numstat). + 등록 위치 원장(실측 지목):
   등록부 = **JKAppModule_music.cpp가 아니라 ClientMusicApp.cpp OnInit의
   SendAgentToolRegister 1회 발송** — 같은 자리에 병합(제2호 upsert 치환
   계약).
5. **★T5 probe 결함 3건(self-fix — fix 이전 견적 런 run_parsebug 원문
   수취)**: ① **probe 파서의 APIC 슬라이스 z+2** — 엔진 APIC 본문
   레이아웃 = [enc][mime NUL][desc NUL][picture]·**그림타입 바이트
   불내장**(ApicFromFrames 보행 동형) → z+2 스킵이 그림 선단 5B(0x00
   포함)을 잠식 → md5 불일치(jpeg_magic=0) — my-parser z+1 보행 수형+런
   반복 — **엔진 사슬(쓰기/ReadCover)은 자가 일관 — 실측 md5 3등호
   무결증명(probe 결함)**. ② **16e-4 아티스트 필터 잔존** — 16e의 마지막
   행동 = 아티스트 필터 타이핑 — 그 필터가 잔존한 채 16f가 도는 사건
   (tagwrite_list 캡처가 필터 매치 행만 보여 태그 반영 행 캡처 비짐) —
   **16f-0a 신선 클라 재건 수형**으로 소각(close+launch — InputText
   클리어 의존 소각). ③ 환경 트랩(설계 수형 회피): heredoc 역사 접힘
   (chr(92)/chr(10) 구성 우회 — BOM 리터럴 2회 재발 → escape 형식 6ASCII
   확정 — **T6에서 재발** = #7-③)·cover base64를 wsl.exe env 전달 시
   0바이트 소실(인라인 수형)·cp949 콘솔 유니코드 print crash(진단 하네스
   만 — probe는 WSL UTF-8).
6. **★step 11-14 위임 세그먼트 MISS(2런 공통 — T3 UI 상속 · honest)** —
   표행 0 더블클릭 3쌍이 전부 MISS → vplayer 본체 미스폰 → glue/play
   원문 미수취·MUSIC-VERDICT FAIL(rc=0 honest). 정직 귀속 = **T3 c8f31f2가
   표에 커버 열+썸네일+편집 버튼을 넣어 행 레이아웃이 이동** — MUS_ROW0_Y
   =132 상수는 그 위의 빈 지대(탭 스트립 지대) 착탄 — **본 라인 계약(기존
   세그먼트 원존)이라 좌표 재보정 불수행 — MUS_ROW0/MUSP_ROW0 상수 재보정
   = 별소관 결정 대기(갱신 65 잔여 원장행 승계 — progress ruling: probe
   11-14 원문은 HEAD상시 MISS·좌표 재보정 태스크 결정 대기)**. 관련
   원장행: 16e TAG-FAIL settle 0/0 = 프레젠트 지연 원장 수형(회귀 아님)·
   16f-5 settle_title=0(밴드 14px < 60 임계 — 캡처 말미 프레임 스테일) —
   **TAGWRITE-OK의 구성항 아님**(파일 수준 round-trip이 강도 축)·T6 폰도
   `MUSP_ROW0_Y=132` 같은 계보·`MUS-TW-FILTER-SKIP` = 폰 타이핑 스킵
   원장 승계(N5와 같은 EYES 축 — WSL 소유).
7. **★T6 함정 6건(런 1 부검 — fix 2건 수형+표기 원장행+유계 1건)**:
   ① **recover_opt 정의 순서(기존 함정 — fix r2 2aa03f0)** — spatial_mix의
   `recover_opt` 호출이 함수 정의보다 앞 → `command not found` →
   **CAPTURE-OK 영수증(144,329B) 있는데 회수 0**(CAP_OK=0) — 수형 = 정의를
   호출 앞으로 이동(본문 무변조) — 본 런의 회수 결손 = 영수증 원문으로
   정직 보존. ② **deploy 드라이버 verdict의 CANON 파싱**(engine/tmp
   driver — file fix) — verdict 파서가 `CANON-A=` 뒤의 **괄호 해설을
   통째로 비교** → **T6-FAIL 오판행(영수증 데이터 전부 green)** — driver
   수치 추출 `awk '{print $1}'`로 정산(재계산 원문 = T6-OK — 재런 불요)·
   fix 이전 출력 = 결함 원문 보존. ③ **BOM 리터럴 재발(fix r1 aed0add)** —
   T5 함정 원장(heredoc 붕괴 — escape 형식 6ASCII 확정)의 재발 — N6 파서
   heredoc에 리터럴 U+FEFF — 런 중 byte-wise 수형(lstrip 계열 — 동작
   동일·계보 표기 통일). ④ **★실행 중 probe 파일 편집 위험 원장행** —
   aed0add(런 중)+2aa03f0(회수 단계와 접침) — bash 증분 읽기와 겹칠 수
   있는 수형 — 본 런 산출 무상처는 출력 원문으로 확인(전 행이 구 수형
   동형으로 수취) — **재발 금지 원장행: 런 중 파일 편집 금지 — 이후 런은
   커밋 완료 후 착수**. ⑤ **시드 mtime 스탬프 vs 쓰기 갱신** — 시드가
   미래 스탬프라 쓰기 갱신(런 시각)이 mtime desc 상단행이 못 된다 — 쓰기
   대상 행은 표 말미에 렌더(캡처 레저의 "상단행" 문구는 스탬프 계보에
   종속 — 성립 판정 도구+파일 재독에 영향 0). ⑥ **재독 파서 프리뷰 표기
   문언 오버** — junk mp3(무태그 전제 = 정상 원문)에
   `magic-unknown (무태그 전제 위반…)` 라벨 — 우사 행·게이트 아님
   (PARSED-A/B·APIC md5가 강도 축) — fix 후보 원장행.
8. **★T6 Important 유계(컨트롤러 ruling — #112 상속 지시 원문)** —
   "recover_opt fix 후 첫 재런의 spatial_mix 회수 성립 원문 미수취 — 폰
   접촉 런 필연 발생 시점에 원문 1행 수취 지시를 **#112 폰 leg 태스크(T5)
   dispatch에 상속** — 비용=#112 폰 런에서 폰 미접속 시 유계 남음" —
   recover_opt 수형(2aa03f0)이 커밋된 뒤의 폰 재런 원문이 아직 0건 —
   RECOVER 성립 원문 1행 = 다음 폰 런 몫(유계 — 폐곡 아님).
9. N6 탭 상속+폰 표기 원장행: `MUS-N6-TW-RECON`(16f-0a 신선 클라 재건
   생략 원장행 — WSL 진원 = 16e-4 필터 잔존·폰 N5 = 타이핑 스킵이라
   진원 없음)+`MUS-TW-TAB-INHERIT: OK`(활성 탭 = 태그 dir — N5 tapsettle
   진실원 계승)·**폰 label 교체 캡처 구별 불가 원장행** — 곡명 열 폭
   절단으로 캡처 문자 구별 불가 — **표기 일치의 강도 축 = 파일 수준
   round-trip(캡처 = 모양 원문 EYES)** — T5 WSL 동일 계보.

### §7.10.4 EYES 게이트 (사용자 선언만 결제 — §5/§7.9.5 원칙 동형 ·
**EYES-PENDING — 본 라인 결제는 사용자 육안/체감 선언만 성립 — 결제 상태 =
미결제(대기)**)

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **[편집] 패널 체감** | 행 [편집] 버튼→태그 편집 패널(프리필 — artist/album 태그 원문·title 부재 = 파일명 stem)·[저장](성공 = "태그 저장 — 재스캔"+목록 갱신·scan 중 비활성+"스캔 중" 표기·실패 = err 원문 1행+입력 보존)·[취소]·[커버 선택] 브라우저(타이핑 0 — #97 부품 실감)·미지원 확장자 "-" 표기 — WSL 캡처 원문 4종 보존(mus_wsl_tagwrite_list/list2/flac/filter) | **대기(육안·체감)** |
| ② | **커버아트 모양+체감** | 행 32px 썸네일(합성 cover 4색 밴드 렌더 — 회색 자리표시 대조)·편집 패널 96px 미리보기·쓰기 후 재스캔 갱신(슬롯 key mtime/size)·커버 교체(D8)의 목록 반영 — 캡처 WSL 4종+폰 3종(+crops 3종) 보존 | **대기(육안)** |
| ③ | **폰 — 태그 쓰기 체감+모양** | mus_phone_tagwrite_list/list2/flac(+crop 3종 — 4색 밴드 썸네일·"태그 저장(도구) — 재스캔" 상태행·7열 [편집] 열)·t6_grid.png/t6_music_win.png(배포 모양 — Music 아이콘·창)·폰 캐논 795 체감 — **[편집] 패널 탭 세그먼트 = 폰 스킵 원장행**(타이핑 신뢰 원장 — WSL 소유) = EYES 축 | **대기(실기기)** |
| ④ | **결정 D1-D3(사용자)+D4-D10(재량)** | 커버아트 표시+삽입 전체·4형식 전부·행 [편집] 버튼+패널 — 재량 7건(조달 0·v2.3 정규화·원자적 쓰기·지연 읽기+LRU·완전 교체·FLAC 재조립·Ogg 재번호) — **기각 시 즉시 수리** | **대기(유효)** |

### §7.10.5 커밋 원장 + IP/호스트 경로 게이트 (#111 라인)

- 라인 체인 = **662ed23(스펙) → cd17d65(플랜) → 15c2bb4(T1) → 2973933(T2)
  → c8f31f2(T3) → 5b95c63(T4) → eaba3a4(T5) → d2c91ce(T6) → aed0add(T6
  fix r1) → 2aa03f0(T6 fix r2)**+본 T7 커밋(§7.10.0 표 = rev-parse 전체
  SHA — git log가 진실원) — push 전부 origin main 직행·amend 없음.
- **IP·호스트 경로 게이트(T7 실측 — 이중)** — 라인 전 커밋 diff+리포트
  6종에서 IPv4 정규식 grep = **0건**(각 리포트 §1 원문 — T6 = "폰 주소는
  어느 파일·로그에도 인쇄 없음 — 정찰 = counts/성격만"·서브넷 표기 = /24
  마스킹 수형 — §7.9 계약 승계)·호스트 경로(드라이브 글자·사용자 루트
  폴더명·/mnt 계열) 표기 = 0건·spatial-player 경로 = **env-only 규약
  승계**(T3 diff 전체 grep 0건 원문).
- **본 T7 커밋은 docs 1파일(`docs/90_music_spatial_leg_asbuilt.md` —
  §7.10 신설+§4.1-5 부기 1행)로 한정** — git add 명시 경로만(#83 사건
  렛슨·`git add -A` 금지)·engine/probe 소스 무접촉. 런 로그(tw run1/2+
  parsebug 2종·t111_t6_scan_recon.txt)·캡처 WSL 4종+폰 3종+pmus_tagwrite
  crops 3종+t6_grid/t6_music_win 2종·드라이버/정찰 스크립트 3종
  (t111_t6_deploy/scan/chain — engine/tmp 미커밋 원장 계승 = §4.1-4 T4
  concern 4 동형)는 engine/tmp untracked — 커밋 금지 충족.
## §7.11 사용→리뷰 루프 원장 (#113 라인 — T1-T3, 2026-10-26)

#113 라인 원장 — 사이클 지도 `docs/use-review-update-cycle-map.md`의 **빈 조각
3건(①사용 저널·②수확·③체감 규약)**을 실장하는 라인. 스펙=
`docs/superpowers/specs/2026-10-26-use-review-cycle-design.md`(188a2f7 — D1-D6),
플랜=`docs/superpowers/plans/2026-10-26-use-review-cycle.md`(dd625ce),
SDD 원장=`.superpowers/sdd/2026-10-26-use-review-cycle/`(task-1..3 리포트+review
diff 2종+ollama-direct-flags-report — 미커밋 관행). 라인 원칙: 저널은 **추가만**
(D5 — events_list·read_receipts 족적 0)·probe는 소싱을 agentctl로 하지 않고
**서버 파일 직독**(T2 §계약)·리뷰 루프의 실행 주체는 세션(자동 패치 0 — D6:
"v1 = 수확+분류만 자동").

### §7.11.0 체인 (rev-parse 전체 SHA 실측 — T3 측정 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 |
| --- | --- | --- |
| 스펙 | `188a2f7` | docs(apps): 사용→리뷰→OS 업데이트 루프 설계 스펙 (#113 — 저널+수확 probe+EYES 규약) |
| 플랜 | `dd625ce` | docs(apps): 사용→리뷰 루프 구현 플랜 (#113 T1-T3) |
| T1 | `1e52392` | feat(apps): 서버 도구 호출 저널 leg (T1) — 3파일 (JKWindowServer .h/.cpp·main.cpp 2w 4건) — Win 837→841(+4)/WSL 814→818(+4) |
| T2 | `eff8f8b` | test(probes): 수확 probe jkharvest (T2) — 1파일 209행 |
| T2 fix r1 | `2fa0224` | test(probes): 수확 probe stderr 정산 — 부재 파일 wc 리다이렉트 순서 (T2 fix r1) |
| T2 fix r2 | `5fce208` | fix(probes): jkharvest FAIL 출구 잔상 — trap EXIT 정리 래더 1원 재용 (T2 fix r2) — 리뷰 Important 1건 소각 |
| T3 | 본 문서를 포함하는 커밋 | docs(apps): 사용→리뷰 루프 as-built+EYES 규약 (T3) — SHA는 git log가 진실원 |
| 병행 밖 | `ad9aa81` | feat(apps): ollama-direct run 렌더 억제 플래그 결합 (#113 소형 수형 — 계약 락 갱신) — **같은 #113 배지의 병행 소형 수형**(스파이크 후속 — 폰 배포 점유 원장 — 아래 §7.11.3) |

### §7.11.1 배선 원리 (원류 원문 1:1 수취 — 리포트 근거 · 코드 재실측 0)

- **①저널 leg(T1)** — 접점 = `JKWindowServer::HandleAgentQuery` 도구 if/else
  사슬 말미(도구 30종+와 launch/close/focus 창 관리 전부 이 1곳 — 접점 1곳만
  DRY), writer = `UseJournalAppend`(void·무예외 — 실패=조용한 스킵 D1) —
  `{"ts":ms,"tool":"..","arg":"..(≤128자 소스 시점 절단)","ok":bool,"err":".."}`
  1행+
·append+fflush·**전용 소형 뮤텍스**(기존 clientsMutex_·events·receipts
  락과 분리 — 경합 원존). ask 게이트 파킹 = `err:"parked"` 정직 표기(T1 재량).
- **②수확 probe(T2)** — `engine/tools/probes/jkharvest.sh` — 모드 2종:
  **OWN**(probe 자체 기동 — pre-clean 브래킷 pkill+-9+#98 소켓 rm 가드→setsid
  부팅→agentctl 5콜[ping·list_windows·launch_app tetris·list_windows 도착
  확인·close_window(기본 deny 게이트 = 의도된 err 행 시료)]→수확→정리
  REMNANT 0 게이트) / **OBSERVE**(폰 동형 무접촉 — 기존 서버에 ping·
  list_windows 읽기 접점만 2콜·launch/close 생략[잔상 원리 — phone_music 폰
  스킵 수형 승계]). 수확 원문 3축 = use_journal.jsonl **파일 직독** tail 20
  (JR: 접두)+bridge_report_*.txt 목록(부재=0 정직)+분류 요약(ok=false 행
  카운트+최근 10건 ERR: 원문). 판정 = `HARV-RUN-VALS`(JOG-RUN-VALS 수형 —
  probe stdout 수취 계약)+`HARVEST-VERDICT: HARVEST-OK` 1행.
- **③EYES 규약(T3)** — 본 문서 §5.1 신설 —
  `EYES-<라인/항목>: <PASS|FAIL|PENDING> — <사용자 원문 인용 1줄>` —
  형식만 규약(사용자 선언만 결제 원칙 불변)·예시 원장행 4건 동봉(§5.1 표 —
  "재생 잘 되네요" PASS·#94/#97 사용자 보고 FAIL·#114 원장행 PENDING).
- **사이클 지도 갱신(T3)** — `docs/use-review-update-cycle-map.md` §2의 빈
  조각 3건에 실장 표기(①=T1·②=jkharvest T2·③=§5.1 규약) — mermaid 구조
  유지+부기만.

### §7.11.2 캐논 계보 (2w 계열) + 수확 probe 실측 영수증

- 계보: **Win 837→841(+4=2w)·WSL 814→818(+4=2w) 등호·posix 304 등호·폰 814**
  (폰 = 구 바이너리 — 2w/2v/ollama 플래그 3레그 미흡수 — **폰 배포 금지 원장
  유지**: ollama 플래그 배치가 배포 축 점유 중 — 재배포 후 재실측 몫·그 때
  PHONE_DEPLOY_BASE 갱신 필요 — ollama-direct-flags-report §5.5). T3는
  probe 스크립트 1행+docs — selftest 어셜션 무접촉이라 재런 불요.
- **WSL OWN 수확 원문(T2 4런 + T3 회귀 1런)**: `HARVEST-VERDICT:
  HARVEST-OK` — journal 행 0→10→15→20→25→**30**(delta=5·콜 5 등호 산치
  매런)·PERSIST-ROWS-FROM-PREV-RUN 봉인(서버 재기동 간 저널 유지 — state
  dir 서수)·err 행 6(close_window permission_denied 5+failsim
  not_implemented 1 — §7.11.3 #7)·REMNANT 0·빌드(ninja) 접촉 0.
- **폰 OBSERVE 원문(T2 2런 + T3 오늘 1런 — 3런 누계)**: `HARV-RUN-VALS:
  mode=OBSERVE calls=2 journal_rows=0` + `HARVEST-VERDICT: HARVEST-OK`·
  **저널 행 0 = 정직 표기 원장행**(폰 = 구 플래그 없는 바이너리 — 저널 leg
  T1 이전 배포)·서버 pid 사슬 런 간 동일(11362..28120 — probe가 기동/종료한
  것 0의 원문)·**오늘 런 list_windows 원문**: Minesweeper 창 2종(id 42
  unfocused·id 93 focused — 사용자 생존 세션 — 읽기 접점만·무변조)·
  bridge_report 0·FONE-REMNANT-LS=0(probe 복사본 소각 확인)·**주소 미인쇄
  계약 유지**(PHONE_HOST=known_hosts BatchMode 자기판명 — log/probe 어디에도
  주소 0).
- **fix r2 실험 원문(T3 — 재런 불요 판정의 보강 원문)**: OK 런 1회(리그레션
  — HARVEST-OK·REMNANT 0·HARV-TEARDOWN 1회 인쇄 = 래더 1원)+failsim 1런
  (engine/tmp 스크래치 사본 — ping→pingxx 치환 — **파일 본체 무편집**) —
  서버 `not_implemented` 회신 → `HARVEST-FAIL` → `HARV-TEARDOWN-LEFT: 0`
  → 종료 후 pgrep -af jkdesktop = 0 실측(RC=1). 조기 fail 경로
  (binary missing)도 trap 부재 no-op 실측(BOOTED=0 — 정리할 것 없음).

### §7.11.3 함정 원장 (#113 신규 — 봉쇄 레슨 축 · 전량 = 리포트/concerns 원문 수납)

1. **FAIL 출구 잔상(T2 리뷰 Important — fix r2 5fce208 소각)** — 원 구조에서
   모든 FAIL 출구가 exit 1 직행이라 자기 서버 정리 없이 남았다. 수형 =
   `trap harv_exit_teardown EXIT`(성공 경로 §9와 **래더 1원** — CLEANED 가드
   로 중복 실행 없음·`HARV_BOOTED` 가드로 **자기가 살린 것만 죽인다** —
   OBSERVE/부팅 전 fail = no-op, 폰 잔상 0 계약 유지).
2. **부재 파일 리다이렉트 순서(T2 fix r1 2fa0224 소각)** — `wc -c < f
   2>/dev/null`은 `<` 개방 실패가 `2>` 적용 전 발생해 stderr 1행을 흘린다 —
   `[ -f ]` 가드로 소거.
3. **grep -c 무매치 exit 1 폴백 2행 함정(T2 소각)** — err 산치는 awk로
   (grep -c 무매치 시 `|| echo` 폴백이 2행을 내놓는다).
4. **드라이버 명명 부정확(T2 원장행)** — "no-op guard"로 명명한 명령이 실제로
   probe를 1런 실행했다(행 10→15) — 명명 부정확 = 원장 사건.
5. **WSL pre-clean = 라이브 데스크톱 서버 종료 접점**(wsl_* 선형 원존) — OWN
   모드 pre-clean이 동빌드(buildwsl) 라이브 서버를 죽인다 — 오늘 실측 시점
   라이브 jkdesktop 부재라 무접촉 성립, 존재 시 접점은 원존 계약.
6. **폰 배포 병행 금지 원장** — ollama 플래그 배치가 배포 축 점유(2w/2v/플래그
   3레그 흡수 = 증분 배포 1런 — PHONE_DEPLOY_BASE 갱신 몫) — 본 라인 폰 접점
   = OBSERVE 읽기만.
7. **fix r2 실험의 부수 원문** — failsim 런이 저널에 `not_implemented` err
   행 1행을 추가했다(probe 소유 — 분류 요약의 err 시료가 deny 1종에서 2종
   으로) — 의도치 않은 다양성 성과 원장행.

### §7.11.4 v1 비-목표 + EYES 게이트

- **v1 비-목표(스펙 §비-목표 원존)**: 저널 UI(앱 화면)·저널 원격 전송·자동
  패치 실행(패치는 세션 승인률 유지 — D6)·폰 무접속 자동 재시도 루프·성능
  프로파일러 통합·저널 회전/상한(T1/T2 concern 승계 — §4.1-8).
- **EYES 게이트 — 수확 리포트 모양 체감 = 사용자 선언만**(플랜 사용자 게이트
  1): `EYES-#113 수확 리포트 모양: PENDING — 사용자 선언 미접수(규약 = §5.1
  형식 — 결제 상태 3치뿐)` — D1-D6 재량 = 기각 시 즉시 수리.

### §7.11.5 커밋 원장 + IP/호스트 경로 게이트 (#113 라인)

- 라인 체인 = §7.11.0 표(188a2f7..본 T3 — rev-parse 전체 SHA 원존 — git log가
  진실원) — push 전부 origin main 직행·amend 없음·**커밋 = 명시 경로만**
  (T1 3파일·T2/fix 1파일·T3 = jkharvest fix r2 1파일+docs — `git add -A` 0).
- **IP·호스트 경로 게이트(T3 실측)** — 본 커밋 diff 전체에서 IPv4 정규식
  grep = 0건(커밋 전 staged 원문 게이트)·폰 주소 = PHONE_HOST 미기록(런
  로그 `engine/tmp/j113_phone_jkharvest_observe.log` 포함 전 리포트/log
  주소 0건 — known_hosts BatchMode 자기판명 수형)·호스트 경로 표기 =
  probe 스크립트 상대 경로만(buildterm/buildwsl — 커밋 무해).
- 본 T3 커밋 = **probe 1파일+docs 2파일**(`engine/tools/probes/jkharvest.sh`
  fix r2·`docs/90_music_spatial_leg_asbuilt.md` §4.1-8/§5.1/§7.11·
  `docs/use-review-update-cycle-map.md` §2 갱신) — 런 로그
  (j113_phone_jkharvest_observe.log)·failsim 스크래치(소각 완료)는
  engine/tmp untracked — 커밋 금지 충족.
## §7.12 opus/m4a 태그+커버아트 원장 (#115 라인 — T1-T4(+fix r1), 2026-10-26)

opus/m4a 라인(#115) 원장 — 백로그 등재("opus/m4a"·갱신 58 이후 지시 대기) =
사용자 "최대한 구현 진행" 지시(2026-10-26 출근)의 착수. **music 라이브러리에서
opus/m4a가 "-" 폴백 행**(#99 읽기 4형식(mp3/FLAC/Ogg/WAV)의 밖)을 두 형식의
**태그 읽기+쓰기+커버아트**로 사슬 병합한다. 스펙=
`docs/superpowers/specs/2026-10-26-opus-m4a-design.md`(944c736 — D1-D6),
플랜=`docs/superpowers/plans/2026-10-26-opus-m4a.md`(944c736 동커밋), SDD
원장=`.superpowers/sdd/2026-10-26-opus-m4a/`(progress·task-1..4 리포트+리뷰
diff 4종). 재생 디코드 = **v1 비-목표(D6)** — spatial-player/decoder 조달 =
별도 합의 라인(플레이버 밴드 "-"·"태그만 지원" 표기 관행 원존 [#110]). 라인
부품 계승: **#99 읽기 사슬**(ParseTag 스니프 분기 확장·PostprocessText/UTF-16
BOM 계약)·**#111 쓰기 사슬**(TagAtomicWrite 사다리·ValidateCover 재용)·
**#112 Ogg 재조립**(PushPacketLacing/BuildOggPage/OggCrc·D10 granule 불변)·
**#92/#97/#99 도구 경계**(music_tag_write relay·스캔 1패스)·**#94 UTF-8
규약**. T4(본 태스크) = probe 세그먼트+폰 배포/캔논+본 문서 — 캔논 수치는
전 태스크 리포트+본 런 원문 수취. 서브에이전트 스폰 0건·worktree 미사용·
main 직행.

### §7.12.0 체인 (rev-parse 전체 SHA 실측 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 (numstat) |
| --- | --- | --- |
| 스펙+플랜 | `944c736ee777914b6c1d40469199c2f724610d61` | docs(apps): opus/m4a 태그+커버아트 스펙+플랜 (#115) — D1-D6(조달 0·ilst 재조립·OpusTags 재용·covr/MBP 병합·UI 무변경·재생 비-목표) |
| T1 | `20e3abf9a8409b9d6257ae47c5b24898f85665ae` | feat(apps): m4a 태그 읽기 파서+2x 시작 — MusicModel.h 138/2·main.cpp 207/0 — Mp4BoxWalk/Mp4BoxAt/Mp4FindBox/DecodeM4aValue/ParseM4aTags+ParseTag ftyp 분기 |
| T2 | `95cb8c6cb04e3e6bdbfd83fa3b49e62c421ae3d2` | feat(apps): opus 태그 읽기+커버아트 2형식 — MusicModel.h 287/41·main.cpp 350/5 — OpusTagsPacket(lacing 인지)/OggPacket1IsOpus/Mp4FindIlst 리팩터/ParseMp4Covr/CoverFromMbpEntries/ReadCover 분기 2건+IsAudioExtension 확장 |
| T3 | `840ea413a475daee876403dc9edd6873503de494` | feat(apps): opus/m4a 태그 쓰기 — WriteTag 사슬 병합 — MusicModel.h 504/26·main.cpp 482/0 — WriteM4aTag(moov 재조립)/BuildM4aIlst/Mp4ReshuffleChildren/Mp4LocateTagTree(스테이지 계보·신설 폴백)/WriteOggCommentTag(isOpus 공용 코어) |
| T3 fix r1 | `ce3f53942aeb32edf055a5c721e6c193da042943` | fix(apps): WriteM4aTag — stco/co64 Δ 패치+free 흡수+거부 경계 (C1 수형) — MusicModel.h 167/0·main.cpp 153/4 — 2x-⑫b(+2 어설션) |
| T4(probe) | `bfe20119078b1ff3576bda400ee7d0b2c51a0eb6` | test(probes): music probe opus/m4a 세그먼트+폰 캔논 계보 갱신 — wsl_music.sh 525/1·phone_music.sh 597/17 — 16f-OM+N7+F0b(WHISPER_ROOT 승계 배선) |
| T4 fix r1 | `a43e4b05aebbd94ec11ab9ab2ad21f6b0e2c4996` | fix(probes): music WSL probe — OM cover B64 리터럴 절단 수리 — wsl_music.sh 1/1 |
| T4 fix r2 | `083997ca7820d9ba0015635544b775f11aa8847f` | fix(probes): music probe — OM 재독파서 cmt_htype 오프셋 수리+폰 계보 행 정산 — wsl 3/3·phone 3/3 |
| T5 | 본 문서를 포함하는 커밋 | docs(apps): opus/m4a as-built (T4) — SHA는 git log가 진실원 |

### §7.12.1 배선 원리 (D1-D6 — 리포트 원문 1:1 수취 · 코드 재실측 0)

- **D1 조달 0** — 두 형식 전부 own 순수 파서(third_party 0건): m4a = MP4
  컨테이너 box 보행(`Mp4BoxWalk`/`Mp4BoxAt` — [4B size BE][4B type]·size=1
  largesize(64BE)·size=0 말미 영역·잘린/배신적 size(선언 < 헤더) = 정직
  거부·보행 상한 4096)+DecodeM4aValue(data atom 본문 = version 1B+flags 3B
  [타입 코드 = p[3]]+locale 4B+값 — 1=UTF-8·2=UTF-16(BOM 우선·**무BOM = BE
  — Apple 타입 2 = UTF-16BE 명시 규약 — T2 리뷰 정정**)+ParseM4aTags(ftyp→
  moov→udta→meta(hdlr 'mdir' 확인 — handler_type = payload [8..12))→ilst→
  ©nam/©ART/©alb 1바이트 0xA9 접두 항목·첫 data atom=첫 값). opus =
  `OpusTagsPacket`(OggS 페이지 연쇄 페이로드 연접+lacing 인지 보행 — **패킷
  완성 지점에서만 "OpusTags" 매기 — 오디오 패킷 내 우연 일치 비매기**)+
  ParseVorbisComments 그대로 재용(코멘트 구조 = vorbis 동형 — D1 조달 0). 
  어떤 파산이든 빈 TagSet 폴백(부분 필드 소각 — 2x-③).
- **D2 m4a 쓰기 = moov 재조립·mdat 원문 무이동** — 최상위 box 스트림 보행
  (버퍼 무이동)·moov 본문만 버퍼 수취 → `Mp4LocateTagTree`(stage 계보
  Ok/NoMoov/NoUdta/NoMeta/NoHdlr/BadHdlr/NoIlst) → BuildM4aIlst(관리 항목
  첫 자리 교체·빈 값 = 소각·미관리 항목 보존·covr = 지정 시 전부 소각+말미
  신설/null = 유지) → 신설 폴백(NoUdta→udta{meta{4B+hdlr(mdir)+ilst}} 등
  순차 신설 — 부재 부모 계보) → box 사이즈 전파 자식→부모(moov/udta/meta/
  ilst 4단)·free atom = 원문 보존(사이즈 전파 성립 — 패드 재계산 불요)·
  moov 밖 box = CopyStreamRange 스트림 복사(비-faststart = head1/tail 분리
  — **moov 위치 무관 쓰기 성립**)·원자적 쓰기 = TagAtomicWrite 사다리 재용
  ·covr 상한 = kTagWriteMax 16MiB+ValidateCover 사전 게이트.
- **T3 fix r1(ce3f539) — stco/co64 청크 오프셋 패치(C1 수형)**: moov
  재조립 Δ = 신구 moov 크기 차 → moov 뒤 mdat(faststart 계열) 절대 위치가
  Δ 이동하면 moov 내 stco/co64 청크 오프셋(= mdat 내 절대 파일 오프셋)이
  Δ 만큼 같이 이동해야 한다 — **태그 자체 재독은 머리 한정이라 못 보는
  블라인드 스팟**(외부 플레이어 재생 인덱스 파산 — C1 원문). ①free/skip
  흡수 leg: moov 다음 free/skip box가 Δ를 삼키면 mdat 자리 고정(파일
  총량 불변·무패치 — Δ>0 = payload 말미 절단·Δ<0 = zero pad 확정·부족
  시 다음 leg) ②무free = stco(32BE)/co64(64BE) 항목 Δ 가산 패치 — 보행 =
  표준 컨테이너 사슬 한정(trak→mdia→minf→stbl — depth 0-3 재귀) ③patches
  부족 = "free 부족" 정직 거부(원본 무변경)·stco/co64 부재 = 무패치 진행
  (청크 오프셋 참조 부재 — 최소 파괴). **T4 fix r3 재심사 유계 3건**:
  (u1) ①의 "파일 총량 불변"은 설계 서술로만 성립 — 2x-⑫b(b)의 명시
  어셜션 부재(`bufB.size()==blobB.size()` — mdat가 말미 box인 지형에서
  총량 오류가 무음 통과할 여지 — 유계)·(u2) covr payload 내 우연 "stco"
  매agic의 봉인은 보행 설계(표준 컨테이너 사슬 한정)의 서술적 입증만 —
  covr 포함 stco 시퀀스의 전용 fixture 미구축(유계)·(u3) **co64 패치 leg =
  대칭 2행 구현·실측 부재** — 2x-⑫b는 stco만 합성(mkStco·co64 합성 0 —
  "stco/co64 Δ 패치" 표기는 co64가 실측된 것으로 오독 금지 — 유계).
- **D3 opus 쓰기** = `WriteOggCommentTag(…, bool isOpus, …)` 공용 코어
  리팩터(#112 WriteOggTag 시그니처 불변 래퍼 원존) — 코멘트 패킷 매agic만
  갈림(leadN = 7(vorbis \x03"vorbis")/8(opus "OpusTags"))·페이지 재번호+
  CRC+D10 granule 불변+**코멘트 페이지 htype 원문 유지**(컴멘트 페이지 EOS
  표지 0x04 = 2x-⑪ 원문)·비선두 배치 = "ogg structure" 정직 거부(RFC 7845
  — 코멘트 페이지 페이로드 선두 개시 계약)·벤더 유지(D9)·MBP = oggPic leg
  재용(완전 교체 흡수 — D8).
- **D4 커버아트 병합** — m4a covr: `ParseMp4Covr`(data 타입 코드 13=JPEG/
  14=PNG(iTunes 규약)+**코드 0(binary) = 이미지 매agic 스니프(FF D8 FF/
  89 "PNG\r\n\x1a\n") — taglib/ffmpeg 계열 binary 0 표기 흡수**)·opus MBP:
  `CoverFromMbpEntries`(METADATA_BLOCK_PICTURE base64 →
  ParseFlacPictureStruct — vorbis leg 부품 공용화). ReadCover 분기 2건 —
  covr leg 머리 read 상한 = kTagWriteMax(커버 블롭이 moov 안 — 스캔 1패스
  상한 kTagHeadCap 1MB와 분리)·부재/파산 = "no_cover" 정직.
- **D5 UI/도구 무변경** — 5→7열(커버+편집 열) 기존 UI 그대로·opus/m4a 행이
  "-" 대신 실값 표기로 전환(IsAudioExtension 확장 — .opus/.m4b/.mp4 추가·
  **스캔 열거/파싱의 분리**: 확장자 = 행 통과 열거, 파싱 = 매agic 진실원 —
  밀표 파일도 파싱 ·비디오 mp4는 ilst 부재·비-'mdir' hdlr로 걸러짐 —
  **오디오 메타만 보므로 ftyp+ilst 합격 = 오디오 행 취급** — T2 본문 주석
  ·T1 기각 확장자 분기 "IsAudioExtension+매agic 분담" 정정 계보)·
  music_tag_write 도구 무변경(WriteTag 내부 몫).
- **D6 재생 비-목표 원장행** — opus/m4a 재생 = spatial-player/decoder 조달
  라인(m4a AAC·opus 디코드) — 별도 합의·플레이버 밴드 "-" 표기·"태그만
  지원" 관행 원존. 본 라인 전 태스크 = 순수 메타 leg(스폰 0 — selftest
  스폰 계약 원존).

### §7.12.2 캐논 계보 (2x 계열) + probe 실측 영수증 (T4)

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#115 라인 개시 = 4de04d0) | 841 | 818 | 304 | 818 | — (2v 14·2w 4·2u 132·2r 10·2s 20·2m 26·2n 17·2o 16·2p 7·2q 7·2i 19·2g 40 보존) |
| T1 `20e3abf` | **844** | **821** | 304 | (818) | +3 (2x ①②③) |
| T2 `95cb8c6` | **849** | **826** | 304 | (818) | +5 (2x ④-⑧) |
| T3 `840ea41` | **854** | **831** | 304 | (818) | +5 (2x ⑨ 2어설션·⑩⑪⑫) |
| T3 fix r1 `ce3f539` | **856** | **833** | 304 | (818) | +2 (2x-⑫b 2어설션 — Win 856/WSL 833 2런 등호) |
| T4 본 라인(probe+bfe2011/a43e4b0/083997c) | 856 | **833** | **304 (승계)** | **833** | 폰 2x 15 흡수 — **CANON-INCLUSION=MUSIC-FULL-2P2QRS-15** (폰 캔논 818→833 — WSL 833 등호) |

- **2x 15의 분해** = ①무태그 m4a 폴백 ②m4a 태그 읽기(UTF-8+UTF-16 BE/LE+
  무BOM BE) ③잘린 box 정직 거부 ④합성 opus 무태그+OggPacket1IsOpus 스니프
  ⑤opus 태그 읽기 3필드 ⑥m4a covr round-trip(png/jpeg·코드 13/14) ⑦opus
  MBP round-trip ⑧잘린 covr/MBP 정직 거부 ⑨m4a 쓰기 round-trip+신설 폴백
  (2어설션) ⑩covr 교체 round-trip(null 유지/지정 교체/opus MBP 신설)
  ⑪opus 쓰기 round-trip+페이지 시퀀스(재번호+CRC+granule 불변+EOS 유지)
  ⑫한글 2형식 round-trip ⑫b stco/co64 Δ 패치(3 경계 — fix r1) — **라벨
  13건/어설션 15**.
- **posix 304 = 구조적 승계 등호** — 하네스 TU 목록에 MusicModel.h 비링크
  (T1 M2 원장 승계 — 본 라인 4커밋 무재런 — 재실측 불요 사유가 구조로
  성립).
- **T4 WSL probe 원문(16f-OM — engine/tmp/wsl_music_run115_{r4,r5}.log —
  2런 등호)**: `selftest rc=0 PASS=833 FAIL=0`·`#115-RUN-VALS: run=2
  stage=1 writes=1/1/1/1 covr_1=covr_2=mbp_3=mbp_4=90155ae6b2bb6f91691fe7724d
  b9b165 vendor=om-vendor cover_md5=90155ae6… mdat=64 canon_wsl=833(2x=15·
  2u=132·2s=20·2m=26·2v=14·2w=4)`·`MUSIC-OPUSM4A-VERDICT: OPUSM4A-OK` 2/2
  ·합성 m4a+opus(2x 픽스처 승계) relay 쓰기 4콜(m4a covr 교체+cover=null
  유지·opus MBP 교체+cover 생략 유지)==파일 수준 round-trip md5 등호 4콜+
  벤더 유지("om-vendor")+코멘트 페이지 EOS 0x04 유지+**mdat 원문 무이동
  (0xAB 64B — D2 라이브 원문)**·OM 시드 잔상 0(17e 가드)·목록 행 캡처 3종
  (mus_wsl_om_list/list2/opus.png — EYES).
- **T4 폰 배포 런 1회(t115_t4_deploy.sh — engine/tmp 산출물 — t112_t5 수형
  재용·커밋 0 — engine/tmp/t115_t4_run.log)**: 전수 git archive HEAD
  (SWEEP-DIFF 12/472 → **SWEEP-POST-DIFF=0**)·CMAKE-RC=0·NINJA-RC=0
  SECONDS=463(aarch64 — leg env 유지배선+WHISPER_ROOT —
  WHISPER-BUILD-NINJA-ENTRIES=297)·`DEPLOY-MARKERS: 2r=11 2s=21 2u=136
  2v=20 2w=4 2x=15`(라벨 계수 — 본문 주석 포함)·MUSP-LEG-SYM
  alcOpenDevice=3(폰 spatial leg WIRED 보존 — #110 계약)·**조달기 스킵
  원장행**(tiny-ko 상존 — `TINYKO-PRESENT: 77691730 bytes 핀 등호` —
  #112 T5 R4 조달 원존 계보)·**selftest A/B = rc=0 PASS=833 FAIL=0
  2u=132 2r=10 2s=20 2v=14 2w=4 2x=15 2런 등호 — CANON-A/B=833(WSL 833
  등호)**·music.jkx 재팕(PACK-RC=0·ICON-ENTRIES=2)·서버 재기동=
  **WHISPER_ROOT+SPATIAL_PLAYER_ROOT 보유 부팅**(구두/spatial leg 클라
  상속 계약 승계 — 사용자 live 세션 leg 유지)·settings 213B/permissions
  33B 무변경 바이트 등호·jkweb 상시 유지(BEFORE=AFTER 2프로세스)·Music
  창 복원(FINAL-WINDOWS)=사용자 화면 승계.
- **T4 폰 probe 동형 2런(N7 — engine/tmp/phone_music_run115_{r1,r2}.log)**:
  **폰 캔논 재실측 = 833**(`PHONE-SELFTEST rc=0 PASS=833 FAIL=0 2m=26
  2i=19 2g=40 2n=17 2o=16 2p=7 2q=7 2r=10 2s=20 2u=132 2v=14 2w=4 2x=15`·
  `CANON-INCLUSION=MUSIC-FULL-2P2QRS-15 (폰 캔논 818→833 상승 — WSL 833
  등호)` 2런)·N7 = **OPUSM4A-PHONE-OK(r2)** — OM 시드 랜딩(N6 소각 뒤)+
  relay 쓰기 4콜+재독 round-trip(md5 등호 4콜=90155ae6…·htype=4·mdat
  0xab=64)+목록 행 캡처 3종(mus_phone_om_list/list2/opus.png — EYES)·
  OM 시드 잔산 gone(REMNANT 게이트)·r1 = **OPUSM4A-FAIL(honest — probe
  재독파서 cmt_htype 오프셋 결함 — T4 fix r2 소각 — 쓰기 4콜+md5 등호
  원문은 성립)** — 런1 원문 정직 수취 원장.
- **캔논 4축 최종: Win 856/WSL 833/posix 304/폰 833 — 2x 15 전부 흡수**.

### §7.12.3 함정 원장 (#115 신규 — 봉쇄 레슨 축 · 전량 = T1/T2/T3+T4 리포트/concerns 원문 수납)

- **합성 블롭이 파스 계약보다 앞서 "성공"하는 함정(T1 — mkData 12B 과다)**:
  data atom 헤더를 version 4B+flags 4B+locale 4B = 12B로 짝은 합성 — Apple
  실계약 = version 1B+flags 3B(=4B)+locale 4B. ①③은 우연 통과(파서가 항상
  p[3] 파산 leg) — 정직 합성(2x-②류가 지목). T2 mkOpusHead 4B 과다(17B
  합성 — preskip+srate 접기)도 시험 전 선수행 수리 — **합성은 파서 접점
  계약의 1:1 재작성**(probe 2x 픽스처 재용 계약의 원존).
- **확장자 분기 기각(T1)→IsAudioExtension 확장(T2 지시)**: ftyp 매agic이
  진실원(ISO-BMFF 규약 — ftyp는 항상 첫 박스)·확장자 보조 분기 = dead code
  소진(T3 fix r1 선례). 스캔 열거(.opus/.m4b/.mp4)와 파싱(매agic)의 분담 —
  .m4b/.mp4의 UI 행 취급 = 오디오(ilst 부재·비-'mdir' hdlr 필터 — 비디오
  mp4 원문 무해).
- **moov 후행 vs 머리 1MB 상한 — 양측 비대칭 원장행(T1 §5·T3 §5 승계)**:
  읽기(ParseTag) = 머리 kTagHeadCap 1MB 1패스(D2 스캔 계약) — **moov가 머리
  1MB 밖(비-faststart 대형 mdat 선두 mux) = ParseM4aTags가 moov 미지목 →
  빈 TagSet 폴백 → "-" 행 유지**·무회귀·정직이나 커버 못함(mdat 건너뛰기
  2차 read = 스코프 밖 — 별도 재판정 소재: **사용자 실기기에서 .m4a "-"
  잔여행 관찰 시** — 대형 비-faststart 파일의 관찰 EYES에 병기). 쓰기 leg
  (WriteM4aTag)는 moov 위치 무관 성립(최상위 box 스트림 복사 — head1/tail
  분리) — **쓰기 성공 그런데 자체 라이브러리 표기 "-" 잔여**의 비대칭 =
  본 원장행(fix r1의 stco 패치도 머리 1MB 안의 재독 전제).
- **data atom = box(payload 아님) — T3 핵심 함정**: BuildM4aDataAtom의
  반환은 data box의 payload인데 ilst 항목에 직접 심어 box 헤더 누락 →
  재독 파산(selftest ⑨ 3회 FAIL — tree dump 분석: ©nam 항목 27B = 1층
  소실). 수리 = BuildM4aDataBox(box 래퍼 내장). T1 mkData 12B 과다와
  짝짓는 계보 계약("atom = box").
- **ReadCover covr 머리 상한 kTagWriteMax(16MiB) 분리(T2)** — 커버 블롭이
  moov 안이라 스캔 1패스 상한(1MB)과 분리 — 대형 mdat 선두 비-faststart
  mp4의 covr = no_cover(태그와 동일 착지 — 무회귀)·covr 그 밖 image/* =
  타입 코드 0(binary)+매agic 스니프 leg 원존.
- **moov 부재/ BadHdlr = 쓰기 거부(T3 재량)**: 부재 부모 신설 계보 =
  udta/meta/ilst까지·moov 신설 = 재생 트리 파괴 위험 fail-closed("mp4
  structure" 정직 거부·원본 무변경)·BadHdlr('mdta' 등 비-'mdir') = 타
  핸들러 표기 침해 봉인(신설 udta 우회도 Mp4LocateIlst가 첫 meta만 걷는다
  — 도달 불가 재제작 방지).
- **복수 ilst/udta/covr = 첫 지목만 교체·나머지 원존 보존(T3 재량)** —
  복수 규약 부재 = 최소 파괴(FLAC 복수 코멘트 소각(D8)과 다른 착지 —
  원장행).
- **C1 블라인드 스팟(외부 플레이어 재생 인덱스) — T3 fix r1(ce3f539)**:
  태그 재독 자체는 통과하지만 moov 재조립 Δ 이동이 mdat 절대 오프셋을
  삼킨다 — stco/co64 Δ 패치+free/skip 흡수+부족 시 정직 거부 3경계
  (2x-⑫b — (a) 무free Δ 패치(b) free 흡수 = mdat 자리 고정·stco 원존
  (c) 상한 근접+무free = 거부·원본 무변경) — **태그 쓰기의 재생 인덱스
  계약 = 재독으로는 검증 불가한 별도 축**이라는 레슨(probe도 파일 수준
  round-trip만 검증 — 재생 인덱스는 selftest 2x-⑫b가 소유)·**fix r3
  재심사 유계 3건**: (u1) (b)의 "파일 총량 등호" 명시 어셜션 부재
  (`bufB.size()==blobB.size()` — mdat 말미 box 지형의 총량 오류 무음
  통과 여지 — 유계 1행·§7.12.1에 수식 부기)·(u2) covr-내 "stco" 우연
  매agic = 구조 봉인(보행 설계)으로만 입증 — covr 포함 stco 시퀀스의
  전용 fixture 미구축(유계 1행)·(u3) **co64 leg 미실측** — 2x-⑫b는
  stco만(mkStco·co64 합성 0) — "stco/co64 Δ 패치" 표기 = co64 대칭 2행
  구현·실측 부재(유계 — 차기 selftest 재판정 몫).
- **probe 결함 3건(T4 — 전부 원문 소각)**: ①OM_COVER_B64 수기 재타이핑
  4자 절단(1252 vs 1256 — 디코드 938B 미성립 jpg — FFD8만 생존해 도구
  게이트는 통과 — "동일 바이트" 원장행 대조 불일치로 검출 — 수리 =
  프로그램 복사(a43e4b0) — **B64 인라인 상수의 재용 = 수기 재타이핑
  금지**)②재독파서 cmt_htype off-by-one(OggS 페이지 = [0..4) capture·
  [4) version·[5) header_type — off+4 = version 0 오판 — **3런 동결함·
  판정 표기는 양분(정정 — fix r3)**: WSL r2/r3 = 결함 판정 행
  (`MUSIC-FAIL(om-mbp-rt … htype=0)` — honest 행)만 인쇄하고 verdict행은
  구 게이트(OM_ALL htype 미채용)가 통과시켜 **OPUSM4A-OK false-OK 원문**
  (pre-fix 게이트 결함 — 18i 조건 누락)·**OPUSM4A-FAIL verdict = 폰 r1
  1건**(폰 N7 게이트는 mbp_rt 경유 htype을 본조건으로 이미 소비) —
  수리 = [5) 오프셋+WSL 18i OM_ALL 본조건 채용(083997c — 양측 판정 일치)·
  post-fix WSL r4/r5 = OPUSM4A-OK(htype=4 본조건 포함)·폰 r2 =
  OPUSM4A-PHONE-OK)③wsl.exe env 전파 함정(MUS_OM_RUN 라벨 — WSLENV "VAR/u"
  접미 없이는 wsl.exe가 env를 전파하지 않는다 — T2 원장(스크립트 파일
  경유)의 env 확장 수형 — r5부터 run=2 표기 성립).
- **phone_music.sh boot_server의 WHISPER_ROOT 누락(구 수형 — 본 라인
  수리)**: #112 T5가 WHISPER_ROOT 보유 부팅(구두 게이트 클라 상속 계약)을
  서버 부팅 env 계약으로 세웠으나 phone_music.sh의 boot_server는
  WHISPER_ROOT를 배선하지 않았다 — 본 T4 폰 런이 구두 leg-less 부팅으로
  사용자 live 세션(구두 게이트)을 다운그레이드할 위험 — **F0b 수리**
  (vendored 트리 상존 시 WHISPER_ROOT export — boot 승계 — bfe2011)·
  런 후 실측: 부팅 서버 env WHISPER_ROOT+SPATIAL 유지 원문(§7.12.2)·
  **원장행(1행 — fix r3)**: 부팅 env 종단 영수증 = WHISPER_ROOT만 원격
  영수증 행에 실측(`WHISPER-ROOT-AT-END`)·**SPATIAL_PLAYER_ROOT 부팅
  env의 종단 영수증 부재**(본 라인 = probe 수동 실측 원문(/proc environ —
  런 중 재조사)만 — 종단 행 = 차기 폰 런 몫).
- **폰 서브넷 이동 재발(갱신 65 유형)**: known_hosts 구 후보 4건
  접속 실패(BatchMode MISS — 구 서브넷 /24에서 8022 listeners 0 — 구
  서브넷 MISS 원장행· octets 미기록 — 기록 금지 계약) → 본 머신의 현
  Wi-Fi 서브넷 재정찰(subnet /24 스윕 = 8022 listeners 1건) → 리스너
  BatchMode SELFRESOLVE-OK(주소 미인쇄 계약 — scan 스크립트 = engine/tmp
  산출물·accept-new 1회 재매개).
- **t115_t4_deploy.sh verdict "bad substitution"(드라이버 몫 — 원장행)**:
  판정 행 전개식의 토큰 치환 표기 결함(CANON A/B 결합 표기) — remote
  영수증(T4-FINISHED·CANON-A/B=833 원문행)은 완전·판정 무영향 — pmus/
  finres 선례("판정 토큰 파싱 표기 결함(드라이버 몫)") 동류 원장행.
- **폰 N4 ⑤ 무전달 hard 마커(갱신 65 승계 — 유계)**: [이 폴더 추가]
  클릭 무전달 재발(2런) — 말단 rc=1 종결(INH_HARD — T6 수형 원존 — 본
  라인 세그먼트 산치(N6·N7·캔논·REMNANT 0)는 그 위에서 정직 정산 — 탭
  좌표 재보정 = 별소관 원장행).

### §7.12.4 비-목표 + EYES 게이트 (사용자 선언만 결제 — §5/§7.9.5 원칙 동형)

- **비-목표(D6/스펙 §3)**: 재생 디코드(m4a AAC/opus)·AAC 디코더 조달·
  m4b 챕터/가사/년도 필드·iTunes 확장 외 표기·폰 라이브러리 대형 스캔
  성능 특화.
- **EYES 대기**:

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **.m4a/.opus 행 실기기 체감** | Win/폰 라이브러리에서 합성이 아니라 **사용자 실제 .m4a/.opus 파일** 행 = "-" 대신 곡명(아티스트)·앨범 실값 표기(스캔 열거 .opus/.m4b/.mp4 포함 — 행 등장 승인 게이트) — probe 캡처(mus_wsl_om_*.png·mus_phone_om_*.png)는 합성 원문 | **대기(육안·체감)** |
| ② | **커버아트 체감** | 행 32px 썸네일+[편집] 패널에서 실기기 커버아트(태그 편집 저장 round-trip — T3 배포 UI의 opus/m4a leg 체감) | **대기(육안·체감)** |
| ③ | **재생 밴드("-" 유지) 체감** | .opus/.m4a 행 재생 = vplayer 위임 openFailed/spatial 위임 표기 유지("태그만 지원" — D6 원존) | **대기(육안)** |
| ④ | **moov-후행 대형 m4a 관찰** | 사용자 실기기에서 비-faststart 대형 .m4a(>1MB 머리 밖 moov)의 "-" 잔여행 관찰 시 §7.12.3 원장행의 재판정 소재(2차 read 확장 여부) | **대기(관찰)** |

### §7.12.5 커밋 원장 + IP/호스트 경로 게이트 (#115 라인)

- 라인 체인 = §7.12.0 표(944c736..본 T5 — rev-parse 전체 SHA 원존 — git
  log가 진실원) — push 전부 origin main 직행·amend 없음·**커밋 = 명시
  경로만**(T1-T3/fix r1 = MusicModel.h+main.cpp 2파일·T4 probe = probe
  2파일·T4 fix r1/r2 = probe 1/2파일·본 T5 = docs 1파일 — `git add -A`
  0·커밋 분리 원존(probe 커밋+docs 커밋).
- **IP·호스트 경로 게이트(T4 실측)** — probe 2파일 커밋 원문에서 IPv4
  정규식 `\b([0-9]{1,3}\.){3}[0-9]{1,3}\b` grep = **0건**(커밋 전 staged
  원문 게이트 — 서브넷 스윕 실측의 주소도 커밋 원문 0건(octets 미기록)·로그=
  engine/tmp/t115_scan2.err 자리표시 마스크 원존)·폰 주소 = scan 스크립트
  변수 안 소멸·호스트 경로 표기 = probe 스크립트 상대 경로만(buildterm/
  buildwsl — 커밋 무해).
- 배포 드라이버(t115_t4_deploy.sh)·mk_t115*.py·런 로그
  (wsl_music_run115_r{1..5}.log·phone_music_run115_r{1,2}.log·
  t115_t4_run.log·t115_t4_deploy*.log)·OM 캡처(mus_wsl_om_*.png·
  mus_phone_om_*.png) = engine/tmp untracked — 커밋 금지 충족.

## §7.13 구두 어휘 확장 원장 (#115b 라인 — T1-T2(+fix r1/r2), 2026-10-27)

#115b 라인 원장. LLM 없이 즉발되는 구두 어휘를 재생(vplayer)+알림 도메인으로
확장(스펙 D2 — 리서치 docs/voice_os_deep_search.md 권장 순서). 스펙=
`docs/superpowers/specs/2026-10-27-voice-vocab-design.md`(fe5e8c9 — D1-D6),
플랜=`docs/superpowers/plans/2026-10-27-voice-vocab.md`(defe70b),
SDD 원장=`.superpowers/sdd/2026-10-27-voice-vocab/`(task-1/2 리포트 — 미커밋
관행). T1 리포트가 어휘 표·접점 실측 표·AppTool 소비 계약의 원문 수취원 — 본
§7.13.1은 그 1:1 재수취+T2 probe/폰 배포 원문.

### §7.13.0 체인 (rev-parse 전체 SHA 실측 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 (numstat) |
| --- | --- | --- |
| 스펙 | `fe5e8c921daa911f6ee8d7281970bbc8e2dc1a26` | docs(apps): 구두 어휘 확장 v1.5 설계 스펙 (#115b — 무인자 우선·재생+알림 도메인·도구 부재 어휘=원장행) — D1-D6 |
| 플랜 | `defe70b5d25853cdc7edacbeba2150ae1f9681ee` | docs(apps): 구두 어휘 확장 v1.5 플랜 (#115b T1-T2) + #115 폐곡 정산 |
| T1 | `21ee4ddda3fec366f6953d5ebafacb03281de34e` | feat(apps): 구두 어휘 확장 — 재생+알림 도메인 (T1) — ChatRouter.cpp(어휘 7행 exact+알림 별명 2행+방어 행 3행)·SttVoice.cpp(kVoicePrompt 갱신+AppTool 질의 조립)·ChatRouter.h(ChatAction.tool+AppTool 소비 계약 주석)·main.cpp 2y ①-⑯ |
| T1 fix r1 | `ab974623ab947c158f8c1963cc7c5bbfb0bd745a` | fix(apps): AppTool 소비자 스위치 — jkweb/jktalk/Submit 안내문 정직 배선 (T1 fix r1) — 공(空) 도구 질의 실전송 fall-through 소각 + 2y-⑰ — ChatAppToolTypingGuide 공유 1본 |
| T2(probe) | `8ad5501f28eea3365440da13e10520bbe3b685e7` | feat(probes): 구두 어휘 신설 E2E 세그먼트 — WSL+폰 probe (T2) — wsl_voice.sh 218/12·phone_voice.sh 248/12 — §13b VOCAB 세그먼트+probe 계보 갱신 3건(fc3/캐논/--prompt) |
| T2 fix r1 | `705ac89426de72c378b2bfd48310519e8117f0fd` | fix(probes): phone voice probe — vocab 세그먼트 115b-1phone 런 결함 4건 수리 — 72/27 — PROBE 접두·FAIL 출구 계약(vcleanup)·병합 무변경·Notifications 접두 |
| T2 fix r2 | `46c47672b8ddd2bccde58587f4ba2b73b6d2e0ec` | fix(probes): phone voice probe — PHV_BTN env 판별이 기정치에 매몰되는 순서 결함 — 9/5 — SET 수취 상단 이동 |
| 본 T2(docs) | 본 문서를 포함하는 커밋 | docs(apps): #115b 구두 어휘 원장 — SHA는 git log가 진실원 |

### §7.13.1 배선·어휘 원리 (T1 리포트 → ChatRouter.cpp/SttVoice.cpp 단일 진실원 1:1 재수취 · 코드 재실측 0)

- **D1 무인자 강제 — exact 게이트** — kRouteTable 행의 `exact=true` 필드:
  정규화 발화 **전체** 등호만 성립(뒤접미 일치만으론 불성립). "볼륨 50으로
  멈춰줘"류 접사 발화 = Info stub(인자 필요 조작은 LLM 턴 몫 — 새 파서 0).
  불성립 뒤접미는 `break`가 아니라 **continue** — "일시정지"↔"정지" 접두
  공유가 표 순서로 갈린다(2y-⑤).
- **D2 도메인 v1 배치 = 재생+알림** — 접점 실측(T1 리포트 §1 표 — 도구 존재
  원문): vplayer `play_pause` = 유일한 무인자 재생 도구(6종 등록 원문
  ClientVPlayerApp.cpp — open/play_pause/seek/set_volume/set_av_delay/
  get_status 중 무인자 = play_pause·get_status)·notify 읽기 도구 = 부재(등록
  0 — Subscribe만)·notify **창 open** = launch_app 재용. **배제 원장행**:
  "다음 곡/이전 곡"(vplayer next/prev 도구 부재)·"알림 뭐 왔어"(읽기 도구
  부재)·spatial_stop(다른 앱 도메인 — 음역 오유)·open/seek/set_volume(인자
  필요 — D1 밖).
- **ChatAction 대차** — 재생 = AppTool kind 1종+`ChatAction.tool` 필드 1개만
  신설(기존 4종 Launch/Close/Focus/ListWindows로 표현 불가)·알림 = 기존
  Launch+별명 재용(신설 kind 0) — LLM 스키마 무변경(app_tool은 스키마 밖 —
  근접 변형 발화의 LLM 오발 봉쇄 = 파싱 실패 stub 폴백 원존).
- **어휘 표(v1.5 — ChatRouter.cpp 단일 진실원)** — 재생 7행(멈춰줘/멈춰/
  정지해줘/정지/일시정지/재생해줘/재생 — 전부 AppTool+vplayer·play_pause,
  exact=true)·알림 Launch 1행("보여줘")+별명 2행("알림"/"notify" —
  kAliasTable)·방어 행 2행("창 목록 보여줘"/"창목록 보여줘" Launch 누수 봉합
  — 2y-⑩). exact행은 **표 끝 배치** — 기존 앞 행의 접두 판정 무접촉.
- **질의 조립(ChatActionQuery — SttVoice.cpp)** — AppTool = `app_tool`
  릴레이 1건 `{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause",
  "args":{}}}`(무인자 = 빈 객체 — 양식 1종)·알림 = `launch_app {"app":"notify"}`
  (기존 양식). **소비 축**: 구두 표면만 전송권(RunVoiceTurn (5) — dispatcher)·
  타이핑·웹·jktalk = ChatAppToolTypingGuide 공유 정직 안내문(3 소비자 1본 —
  T1 fix r1(2y-⑰) — 전송 배선 부재 = 후속 라인 원장행).
- **kVoicePrompt(D4) 갱신** — "지뢰찾기 테트리스 알림 … 멈춰줘 멈춰 정지해줘
  정지 일시정지 재생해줘 재생 보여줘" — **표 성립 어휘만**(배제 어휘 미혼입).
  문자 리터럴 수형(TU 내부 constexpr — 직링크 불가) — **표 갱신 시 동기
  원장행**(probe dirrun의 --prompt 수형 포함).

### §7.13.2 캐논 계보 (2y 계열) + probe 실측 영수증 (T2)

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#115 라인 종착 = §7.12.2 T4 행) | 856 | 833 | 304 | 833 | — (2v 14·2x 15·2w 4 보존) |
| T1 `21ee4dd` | **872** | **849** | 304 (승계) | (833) | +16 (2y ①-⑯) |
| T1 fix r1 `ab97462` | **873** | **850** | 304 (승계) | (833) | +1 (2y-⑰ — 공유 문언) — T1 리포트 §6 2런 등호 |
| T2 본 라인(8ad5501/705ac89/46c4767 — probe 전용·src 무변경) | 873 | **850** | **304 (승계)** | **850** | 폰 2y 17 흡수 — **CANON=VOICE-VOCAB-2Y-17** (폰 833→850 — WSL 850 등호 — 배포 A/B 2런 등호) |

- **2y 17의 분해** = ①신설 등호(멈춰줘 AppTool·토글 안내문·질의 조립·재생해줘
  ·정지 갈림·알림 보여줘·launch_app 조립 = ①-⑦) ②기존 회귀(지뢰찾기·꺼줘 =
  ⑧⑨) ③별명 우선순위(창 목록 보여줘·알림 꺼줘 = ⑩⑪) ④무인자 강제(⑫⑬)
  ⑤D3 배제 stub(다음 곡·알림 뭐 왔어·InfoGuide 훑기 = ⑭⑮⑯) ⑥공유 문언
  (⑰ fix r1).
- **posix 304 = 구조적 승계 등호(사유 정정 — final fix 2026-10-27)** —
  "하네스 TU 목록 미링크"(갱신 65 문안)는 부정확 원장행: ChatRouter는 posix
  TU 링크 성립(`engine/tools/posix_selftest/build.sh:66` —
  `engine/src/apps/ChatRouter.cpp` 직접 링크 실측). 실사유 = llm16(case 16)
  어설션이 substring 관용(`find() != npos` — 발화 원문 전체 등호 카운트
  판정 없음)이라 kRouteTable 신설 어휘 행 원문에 무접촉 + 신설 2y 케이스
  17건은 posix 하네스 미수용(grep 실측 0 — Win/WSL 셀프테스트 소관) → 셈
  원문 변동 0 = 본 라인 재런 불요.
- **final fix 재런 원문(리뷰 지정 상쇄 쌍 소각 — probe 2파일·src 무변경·
  재빌드 0·2026-10-27)**: 진원 = ①notify 행 grep 패턴 `tool=[] ` 누락(printf
  실인쇄와 불일치 — grep 불성립 → 전 vocab 런 spurious
  `VOCAB-FAIL(route: 알림 보여줘)` 에코+산치 미가산)·②"볼륨 50으로 멈춰줘"
  exact-invoke 검사가 VROUTE 산치에 이중 계수(q=0 행 — 즉발 버킷 산치 오염
  — target /5 대비 가산 1) — +1/−1 상쇄로 `VOCAB-VERDICT: VOCAB-OK` 마스킹
  (115b-3 런 rows=5/5의 실체 = notify 미가산+exact 부정 가산). 수형 = notify
  패턴 `tool=[] ` 삽입+exact-invoke를 자체 판정행으로 분리(`VEXACT` —
  `q=0 query=[no no]` 무인자 강제 원문까지 등호 — `kind=5` 만의 느슨 패턴은
  배제 stub(동 kind) 충돌 원문)+산치 target 재산정 **6**(=즉발 5+무인자 강제
  1) — **재런 1런/축**: `engine/tmp/wsl_voice_run115b_fix1.log` =
  `VOCAB-ROUTE-OK: rows=6/6 (route 산치 분리 — route=5/5+exact=1/1)
  excl=2/2`+`VOCAB-VERDICT: VOCAB-OK`+`VOICE-VERDICT: VOICE-OK`
  (canon=850·2y=17·fc3=3 등호 — spurious FAIL 에코 소멸 — probe_rc=0)·
  `engine/tmp/phone_voice_run115b_fix1.log` = rows=6/6(route=5/5+exact=1/1)
  등호+`VOCAB-PHONE-VERDICT: VOCAB-PHONE-OK`(`VOICE-PHONE-VERDICT:
  VOICE-PHONE-OK`·tap 2/2·permissions 33B 등호 — PHONE_RC=0) — 기존 legs
  회귀 0·폰 접속 성립 런(미접속 원장행 없음).
- **WSL probe 원문(#115b-RUN-VALS — engine/tmp/wsl_voice_run115b_{2,3}.log —
  2런 FULL 등호)**: `canon=850 twov=14 twoy=17 fc3=3 direct=1 direct_t2=1
  stt_t2_canon=0 fixt=1 keyturn=2 clickturn=2 route=1 vocab=1 vprelay=1
  vptoggle=1 notify=1 probe_rc=0` — `VOICE-VERDICT: VOICE-OK` 2/2+**신설
  `VOCAB-VERDICT: VOCAB-OK` 2/2** — 기존 legs 회귀 0(fc3=3·direct 등호·canon
  ·lq·route·fixt·keyturn·clickturn 전항 등호).
- **신설 어휘 E2E 원문(§13b — R3)**: 라우터 원문 8건 — `VOCAB text=[멈춰줘]
  kind=4 app=[vplayer] tool=[play_pause] usedLlm=0 q=1 query=[app_tool
  {"app":"vplayer","tool":"play_pause","args":{}}]`(재생해줘/정지 등호·
  알림 보여줘 = `kind=0 app=[notify] … query=[launch_app {"app":"notify"}]`·
  볼륨 50으로 멈춰줘 = kind=5 q=0(무인자 강제)·다음 곡/알림 뭐 왔어 =
  kind=5(배제 stub)·창 목록 보여줘 = kind=3(방어 행)) → **소비 원문**:
  `app_tool open → {"ok":true,"windowId":19,"result":{"accepted":true}}` →
  get_status `"opened":true "paused":false "dur":30.000`(엔진 소유 합성
  클립 vpt2_test.mp4) → play_pause 1탭 reply `{"paused":true}`+status
  `"paused":true`(기록 반영) → 2탭 `{"paused":false}`+status 등호(토글 2탭
  상반 — T1 토글 수용 원장행의 라이브 재판정) → `'알림 보여줘' →
  {"ok":true}`+Notifications 창 원문(배지 "Notifications (3)" — 접두 매치)·
  캡처 voc_wsl_vocab_vp.png(478,881B)+voc_wsl_vocab_notify.png(678,370B) ·
  창 소각+좌표 원복(REMNANT 0 2런).
- **T2 폰 배포 런(t115b_t2_deploy.sh — engine/tmp 산출물 — t115_t4 수형 승계
  ·커밋 0 — engine/tmp/t115b_t2_deploy_run.log)**: DEPLOY-BASE `ab97462`
  (T1+fix r1 포함 HEAD 전수 — **SWEEP-DIFF 10/472 → SWEEP-POST-DIFF=0** hard
  게이트)·LEG-STAGE-WIRED(SPATIAL leg 유지배선 — libopenal.a 1,969,238B·
  `-lOpenSLES -llog`)+WHISPER-ROOT-WIRED(tree 149)·CMAKE-RC=0·
  NINJA-RC=0 SECONDS=475(aarch64 — WHISPER-BUILD-NINJA-ENTRIES=297)·
  `DEPLOY-MARKERS: 2r=11 2s=21 2u=136 2v=22 2w=4 2x=15 2y=18`(라벨 계수 —
  본문 주석 포함 — PASS 2v=14·2x=15·**2y=17**)·MUSP-LEG-SYM alcOpenDevice
  strings=3(leg 링크 유지)·whisper-cli aarch64 산출(무인자 cold run 정상·
  LDD not-found 0)·tiny-ko 핀 77,691,730B 등호(조달 재용 불요)·music.jkx
  재팕(ICON-ENTRIES=2·PACK-RC=0)·서버 재기동 **WHISPER_ROOT 보유 부팅(부팅
  env 승계 원존 — 구두 leg 게이트의 클라 상속 계약)**·Music 창 복원 ok·
  `CANON-A=850`/`CANON-B=850`(A/B 2런 등호·2y=17 양런)·settings 213B/
  permissions 33B **무변경 등호**·jkweb 생존(pid 동일)·REMNANT-LS-RC=2
  (self-destruct 등호) — `T2-DEPLOY-VERDICT: T2-DEPLOY-OK`.
- **폰 probe 동형 세그먼트(phone_voice.sh — engine/tmp/
  phone_voice_run115b_{1,2,3}.log — 3런: 1=fix r1 대상 honest·2=fix r2 대상
  tap 미달·3=ALL OK)**: 최종 런(115b-3) `VOICE-PHONE-VERDICT: VOICE-PHONE-OK`
  (+**`VOCAB-PHONE-VERDICT: VOCAB-PHONE-OK`**)— tap 런 2/2(탭 좌표 재보정
  (1283,720) — rec→done 바이트 증가 +3,303B/+38,167B = 기록 행 렌더 영수증)·
  fail-closed 3판 원문 등호(스폰 전 fail-closed 계약 — 양쪽 수용)·Termux:API
  마이크 leg(mic-start/stop=1·isRecording 세션 판정)·fixture 사슬==직접 스폰
  등호·probe VOCAB 행 등호 5/5+배제 2/2(**라우터 = 방금 배포된 buildterm
  진실원 재실측** — **이 5/5 = 상쇄 마스킹 수치**(notify 행 grep `tool=[] `
  누락 미가산+볼륨 50 exact-invoke 산치 이중 계수 가산이 상쇄 — final fix
  재런 rows=6/6(route=5/5+exact=1/1) 재산정 — 위 final fix 재런 원문 행))
  ·라이브 소비 — vplayer open(합성 wav 재용 —
  `{"accepted":true}`→`"opened":true "ended":true "pos":2.176 "dur":3.000`)
  →play_pause 2탭 `{"paused":true}`/`{"paused":false}`+get_status 등호 →
  `'알림 보여줘'→{"ok":true}`+`"title":"Notifications (1)"` 창 원문 →
  close 2건+permissions **무변경 33B**+잔상 0(REMNANT left=0)·최종 화면 =
  Music 창 단독(사용자 화면 복원) — 캡처 pv_vocab_vp.png/pv_vocab_notify.png.
- **stt_t2_canon=0 원장행(CANON 밖 — §2.4)**: T2 계보 원문 [구독 / 좋아요]
  MISS·본 라인 실측 "(뽀송한 소리)"(440Hz 사인의 환각 계열 — 전사 본문은
  사용자 실발화 게이트 밖 — 결정 축은 chain==direct 등호 2런 등호로 승계).

### §7.13.3 함정 원장 (#115b 신규 — 봉쇄 레슨 축 · 전량 = T1/T2 리포트+런 원문 수납)

- **probe 계보 원문 = 원장 상수와 함께 난다(기존 세그먼트 원존·재런 0의 대가)**
  — fc3 기대 원문(T6 rc=3 스폰 leg)은 resolve 계약 변경(park-5 쌍둥이 정합 —
  env 부재 경로 = 빈 반환) 이후 스폰 전 fail-closed 원문으로 갱신됐다 — probe
  가 마지막으로 풀런된 지점 이후의 계약 변경을 셀프테스트가 못 잡는다(셀프테스트
  밖 관행 원존) → 재런 시 계약 원문 대차 원장 필수(본 라인 소각 — 8ad5501:
  현재 계약 원문 우선+T6 계보 원문 호환 양쪽 수용).
- **전사 대조의 인자 등가 대차** — RunVoiceTurn 스폰 원문 = `-l ko --prompt
  kVoicePrompt`·probe dirrun(기존) = `-l ko` 만 — 인자 비등가 대조라 전사
  비결정의 잠재 계통(실측: chain=[(뽀송한 소리)] vs direct=[[구독 / 좋아요]]
  갈림) — 수형 = dirrun에 --prompt(kVoicePrompt 문자 리터럴 수형 — TU 내부
  constexpr 직링크 불가) — **kVoicePrompt 갱신 시 probe 원문 동기 원장행**.
- **기정치 채우기와 env 지정 판별의 순서(115b-2phone 런)** —
  `PHV_BTN_X=${PHV_BTN_X:-835}`를 먼저 쓰면 env-지정 판별용 SET 원취가
  "835"로 만성화해 AUTO 재산출이 만년 스킵된다 — SET 수취가 선행 계약.
  재판 2런(835,602 미달 반복 → (1283,720) 착탄 2/2).
- **FAIL 출구 잔상 계약(phone 원격 스크립트)** — FAIL()가 exit 1로 즉사하는
  동안 permissions 병합 원복+leftover 창 소각을 건너뛴다 — 본 런 실측:
  중복 close_window 행(33→62B)+Video Player/Notifications 창 잔존 → 수기
  원복 소각(33B 등호+close 2건) — 수형 = vcleanup() 무해 재입(멱등 —
  wsl_music HARV-TEARDOWN 래더 원존 승계)+**병합 생략**(close_window 상존
  시 — 무변경 0). 전문 런 끝에 permissions 33B/setting 213B 무변경 등호
  재확인 완료.
- **wsl.exe env 전파(런 라벨)** — WSLENV 접미 없는 VOICE_RUN_NO 전파 = 소실
  (run=auto — 115b-R2 런) — #115 T4 원장 승계 누락 원장행 — R3에서
  WSLENV="VOICE_RUN_NO/u"로 판정(run=115b-R3). 
- **알림 창 제목 = 배지 실칭** — notify 창 제목은 누적 카운트 배지를 실는다
  ("Notifications (3)"·"(1)" — ClientNotifyApp SetTitle) — 정확 등호 매치
  금지·접두 매치(WSL/폰 동형 수리 — 115b 런 실측 2건).
- **app_tool 중계 응답 = 봉투 랩** — `{"ok":true,"windowId":N,"result":{…}}`
  이층(T5 봉투/본문 원존 수형) — reply의 본문 판정은 result 부분열 매치
  ("accepted":true·"paused":true) — 표면 전체 등호 매치 금지.
- **캐논 표기 계약(브리프 "873 승계" 재판정)** — 폰 = **WSL 등호 계약**
  (668/818/833 계보 원존) — Win 873은 Win 축 값(§4.1-10 원장행). 폰 실측
  850 = WSL 등호로 정산.

### §7.13.4 비-목표 + EYES 게이트 (사용자 선언만 결제 — §5/§7.9.5 원칙 동형)

- **비-목표(스펙 §3)**: 인자 필요 조작의 즉발 연결(창 이동/리사이즈·volume —
  LLM 몫)·TTS·파일/갤러리 도메인(차기 v1.6)·새 도구 신설(D3 배제 원존)·
  타이핑 표면 app_tool 전송 배선(후속 라인 — ChatAppToolTypingGuide 정직
  안내문만 원존).
- **EYES 대기**:

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **폰 신설 어휘 발화 체감** | 폰 chat [구두]에서 실발화 — "멈춰줘/재생해줘/정지(토글)"+"알림 보여줘"의 동작 체감(라우터 즉발 — LLM 지연 0) — probe는 합성 fixture 전사+텍스트 직발 라우터 원문만(CANON 밖) | **대기(발화 체감)** |
| ② | **WSL/Win 발화 체감** | 데스크 축 F6 hold-to-talk + 신설 어휘 발화(WSLg 마이크 — 상태행 캡처 원문 존재) | **대기(육안·체감)** |
| ③ | **배제 어휘 stub 체감** | "다음 곡"/"알림 뭐 왔어" 발화 = 정직 stub 안내(도구 부재 배제 원장행 — 음역 오유 방지) | **대기(육안)** |

### §7.13.5 커밋 원장 + IP/호스트 경로 게이트 (#115b 라인)

- 라인 체인 = §7.13.0 표(fe5e8c9..본 docs 커밋 — rev-parse 전체 SHA 원존 —
  git log가 진실원) — push 전부 origin main 직행·amend 없음·**커밋 = 명시
  경로만**(T1 = src 3파일+main.cpp·T1 fix r1 = 4파일·T2 probe = probe 2파일
  ·T2 fix r1/r2 = phone_voice.sh 1파일·본 docs = docs/90 1파일 —
  `git add -A` 0·커밋 분리 원존(probe 커밋+fix 커밋 2건+docs 커밋).
- **IP·호스트 경로 게이트(T2 실측)** — probe 커밋(8ad5501)+fix 2건
  (705ac89·46c4767) 커밋 원문 IPv4 정규식 grep = **전부 0건**·폰 주소 =
  known_hosts BatchMode 자기 판명 변수 안 소멸(주소 미인쇄 계약 — 폰 런
  로그 전체에도 0건 — 기록 계약 원존).
- 배포 드라이버(t115b_t2_deploy.sh)·런 로그(wsl_voice_run115b_{1,2,3}.log·
  phone_voice_run115b_{1,2,3}.log·t115b_t2_deploy*.log)·캡처(voc_wsl_*.png·
  pv_run*_*.png·pv_vocab_*.png) = engine/tmp untracked — 커밋 금지 충족.

## §7.14 구두 응답 TTS 원장 (#117 라인 — T1-T2(+fix r1-r5), 2026-10-27)

#117 라인 원장. 폰에서 구두 명령의 답을 소리로 듣는다 — 구두 표면(chat [구두])
의 답신 도착에 `termux-tts-speak`(Termux:API — 조달 0)로 읽어준다(스펙 D1-D6).
스펙=`docs/superpowers/specs/2026-10-27-tts-response-design.md`(7981c45),
SDD 원장=`.superpowers/sdd/2026-10-27-tts-response/`(task-1 리포트 = 미커밋
관행 ·task-2 리포트 = T2 fix r6 커밋 승격). T1 리포트가 표지 사슬·스폰
leg·절단 부품·2z 9건의 원문 수취원 — 본 §7.14.1은 그 1:1 재수취, §7.14.2-3은
T2 배포·probe 실측 원문.

### §7.14.0 체인 (rev-parse 전체 SHA 실측 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 (numstat) |
| --- | --- | --- |
| 스펙 | `7981c45…`(docs — git log) | docs(apps): 구두 응답 TTS leg 스펙+플랜 (#117) — D1-D6 |
| T1 | `ae0dfb5` | feat(apps): 구두 응답 TTS leg — termux-tts-speak (T1) — ClientChatApp.h(jk::tts 인라인 부품 3종+origin 표지)·ClientChatApp.cpp(#ifdef __ANDROID__ 스폰 leg+D3 표지 수명주기+DrainVoice 트리거)·main.cpp 2z 9건 |
| T2(probe) | `a32a26d` | feat(probes): phone voice probe — #117 TTS 세그먼트 (T2) — 344/1 — B3 세그먼트(stamp 원문·마침 탭→stamp 폴링·CLI 개발력 3표본·VOICE_TTS=0 opt-out 1판·표준 부팅 복귀·BLOCKED 원장행) |
| T2 fix r1 | `48c5474` | fix(probes): phone voice probe — TTS 전용 런 페이즈 상속+이중경로 결함 (T2 fix r1) — 78/36 — 페이즈 정규화(마침)+Session 판정 행·180s 폴링·ROOT 3단 수형(engine/engine/tmp 이중경로)·RUN-VALS tts 필드 가산 |
| T2 fix r2 | `d18c639` | fix(probes): phone voice probe — TTS tap 포커스 선행+FAIL 출구 permissions 원복 (T2 fix r2) — 23/0 — focus_window 선행+FAIL 출구 원복(멱등)+병합 생략 가드 |
| T2 fix r3 | `d0bec85` | fix(probes): phone voice probe — TTS tap 세션 검증 재시도 (T2 fix r3) — 23/11 — 개시/마침 -i 세션 검증 재시도 루프(최대 3)+T0 재기점 |
| T2 fix r4 | `ce0b760` | fix(probes): phone voice probe — TTS 웨지 수속+판독 접두 결함 (T2 fix r4) — 39/16 — helper 잔존 소각(진입/종료)+CLI per-call timeout 20s·표본 >=1 성립·SESSION 판독 원문 패턴 수리·done>rec 관측행 격하 |
| T2 fix r5 | `0746680` | fix(probes): phone voice probe — TTS 세션 판독 선행 숫자 절단 (T2 fix r5) — 6/4 — SESSION 행 괄호 후미 흡수 판독 결함(=1 등호 실패) 수리 |
| T2 docs(a54ecc4) | `a54ecc4` | docs(apps): #117 구두 응답 TTS 원장 — docs/90 §7.14+§4.1-11 (T2) — 217/1 |
| T2 fix r6 | 본 문서를 포함하는 커밋 | docs(apps): TTS 웨지 원장 정정 — hang 스폰 원진+수형 후보 재판정 (T2 fix r6) — docs/90+task-2-report.md(런/probe 재실측 불요·src 무변경) — SHA는 git log가 진실원 |

### §7.14.1 배선·설정 원리 (T1 리포트 → ClientChatApp 단일 진실원 1:1 재수취 · 코드 재실측 0)

- **D1 폰 전용** — 스폰 leg 전량 `#ifdef __ANDROID__`(ClientChatApp.cpp §85-
  TtsTermuxPrefix/TtsResolveBin/TtsQuoted/TtsSpawnSpeak — 전처리기 폐기 —
  Win/WSL 빌드 심볼·코드 0실측[2축 warning 0]). 스폰 = `/bin/sh -c` 래핑
  (JKProcess_posix.cpp:4 원문 — 절대 경로 resolve+인용이 PATH 상실 서버 계통
  에서도 성립)·마감 15s(kTtsSpeakDeadlineMs)+TerminateJobTree(kill-on-close
  job)·파이프 read end 폐기.
- **D3 표지 사슬** — `Turn.originVoice` 표지+`voiceOriginPending_` 세션 표지:
  StartVoice(구두의 유일 개입 통로 — F6 hold·[구두] 버튼 2계통)에서 새김 →
  FinishVoice placeholder push 접점 1곳 수납 → DrainVoice 치환에 앞서 원독
  (실패 치환 대입이 표지를 지우기 전 1독) → [시스템] 답신 도착 1곳에서
  `TtsSpeakWanted(true, guide.empty(), optOut)` 관문 → detached 단명 스레드로
  `TtsSpawnSpeak(TtsReplyClip(guide))`. **타이핑 발화의 답신 = 구조적 부정**
  (Submit 표면은 표지 없는 턴만 만든다 — 스폰 접점 자체가 voiceOrigin 게이트
  뒤). **OnClose** = placeholder 없이 소각(표지 잔상 금지).
- **D2 기본 켬+opt-out** — `VoiceTtsEnabled`(표의 유일 진실원)·
  `VoiceTtsOptOutRuntime` = getenv("VOICE_TTS") — **"0" 문자 그대로만 끔**
  (기본 켬·빈 값/"1" = 켬 — 2z-③②c/③c). opt-out = 스폰 밖 결정 부품
  (판정 단계만 소비 — env 중독 없음) — 폰 E2E 실측 = T2 probe 몫(§7.14.3).
- **D4 쓰는 문언** — [시스템] 답신의 reply 원문(전사 아님 — guide/앱 실행
  문언)·`TtsReplyClip` = 상한 120 **문자**(UTF-8 코드포인트 절단·연속 바이트
  흡수) 절취+"…" 접미 — `over` 플래그(121번째 문자 실존 판정) = 2z-①이 잡은
  초판 결함의 수형(TDD 효과 — T1 리포트 §2).
- **D5 실패=무시** — termux-tts-speak 부재/스폰 실패/15s 초과 = 조용히 스킵
  (UI·기록 무영향·재시도 없음) — stderr trace stamp 1행
  (`[TTS] speak: <절단판>`/`[TTS] fail (...)`)만 — 폰 E2E probe의 진실원 행
  (스펙 §2·T2 소각 금지 원장행).
- **D6 계보** — selftest **2z** 9건(①절단 3[121→120+"…"·한글 121자=120+…·상한
  등호 원문]·②구두 판정 3[표지 턴=대상·타이핑 턴 부정·빈 답신 스킵]·③opt-out
  3[미설정=켬·"0"=끔 1바이트 등호·"1"/""=켬]) — 헤더 인라인 순수 결정 부품만
  (스폰 어셜션 금지 — 비결정 원존). CMake 무변조(신규 TU 0·신설 타깃 0).

### §7.14.2 캐논 계보 (2z 계열) + T2 배포·probe 실측 영수증

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#115b 라인 종착 = §7.13.2 T2 행) | 873 | 850 | 304 | 850 | — (2v 14·2x 15·2y 17 보존) |
| T1 `ae0dfb5` | **882** | **859** | 304 (승계·313 재판정 대기[§4.1-11]) | (850) | +9 (2z ①-⑨) |
| T2 본 라인(a32a26d…0746680 — probe 전용·src 무변경) | 882 | 859 | 304 (승계) | **859** | 폰 2z 9 흡수 — **CANON=2Z-9** (850→859 — WSL 859 등호 — 배포 A/B 2런 등호) |

- **2z 9의 분해** = ①절단 3(121자·한글 121자·상한 등호) ②구두 판정 3(표지 턴·
  타이핑 부정·빈 답신) ③opt-out 3(미설정·"0"·"1"/"") — T1 리포트 §6(2축 등호
  — Win 873+9·WSL 850+9 — RED/GREEN 재판정 1회 완결).
- **posix 304 승계 = 승계 원존**(T1 리포트 §9.1 — 폰·posix 접촉 계약 밖 —
  재판정 채권 §4.1-11).
- **T2 폰 배포 런(t117t2_deploy.sh — engine/tmp 산출물 — t115b_t2 수형 승계
  ·커밋 0 · engine/tmp/t117t2_deploy.log+t117t2_run.log)**: DEPLOY-BASE
  `a32a26d`(T1+T2 probe 커밋 — fix r1-r5[0746680까지] = probe 전용·src
  무변경이라 폰 바이너리 무영향 — **배포 유효 원존**: #115b 선존 "T2 probe
  전용·src 무변조" 계보 승계) — **SWEEP-DIFF 6/472 → SWEEP-POST-DIFF=0** hard
  게이트·LEG-STAGE-WIRED(SPATIAL leg 유지배선 — libopenal.a 1,969,238B·
  `-lOpenSLES -llog`)+WHISPER-ROOT-WIRED(tree 149)·CMAKE-RC=0·**NINJA-RC=0
  SECONDS=472**(aarch64 — whisper 신증 전량 재빌드)·JKDESK-BYTES 37,808,552B·
  `DEPLOY-MARKERS: 2r=11 2s=21 2u=136 2v=22 2w=4 2x=15 2y=18 **2z=9**`·
  MUSP-LEG-SYM alcOpenDevice strings=3(leg 링크 유지)·whisper-cli aarch64
  (17,948,320B — 무인자 cold run "no input files" 정상·LDD not-found 0)·
  tiny-ko 핀 77,691,730B 등호(조달 재용 불요)·music.jkx 재팕(PACK-RC=0·
  ICON-ENTRIES=2 — 554B/995B)·서버 재기동 **WHISPER_ROOT 보유 부팅**(부팅 env
  승계 원존)·`BOOT-SECONDS=8`/`BOOT2-SECONDS=8`·SCAN-COUNT=27+SCAN-SORTED=OK
  ·Music 창 복원 ok — **`PHONE-SELFTEST-A rc=0 PASS=859 FAIL=0 2u=132 2r=10
  2s=20 2v=14 2w=4 2x=15 2y=17 2z=9 SECONDS=7`**·`CANON-A=859`·런B 동일
  **`CANON-B=859`**(2z=9 양런) — settings 213B/permissions 33B **무변경 등호**
  ·jkweb 생존(pid 동일 — 절사 0)·REMNANT-LS-RC=2(자기 소각 등호) —
  `T2-DEPLOY-VERDICT: T2-DEPLOY-OK`.
- **사용자 live 세션 존중 원문** — 진입 창 원문 `Music focused`(사용자 화면)를
  서버 교체 전 진입 영수증으로 수취, 종료 = Music 창 복원(`FINAL-WINDOWS:
  Music focused` — 돌아오면 그대로) — 잔상 0(REMNANT left=0).

### §7.14.3 probe 원문 (phone_voice.sh B3 세그먼트 — stamp·세션·CLI·opt-out)

- **stamp 원문(진실원 — 도착 소관 수취)** — 전 런(T2 세그먼트 소각 금지 행 —
  클라 stderr → 서버 boot log 도달: posix 스폰 계약 `inheritedStdioPipes`
  미설정 = 부모 stdio 상속[JKProcess_posix.cpp fork leg — 자식 stdin=/dev/null
  ] → §B 부팅 리다이렉트 진실원 t6pv_srv.log): `[TTS] speak: 인식하지 못했습
  니다. 채팅에서 쓸 수 있는 명령:`(+계행 후속 행 `'닫아줘' — 창 닫기`… —
  stamp 행 = D4 절단판의 **1행만**[multi-line guide clip의 개행까지 — 함정
  §7.14.4]) — 런(R1-R4·R6-R7) 전부 2-3행 도출·`[TTS] fail (rc=137)` 원문 행
  = 15s deadline TerminateJobTree 실측(R6/R7 — 웨지 상태의 정직 무시 원문).
- **마침 탭→stamp 지연(원문)** — dedicated tap 런(세션 검증 통과 런): R5
  `18915ms`·R6 `18520ms`·R7 `20490ms` — STT(tiny-ko 폰 전사)+답신+스폰 개시
  구간 통괄(개발력 **상한** 표기 — 리서치 문턱 1s = "TTS 첫 단어" 구간 대조
  표기·판정 어설션 아님 — 실음 체감 = EYES 몫).
- **세션 전이 원문(-i 진실원 — `termux-microphone-record -i` = `{"isRecording":
  bool}`)** — R5/R6/R7 dedicated: 개시 성립 1+마침 성립 1(START-RETRY 1건
  원문 포함 — §7.14.4 함정 1·2) — stamp 판정의 유효성 통제.
- **CLI 개발력 표본(termux-tts-speak 스폰→종료)** — R1-R4(웨지 이전) **12
  표본**: 3374-3548ms(R1)·3414-3521ms(R2)·2658-2879ms(R3)·2881-2989ms(R4)
  (exit = 첫음+발화 포괄 **상한** — 즉귀형이면 개발력 그 자체)·R5 = CLI 구간
  stall ~25분(**빈 CLIOUT receipt** — per-call timeout 이전 — 수기 회복 뒤
  런 완주 — §7.14.4 함정 7)·R6/R7 = 표본 전량 `TTS-CLI-TIME-OUT`(20s —
  per-call timeout 경화 후) — 웨지 사건 원문(런 밖 원장행·§4.1-11).
- **opt-out(D2) 1판 원문(유효 결정판 — R7 단독)**: `VOICE_TTS=0` 부팅(/proc/<pid>/
  environ 영수증 `VOICE_TTS=0` — 클라 상속 계통의 원천) → fresh boot log
  stamp **부존 0건**(180s stamp 폴링 — 도착 없는 부존 봉쇄)+세션 전이 양순
  (개시 1·마침 1 — 구두 표면의 절차 실측)+기록 행 도착 관측(+38220B — 렌더
  영수증) → `TTS-OPTOUT-OK`. **R5·R6 = SESSION 판독 결함 런 — raw 영수증 행만
  유효**(세션 전이 원문 행[stop=1]·stamp 부존 0·기록 도착 관측 — 판정행
  본체는 접두/괄호 판독 결함으로 n/a — §7.14.4 함정 6·fix r5 원장).
  **R1/R2의 opt-out 판정 = 무효 원장행**(30s 고정 대기+done>rec 가양성 —
  §7.14.4 함정 3 — fix r1 격하).
- **RUN-VALS(요약 스트림 계약 — R7 최종 런)**: `tap_runs=2 gate=3판
  fixture_direct=1 mic=1 tap=OK_ALL runverdict=VOICE-PHONE-OK vocab=1
  vocab_route=5 vocab_excl=2 vocab_live=1 tts_runverdict=TTS-PHONE-FAIL
  tts_stamp=1 tts_session=1 tts_optout=1 tts_cli=0 tts_lat=20490` —
  **VOICE-PHONE-VERDICT: VOICE-PHONE-OK+VOCAB-PHONE-VERDICT: VOCAB-PHONE-OK**
  (기존 legs 회귀 0 — 전 런)·별행 TTS 판정(R7): stamp/session/optout 전원 1·
  cli=0(웨지 — 런 판정 honest FAIL — §7.14.4).
- **미접속 = BLOCKED 원장행** — 정찰 MISS 코드 경로(`TTS-PHONE-BLOCKED: …`)
  본존·**실측 0**(전 런 폰 접속 성립 — §4.1-11 원장행).
- **7런 원장(R1-R7 — T2 세그먼트 신설이후)**: R1/R2 = fix 이전(전 런 VOICE-
  PHONE-OK·stamps 2+2·CLI 3표본·opt-out 무효판[가양성])·R3 = fix r2 대상
  (개시 실패 honest FAIL — permissions 드리프트 사건)·R4 = fix r2 재판(focus
  후에도 개시 실패 — 활성화 소비 탭 진원 확정)·R5 = fix r3 대상(stamp 도착
  +지연 18915·CLI 구간 stall ~25분(빈 CLIOUT receipt)뒤 수기 회복 — **런
  완주**(opt-out raw 영수증 유효·판정행 판독 결함 — fix r5 원장·웨지 시작 —
  스폰 9건 무-fail receipt)·R6 = fix r4 대상(웨지 유지 — CLI 0/3·세션 판독
  결함)·R7 = fix r5 대상
  (**stamp=1 session=1 optout=1 lat=20490 cli=0[웨지] — T2 결정 축 전원**),
  전 런 jkweb 생존·permissions/settings 무변경·잔상 0·Music 복원.

### §7.14.4 함정 원장 (#117 신규 — 봉쇄 레슨 축 · 전량 = T1/T2 리포트+런 원문 수납)

- **전용 tap 런의 토글 페이즈 3원 결함(R1-R4 — 3회 재판정 소각)** — ①선행
  세그먼트의 미마침 녹음 생존 = tap1=마침·tap2=개시 어긋남(dedicated detect=0
  2런 — pv_tts_done `녹음 중` 행 원문) ②미포커스 창의 첫 탭 = 포커스 소비(R3
  — 개시 세션 미성립 honest FAIL) ③포커스 해결 뒤에도 **첫 탭 = 활성화 소비**
  (R4 — 수동 진단 등호: send_input allow+focus_window 상존·첫 탭 무효·두번
  째 탭부터 `-i` true) — 수형 = **-i 세션 검증 재시도 루프(최대 3 — 성립 즉시
  break = 토글 이중 반영 봉쇄)**+페이즈 정규화(진입 시 `isRecording:true`면
  마침 탭 1회) — `termux-microphone-record -i` = 세션 판정 진실원(T5 어휘
  승계).
- **stamp 폴링 상한(R1/R2 120s 만료)** — 폰 tiny-ko 전사 = 마침 탭→stamp
  18-20s 관측(R5-R7)이나 R1/R2 전용 런은 120s에서도 만료(=표면 미도달 — 페이즈
  결함이 원인 — 위 함정) — 수형 = 180s+세션 전이 판정 병기(표면 도달 없는
  부존을 부존으로 셈 없게).
- **done>rec 가양성(R1/R2 opt-out — fix r1 격하)** — "녹음 시작" 행 렌더도
  done을 증가시킨다(+3303B = §B/R1·R2·R6·R7 opt-out 동일 수치 원문 — 기록 행
  도착 +38167/+38220B와 별개 패턴) — done>rec = 관측행만(stamp 도착이 도착 결정
  원문)·stamp 미검출 런에서만 결정 proxy.
- **FAIL 출구 permissions 드리프트(R3)** — tap 스크립트 FAIL이 병합 원복을
  건너뛰어 33→60B 드리프트가 **런 밖 생존**(중복 send_input 행+close_window
  재병합 = 87B·깨진 JSON 원문 — 수기 원복 = t117t2_perm_backup 33B 등호) —
  수형 = FAIL 출구 원복(멱등)+병합 생략 가드(send_input 상존 시 무변경 0) —
  vcleanup 멱등 원존 승계.
- **probe ROOT 이중경로(#115b 계보 — fix r1 수형)** — `dirname $0/../..` =
  2단(engine) → SCRATCH=**engine/engine/tmp** 캡처·RLOG 랜딩(회수는 양변
  동경로라 md5 등호로 무결 — 발견 = 본 라인의 pv_* 부재 실측) — 수형 = 3단
  — 엔진/폰 무영향·probe 산출물 경로만.
- **stamp 판독 2회 결함(R5/R6 — fix r4/r5 수형)** — ①`TTS[n]:` 접두는 인쇄
  시에만 존재(TTSOUT = 원문 — 판독은 원문 패턴)·②SESSION 행 후미 괄호 표기
  흡수 = `=1` 등호 실패(선행 숫자만 수취).
- **TTS 엔진 웨지 사건(R5 돌입 — §4.1-11 park)** — termux-tts-speak 표본 1건
  무한 블록(CLI 구간 stall ~25분 — 빈 CLIOUT receipt) → libexec TextToSpeech
  helper 잔존(helper 소각 뒤에도 무응답 — 시스템 TTS 엔진 서비스 측 — 원진
  미확정 원장행)·앱의 speak = 계속 스폰(stamp 도출)·`[TTS] fail (rc=137)` =
  **hang 스폰의 마감 kill 증상**(웨지 발생 후 R6/R7에만 출현 — R1-R5 = 무-fail
  receipt ~10건[120자 발화 15s 내 종료] — "15s 마감×장문 guide 겹침" 계통
  요소 폐기[fix r6 — 원장 정정])·Termux `am` = force-stop 미지원(원문) —
  **speak stamp = spawn 성립 증거(발화 성공 증거 아님 — EYES 별개)**. 회복 =
  폰 현장(재부팅 권장 — 사용자 몫)·probe 경화 = helper 잔존 소각(진입/종료)+
  표본 per-call timeout(20s). **수형 후보 순위(fix r6 재판정)**: **① 스폰 후
  즉시 반환(백그라운드 spawn — detach) = hang rc=137 반복 구조 소거 — 1순위**
  ② 마감 비례 상향 = near-no-op 표기(hang 스폰에는 마감 연장도 kill일 뿐).
- **수기 진단 원문 공백 원장행(fix r6)** — "helper 6 잔존/60s+ 무응답" 수치 =
  수기 진단(pgrep 1행 카운트+timeout 표본)의 세션 stdout **미수취** 관행
  (JOG-RUN-VALS 수취 원존 계약 승계) — 재현 시 stdout 수취 계약(진단 절차의
  원문 행 전량 수취).
- **§C 소각 목록 밖 잔존+판독 공전 원장행(fix r6)** — ①`$TMPD/
  t6pv_tts_perm_backup` = 탭 스크립트 사망(ssh 단절 등) 시 생존하고 §C 소각
  목록에는 없다(원복 경로의 잔존 edge — 후속 probe 커밋에서 소각 목록 가산
  몫) ②opt-out 좌표 조립 행의 `[ "$OTX" = "$OTX" ]` = 항등식 무해 공전
  (판정 무영향 — 원장 기록 소화).
- **pkill 자기 매치 2회(런 밖 원장)** — 원격 heredoc 본문과 pkill 패턴의 자기
  매치(ssh 255 — probe 함정 1 원존의 변형: heredoc의 문자열 조립 행이 패턴을
  품는다) — 진단 세션은 별도 ssh로 분리(셸 자기 매치 봉쇄 원칙 승계).

### §7.14.5 비-목표 + EYES 게이트 (사용자 선언만 결제 — §5/§7.9.5 원칙 동형)

- **비-목표(스펙 §3)**: Win/WSL TTS(SAPI — 별도 라인)·TTS 톤/속도 설정·구두
  외 표면(타이핑 답신 — D3 무음 존중)·TTS 큐(연속 발화 처리 — v1 단건).
- **EYES 대기**:

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **폰 답듣기 체감** | 폰 chat [구두] 발화 → **답을 소리로 듣는 체감**(termux-tts-speak — 스펙 §2) — probe = stamp·세션·CLI 표본 원문만(CANON 밖·실음 체감 불가) — **전제 = 폰 TTS 엔진 웨지 복구(재부팅 권장 — §7.14.4 함정)** | **대기(체감 — 사용자 선언만)** |

### §7.14.6 커밋 원장 + IP/호스트 경로 게이트 (#117 라인)

- 라인 체인 = §7.14.0 표(7981c45..본 docs 커밋 — rev-parse 전체 SHA 원존) —
  push 전부 origin main 직행·amend 없음·**커밋 = 명시 경로만**(T1 = src
  3파일·T2 probe = phone_voice.sh 1파일·fix r1-r5 = 각 1파일·본 docs =
  docs/90 1파일) — `git add -A` 0·커밋 분리 원존(probe 커밋+fix 5건+docs
  커밋)·배포 드라이버(t117t2_deploy.sh)·런 로그(phone_voice_run117_{1..7}.log
  ·t117t2_deploy*.log)·캡처(pv_tts_*.png·pv_run*_*.png) = engine/tmp
  untracked — 커밋 금지 충족.
- **IP·호스트 경로 게이트(T2 실측)** — probe 커밋(a32a26d)+fix 5건
  (48c5474·d18c639·d0bec85·ce0b760·0746680) 커밋 원문 IPv4 정규식 grep =
  전부 0건·폰 주소 = known_hosts BatchMode 자기 판명 변수 안 소멸(주소
  미인쇄 계약 — 런 로그 전체 0건·SSH 로우 = 변수만)·`8022` 표기 = 기존 선존
  (포트 기정 — 8ad5501 원존).

## §7.15 창 번호 오버레이 원장 (#118 라인 — T1-T2(+fix r1-r5), 2026-10-27)

#118 라인 원장. 구두로 쉘의 창을 누르는 두 번째 방식 — "창 번호 보여줘"→각
보이는 창의 제목줄 좌측 번호 배지(compositor 오버레이 — T1)→"창 3"→focus_window
(T2 어휘 접음). 스펙=`docs/superpowers/specs/2026-10-27-window-overlay-design.md`
(8ad6d9e — D1-D5), 플랜=`docs/superpowers/plans/2026-10-27-window-overlay.md`
(d9ef40b), SDD 원장=`.superpowers/sdd/2026-10-27-window-overlay/`(task-1/2
리포트 — 미커밋 관행). T1 리포트가 배지 렌더·도구·원존 수형(락 교착 라이브
결함)의 원문 수취원 — 본 §7.15.1은 T2 어휘+부품의 1:1 재수취, §7.15.2-3은
probe/폰 배포 실측 원문+함정.

### §7.15.0 체인 (rev-parse 전체 SHA 실측 · git log가 진실원)

| 태스크 | 커밋(SHA — 실측 rev-parse) | 내용 (numstat) |
| --- | --- | --- |
| 스펙 | `8ad6d9e` | docs(apps): 창 번호 오버레이 설계 스펙 (#118 — 접근성 표준 UX·compositor 배지·창 단위 v1) — D1-D5 |
| 플랜 | `d9ef40b` | docs(apps): 창 번호 오버레이 플랜 (#118 T1-T2) |
| T1 | `6708134` | feat(apps): 창 번호 오버레이 배지+도구 (T1) — JKWindowServer.h(+68)/.cpp(+389/−32)·main.cpp(+87) — overlay_numbers 도구+DrawWindowNumberBadges+순수 부품 4종+3a 13건 — **[커밋 메시지 오염 1행 원장(T1 리포트 concern 1 — amend 금지 원본 유지)]** |
| T2 | `813ac43` | feat(apps): 창 번호 오버레이 구두 어휘+숫자판정 부품 (T2) — ChatRouter.h/.cpp+SttVoice.cpp+ClientChatApp.cpp+JKWindowServer.h/.cpp+main.cpp+jkweb+jktalk 9파일(+338/−20) — 어휘 exact 2행+ParseWindowNumberSpeech+ChatAction.winNum+OverlayNumbers kind+focus_window args.num+**원존 수형(배지 스냅샷 수집기 — 캡처 오버레이 면제 — T1 리뷰 Minor)**+2y-⑲ 10건·3a-⑤ 2건 |
| T2 probe | `2658c97` | feat(probes): #118 오버레이 E2E 세그먼트 — WSL+폰 probe (T2) — wsl_voice.sh(+172/−10)·phone_voice.sh(+195/−10) — §13c/B4 세그먼트+계보 갱신(캐논 850→892·2y→31·2z/3a 수취 행·#118-RUN-VALS+VOVER-VERDICT 별행 원존) |
| T2 fix r1 | `2b4c950` | fix(probes): wsl voice probe — click 1탭 직성립 경로의 회수 프레임 결함 — 118-R1 런 hard FAIL(clickrec copy) 원문 소각 |
| T2 fix r2 | `f641cfa` | fix(probes): voice probes — 오버레이 세그먼트 판독 결함 2건 — focused 케이스 어긋냄(str(True)+True→True 가양성 — 미포커스 대상 수형) |
| T2 fix r3 | `5ea90c5` | fix(probes): wsl voice probe — VOVER 판정 블록을 RUN-VALS 행 앞으로 이동 — RUN-VALS overlay=0 고착 소각 |
| T2 fix r4 | `3c706d6` | fix(probes): phone voice probe — B4 failout 인용 이중벗김 결함 — 원격 스크립트 파스 불성립(전면 미실행 — 무접촉 잔상 0) 소각 |
| T2 fix r5 | `a01e832` | fix(probes): phone voice probe — B4 배지 캡처 회수 순서 결함 — 원격 선소각(RECOVER-MISS) 소각 |
| T2 fix r6 | `763fedf` | fix(probes): #118 오버레이 — 번호 발화 스윕 정찰 — 세그먼트를 num=1..NVIS 스윕(focused 등호 원문+배지 캡처 전후 — 118-R1 가양성 원장행의 구조적 소각)·1차 정찰 = j118_swing_diag.sh(WSL) |
| T2 fix r7 | `966bc62` | fix(probes): #118 오버레이 — 스윕 정찰 결함 2건 — phone wjson >> 누적(stale 첫 행 — 118-P5 원문)+wsl 13c 창 도달 폴링(118-R5 창 2 원문) |
| T2 fix r8 | `3a953c5` | fix(probes): wsl voice probe — 13c NVIS 산치 행 누락 복원 |
| 본 T2(docs) | 본 문서를 포함하는 커밋 | docs(apps): #118 창 번호 오버레이 원장 — docs/90 §7.15+§4.1-12 + **fix r6 정산 원장(docs 정정 판 — 036622e 이후) — SHA는 git log가 진실원** |

### §7.15.1 어휘+부품 원리 (T2 → ChatRouter/SttVoice/JKWindowServer 단일 진실원 1:1 재수취 · 코드 재실측 0)

- **D3 어휘 = exact 2행 + 숫자 화법의 이분**: ①"창 번호 보여줘"/"번호 보여줘" = kRouteTable
  exact 행 2행(정규화 발화 전체 등호 — 접사("번호 자주 보여줘")=stub — D1 무인자 강제 정합).
  **Launch "보여줘" 행보다 앞 배치** — 뒤접미 판정은 배열 순서로 갈리므로 "번호 보여줘"가
  "보여줘"(Launch·비exact)에 눌려 앱어 "번호"의 launch로 새는 것을 막는 순서 계약("창 목록
  보여줘" 방어 행 2y-⑩ 동형 — 2y-⑲(b)(e)). ②"창 3"/"3번 창" = 표 행이 될 수 없는 가변
  숫자 발화(brief 원존 — "정확매치 어휘 한계") → **순수·결정 부품
  ParseWindowNumberSpeech**(2y-⑲(f)(f-2) — 4형: "창 3"/"3번 창"+접어체 "창3"/"3번창"
  (ExtractAppWord 접어체 무공백 원존 계승) — 0 = 화법 아님·"창 0"(배지 부정) 포함 — 상한
  99999 파서 자기 보호·**한글 수사("창 삼") = v1 미구어**(stub — 원장행: 전사 계보의 실관측
  EYES 몫 — 2y-⑲(h) 어설션).
- **ChatAction 대차** — Focus 재용(스펙 D3 "ChatAction Focus 재용" — 신설 kind는 표지 온만)
  +`winNum` 필드 1개 신설(0 = 미지정 — 기존 Focus 원존 무접촉·상호배타: 표 행 vs 숫자 부품이
  다른 길) + OverlayNumbers kind **Info 뒤 신설** — 기존 kind 수치 계보 보존(probe committed
  원문 행 kind=0/2/3/4/5 등호 판정 — 파손 방지). fail-clear 2곳(ChatRouterRoute·
  ChatLlmActionParse 진입에 winNum=0 — fail-closed 동형). **LLM 스키마 불변(D4 — 스펙)**
  — 번호 발화는 즉발 몫(파서 진입 winNum=0 — 모델이 winNum을 못 낸다 — 방어 사례만
  DefaultActionGuide에 문어 1행).
- **질의 조립(ChatActionQuery — SttVoice.cpp)** — OverlayNumbers =
  **`{"tool":"overlay_numbers","args":{}}`** 단일 양식(args 빈 객체 = on 부재 = 켬 —
  3a-④ 원존 재용·새 도구 0)·Focus-with-num = `focus_window {"num":3}`(1기반 — 서버 스냅샷
  판정 위임). **kVoicePrompt 갱신**(표=진실원 동기 계약 — #115b 원존): +"창 번호 보여줘
  번호 보여줘 번 창"(숫자 자체 = 가변 — 어휘 뼈대만 — wsl_voice.sh VPROMPT 문자 리터럴
  쌍둥이 동기 = 계보 원장행).
- **서버 판정 = 배지 스냅샷 수집기 1본(T2 원존 수형)** — HandleAgentQuery 상단의
  `badgeTargets` 람다: overlay_numbers 도구(answer의 count)와 focus_window "num" 양쪽이
  같은 필터·순서를 먹는다 — 입력 양식 = OverlayBadgeOrder의 그것(3a 원존·렌더 패스
  DrawWindowNumberBadges와 동기). **원존 수형(T1 리뷰 Minor — 도구 1행)**: 도구 분기가
  캡처 오버레이 클라를 면제 못 해 렌더 패스와 순간 어긋났던 것(렌더 :8682 면제·도구 미면제)
  — 수집기 1본으로 소각("번호의 진실원 = 화면 배지" 계약).
- **focus_window args.num (스펙 D4 focus_window 재용)** — **OverlayBadgeWindowForNumber
  순수 부품**(3a-⑤·⑤(b)) — 1기반 배지 숫자 → OverlayBadgeOrder 등호 스냅샷의 창 id·
  범위 밖/num<=0/빈 스냅샷 = 0 → `window_not_found` 정직 회신. **포커스 창 폴백 금지**
  (argless 원존[focusedClientId_ 폴백]은 지시 실패 형이고 num은 "지정했는데 화면에 없다"
  의 정직 실패). id(app)·num 상호배타 — hasId가 앞선다(기존 id 직접호출 계약 보존).
- **소비자 3좌 전송 계약** — 채팅 타이핑(ClientChatApp::Submit)·jkweb·jktalk은
  OverlayNumbers/번호 Focus를 **서버 도구로 실전송**(ListWindows 동형 — overlay_numbers는
  승인 사건 없는 표지 토글이라 app_tool 릴레이의 "구두 전송표면 한정" 계약과 다른 계열)·
  구두 = RunVoiceTurn dispatcher 경로(원존).
- **제어 관행 원존 봉쇄** — "창 번호 꺼줘"류 = "꺼줘"(Close) 접미 행이 잡는다(기존 X 꺼줘
  가족 원존 — app어 "번호"의 close = 서버 window_not_found 정직)·**off 어휘 = v1 생략**
  (D3 원존 — 10분 소멸이 끔을 대신한다 — 안내문에 그 사실 정직 표기 — 2y-⑲(d)).

### §7.15.2 캐논 계보 (2y/3a 계열) + probe·배포 실측 영수증

| 시점 | Win | WSL | posix | 폰 | 신설 |
| --- | --- | --- | --- | --- | --- |
| 기준(#118 T1 종착 = task-1 리포트) | 903 | 880 | 304 (승계 채권) | (880) | — (3a 13 보존) |
| T2 `813ac43` | **915** | **892** | 304 (승계) | (892 대기) | +12 (2y-⑲ 10·3a-⑤ 2) |
| probe/fix r1-r5(2658c97..a01e832 — probe 전용·src 무변경) | 915 | **892** | 304 (승계) | **892** | 폰 12 흡수 — **CANON=118T2-892** (배포 A/B 2런 등호 — WSL 892 등호 계보) |

- **Win 915 = 산치 승계 표기(1행)**: Win 축 = Win 903(T1 종착)+12 = 915 — 본 라인
  T2의 2축 ninja 직접 실측(Win selftest 1런 — `AppSelfTest: 0 failure(s)`·PASS=915
  원문)과 계보 산치 승계의 등호 — 플랫폼별 23차(2r/2s 등 데스크 축 케이스) 무변.
- **2y-⑲ 10의 분해** = ①창 번호 보여줘 즉발(kind 6 — OverlayNumbers) ②번호 보여줘 등호
  (exact 2행·순서 계약) ③overlay_numbers 질의 조립 등호(args 빈 객체 = on 부재 = 켬)
  ④안내문 10분 소멸 문어(v1 off 어휘 생략의 정직 표기) ⑤기존 회귀(알림 보여줘 Launch·창
  목록 보여줘 ListWindows — "보여줘" 접두 판정 무접촉) ⑥창 3·3번 창 = Focus winNum=3
  ⑦접어체(창3·3번창) ⑧focus_window num 질의 조립 등호 ⑨미매치 stub(창 0·창 삼·접사)
  ⑩기존 Focus 원존 회귀(앞으로 = winNum 0) — **3a-⑤ 2** = 배지 숫자→창 등호(비가시 소모 0
  계승)+범위 밖·num=0·빈 스냅샷 = 0.
- **posix 304 승계 = 승계 채권 원존(T1 리포트 §"posix 승계 재판정" 승계)** — posix 머신
  접촉 = 폰 접촉 계약 밖(§4.1-11 채권과 동일 관행 — ChatRouter.cpp는 posix 링크 성립 원존
  — 신설 2y/3a 케이스는 posix 하네스 미수용 → 셈 원문 변동 0 = 재런 불요 판정 승계).
  **채권 산치 = +12 예상**(오평 원장행 아님 — T1 리포트의 3a +13 채권과 합산 = +25).
- **WSL probe 2런(`wsl_voice_run118_{6,7}.log — 118-R6/R7 · fix r8(3a953c5) 이후
  probe — 최종 커밋의 2런)`**:
  `canon=892 twov=14 twoy=31 twoz=9 threea=15 fc3=3 direct=1 direct_t2=1 stt_t2_canon=0
  fixt=1 keyturn=2 clickturn=2 route=1 vocab=1 vprelay=1 vptoggle=1 notify=1 overlay=1
  ovcnt=1 ovfocus=1 ovnf=1 ovoff=1 probe_rc=0` — **VOICE-VERDICT: VOICE-OK +
  VOCAB-VERDICT: VOCAB-OK + VOVER-VERDICT: VOVER-OK 2/2** — 기존 legs 회귀 0.
- **§13c 오버레이 E2E 원문(R6/R7 등호):** 발화 4행 질의 양식 등호(`VOVER-ROUTE-OK:
  rows=4/4 … excl=2/2` — `text=[창 번호 보여줘] kind=6 … query=[overlay_numbers {}]`·
  `text=[창 3] kind=2 … query=[focus_window {"num":3}] num=3`·`text=[3번 창] … 등호`·
  미매치 2건 kind=5) → 창 3개 상태(chat+Video Player+Notifications)에서
  `overlay_numbers` → `{"ok":true,"count":3}` — **count 등호(OVER-COUNT: 3/3 —
  스냅샷 수집기 원존의 라이브 원문)** → **배지 원문 캡처(capture_region — diag_j118 수형
  재용)** `voc_wsl_badge3.png`(1280x720 — **Chat=1·Video Player=2·Notifications=3** —
  PW 표 원문 idx1=id 4(Chat)·idx2=id 35(Video Player)·idx3=id 36(Notifications)와
  배지 3개 육안 등호 — T1 캡처 원존 계보) → **번호 발화 스윕(fix r6 — num=1..3 각각
  focus → list_windows focused 등호 원문)**: `PW-POST: num=1 → idx1(id=4) true·num=2 →
  idx2(id=35) true·num=3 → idx3(id=36) true` — **전원 등호(배지 숫자↔창 매칭 = 도구
  판정 — 스윕 세그먼트 캡처 voc_wsl_badge_f1..3.png · 118-R7 PW 표 원문)** → **범위 밖
  num=99 = `{"ok":false,"error":"window_not_found"}` 정직(D5)** → **off 배지 소멸
  대조(voc_wsl_badge3_off.png — 배지 흔적 0 육안)** → 세그먼트 창 소각(REMNANT 0·
  permissions 원복 등호).
- **폰 배포 런(t118t2_deploy.sh — engine/tmp 산출물 — t117t2 수형 승계·커밋 0 —
  engine/tmp/t118t2_run.log+t118t2_deploy.log)**: DEPLOY-BASE = HEAD 전수(T1 6708134+T2
  813ac43+probe 커밋 전수 — SWEEP-POST-DIFF=0 hard 게이트)·LEG-STAGE-WIRED(SPATIAL leg
  유지배선 — libopenal.a)+WHISPER-ROOT-WIRED(tree 149)·CMAKE-RC=0·**NINJA-RC=0
  SECONDS=447**(aarch64)·JKDESK-BYTES 38,074,672B·`DEPLOY-MARKERS: 2r=11 2s=21 2u=136
  2v=22 2w=4 2x=19 2y=34 2z=9 3a=16`(라벨 계수 — 본문 주석 행 포함 — PASS 기준 2y=31·
  3a=15)·MUSP-LEG-SYM alcOpenDevice strings=3(leg 링크 유지)·whisper-cli aarch64
  cold-run 정상(LDD not-found 0)·tiny-ko 핀 77,691,730B 등호(조달 재용 불요)·music.jkx
  재팕(PACK-RC=0·ICON-ENTRIES=2)·서버 재기동 **WHISPER_ROOT 보유 부팅**(부팅 env 승계
  원존)·`PHONE-SELFTEST-A rc=0 PASS=892 FAIL=0 2y=31 3a=15`·**CANON-A=892**·런B 동일
  **CANON-B=892**(2y=31·3a=15 양런 — WSL 892 등호)·settings 213B/permissions 33B
  **무변경 등호**·REMNANT-LS-RC=2(자기 소각) — **`T2-DEPLOY-VERDICT: T2-DEPLOY-OK`**.
  **brief 표기 정산 원장행**: brief "기대 폰 캐논 = 880 승계"의 880 = T1 종착 시점 값 —
  본 T2 신설 +12(2y-⑲ 10·3a-⑤ 2)의 승계로 폰 기대 = **892**(WSL 등호 계약)로 갱신 —
  실측 등호.
- **폰 probe 동형 세그먼트(phone_voice.sh B4 — engine/tmp/phone_voice_run118_{6,7}.log —
  2런 ALL OK — P1~P5 = 결함 발견 런(§7.15.3)은 원장 정산)**:
  `VOICE-PHONE-VERDICT: VOICE-PHONE-OK`(+**`VOCAB-PHONE-VERDICT: VOCAB-PHONE-OK`**+
  **`VOVER-PHONE-VERDICT: VOVER-PHONE-OK`** 2/2)·**배지 원문 캡처 6종 md5 등호 회수**
  (`pv_badge_n.png`(154,254B — **Chat=1·Video Player=2·Notifications=3 — PW 표 원문
  idx1=Chat(id 11)·idx2=Video Player(13)·idx3=Notifications(14)와 배지 육안 등호 —
  z등호 원존의 폰 원문**·**[118-R1 리포트 오기("Video Player=1·Chat=2") = 스왑 오기 —
  본 문서에서 정정·정찰 원장행으로 소각 — §7.15.3]**)·`pv_badge_off.png`(배지 소멸
  대조)·`pv_badge_f1..3.png`(스윕 캡처 — 회수 후 소각))·OVE 세그먼트 원문 등호:
  `OVE-ON {"ok":true,"count":3}`·`OVE-COUNT: 3`·**`OVE-FOCUS-SWEEP-OK: 1/3`(fix r6
  스윕 — num=1/2/3 전원 focused 등호 — PW 표 원문: num=1 → idx1(Chat·id 11) true·
  num=2 → idx2(Video Player·id 13) true·num=3 → idx3(Notifications·id 14) true —
  118-P6/P7)**·`OVE-FOCUS-99 {"ok":false,"error":"window_not_found"}`·
  `OVE-MUSIC-RESTORE ok`(사용자 화면 = Music 복원 원존)·**OVE-REMNANT: phone overlay
  scratch left=0**(잔상 0 계약)·permissions 무접촉(overlay/focus/capture = ask 아님 —
  T1 스모크 원존).
  `#118-RUN-VALS: overlay=1 ovlroute=4 ovlive=1`(양런 — RUN-VALS 스트림에 overlay 필드
  전원 가산 원존).
- **TTS 별행 = `TTS-PHONE-FAIL(honest — cli=0[웨지 3표본]·stamp=1·session=1·optout=1·
  lat 21,166/18,492ms)`** — **#117 런 밖 웨지 원장행 승계**(§7.14.4·§4.1-11 — 재부팅 =
  사용자 몫)·별행 판정 계약(VOICE-PHONE-VERDICT 무변조 — #115b VOCAB 별행 원존) 그대로
  — 본 라인 소관 아님(정직 기록).
- **stt_t2_canon=0**(전사 비결정 — §2.4 CANON 밖 — #115b 원장 승계).

### §7.15.3 함정 원장 (#118 T2 신규 — 봉쇄 레슨 축 · 전량 = 런/T1 리포트 원문 수납)

- **click 1탭 직성립 경로의 회수 프레임 결함(fix r2b4c950 — 118-R1 hard FAIL)** —
  `VOC-FAIL: clickrec copy`: 1탭이 곧 착탄(재시도 없음)이면 voc_ck2.png가 존재하지 않는데
  성공 분기가 그것을 회수 — 정찰 원존("클릭 1 = 무음 소멸이 관행")이 2탭 경로만 실측시킨
  함정(숨은 경로 unbekannt) — 수형 = CKFRAME(실제 변화 프레임) 회수.
- ** focused 판독 케이스 어긋냄 + "True→True" 가양성(fix r2 f641cfa — 118-R1)** —
  python `str(True)` = `True`(리스트_windows JSON의 `true`와 케이스 어긋남 — 등호 판독
  MISS)+이미 포커스된 창에 num을 찍으면 착탄이 기저 상태와 무차별 — 수형 = focused
  소문자 판독+라이브 소비 대상 = **미포커스 창의 배지 숫자**(즉발 행은 "창 3" 고정 —
  VOV 양식 등호는 유지).
- **RUN-VALS 행의 overlay=0 고착(fix r3 5ea90c5 — 118-R2)** — RUN-VALS 요약 행이
  VOVER_OK 계산 이전에 인쇄(같은 로그의 #118-RUN-VALS=1과 모순) — 수형 = 판정 블록을
  RUN-VALS 행 앞으로 이동. **nit 원장행**: 이동 후 "=== 15b." 헤더 라벨이 물리 순서상
  "=== 15." 앞에 온다(라벨만 — 산치·판정 무영향).
- **failout 인용 이중벗김(fix r4 3c706d6 — 118-P1)** — 원격 스크립트의 grep/ctl 인용이
  디코딩 계통에서 벗겨져 `unexpected EOF while looking for matching quote` — **원격
  스크립트가 파스 불성립으로 전면 미실행**(창·permissions 무접촉 — 잔상 0 — 파스 단계
  실패의 부작용 = 무해 원문) — 수형 = 단일 인용 조립(이스케이프 계통 소거). **probe
  본문 bash -n 통과와 원격 스크립트 파스는 별개 검사** 원장행(heredoc 본문 = 런타임까지
  파스 안 됨).
- **배지 캡처 원격 선소각(fix r5 a01e832 — 118-P2)** — 원격 스크립트가 호출자의 md5 회수에
  앞서 배지 PNG를 소각(`RECOVER-MISS: $TMPDIR/t6pv_badge_n.png` 원문 2건 — 회수 0) —
  수형 = 원격 소각 목록에서 배지 PNG 제외(호출자가 회수 뒤 소각 — t5_chat_* 회수
  원존 계약과 같은 순서 — **캡처의 회수-소각 순서 = recover 계약 원장행**).
- **"2/3 스왑" 관측 = 리포트 문언 오기 — 정찰로 미단정 해소(fix r6 763fedf + 1차 원문)**:
  [원장행 — 원문 우선] 배지 원문 캡처와 focus 판정의 순서 부정합(스왑) 여부를
  분절하는 정찰 — 1차 = `engine/tmp/j118_swing_diag.sh`(WSL 1런 — 원문): 창 3개
  (PW 표 idx1=Chat(id 3)·idx2=Video Player(5)·idx3=Notifications(7))에서 num=1/2/3
  각각 focus → **전원 focused=true 등호**(`FOCUS-EQ-1: idx1 [3]`·`FOCUS-EQ-2: idx2 [5]`·
  `FOCUS-EQ-3: idx3 [7]` — j118_swing_focus1..3.png 원문 — **배지 라벨 확정 =
  Chat=1·Video Player=2·Notifications=3**) → 2차 = 세그먼트 스윕 확장(fix r6 — WSL
  R6/R7·폰 P6/P7 2런 등호 — §7.15.2 원문). **정산: 스왑 관측 = 리포트 문언 오기
  (폰 배지 라벨의 Video Player↔Chat 스왑) — 렌더/도구 부정합 불성립** — 구조 근거:
  렌더(DrawWindowNumberBadges)와 도구(badgeTargets 수집기)가 같은 clients_ 순회(같은
  순서·필터 — T2 스냅샷 수집기 원존) — **코드 수형 불요**(원문이 확정됐을 때만 수형
  원칙 준수 — badgeTargets 단일화 제안은 이미 성립). **원장행(잔여)**: 스윕 캡처는
  폐색 창의 배지가 앞 창 위로 뜨는 관측을 함께 수취(T1 리뷰 Important 원장행의
  EYES ③ 증거 원문 — voc_wsl_badge_f2.png·pv_badge_f1..3.png).
- **표본 파싱 누적 결함(fix r7 966bc62 — 118-P5)** — wjson 표본 .txt의 `>>` 누적:
  재호출 시 awk 단정이 **첫 행 = 전 focus 표본**(stale)을 본다 — 본 런 실측:
  num=2/3 등호 MISS 오진(`OVE-FOCUS-FAIL: num=2 focused=[false] id-이동=[13 → 13]`)
  — **서버측 포커스 전이는 PW 표 후반 행에서 전원 확정**(num=2 → Video Player true·
  num=3 → Notifications true — 판독 leg만의 결함) — 수형 = overwrite(WSL 동형).
- **창 도달 레이스(fix r7 966bc62 — 118-R5)** — 13c의 sleep 4 고정 대기 = vplayer 창의
  spawn 레이스(실측: **창 2** — Video Player 미도달·count=2 — 원문) — 수형 = 창 2종
  도달 폴링(§13/§13b 폴링 원존 수형)·**fix r8 3a953c5(118-R6)** = 폴링 블록 전개에서
  NVIS 산치 행 드랍(OVER-VISIBLE-WINDOWS=0 표기 — 폴링 자체는 2회차 창 3 도달 성립
  — 판독 행만의 결함) — 복원.
- **fix r1 결함 원문 수취(본 문서 — T2 리포트 원문 1행 누락 정산)** — 118-R1 런의
  hard FAIL 원문 행: `VOC-FAIL: clickrec copy`(§12 클릭 세그먼트 — 1탭 직성립 경로의
  voc_ck2.png 미존재 회수)·전사 축은 그 앞에서 원문 등호(direct 등호 2판·fixture
  전사 [(뽀송한 소리)] 원문) — 런은 hard 출구(복원 미수행 — 서버·permissions·
  /tmp/voc* 잔상)로 수기 소각 실측(서버 0·permissions rm·scratch rm — 원장행).
- **복합화법 원장행(T2 fix r6 정산 — 정직 부기)** — "창 3 앞으로"류 복합 발화 =
  표 접미 행("앞으로" Focus) 매치가 우선(배열 순서 계약 원존) — **숫자 부품
  비도달**: `jktalk --route "창 3 앞으로"` 원문 = `kind=Focus app=`(가이드 "포커스
  창을 앞으로 가져옵니다") — Focus는 appless로 서버 argless(포커스 창)를 쳐준다
  — "3"의 의미 소실(무해·정직 회신 — window_not_found 아님) — v2 어휘 합의 시
  복합화법 판정(번호 우선 여부) 재판정 후보.
- **T1 리포트 concerns 전량 원문 수취(T1 리포트 §concerns 6건 — 원장행 유지)**:
  ①T1 커밋 메시지 오염 1행(amend 금지 계약 — 원본 유지·코드 무영향) ②10분 소멸의 live
  실측 미수행(순수 판정(3a)만 폐곡 — 런타임 상수 단축 불가 — 다음 세션 승계 채권 — 본
  T2도 미수행 정직 기록) ③최소화 창의 번호 소모 0(보이는 창 1..N 재부여 — 재량 —
  list_windows 행수와 어긋나는 창 존재 시 대화 1 어긋남 — v2 어휘 합의 시 재판정 후보)
  ④배지의 "capture_region 등장" = compositor 훅 계약 원존("화면에 보이는 것이 곧
  캡처다" — 캡처 원문 배제 요구 시 별도 합의) ⑤3a 건수 13건 = 원소 단정 관행(부품별
  1행 — 본 T2 +2로 15) ⑥WSL 9p 캐시 사건 원장(병행 ninja 경합 — wsl --shutdown 후
  단독 재빌드 소각 — "캐논 재측정 0").
- **T1 리뷰 Important 원장행(전수 — 본 문서 필수 행)**: **폐색(가려진) 창도 번호 배지를
  얻는다**(배지 = z 최상단 그리기 — brief 계약 원존·구현자 diag_j118_badge_two_windows.png
  증거 원문) — "뒤에 가린 창의 번호도 배지가 나온다"는 UX 이중성: 번호 보여주기의 목적
  (가려진 창을 번호로 부르기)과 상면 충돌하는 면도 있다. **EYES 게이트에서 사용자 기각 시
  수리 후보 = ①폐색 창 면제(가려진 창 배지 제외) ②z-순위 인지 그리기(가려짐 투명도/번호
  계열)** — v1 = 현행 유지(원장 기록 — 재판정 대기).
- **nit 원장행 2건 정정(T2 fix r6 정산 — 인용 원문 대차)** — ①null-캐시 규약 주석:
  인용 위치 정정 = `JKWindowServer.cpp:8785-8786`(T1의 "8702 부근" 표기 오기) —
  주석 원문 = "폰트 실패(텍스처 null) = 배지 없이 다음 번호로 — 매 프레임 재시도를
  / 막는 null 캐시 엔트리 규약(승인 배너의 실패는 null 엔트리로 기록)" — 규약 본문 =
  BadgeNumberTexture(:8902 — "문자열당 1회 찍어 캐시·실패는 null 엔트리로 기록") —
  함정 = 주석의 "재시도를 막는"이 규약 본문(같은 문자열의 **실패 엔트리 캐시 기록**
  — null 텍스처도 캐시되어 로그 반복·재생성 없음)과 다르게 읽힌다 — 부기(코드
  무변경 — 다음 접촉 몫) ②off 대조 diff 원장행의 "배지 영역만 변해야" 문언이 원문
  행의 사실(bbox 광역 — 포커스 z변경 혼재)과 어긋나게 읽혔다 — 정정 = 배지 소멸+
  포커스 z변경의 혼재(아래 행) 유지.
- **부품 Files 갱신 원장행(brief/플랜 갭)**: "창 3" 화법의 서버 판정 = focus_window
  args.num 확장 — 플랜 Task 2 Files 원존(ChatRouter.cpp만)에 없는 **JKWindowServer.h/.cpp
  변경 1건** — "focus_window(id = z등호 원존)" 구현을 위한 불가피 확장(F1 계약 확장
  선례 — args.id 직접호출 계약 보존 + 선택 인자 추가) — close_window/focus_window의
  id/app 형 무접촉.
- **off 대조 대차 원장행**: OVER-DIFF-OFF(1105px — bbox 광역) = 배지 소멸+포커스가 낳는
  z변경의 혼재(포커스 먼저 — 차분 순서 원존) — 배지 단증 = off 캡처 육안(배지 흔적 0
  — 위 원문) — 차분 단독 판정 아님(정직 표기).

### §7.15.4 비-목표 + EYES 게이트 (사용자 선언만 결제 — §5/§7.9.5 원칙 동형)

- **비-목표(스펙 §3·D3)**: 요소 그리드/드릴다운(v2)·오버레이 마우스 클릭(이벤트 통과
  금지 — D2)·번호 편집·off 어휘(v1 생략 — 10분 소멸)·한글 수사 화법("창 삼")·LLM 스키마
  확장(D4 불변)·폐색 창 배지 면제(위 원장행 — 기각 시 수리).
- **EYES 대기**:

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **폰 "창 번호 보여줘"→배지→"창 3" 포커스 체감** | 폰 chat [구두]에서 실발화 — 배지 표시(체감)→"창 3" 발화→해당 창이 앞으로 오는 체감 — probe = 도구 경로 원문+캡처 원문만(pv_badge_n.png는 합성 단계 원문 — 실발화 전사 본문 = CANON 밖 §2.4) — **플랜 사용자 게이트 1** | **대기(발화 체감)** |
| ② | **WSL/Win 배지 모양+번호 포커스 체감** | 데스크 축 F6 발화 + 배지 모양(반투명 검은 바탕+흰 숫자 — voc_wsl_badge3.png 원문)·10분 소멸 관측 | **대기(육안·체감)** |
| ③ | **폐색 창 배지 수용/기각 판정** | T1 리뷰 Important 원장행 — 가려진 창도 배지를 얻는 현행의 수용·기각 — **기각 시 = 즉시 수리(폐색 면제 또는 z-순위 인지 그리기)** — 정찰 원문 존재(diag_j118_badge_two_windows.png[T1]·voc_wsl_badge_f2.png·pv_badge_f1..3.png — 앞 창 위로 뜨는 가려진 창의 배지) | **대기(육안·판정)** |
| ④ | **한글 수사 전사 관측** | "창 삼"류 전사 출현 관측 시(negative 어설션 2y-⑲(h) = v1 미구어) 수사 부품 확장 재판정 | **대기(관찰)** |

### §7.15.5 커밋 원장 + IP/호스트 경로 게이트 (#118 라인)

- 라인 체인 = §7.15.0 표(8ad6d9e..본 docs 커밋 — rev-parse 전체 SHA 원존 — git log가
  진실원) — push 전부 origin main 직행·amend 없음·**커밋 = 명시 경로만**(T1 = src 2파일+
  main.cpp[커밋 메시지 오염 1행 — 위 원장행]·T2 = src+include+jkweb/jktalk 9파일·T2
  probe = probe 2파일·fix r1 = wsl 1파일·fix r2 = probe 2파일·fix r3 = wsl 1파일·
  fix r4/r5 = phone 1파일·fix r6 = probe 2파일·fix r7 = probe 2파일·fix r8 = wsl
  1파일·본 docs = docs/90 1파일) — `git add -A` 0·커밋 분리 원존(어휘 커밋+probe
  커밋+fix 8건+docs 커밋 2건[원장 + fix r6 정산]).
- **IP·호스트 경로 게이트(T2 실측)** — 어휘 커밋(813ac43)+probe 커밋(2658c97)+fix 8건
  (2b4c950·f641cfa·5ea90c5·3c706d6·a01e832·763fedf·966bc62·3a953c5) 커밋 원문 IPv4
  정규식 grep = **전부 0건**(커밋 전 staged 원문+커밋 후 `git show` 이중 게이트)·폰 주소 = t118t2_deploy.sh의
  known_hosts BatchMode 자기 판명 변수 안 소멸(주소 미인쇄 계약 — deploy/probe 런 로그
  전체 0건·SELFRESOLVE: MATCH 행만)·`8022` 표기 = 기존 선존(포트 기정 — 8ad5501 원존).
- 배포 드라이버(t118t2_deploy.sh·t118t2_remote.sh)·런 로그(wsl_voice_run118_{1..7}.log
  ·phone_voice_run118_{1..7}.log·t118t2_*.log·j118_swing_diag.sh[정찰 — engine/tmp
  1회성 원문])·캡처(voc_wsl_badge3*.png·voc_wsl_badge_f*.png·pv_badge_*.png·
  j118_swing_*.png·diag_j118_*.png[T1 증거 원존]) = engine/tmp untracked — 커밋 금지
  충족.
### §7.16 폰 자연어 릴레이 leg — chat.json "ollama_host" 원존+PC 접수 준비 (#119 T1-T2, 2026-10-27/28)

폰의 구두/타이핑 자연어를 PC의 ollama(내장 cloud GLM)로 릴레이해 턴 품질을
끌어올리는 접점(#119 — 폰 LLM 턴 재실측 스파이크
`.superpowers/sdd/2026-10-25-jkapk/phone-llm-respike-report.md`의 수형 후보 후속
계약). 근거 원문 = 스파이크 §3b(폰 /api/chat 직행 1.58s 완건 JSON — 렌더층 원진
수형 ad9aa81 #113)·§5(2안 — CLI 플래그 결합/D2 직행). 엔진 코드 계약 =
ChatConfig(JKLmEngine.h — 승격 게이트 fileKnown 선존 계열)·조립식 계약 락
(BuildOllamaDirectCmd — kStubShellCmd* 선례 단일 근원).

#### §7.16.1 구현 원문 (T1 — 커밋 f1d57e6 — 3파일 +177/−5)

- **조사 판정(원문 실측)**: JKLmEngine에 "HTTP/11434" leg는 **원존하지 않는다**
  — engine 계열 전부 CLI 스폰("ollama" = `ollama launch claude`·"ollama-direct"
  = `ollama run`)이고 서버 상대 지정은 코드 내 URL 하드코딩 0건(grep 실측 —
  src 전역 11434/localhost 리터럴 0건)으로 ollama CLI 관례(OLLAMA_HOST env 미
  설정 = localhost:11434)에 의존한다. 따라서 원격 호스트 지원 = **조립식에
  OLLAMA_HOST 접두를 실리는 1키 원존**("ollama_host")이 최선 형태 — chat.json
  키 1건 원존 추가(기존 키 무접촉·빈 값/키 부재 = 기본 localhost = OLLAMA_HOST
  미설정과 동일 — fail-closed 계약 유지).
- **NormalizeOllamaHost** (JKLmEngine.h 선언+정의 계약 락) — 문자 집합 판정부:
  [http://|https://](선택)+호스트([A-Za-z0-9._-] 1+)+(":"+숫자 포트 선택) ·
  양끝 trim·위배(& | $ \` " 공백·경로·숫자 아닌 포트·스킴만 있는 기형) 전면
  부정 → 빈 문자열 무음 접기. 접두 조립(OllamaHostEnvPrefix)이 이 판정의
  인용·메타문자 소진에 의존(이스케이프 불요 계약).
- **OllamaHostEnvPrefix** — ollama 계열 leg 양쪽 단일 근원: posix
  `OLLAMA_HOST="<host>" ` 접두 / win32 `set "OLLAMA_HOST=<host>" && ` 접두
  (cmd.exe /c 접두 뒤 set 먼저 — 조립식 본선과 && 연결). "claude"/"stub" leg
  무영향.
- **BuildOllamaDirectCmd** — 접두는 스팬 밖 선두(모델·프롬프트 ShellDqEscape
  인용 계약 무변조)·빈 값 = 계약 변경 이전 형태 바이트 등호(변경 1 = 2026-10-26
  #113 렌더 플래그 결합 — 이후 "계약 변경 2" 원장행).
- **selftest 2aa 신설**(2z 뒤 — main.cpp RunAppSelfTest — 4건): ①빈 값·blank =
  빈 값(기본 localhost 원존) ①b 스킴 양형+플레인 host:port·플레인 호스트 trim
  승계 ②빈 값 조립 = 접두 없음+구성 조립 = 접두 착지+suffix 등호(win cmd set
  접두/posix sh env 접두 #ifdef 분기) ③위배 상대 8형(h&calc·h$(id)·h"x·h o c·
  경로·:11434·PC_HOST:port·https://) 조립 = 기본 형태 바이트 등호(무음 접기 —
  조립식 주입 표면 0). **스폰 어셜션 금지(D6 — 비결정 원존 2z 승계) — 실 릴레이
  턴은 probe/E2E 몫.**

#### §7.16.2 캐논 승계 (4축 실측 — 2aa +4)

| 축 | 값 | 근거 |
| --- | --- | --- |
| Win | **919** (915+4) | `jkdesktop.exe test` 실측 — 2aa 4건 PASS·AppSelfTest: 0 failure(s) |
| WSL | **896** (892+4) | buildwsl selftest RC=0 — 2aa 4건 PASS |
| posix | **304 등호** | posix_selftest build+run — 0 failure·llm15 조립식 원문 전 항 등호(빈 값 = 계약 변경 이전 형태 — 기존 단정 무침범의 증명) |
| 폰 | **896** (892+4) | T2 배포 런(§7.16.4) — aarch64 selftest rc=0·2aa 4건 PASS |

#### §7.16.3 PC 접수 준비 조사 (T1-2 — netstat 원문·portproxy 불요 판정)

- **원론 대비 실측 정정 원장행**: 원론 전제 "PC ollama가 127.0.0.1 바인딩(외부
  수신 불가)" = **미성립** — netstat 실측 원문: `TCP 0.0.0.0:11434 LISTENING
  3928`+[::] 동일(ollama 0.40.11 — `ollama list` glm-5.3-flash:cloud 등 cloud
  모델 원존·127.0.0.1 왕복 등록 연결 2건)·인바운드 방화벽 규칙 **ollama.exe
  허용 2건(TCP+UDP — 프로그램 규칙 전포트 Allow)** 실측 — LAN 수신 도달은
  기존 상태에서 성립한다.
- **WSL 릴레이 턴 원문(도달 증명)**: WSL→PC 11434 TCP 등호 도달+`/api/chat`
  stream=false 턴 = **2.297s·thinking 별도 필드 분리·content 완건**(스파이크
  §3b 원문 계형 — 스파이크 1.58s 대조 = PC 로컬 대비 +0.7s 수준).
- **portproxy 불요 판정** — 위 실측에 따라 netsh portproxy 수행 **0**(관리자
  권한 스폰 불요). 수기 시 원존 1커맨드(기록 보존 — 127.0.0.1 단일 바인딩으로
  돌아간 경우 몫):
  추가 = netsh interface portproxy add v4tov4 listenport=11434 listenaddress=0.0.0.0 connectport=11434 connectaddress=127.0.0.1 /
  삭제 = netsh interface portproxy delete v4tov4 listenport=11434 listenaddress=0.0.0.0
- **보안 원장행(옵트인 계약 원존 — 현 상태 고증)**: 11434 LAN 노출 = **무인증
  ollama API**(LAN 내 임의 기기가 모델 턴 실행 가능) — 사용자 자택 LAN 전제.
  단, 본 라인은 **노출을 새로 만들지 않았다**(0.0.0.0 바인딩+ollama.exe 규칙 =
  기존 상태 실측 — portproxy 수행 0·방화벽 변경 0) — 릴레이 옵트인 판정 =
  폰 구성(§7.16.4 병합)만. 끄기 원존: 폰 chat.json의 "ollama_host" 키 삭제
  (기본 localhost 복귀)+원한다면 ollama를 127.0.0.1 바인딩으로(서비스 env
  OLLAMA_HOST=127.0.0.1)·portproxy 채택 시 위 원존 삭제 1커맨드.
- **수기 win CLI probe 웨지 원장행(수행 0 정산 포함)**: set OLLAMA_HOST 수기
  cmd 경유 CLI 턴 1런 = **stdin 생존 무응답 웨지**(2.0-5.5s 기대 구간 120s+
  초과 — 스파이크 §3c CLI stdin 함정의 win leg 재실측) — 백그라운드 태스크 강제
  소각 정산. 엔진 스폰 계약 무영향(win = CreateProcessW 파이프 스폰·posix =
  fd-0 /dev/null — 폰 jkweb spawn 계약, 스파이크 §3c 원존). 본 계보의 실 릴레이
  턴 진실원 = WSL 직행 원문+폰 E2E(§7.16.4).

#### §7.16.4 폰 배포 런 원문 (T2 — 5런·드라이버 engine/tmp/t119_t2_deploy.sh — 커밋 0)

- **폰 chat.json 병합(무손실 게이트)**: 시작 sha256 =스파이크 §1 원문 등호
  (59be04fadae37b2f 접두 — 104B) → python json 병합(engine·model·directory 키
  등호 보존+"ollama_host" 가산) = **MERGE-LOSSLESS missing=0**·149B·keys
  engine,model,directory,ollama_host. 병합값은 유지 원존(PC 접수 후 폰 턴이
  릴레이하는 구성 — 끊는 것 = 사용자 몫·키 삭제 = 기본 localhost 복귀).
- **폰 재빌드+캐논(run5 — 본 런의 수형)**: NINJA-RC=0 SECONDS=403(aarch64 -
  증분)·selftest rc=0·AppSelfTest: 0 failure(s)·**PASS=896**·**2aa 4건 전부
  PASS**(원문 = deploy log SELFTEST-2AA 행 4건).
- **BLOCKED 원장행(폰 /talk 릴레이 턴 — 미실측 정직)**: 폰→PC 11434 TCP =
  **[Errno 113] No route to host**·폰→PC ICMP 100% loss(+2 errors)·폰 ARP 표
  0건·폰 자기 소스 주소 = PC 서브넷 동일 접두(getsockname 실측)/PC→폰 8022 ssh
  = 양시 성립 — **일측 L2 격리**(AP 클라이언트 격리 또는 대역/AP 분리 원론).
  진단 원문 = deploy log PC-CONNECT-FAIL 행+진단 런 3건(마스킹 인쇄 — 주소
  일절 미인쇄 계약). 폰 릴레이 턴 원문(비매치 2건×2회+즉발 회귀 1건)·지연 대조
  (스파이크 1.58s) = **미실측 — 드라이버 run5의 D-H 세그먼트 정직 미수행**.
  해제 = 사용자 네트워크 조치 몫(공유기 AP 격리 설정·폰 SSID/대역 재접속) —
  재런 = t119_t2_deploy.sh 재실행(D-H 세그먼트만 증분 수행).
- **배포 절차 함정 원장(runs 1-4 — 봉쇄 레슨 축)**:
  1. 전수 git archive HEAD(71.9MB) tar-over-ssh = 폰 sshd 절단
     ("client_loop: send disconnect" — run1) — **증분 배포**로 대체: 등화 기준
     a7424fa(#118 T2 배포 등화)·git log 차분 = 3수형 파일만 push+md5 게이트.
  2. **Windows core.autocrlf=true → git archive 멤버 CRLF 스머지**(실측:
     jkweb/main.cpp blob 31318LF vs 멤버 31935CRLF — run2/run3 md5 오진
     원장행) — 아카이브 배포의 무결성 게이트 비교 기준 = **아카이브 멤버 바이트
     md5**(작업복사 md5가 아니다).
  3. 폰 트리 전수 등화 보증 불성립 실측(jkweb/main.cpp CRLF 잔유) → 리빌드
     입력 서브셋(src/include/tools-jkweb/tools-jkbridge/CMakeLists — 4.47MB)
     전량 등화+post-equalize md5 4종 등호 게이트(run3 이후 성립).
  4. 폰 /tmp 쓰기 불가(기존 원장 승계 — run4 Permission denied) → $TMPDIR 몫.
  5. [run1 부작용 원장행] 절단 전수 archive가 폰 트리 일부를 CRLF+HEAD 내용으로
     부분 교체했다(절단 전 수신분) — 리빌드 입력 서브셋 등화가 전량 복구 —
     잔여(diff 영역 밖 파일의 CRLF화)는 빌드 무영향 원장(비재발 관리 원존 =
     서브셋 등화 원존).
- **폰 창/서버 상태 원복**: 서버 UP 유지(run 전 1 = run 후 1)·jkweb = run 전
  0(재기동 세그먼트에는 도달하지 못한 정직 미수행 — 재런의 G 세그먼트 몫)·
  permissions 무접촉·폰 TMPDIR 잔상 0(사후 소각 실측 REMNANT=0).

#### §7.16.5 비-목표 + EYES 게이트 (사용자 선언만 결제)

- **비-목표**: claude/stub leg의 호스트 개념(claude CLI는 상대 개념 없음)·
  jkweb UI 인증(무인증 loopback 기본 원존 유지)·ollama 대신 다른 백엔드.
- **EYES 대기**:

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **폰 릴레이 턴 체감** | 폰 자연어(비매치 발화) → PC cloud GLM 답신의 지연(스파이크 1.58s 대조)·품질(stub 안내문 → 행동 JSON 턴) 체감 — **전제 = 폰→PC L2 격리 해제(§7.16.4 BLOCKED — 사용자 네트워크 조치)** 후 t119 드라이버 재런 | **대기(전제 결손 — 사용자 조치)** |
| ② | **LAN 노출 옵트인** | 11434 무인증 LAN 노출의 지속 동의(현 상태 = 기존 실측 — 신규 노출 아님) — 기각 시 폰 chat.json 키 삭제(기본 localhost 복귀) | **대기(사용자 판정)** |

#### §7.16.6 커밋 원장 + IP/호스트 경로 게이트 (#119 라인 — 현행까지)

- 라인 체인: **f1d57e6**(T1 — agent 3파일) + 본 docs 커밋 = docs/90 1파일 —
  push origin main 직행·amend 없음·커밋 = 명시 경로만·git add -A 0.
- **IP·호스트 경로 게이트(T2 실측)**: 커밋 원문 IPv4 정규식 grep = 0건
  (selftest 원문도 "PC_HOST" 자리표시 리터럴 — 인쇄 규약 준수)·PC 주소 =
  temp 파일 판독+ssh argv 전달(원격 파일·런 로그 리터럴 0건 — PC-HOST-LEN: 15
  행만 인쇄)·폰 주소 = known_hosts BatchMode 자기 판명+SELFRESOLVE: MATCH 행만
  ·진단 런 3건 = 주소 마스킹 인쇄(호스트부 0건). **[원장행 — 세션 콘솔 누출
  1회**: 수기 CLI probe의 PowerShell 오류 메시지에 PC 주소 1회 인쇄(커밋·런
  로그·보존 파일 0건 — 세션 기록에만 존재) — 수기 probe 폐기(§7.16.3 웨지
  원장행)와 함께 소화 원장]. 본 문서 원문도 PC 주소 리터럴 0건(점문 게이트 =
  커밋 전 grep 실측).
- 배포 드라이버(t119_t2_deploy.sh)·런 로그(t119_t2_deploy.log)·진단 스크립트
  3건 = engine/tmp untracked — 커밋 금지 충족.

#### §7.16.7 폰 단독 승계 원장행 (스파이크 2 + 전환 승계 — 2026-10-28, 원문 리포트
`.superpowers/sdd/2026-10-27-relay-relay/phone-local-respike-report.md`)

- **재실측 원문(렌더 플래그 수형 ad9aa81 후 — 드라이버 engine/tmp/prb2_driver.sh,
  원문 로그 prb2_run.log — 커밋 0)**: POST /talk 비매치 3건×2회+스파이크 동일 즉발
  회귀 1건 = **8/8 유효 JSON 성립(stub 폴백 0건 — 스파이크 §2의 "인식하지 못했습니다"
  안내문 재실측 0)**·지연 **2.14-3.82s 안정 밴드(median 3.7s)**·**7/8 ≤5s** —
  예외 = serve 기동 직후 첫 cloud 턴 1건 **14.60s**(콜드 아웃라이어 — 3회 재현
  재측정 = 후속 원장행; 전환 후 단독 세션 첫턴은 3.618s로 재현 0/1 — 원문 아래
  전환 행). argv stamp(/proc cmdline) = **9/9 `ollama run --hidethinking
  --nowordwrap glm-5.3-flash:cloud …`**(prb2 8+prb3 1) — ad9aa81 플래그가 폰
  스폰에 실제 적용된 원문.
- **판정(컨트롤러 승계 — 폰 단독)**: PC 릴레이는 폰→PC L2 격리(§7.16.4 BLOCKED —
  사용자 네트워크 조치 전제)를 요구하는 데 비해 폰 로컬은 구성만으로 성립하고
  지연도 WSL→PC 릴레이 대조 원문(2.297s)과 같은 2-4s 밴드 — **chat.json
  "ollama_host" 키 유지·값 삭제(빈 값)로 전환**(§7.16.1의 2aa-① 빈 값 = 기본
  localhost 계약의 실기기 적용). PC 릴레이 구성은 **조건부 원존**(L2 격리 해제 시
  재판정 가능 — 진입 #119 병합값이 백업 원문으로 보존됐었다).
- **전환 원문(드라이버 engine/tmp/prb3_transition_driver.sh — 원문 로그
  prb3_run.log)**: 폰 chat.json 진입 sha256 `b790f7334c3022a9c9020542c4092f
  7538534fcbdb0977cc1537c18ccbb14fe9`(149B — §7.16.4 병합 원문 등호) → 전환 sha
  `3424b488a726291e3cba06fa503e5cc55806c82d100f065de16703108ba339ca` —
  `{"engine":"ollama-direct","model":"glm-5.3-flash:cloud","directory":"…",
  "ollama_host":""}` — **keys 4 유지·engine/model/directory 등호(PY-VALID
  원문)+ollama_host 빈 값**(python json leg — python3 3.14.6 폰 원존; 셸 재조립
  폴백 leg도 드라이버 병기). 전환 후 /talk 재측정 1회 = **3.618s 유효 JSON**
  (talk Info — 모델 문장 원문)·스폰 argv OLLAMA_HOST 접두 0.
- **serve 계약 원장행(미수형 — 후속 과제)**: 데몬 DOWN = 즉발 stub 폴백(LLM 개입
  0 — 스파이크 §2 원문 계형). 상시화(부팅 스크립트 합병/예열 턴)는 다음 라인
  후보. **폰 사용 시 1커맨드 원존**: `ollama serve`(nohup 권장 —
  `nohup ollama serve >$TMPDIR/ollama_serve.log 2>&1 &`; stdin 웨지 함정 =
  스파이크 §3c 원존 — CLI 직접 `ollama run`은 stdin 닫힘 전제).
- **정산**: 스파이크 측정 부작용 Music 창 2건 = close_window 2건 소각(Taskbar만
  재실측)·서버/jkweb 진입=종료 등호·serve 진입 DOWN 복귀·폰 `TMPDIR` 잔상 0.
  본 전환 후 폰 chat.json 현재 = 빈 값 상태(원복 불요 — 승계된 구성).
- **EYES 갱신(§7.16.5 표 행 변형 없이)**: 게이트 ①(폰 릴레이 턴 체감)의 전제
  L2 격리는 해소되지 않았다 — 폰 단독 승계로 체감 대상이 **폰 로컬 턴 체감**
  (지연 2-4s 밴드·품질 = 행동 JSON/모델 문장)으로 바뀐다(사용자 선언만 결제 —
  게이트 ② LAN 노출 옵트인은 폰 단독 구성에서 무관해진다 — 11434 송수신 미개입).

### §7.17 JK APK v2 — 내장 전사 leg — NDK+whisper JNI+AudioRecord 구두 사슬 (#121 T1-T3, 2026-10-28)

플랜 = `docs/superpowers/plans/2026-10-28-jkapk-v2.md`·스펙 =
`docs/superpowers/specs/2026-10-25-jkapk-design.md`(v2 원장 부기 D6)·SDD 원장 =
`.superpowers/sdd/2026-10-28-jkapk-v2/`(task-1..3 brief/report). 원존 = docs/92
(v1 as-built — D1-D7 전환원장·조달기 원존)·#112 STT 구두(SttVoice kVoicePrompt/
kVoiceLanguage/wav 규약·R2/R5 원존)·#114 v1(/talk 조작 사슬 무변조 대상)·
#110 T5(폰 vendored whisper ninja). 라인 원존 재판정 사슬 — **#3117 격리 근거
원존의 정직 예외**: PC/폰 원존은 whisper-cli **별도 프로세스 스폰**+링크
금지인 바 — 그 금지 본질 = "호스트 프로세스가 whisper 결함으로 죽지 않는
격리"(폰 segfault 원존). APK는 Android 10+ W^X/targetSdk 34에서 cli exec
구조 불성립 → **APK 자체 프로세스 = 격리 본질 등호**(whisper 결함이 죽이는
대상이 APK 본인 — 호스트 데스크톱과 무관)라 full API 직접 링크 채택 =
원존 링크 금지의 **정직 예외**(근거 원문 = T1 리포트 §2·whisper_jni.c 헤더
주석 — vendored whisper.cpp v1.9.5(원존 깃허브 whisper.cpp — vendor 커밋
6708134 계보)·stdout 파서 계층 0(cli stdout 진실원 = 세그먼트 열 — API
직순회 승격)).

#### §7.17.0 체인 (rev-parse 전체 SHA 실측 · git log가 진실원)

| 태스크 | 커밋(SHA) | 내용 |
| --- | --- | --- |
| 플랜 | `2314b8b0ae93699a2fc97aaf5ffb94aea276f07c` | docs(apps): JK APK v2 내장 전사 플랜 (#121 — NDK+JNI+asset 전사 사슬) |
| T1 | `469de84dafed91d2ae75effab0bca52dff1a9497` | feat(android): NDK+whisper JNI 안장 (v2 T1) — 조달기 §6/§7+JNI+gradle 배선 |
| T2 | `607e2a5fd43d963ef8829a49ae424d3968c02bb9` | feat(android): APK 구두 버튼+전사 사슬 (v2 T2) — UTF-8 정리 계약+AudioRecord+R5 표기 |
| T2 fix r1 | `a0293b6bd69dba307552457e921beeaa2fa052c2` | fix(android): 권한 승인 무홀드 자동기동 철수+pcm 상한 (v2 T2) |
| T2 fix r2 | `f22dbe6a23484dd48d8dd0e34f1a5a1bb88a65ab` | fix(android): pcm 상한 도달 mic 해제 단일 소관 (v2 T2) |
| T3 probe | `ecf0508366ad792354322356973efa0a56b841dc` | test(probes): phone_apk v2 세그먼트 — prescan 함정 소각+fail-closed 3판+설치 md5 재판 |
| T3 docs | 본 커밋 | docs: #121 v2 원장 (T3) — docs/90 §7.17+§4.1-14 |

#### §7.17.1 사슬 원문 (D계약 승계 · D4-D6 재판정 등호)

- **사슬**: 조달기(`install_android_sdk.sh` §6 NDK r28.2.13676358·§7 tiny-ko
  asset sha256 핀 `dd6d9f46…` — 멱등 1.095s 원문) → gradle cmake vendored
  whisper.cpp 정적 링크 → JNI 1종 `jkapkTranscribe(wav, model, prompt)`
  (**UTF-8 정리 계약** — 세그먼트 연결 후·trim 전 `jk_utf8_clean` 1회:
  유효 시퀀스만 생존·orphan 바이트 소각(발명 0·U+FFFD 주입도 금지)·4바이트 =
  modified UTF-8 서러게이트 쌍 치환·CESU-8/overlong 소각·소각 후 결실 0 =
  정직 거부) → Kotlin `JniWhisper`(`@JvmStatic external`·kVoicePrompt 수형
  **123자 EQU** python 정밀 비교·Java 측 보조 원존 `sanitizeTranscript`
  단독 서러게이트 소각) → [구두] 버튼 push-to-talk(ACTION_DOWN=캡처 시작·
  UP/CANCEL=마침 — **R2 재판정**: 데스크톱 F6 hold-to-talk의 터치 직역·
  토글 기각 근거 헤더 원존 — 기각 시 즉시 수리 계약) → AudioRecord
  16 k/모노/s16le(VOICE_RECOGNITION)·WAV = 표면 소유 진실원(cacheDir+pid+
  발화 순서 접미 — 데스크톱 T3 원존 수형)·캐넌 44B 헤더(BuildWavBlob 수형)
  → R5 표기(녹음 중…/전사 중…+[나] placeholder 전사 도착 시 슬롯 치환 —
  logLines append-only 행열 확장·유일 변경점 = 치환) → 전사 = voiceExec
  별도 스레드(전사 수십 초가 /talk 사슬 비봉쇄) → **v1 조작 사슬 무변조**
  (doTalk/postTalk/jsonEsc/jsonGetString 기능 라인 변경 0 — 전사 원문 =
  타이핑 동형 sendPrepared 꼬리).
- **D계약 승계**: D4 무추적(NDK/모델/APK/자기검증 이진 전부 무추적 — 커밋 =
  조달기+소스만)·D5 설치 = 사용자 속 1회(폰 반입 = scp+bytes/md5 양측 등호
  봉인 런 — 절사 금지 — 본 라인 3건 전량 준수)·D6 = 본 라인으로 승격(tiny-ko
  74MB assets — sha256 추출 등호 fail-closed)·D7(외부 STT 앱 품질 승계 =
  v1 몫 — 본 라인은 내장 전사로 별개)·#112 마이크 권한 fail-closed(거부 =
  정직 거부 표기 — 조용한 개방 금지).

#### §7.17.2 성립 원문 (빌드·검증·폰 실측 — 봉인 로그 = 진실원)

- **정리부 부품 등호(2zb 계열 확장 — 라벨 2zb-5a..h)**: pure C(jni/whisper
  의존 0)라 `-DJKSANITIZE_SELFTEST_MAIN` 단독 컴파일 — **Win(gcc 16.2.0)/
  WSL(gcc)/폰 aarch64(NDK r28.2 cross) 3축 8/8 등호 RC=0**(T1 R2 실측 orphan
  `a5 bc` 소각·U+1F31A→서러게이트 쌍·전면 orphan 결실 0 전부 단정) — JNI
  전사 축은 비결정이라 어셜션 금지 원존 계약 준수(결정 부품만). Kotlin 측
  부품 단정 = overflow 뒤 `check(audioRecord == null)` fail-loud(fix r2).
- **fail-closed 3판 rc 원문**(stderr 파일 분리 수취 확정판 — 함정 원장 승계):
  A rc=0(tiny-ko+fixture — 결실 "." 수취·T1 원문 등호·29.8s)·B rc=1
  `jkapk wav 개방 실패:`·C rc=1 `jkapk wav 부적합 (RIFF/WAVE 아님)` — probe
  ⑧ 세그먼트로 승계(2런 전부 등호 재실측).
- **폰 반입 원문**(fix r1/r2 봉인 런): `bytes=77387398(양측) md5
  f73a5a26d08018e6f7fc4fdc1ce81f4c(양측 등호)` — 폰 실물 md5 사전 원문 수취 =
  이전 봉인(07a20f47) 등호로 재전송 불요 원장 확정 → clean 빌드 교체.
  **설치는 D5 사용자 속 1회** — 컨트롤러 안내 = f73a5a26 판.
- **/talk 조작 사슬**(v1 무변조 재실측 3건): POST /talk 지뢰찾기 켜줘 = 200·
  kind=Launch·app=minesweeper·답신 원문 등호 — 신규 창 id 42/8/9/22 실측
  등장 후 id 지명 소거·기존 창 무접촉·잔상 0.
- **probe 2런 원문**(phone_apk_run1/2.log — T3): run=1 MS-WIN-ID=9·run=2 id
  22 — 전부 WIN-DISPOSE=spawned-new | V2-INSTALL=77387398/f73a5a26…-EQ |
  V2-FC=A-0/B-1/C-1 | V2-SANITIZE=8of8 | V2-AOUT=. | V2-RESIDUAL=0 |
  STATUS=APK-PHONE-OK 이중 등호.
- **캐논 표기**: 엔진 무접촉(커밋 전원 = jkapk dir/조달기/probe만) — Win
  selftest **[PASS] 922·[FAIL] 0·AppSelfTest 0 failure(s)** 본 라인 2회
  재실측(jk-selftest-t2-canon.log)·WSL/posix/폰 engine 캐논 = 재실측 불요
  원존 승계(APK 별도 dir·별도 gradle 빌드 — 캐논 무영향 D-원존 — 상위 라인
  최종 계보 승계).

#### §7.17.3 함정 원장 (#121 T1-T3 — 봉쇄 레슨 축 · **T1 리포트 정정 원장 포함**)

1. **probe prescan rc 전파**(T3 소각 완료): 구 MSW_LIST의 Minesweeper 미매치
   rc=1이 prescan FAIL로 전파 — 부제 세션(정상 배치)을 agentctl 부재와 동일
   FAIL로 묶음 — 2런 연속 재현 → census(ok 게이트)와 필터 분리(ecf0508) —
   §7.17.2 probe 2런 등호 폐곡.
2. **T1 리포트 §4 링크 표기 정정 원장**: `-static-libstdc++`는 C 드라이버에서
   C++ runtime을 링크하지 않는다(17심볼 undefined 실측) — T1 자기검증 이진의
   실제 링크 = `-lm -lc++_static -lc++abi`(T2 r1 수형 — 이진 +2,416B 원존).
3. **stdout 버퍼 후행 순서 함정**: 파이프 결합(2>&1 | tail)에서 stderr 원문이
   stdout 마지막 라인(JKSANITIZE-PASS) 뒤로 밀려 fail-closed err 원문이 묻힘
   — **stderr는 파일 분리 수취 계약**(fix r1 확정판 — probe ⑧ 승계).
4. **권한 승인 무홀드 자동기동(fix r1 소각)**: 승인 분기 즉시 캡처 개방 =
   권한 다이얼로 손가락이 떨어진 홀드의 무인 녹음→잡음 전사 경로 — 승인 =
   상태 소거만(다음 홀드에서 시작).
5. **overflow mic 미해제(fix r2 소각)**: pcm 상한 도달 분기가 release 없이
   capturing=false만 — 마이크 계속 개방·다음 홀드가 구 AudioRecord 덮어쓰기
   → 캡처 해제 단일 소관 헬퍼 `releaseCaptureState` 1종에 봉함(finishVoice/
   overflow/onDestroy 3지점 동형)·pcm 상한 12 MB(도달 = 절단+정직 거부
   표기).
6. **UTF-8 orphan 수형 계약(T1 결함 수형 — T2)**: 전사 원문 raw bytes에
   orphan 실측(R2 선두 a5 bc·R3 중간 eb a7 — byte-fallback 토큰 파열) →
   NewStringUTF(modified UTF-8) invalid 수입 = RuntimeException 수렴(OOM
   유사 표기) — 유용 전사까지 사망 → T2 정리 계약(§7.17.1)으로 소각.
7. **폰 잔상 표기 원존**: toybox(ls) 미매치 = 종료코드 2 — `JK-RESIDUAL=2`
   = 잔상 0의 실측 표현(초기 원장 "JK-RESIDUAL=0" = 실원문 오기 — 정직
   정정 봉인)·scp 덮어쓰기는 +x를 회수한다(chmod 래치)·폰 /tmp 쓰기 불가
   (로그·산물 = ~/tmp).
8. **폰 재빌드 경합**: 런 중 타 세션의 폰 ninja 재빌드로 jkdesktop 사망
   관측(loadavg 10.5·c++ 컴파일 원문) — 정직 BLOCKED 2건 봉인 후 서버 복귀
   폴링 완수(타 세션 빌드 접촉 존중 원존)·폰 주소 이동(known_hosts 후보
   재판명 — 셀프판명 원존 승계 — 주소 인쇄 0 유지).

#### §7.17.4 EYES 게이트 (사용자 선언만 결제 — EYES-PENDING)

| # | 게이트 | 내용 | 상태 |
| --- | --- | --- | --- |
| ① | **폰 단독 전사 체감** | JK 앱 [구두] **홀드 발화→해제**(첫 홀드 = 권한 승인 1회 — 승인 후 다시 홀드 — fix r1 계약) → "녹음 중…"→"전사 중…"(29.8-47s 대기 — tiny-ko 폰 실측) → 기록 [나] 전사 원문 치환 → DeX 서버 있으면 /talk 조작·부재 시 "서버 미기동" 정직 표기 — **폰 단독(서버 부재 상태) 전사·대화 표기 = 플랜 사용자 게이트 1** | **대기(육안·체감)** |
| ② | **전사 품질 — tiny-ko 어휘 prompt** | 실발화(지뢰찾기 켜줘 계열) 전사 정확도 — kVoicePrompt(123자 등호 수형) 적용 효과 — 스펙 사용자 게이트 2·**합성 fixture는 tiny-ko 결실 "." 3판 원장(T1 §5 정직 — 합성= TTS 기계음 CANON 밖)이라 실발화로만 폐곡 가능** — small-komixv2 전환(~490 MB — asset 외부화 재판정 동행) = 사용자 게이트 3 | **대기(청안)** |
| ③ | **APK 모양+버튼 형태** | [구두] 홀드형(토글 기각 원존 — 헤더 주석)·R5 표기 2행·전사 대기 수십 초 체감 — 기각 시 즉시 수리 계약 | **대기(육안)** |
| ④ | **설치 판정** | 폰 Download 최종판 = f73a5a26(봉인 런 원문) — 사용자 설치(덮어쓰기 — 이전 판(07a20f47/2c3513e7) 설치 이력 있으면 재설치)·설치/런치/구두 1회 | **대기(사용자 속)** |

#### §7.17.5 커밋 원장 + IP/호스트 경로 게이트 (#121 라인)

- 커밋 = 명시 경로만(`git add -A` 금지·amend 0·push origin main 직행) — 경로
  원문 = §7.17.0 표와 등호·전부 트레일러
  Co-Authored-By: Claude Code <noreply@anthropic.com>.
- **IP·폰 주소 게이트**: 신설 소스 = engine 소유 원존(vendored whisper.cpp
  재용·#112 어휘 리터럴 수형)·SAPI fixture/모델 ggml/APK/자기검증 이진
  전부 무추적 커밋 0·폰 주소 = known_hosts BatchMode 셀프판명(SELFRESOLVE:
  MATCH 행만 인쇄 — 리포트/커밋/문서 인쇄 0)·폰 모델 경로(buildterm/
  stt-models)는 주소가 아닌 절대 경로라 게이트 밖.
- 본 문서 원문 IPv4 리터럴 소유 0건(커밋 전 점문 grep 게이트).

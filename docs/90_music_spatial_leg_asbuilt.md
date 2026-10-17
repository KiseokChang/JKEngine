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

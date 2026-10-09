# 뮤직 spatial leg — audio_core 연동 (#91) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** music 앱에 내장 spatial 재생 leg — spatial-player의 audio_core(mp3/flac/ogg/wav 스트리밍+POSITIONAL HRTF)를 [spatial] 버튼 채널로 연동.

**Architecture:** `SPATIAL_PLAYER_ROOT` env로 add_subdirectory(EXCLUDE_FROM_ALL)+audio_core 정적 링크 — 미설정/폰=빌드 생략(vplayer 위임 유지·무손상). 재생은 StreamPlayer pump()를 OnIdle 폴링 — idle 계약(docs/88) 준수.

**Spec:** docs/superpowers/specs/2026-10-10-music-spatial-leg-design.md

## Global Constraints

- spatial-player 저장소 **무변경**(소비자 입장 — D 계약). 커밋파일에 그 경로 기입 금지(env만) — IP 게이트 동급 취급.
- 원존 계약: 더블클릭=vplayer 위임(#90 D1) 무변조·[spatial]이 2채널 공존.
- idle 계약(#89 docs/88): 재생 중 진행 표기 갱신만 더티 — pump 자체는 무더티(변화 있을 때만).
- 캐논(현 Win 614/WSL 591/posix 296/폰 591) — 신설 `2n` 계보 표기 의무.
- 커밋: 명시 경로만+`--stat` 검증+rev-parse+origin main 직행+amend 금지+Co-Authored-By 트레일러. IP 게이트 0건. 폰 ssh 수속 계약(known_hosts self-resolve·파일 기록 금지·$TMPDIR·pkill 브래킷·`< /dev/null` 금지·jkweb 절사 금지·서버 존중).
- Windows 빌드 `ninja -C engine/build -j3`; WSL `ninja -C buildwsl -j3`+selftest WSL 내부 리다이렉트; posix 풀 로그.
- 테스트 미디어: i:\@keep 금지 — 합성(mp3/wav — ffmpeg 또는 audio_core 지원 포맷 생성)만.
- 서브에이전트 스폰 금지(구현자). 첫 configure의 openal-soft FetchContent 네트워크 소요 — 원장 표기.

---

### Task 1: leg 골격 — 순수 부품+2n selftest+CMake fail-closed 배선

**Files:**
- Create: `engine/include/apps/MusicSpatialLeg.h` — `jk::music::leg` 네임스페이스:
  - `struct LegSupport { enum class State {Unsupported, Supported}; static State OfExt(const std::string& path); };` — 확장자 대소문자 무시: mp3/flac/ogg/ogg vorbis/wav=Supported, m4a/aac/wma=Unsupported(스펙 D4 표).
  - `enum class LegEvent { Start, StopRequested, DeviceFailed, Eos };`
  - `struct LegState { bool active=false; bool positional=true; bool deviceOk=false; std::string err; std::string path; double posSec=0; double durSec=0; };` + 순수 전이 `LegState Apply(const LegState&, LegEvent);` (Start→active+deviceOk 전제, DeviceFailed→active 해제+err 기록·vplayer 위임 안내 플래그 등 UI 라벨 소비 계약 — 리포트에 명세).
  - `std::string ToolJson(const std::string& tool, const std::string& path="")` — spatial_play/stop/status 3종의 app_tool 요청 원문(앱 도구 허브 계약 — 2m-g OpenRequestJson 쌍둥이).
- Modify: `engine/src/main.cpp` — 2m-h 아래 **2n** selftest: ①OfExt 6케이스(대소문자·숨김 확장자·빈 문자열) ②Apply 상태머신 4전이(Start/DeviceFailed/Eos/StopRequested) ③ToolJson 3종 원문 ④포맷 표 정합(D4 — 지원 5·미지원 3) — 2n 14건 부근(실수 재량·계보 표기).
- Modify: `engine/CMakeLists.txt` — `SPATIAL_PLAYER_ROOT` env **설정 시에만**:
  ```cmake
  if(DEFINED ENV{SPATIAL_PLAYER_ROOT})
      add_subdirectory("$ENV{SPATIAL_PLAYER_ROOT}" "${CMAKE_BINARY_DIR}/spatial-player-build" EXCLUDE_FROM_ALL)
      target_link_libraries(jkapp_music PRIVATE audio_core)
  endif()
  ```
  (audio_core PUBLIC 헤더 경로가 전파 — jkapp_music 소스의 leg include는 매크로 가드 아래: `JK_MUSIC_SPATIAL_LEG` — CMake에서 설정 시에만 `target_compile_definitions`. env 미설정 빌드의 leg 상태=전원 Unsupported.)
- [ ] **Step 1:** 2n selftest 원문(main.cpp 2m-h 아래)
- [ ] **Step 2:** MusicSpatialLeg.h 구현
- [ ] **Step 3:** CMake 배선(위 조각)+**미설정 빌드 2회 실측**(env 미설정=RC=0·leg 빌드 생략 — fail-closed 원장)+**설정 빌드 1회**(audio_core+openal-soft 컴파일 원문 — FetchContent 네트워크 첫 소요 원장)
- [ ] **Step 4:** selftest 3축 — 캐논 Win 614→(2n n)/WSL 591→(2n n) 계보 표기(posix 296 등호 사유 — 하네스 TU 분석)
- [ ] **Step 5:** 커밋 `feat(apps): music spatial leg 골격+2n+CMake fail-closed 배선 (T1)`

### Task 2: leg 구현 — 재생 채널+UI+도구 등록

**Files:**
- Modify: `engine/src/apps/ClientMusicApp.cpp`(+h) — ①`JK_MUSIC_SPATIAL_LEG` 가드 하 audio_core 소비: `BufferQueueStreamPlayer` 인스턴스+`SpatialEngine` 초기화(ALC fail → LegEvent::DeviceFailed+status 표기+폴백 안내) ②[spatial] 버튼(행별 — LegSupport 표) ③슬라이더 3조(방위각 -180..180·거리 0.5..20·고도 -45..45 — 재생 중 활성) ④pump() OnIdle 폴링+재생 중 진행 표기 갱신만 더티(#89 계약·vplayer 수형 동형) ⑤app 도구 등록 3종(T1 ToolJson 소비) ⑥미지원 행 회색+툴팁.
- **Interfaces:** Consumes: T1 MusicSpatialLeg.h·audio_core(`Decoder::open`/`BufferQueueStreamPlayer`/`SpatialEngine`). Produces: app 도구 `spatial_play{path}·spatial_stop·spatial_status`(probe/도구 허브 계약).
- Test: selftest — 신설 어설션 없음(T1이 순수 부품 전부 소유) — 캐논 등호(leg 가드 컴파일 변형 실측).
- [ ] **Step 1:** leg 구현+UI+도구
- [ ] **Step 2:** 빌드 2변형(env 설정/미설정 — **양쪽 RC=0 필수**)+selftest 3축 계보
- [ ] **Step 3:** 커밋 `feat(apps): music spatial 재생 leg+UI+도구 3종 (T2)`

### Task 3: WSL probe 확장 — spatial leg 실측

**Files:**
- Modify: `engine/tools/probes/wsl_music.sh` — ①(기존 위임 실측 유지) ②**spatial 실측 세그먼트 신설:** SPATIAL_PLAYER_ROOT 설정 빌드 배포 → music 런치 → `app_tool` `spatial_play`(합성 wav/mp3 — mp3는 ffmpeg로 합성·lame 여부 실측 — 없으면 wav) → spatial_status 재생 수치 원문(pos 진행) → spatial_stop → 상태 원문. ③기존 vplayer 위임 회귀 확인(더블클릭/절차 원문 유지).
- [ ] **Step 1:** probe 확장+실행(풀 로그) 2변형(env 설정 런 — leg OK 원문)
- [ ] **Step 2:** 커밋 `test(probes): music WSL probe spatial leg 실측 확장 (T3)`

### Task 4: 폰 probe — 캐논 흡수+leg 배제 원문

**Files:**
- Modify: `engine/tools/probes/phone_music.sh` — ①폰 selftest 캐논(591→+2n) ②**spatial 버튼 미활성 표기 원문(D5 정직 계약)** ③기존 위임 재실측(회귀 확인).
- [ ] **Step 1:** probe 확장+폰 실측(SWEEP-DIFF=0 sweep canary 계약)
- [ ] **Step 2:** 커밋 `test(probes): music 폰 probe — spatial leg 배제+캐논 흡수 (T4)`

### Task 5: as-built docs/90

- Create: `docs/90_music_spatial_leg_asbuilt.md`(docs/89 문체) — §0 체인·§1 배선 원장(env 계약 원문)·§2 캐논+leg 실측 표·§3 함정(봉투/본문 원장 승계+신규)·§4 deferred·§5 EYES(청안 게이트). Modify: docs/89 §6 형제 포인터.
- [ ] **Step 1:** docs/90 작성 2) 커밋 `docs(apps): music spatial leg as-built (T5)`

## 사용자 게이트 요약 (플랜 밖)

1. 청안 게이트(WSL/Win에서 mp3/flac HRTF 위치감 — 슬라이더) = 사용자 선언만 결제.
2. 폰 leg 정착(OpenAL/디바이스 백엔드 재판정) — 청안 후 후속 판정.
3. 결정 2건(내장·fail-closed) — 스펙 확정·기각 시 즉시 수리.